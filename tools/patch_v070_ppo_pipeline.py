from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


header = Path("src/ppo.hpp")
text = header.read_text(encoding="utf-8")
text = replace_once(
    text,
    "#include <string_view>\n#include <vector>\n",
    "#include <string_view>\n#include <thread>\n#include <vector>\n",
    "thread include",
)
text = replace_once(
    text,
    """        void train_one_update();
        void step_preview(float dt = 1.0f / 60.0f);
""",
    """        void train_one_update();

        // Staged update API used by the C++23 coroutine scheduler. Dispatch calls
        // return immediately; wait calls block only the training-owner thread while
        // persistent workers run. No UI-facing thread enters these waits.
        void begin_update();
        [[nodiscard]] bool wait_for_rollout(std::stop_token stop_token);
        void finish_rollout();
        void compute_advantages();
        void begin_policy_update();
        [[nodiscard]] bool policy_update_complete() const noexcept;
        [[nodiscard]] bool wait_for_policy_batch(std::stop_token stop_token);
        void finish_policy_batch();
        void finalize_update_metrics();
        [[nodiscard]] bool evaluation_due() const noexcept;
        void begin_evaluation();
        [[nodiscard]] bool wait_for_evaluation(std::stop_token stop_token);
        void finish_evaluation();

        void step_preview(float dt = 1.0f / 60.0f);
""",
    "staged public API",
)
text = replace_once(
    text,
    """        struct ParallelState;

        [[nodiscard]] float random_uniform() noexcept;
""",
    """        struct ParallelState;

        struct PolicyUpdateProgress
        {
            std::vector<std::size_t> indices{};
            std::size_t epoch{};
            std::size_t begin{};
            std::size_t end{};
            std::size_t sample_count{};
            float total_policy_loss{};
            float total_value_loss{};
            float total_entropy{};
            bool batch_pending{};
            bool complete{ true };
        };

        [[nodiscard]] float random_uniform() noexcept;
""",
    "policy update progress",
)
text = replace_once(
    text,
    """        void update_policy();
        void evaluate_policy();
        void reset_training_state(bool clear_best = true) noexcept;
""",
    """        void shuffle_policy_indices();
        void dispatch_policy_batch();
        void reset_training_state(bool clear_best = true) noexcept;
""",
    "replace monolithic policy helpers",
)
text = replace_once(
    text,
    """        void parallel_accumulate_batch(
            const std::vector<std::size_t>& indices,
            std::size_t begin,
            std::size_t end,
            float clip_range,
            float value_coefficient,
            float entropy_coefficient,
            float& policy_loss,
            float& value_loss,
            float& entropy);
        void parallel_evaluate_policy();
""",
    """        void dispatch_parallel_gradient_batch(
            const std::vector<std::size_t>& indices,
            std::size_t begin,
            std::size_t end,
            float clip_range,
            float value_coefficient,
            float entropy_coefficient);
        [[nodiscard]] bool wait_parallel_job(std::stop_token stop_token);
        void finish_parallel_gradient_batch(
            float& policy_loss, float& value_loss, float& entropy);
        [[nodiscard]] bool dispatch_parallel_evaluation();
        void finish_parallel_evaluation();
""",
    "asynchronous parallel helpers",
)
text = replace_once(
    text,
    """        std::condition_variable rollout_done_cv_{};
        std::uint64_t rollout_generation_{};
""",
    """        std::condition_variable_any rollout_done_cv_{};
        std::uint64_t rollout_generation_{};
""",
    "stop-aware rollout completion",
)
text = replace_once(
    text,
    """        std::vector<std::jthread> rollout_workers_{};
        std::shared_ptr<ParallelState> parallel_{};
""",
    """        std::vector<std::jthread> rollout_workers_{};
        std::shared_ptr<ParallelState> parallel_{};

        RolloutTotals current_rollout_totals_{};
        std::uint64_t current_update_seed_{};
        bool update_active_{};
        bool rollout_pending_{};
        PolicyUpdateProgress policy_update_{};
        bool evaluation_pending_{};
        bool serial_parallel_ready_{};
        float serial_policy_loss_{};
        float serial_value_loss_{};
        float serial_entropy_{};
""",
    "pipeline state fields",
)
header.write_text(text, encoding="utf-8")

trainer = Path("src/ppo_trainer.cpp")
text = trainer.read_text(encoding="utf-8")
start = text.index("    void PpoTrainer::train_one_update()")
end = text.index("\n\n    bool PpoTrainer::restore_best_policy()", start)
staged = r'''    void PpoTrainer::begin_update()
    {
        if (update_active_)
            return;

        const std::size_t environment_count = environments_.size();
        rollout_.assign(rollout_horizon * environment_count, Transition{});
        current_rollout_totals_ = {};
        current_update_seed_ = random_state_
            ^ (metrics_.update + 1u) * 0x9E3779B97F4A7C15ULL;
        update_active_ = true;

        if (rollout_workers_.empty())
        {
            current_rollout_totals_ = collect_rollout_partition(0, 1, current_update_seed_);
            rollout_pending_ = false;
            return;
        }

        {
            std::scoped_lock lock(rollout_mutex_);
            rollout_completed_ = 0;
            rollout_update_seed_ = current_update_seed_;
            rollout_active_worker_count_ = std::min(active_worker_count_, rollout_worker_count_);
            ++rollout_generation_;
        }
        rollout_pending_ = true;
        rollout_start_cv_.notify_all();
    }

    bool PpoTrainer::wait_for_rollout(std::stop_token stop_token)
    {
        if (!rollout_pending_)
            return true;
        std::unique_lock lock(rollout_mutex_);
        return rollout_done_cv_.wait(lock, stop_token, [this]
        {
            return rollout_completed_ == rollout_worker_count_;
        });
    }

    void PpoTrainer::finish_rollout()
    {
        if (!update_active_)
            return;
        if (rollout_pending_)
        {
            current_rollout_totals_ = {};
            for (const RolloutTotals& worker : rollout_worker_totals_)
            {
                current_rollout_totals_.accumulated_speed += worker.accumulated_speed;
                current_rollout_totals_.completed_reward += worker.completed_reward;
                current_rollout_totals_.completed_distance += worker.completed_distance;
                current_rollout_totals_.completed_episodes += worker.completed_episodes;
            }
            rollout_pending_ = false;
        }
        random_state_ ^= current_update_seed_ + 0xA0761D6478BD642FULL;
    }

    void PpoTrainer::compute_advantages()
    {
        constexpr float gamma = 0.995f;
        constexpr float gae_lambda = 0.95f;
        const std::size_t environment_count = environments_.size();

        std::vector<float> next_values(environment_count, 0.0f);
        for (std::size_t environment_index = 0; environment_index < environment_count; ++environment_index)
            next_values[environment_index] = policy_.evaluate(environments_[environment_index].observation()).value;

        for (std::size_t environment_index = 0; environment_index < environment_count; ++environment_index)
        {
            float next_value = next_values[environment_index];
            float next_advantage = 0.0f;
            for (std::size_t reverse = rollout_horizon; reverse-- > 0;)
            {
                Transition& transition = rollout_[reverse * environment_count + environment_index];
                const float continuation = transition.terminal ? 0.0f : 1.0f;
                const float delta = transition.reward + gamma * next_value * continuation - transition.value;
                transition.advantage = delta + gamma * gae_lambda * continuation * next_advantage;
                transition.return_value = transition.advantage + transition.value;
                next_advantage = transition.advantage;
                next_value = transition.value;
            }
        }

        float mean_advantage = 0.0f;
        for (const Transition& transition : rollout_)
            mean_advantage += transition.advantage;
        mean_advantage /= static_cast<float>(rollout_.size());
        float variance = 0.0f;
        for (const Transition& transition : rollout_)
        {
            const float delta = transition.advantage - mean_advantage;
            variance += delta * delta;
        }
        const float inverse_std = 1.0f
            / std::sqrt(variance / static_cast<float>(rollout_.size()) + 1.0e-6f);
        for (Transition& transition : rollout_)
            transition.advantage = (transition.advantage - mean_advantage) * inverse_std;
    }

    void PpoTrainer::shuffle_policy_indices()
    {
        for (std::size_t index = policy_update_.indices.size(); index > 1; --index)
        {
            const std::size_t other = static_cast<std::size_t>(
                random_uniform() * static_cast<float>(index));
            std::swap(policy_update_.indices[index - 1],
                policy_update_.indices[std::min(other, index - 1)]);
        }
    }

    void PpoTrainer::dispatch_policy_batch()
    {
        constexpr std::size_t minibatch_size = 256;
        constexpr float clip_range = 0.20f;
        constexpr float value_coefficient = 0.50f;
        constexpr float entropy_coefficient = 0.0020f;

        policy_update_.end = std::min(
            policy_update_.indices.size(), policy_update_.begin + minibatch_size);
        dispatch_parallel_gradient_batch(
            policy_update_.indices,
            policy_update_.begin,
            policy_update_.end,
            clip_range,
            value_coefficient,
            entropy_coefficient);
        policy_update_.batch_pending = true;
    }

    void PpoTrainer::begin_policy_update()
    {
        policy_update_ = {};
        policy_update_.indices.resize(rollout_.size());
        std::iota(policy_update_.indices.begin(), policy_update_.indices.end(), 0);
        policy_update_.complete = policy_update_.indices.empty();
        if (policy_update_.complete)
            return;
        shuffle_policy_indices();
        dispatch_policy_batch();
    }

    bool PpoTrainer::policy_update_complete() const noexcept
    {
        return policy_update_.complete;
    }

    bool PpoTrainer::wait_for_policy_batch(std::stop_token stop_token)
    {
        return !policy_update_.batch_pending || wait_parallel_job(stop_token);
    }

    void PpoTrainer::finish_policy_batch()
    {
        constexpr std::size_t epochs = 4;
        constexpr float max_gradient_norm = 0.70f;
        if (!policy_update_.batch_pending || policy_update_.complete)
            return;

        float batch_policy_loss = 0.0f;
        float batch_value_loss = 0.0f;
        float batch_entropy = 0.0f;
        finish_parallel_gradient_batch(
            batch_policy_loss, batch_value_loss, batch_entropy);
        policy_update_.batch_pending = false;

        const std::size_t batch_samples = policy_update_.end - policy_update_.begin;
        const float inverse_batch = 1.0f / static_cast<float>(batch_samples);
        float norm_squared = 0.0f;
        for (const float gradient : policy_.gradients())
        {
            const float scaled = gradient * inverse_batch;
            norm_squared += scaled * scaled;
        }
        const float norm = std::sqrt(norm_squared);
        const float clip_scale = norm > max_gradient_norm
            ? max_gradient_norm / norm
            : 1.0f;
        const float learning_rate = metrics_.learning_rate
            * std::max(0.10f, 1.0f - static_cast<float>(metrics_.update) / 5000.0f);
        apply_adam(learning_rate, inverse_batch * clip_scale);

        policy_update_.total_policy_loss += batch_policy_loss;
        policy_update_.total_value_loss += batch_value_loss;
        policy_update_.total_entropy += batch_entropy;
        policy_update_.sample_count += batch_samples;
        policy_update_.begin = policy_update_.end;

        if (policy_update_.begin >= policy_update_.indices.size())
        {
            ++policy_update_.epoch;
            if (policy_update_.epoch >= epochs)
            {
                const float inverse_samples = policy_update_.sample_count > 0
                    ? 1.0f / static_cast<float>(policy_update_.sample_count)
                    : 0.0f;
                metrics_.policy_loss = policy_update_.total_policy_loss * inverse_samples;
                metrics_.value_loss = policy_update_.total_value_loss * inverse_samples;
                metrics_.entropy = policy_update_.total_entropy * inverse_samples;
                policy_update_.complete = true;
                return;
            }
            policy_update_.begin = 0;
            shuffle_policy_indices();
        }
        dispatch_policy_batch();
    }

    void PpoTrainer::finalize_update_metrics()
    {
        if (!update_active_)
            return;
        ++metrics_.update;
        metrics_.environment_steps += rollout_.size();
        metrics_.mean_speed = current_rollout_totals_.accumulated_speed
            / static_cast<float>(rollout_.size());
        if (current_rollout_totals_.completed_episodes > 0)
        {
            metrics_.mean_reward = current_rollout_totals_.completed_reward
                / static_cast<float>(current_rollout_totals_.completed_episodes);
            metrics_.mean_episode_distance = current_rollout_totals_.completed_distance
                / static_cast<float>(current_rollout_totals_.completed_episodes);
        }
        else
        {
            const float partial_reward = std::accumulate(
                episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
            metrics_.mean_reward = partial_reward / static_cast<float>(episode_rewards_.size());
        }
        append_history(reward_history_, metrics_.mean_reward);
        append_history(speed_history_, metrics_.mean_speed * 3.6f);
        controller_state_ = ControllerState::training;
        update_active_ = false;
    }

    bool PpoTrainer::evaluation_due() const noexcept
    {
        return metrics_.update == 1 || metrics_.update % 5 == 0;
    }

    void PpoTrainer::begin_evaluation()
    {
        evaluation_pending_ = dispatch_parallel_evaluation();
    }

    bool PpoTrainer::wait_for_evaluation(std::stop_token stop_token)
    {
        return !evaluation_pending_ || wait_parallel_job(stop_token);
    }

    void PpoTrainer::finish_evaluation()
    {
        if (!evaluation_pending_)
            return;
        finish_parallel_evaluation();
        evaluation_pending_ = false;
    }

    void PpoTrainer::train_one_update()
    {
        begin_update();
        if (!wait_for_rollout({}))
            return;
        finish_rollout();
        compute_advantages();
        begin_policy_update();
        while (!policy_update_complete())
        {
            if (!wait_for_policy_batch({}))
                return;
            finish_policy_batch();
        }
        finalize_update_metrics();
        if (evaluation_due())
        {
            begin_evaluation();
            if (!wait_for_evaluation({}))
                return;
            finish_evaluation();
        }
    }
'''
text = text[:start] + staged + text[end:]

policy_start = text.index("    void PpoTrainer::update_policy()")
policy_end = text.index("\n\n    void PpoTrainer::apply_adam", policy_start)
text = text[:policy_start] + text[policy_end + 2:]
trainer.write_text(text, encoding="utf-8")

parallel = Path("src/ppo_parallel.cpp")
text = parallel.read_text(encoding="utf-8")
text = text.replace("std::condition_variable done_cv{};", "std::condition_variable_any done_cv{};")
start = text.index("    void PpoTrainer::parallel_accumulate_batch(")
end = text.rindex("\n}")
replacement = r'''    void PpoTrainer::dispatch_parallel_gradient_batch(
        const std::vector<std::size_t>& batch_indices,
        std::size_t batch_begin,
        std::size_t batch_end,
        float batch_clip_range,
        float batch_value_coefficient,
        float batch_entropy_coefficient)
    {
        serial_parallel_ready_ = false;
        serial_policy_loss_ = 0.0f;
        serial_value_loss_ = 0.0f;
        serial_entropy_ = 0.0f;

        if (!parallel_ || parallel_->worker_count == 0)
        {
            policy_.zero_gradients();
            for (std::size_t cursor = batch_begin; cursor < batch_end; ++cursor)
            {
                const Transition& transition = rollout_[batch_indices[cursor]];
                policy_.accumulate_gradient(
                    transition.observation,
                    transition.action,
                    transition.log_probability,
                    transition.advantage,
                    transition.return_value,
                    batch_clip_range,
                    batch_value_coefficient,
                    batch_entropy_coefficient,
                    serial_policy_loss_,
                    serial_value_loss_,
                    serial_entropy_);
            }
            serial_parallel_ready_ = true;
            return;
        }

        ParallelState& state = *parallel_;
        {
            std::scoped_lock lock(state.mutex);
            state.job = ParallelState::Job::gradient;
            state.completed = 0;
            state.active_workers = std::min(active_worker_count_, state.worker_count);
            state.indices = &batch_indices;
            state.begin = batch_begin;
            state.end = batch_end;
            state.clip_range = batch_clip_range;
            state.value_coefficient = batch_value_coefficient;
            state.entropy_coefficient = batch_entropy_coefficient;
            ++state.generation;
        }
        state.start_cv.notify_all();
    }

    bool PpoTrainer::wait_parallel_job(std::stop_token stop_token)
    {
        if (serial_parallel_ready_ || !parallel_ || parallel_->worker_count == 0)
            return true;
        ParallelState& state = *parallel_;
        std::unique_lock lock(state.mutex);
        return state.done_cv.wait(lock, stop_token, [&state]
        {
            return state.completed == state.worker_count;
        });
    }

    void PpoTrainer::finish_parallel_gradient_batch(
        float& policy_loss, float& value_loss, float& entropy)
    {
        if (serial_parallel_ready_)
        {
            policy_loss = serial_policy_loss_;
            value_loss = serial_value_loss_;
            entropy = serial_entropy_;
            serial_parallel_ready_ = false;
            return;
        }
        if (!parallel_ || parallel_->worker_count == 0)
            return;

        ParallelState& state = *parallel_;
        policy_.zero_gradients();
        std::vector<float>& reduced = policy_.gradients();
        for (std::size_t worker = 0; worker < state.active_workers; ++worker)
        {
            const std::vector<float>& local = state.local_policies[worker].gradients();
            for (std::size_t index = 0; index < reduced.size(); ++index)
                reduced[index] += local[index];
            policy_loss += state.gradient_totals[worker].policy_loss;
            value_loss += state.gradient_totals[worker].value_loss;
            entropy += state.gradient_totals[worker].entropy;
        }
    }

    bool PpoTrainer::dispatch_parallel_evaluation()
    {
        constexpr std::size_t evaluation_agents = 6;
        if (!parallel_ || parallel_->worker_count == 0)
            return false;

        ParallelState& state = *parallel_;
        {
            std::scoped_lock lock(state.mutex);
            state.job = ParallelState::Job::evaluation;
            state.completed = 0;
            state.active_workers = std::min({ active_worker_count_, state.worker_count, evaluation_agents });
            state.stage = course_stage_;
            state.difficulty = course_difficulty_;
            ++state.generation;
        }
        state.start_cv.notify_all();
        return true;
    }

    void PpoTrainer::finish_parallel_evaluation()
    {
        constexpr std::size_t evaluation_agents = 6;
        if (!parallel_ || parallel_->worker_count == 0)
            return;

        ParallelState& state = *parallel_;
        ParallelState::EvaluationTotals totals{};
        for (std::size_t worker = 0; worker < state.active_workers; ++worker)
        {
            const ParallelState::EvaluationTotals& local = state.evaluation_totals[worker];
            totals.reward += local.reward;
            totals.distance += local.distance;
            totals.speed += local.speed;
            totals.survival += local.survival;
            totals.collisions += local.collisions;
            totals.airborne += local.airborne;
            totals.strides += local.strides;
            totals.speed_samples += local.speed_samples;
            totals.invalid_runs += local.invalid_runs;
        }

        const float inverse_agents = 1.0f / static_cast<float>(evaluation_agents);
        metrics_.evaluation_reward = totals.reward * inverse_agents;
        metrics_.evaluation_distance = totals.distance * inverse_agents;
        metrics_.evaluation_speed = totals.speed_samples > 0
            ? totals.speed / static_cast<float>(totals.speed_samples)
            : 0.0f;
        metrics_.evaluation_survival = totals.survival * inverse_agents;
        metrics_.evaluation_collisions = totals.collisions * inverse_agents;
        metrics_.evaluation_airborne_ratio = totals.airborne * inverse_agents;
        metrics_.evaluation_stride_events = totals.strides * inverse_agents;
        metrics_.evaluation_invalid_runs = totals.invalid_runs;
        metrics_.evaluation_valid = totals.invalid_runs == 0;

        if (course_stage_ == sim::CourseStage::balance)
        {
            metrics_.evaluation_score = metrics_.evaluation_valid
                ? metrics_.evaluation_survival * 0.10f + metrics_.evaluation_reward
                    - std::abs(metrics_.evaluation_distance) * 0.20f
                : -1000.0f - static_cast<float>(totals.invalid_runs) * 100.0f;
        }
        else
        {
            metrics_.evaluation_score = metrics_.evaluation_valid
                ? metrics_.evaluation_reward + metrics_.evaluation_distance * 0.75f
                    + metrics_.evaluation_survival * 0.025f
                    + metrics_.evaluation_stride_events * 0.03f
                    - metrics_.evaluation_collisions * 0.18f
                    - metrics_.evaluation_airborne_ratio * 0.75f
                : -1000.0f - static_cast<float>(totals.invalid_runs) * 100.0f;
        }
        ++metrics_.evaluation_count;

        if (metrics_.evaluation_valid
            && (best_parameters_.empty() || metrics_.evaluation_score > metrics_.best_evaluation_score))
        {
            best_parameters_ = policy_.parameters();
            metrics_.best_evaluation_distance = metrics_.evaluation_distance;
            metrics_.best_evaluation_score = metrics_.evaluation_score;
            metrics_.best_update = metrics_.update;
        }
    }
'''
text = text[:start] + replacement + "}\n"
parallel.write_text(text, encoding="utf-8")
