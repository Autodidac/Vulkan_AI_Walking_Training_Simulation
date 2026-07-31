#include "ppo.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <thread>
#include <vector>

namespace epochrunner::rl
{
    namespace
    {
        constexpr float epsilon = 1.0e-8f;

        [[nodiscard]] float next_uniform(std::uint64_t& state) noexcept
        {
            state ^= state >> 12;
            state ^= state << 25;
            state ^= state >> 27;
            const std::uint64_t value = state * 2685821657736338717ULL;
            return std::max(1.0e-7f, static_cast<float>(value >> 40) * (1.0f / 16777216.0f));
        }

        [[nodiscard]] float next_normal(std::uint64_t& state) noexcept
        {
            const float u1 = next_uniform(state);
            const float u2 = next_uniform(state);
            return std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * pi * u2);
        }
    }

    PpoTrainer::PpoTrainer(const sim::CreatureBlueprint& blueprint, std::size_t environment_count,
            bool enable_rollout_workers)
            : blueprint_(blueprint), preview_(blueprint, 0xDEADBEEFu), policy_(0xC0FFEEu)
        {
            environment_count = std::clamp<std::size_t>(environment_count, 8, 256);
            const std::size_t hardware = std::max<std::size_t>(1, std::thread::hardware_concurrency());
            const std::size_t available = hardware > 2 ? hardware - 2 : hardware;
            rollout_worker_count_ = enable_rollout_workers
                ? std::clamp<std::size_t>(available, 1, std::min<std::size_t>(16, environment_count))
                : 0;

            environments_.reserve(environment_count);
            for (std::size_t index = 0; index < environment_count; ++index)
            {
                environments_.emplace_back(blueprint_, 0x1000u + index * 7919u);
                environments_.back().set_course(course_stage_, course_difficulty_);
            }
            preview_.set_course(course_stage_, course_difficulty_);
            episode_rewards_.assign(environment_count, 0.0f);
            episode_distances_.assign(environment_count, 0.0f);
            adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
            adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
            rollout_worker_totals_.resize(rollout_worker_count_);
            rollout_workers_.reserve(rollout_worker_count_);
            for (std::size_t worker = 0; worker < rollout_worker_count_; ++worker)
            {
                rollout_workers_.emplace_back([this, worker](std::stop_token stop_token)
                {
                    rollout_worker_main(worker, stop_token);
                });
            }
            set_cpu_mode(4);
            initialize_parallel_workers();
        }


    PpoTrainer::~PpoTrainer()
        {
            shutdown_parallel_workers();
            for (std::jthread& worker : rollout_workers_)
                worker.request_stop();
            rollout_start_cv_.notify_all();
            rollout_workers_.clear();
        }


    PpoTrainer::RolloutTotals PpoTrainer::collect_rollout_partition(std::size_t worker_index,
        std::size_t worker_count, std::uint64_t update_seed)
    {
        RolloutTotals totals{};
        std::uint64_t local_random = update_seed ^ (worker_index + 1u) * 0xD1B54A32D192ED03ULL;
        const std::size_t environment_count = environments_.size();
        for (std::size_t environment_index = worker_index; environment_index < environment_count;
            environment_index += worker_count)
        {
            sim::Environment& environment = environments_[environment_index];
            for (std::size_t step = 0; step < rollout_horizon; ++step)
            {
                Transition& transition = rollout_[step * environment_count + environment_index];
                transition.observation = environment.observation();
                const PolicyNetwork::Evaluation evaluation = policy_.evaluate(transition.observation);
                transition.value = evaluation.value;
                transition.action = sample_action(evaluation, local_random, transition.log_probability);
                const sim::StepResult result = environment.step(transition.action);
                transition.reward = result.reward;
                transition.terminal = result.terminated;
                episode_rewards_[environment_index] += result.reward;
                episode_distances_[environment_index] = environment.distance_travelled();
                totals.accumulated_speed += result.forward_speed;
                if (result.terminated)
                {
                    totals.completed_reward += episode_rewards_[environment_index];
                    totals.completed_distance += episode_distances_[environment_index];
                    ++totals.completed_episodes;
                    episode_rewards_[environment_index] = 0.0f;
                    episode_distances_[environment_index] = 0.0f;
                    environment.reset(0x100000u + metrics_.environment_steps
                        + environment_index * 17u + step + metrics_.update * 131u);
                }
            }
        }
        return totals;
    }

    void PpoTrainer::rollout_worker_main(std::size_t worker_index, std::stop_token stop_token)
        {
            std::uint64_t observed_generation = 0;
            while (!stop_token.stop_requested())
            {
                std::uint64_t update_seed = 0;
                std::size_t active_workers = 1;
                {
                    std::unique_lock lock(rollout_mutex_);
                    rollout_start_cv_.wait(lock, stop_token, [this, observed_generation]
                    {
                        return rollout_generation_ != observed_generation;
                    });
                    if (stop_token.stop_requested())
                        return;
                    observed_generation = rollout_generation_;
                    update_seed = rollout_update_seed_;
                    active_workers = rollout_active_worker_count_;
                }

                RolloutTotals totals{};
                if (worker_index < active_workers)
                    totals = collect_rollout_partition(worker_index, active_workers, update_seed);
                {
                    std::scoped_lock lock(rollout_mutex_);
                    rollout_worker_totals_[worker_index] = totals;
                    ++rollout_completed_;
                }
                rollout_done_cv_.notify_one();
            }
        }


    void PpoTrainer::set_blueprint(const sim::CreatureBlueprint& blueprint, bool preserve_policy)
    {
        const bool changed = blueprint.signature() != blueprint_.signature();
        blueprint_ = blueprint;
        for (sim::Environment& environment : environments_)
        {
            environment.set_blueprint(blueprint_);
            environment.set_course(course_stage_, course_difficulty_);
        }
        preview_.set_blueprint(blueprint_);
        preview_.set_course(course_stage_, course_difficulty_);
        if (!changed)
            return;

        if (preserve_policy)
        {
            reset_training_state();
            controller_state_ = ControllerState::transferred;
        }
        else
        {
            reset_policy();
        }
    }

    void PpoTrainer::set_course(sim::CourseStage stage, float difficulty, bool preserve_best)
    {
        difficulty = clamp(difficulty, 0.10f, 1.0f);
        if (stage == course_stage_ && std::abs(difficulty - course_difficulty_) < 1.0e-5f)
            return;
        course_stage_ = stage;
        course_difficulty_ = difficulty;
        for (sim::Environment& environment : environments_)
            environment.set_course(course_stage_, course_difficulty_);
        preview_.set_course(course_stage_, course_difficulty_);
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        metrics_.evaluation_reward = 0.0f;
        metrics_.evaluation_distance = 0.0f;
        metrics_.evaluation_speed = 0.0f;
        metrics_.evaluation_score = -std::numeric_limits<float>::infinity();
        metrics_.evaluation_survival = 0.0f;
        metrics_.evaluation_collisions = 0.0f;
        metrics_.evaluation_airborne_ratio = 0.0f;
        metrics_.evaluation_stride_events = 0.0f;
        metrics_.evaluation_invalid_runs = 0;
        metrics_.evaluation_valid = false;
        if (!preserve_best)
        {
            best_parameters_.clear();
            metrics_.best_evaluation_distance = -std::numeric_limits<float>::infinity();
            metrics_.best_evaluation_score = -std::numeric_limits<float>::infinity();
            metrics_.best_update = 0;
        }
    }

    void PpoTrainer::reset_training_state(bool clear_best) noexcept
    {
        adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.step = 0;
        metrics_ = {};
        reward_history_.clear();
        speed_history_.clear();
        if (clear_best)
            best_parameters_.clear();
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (std::size_t index = 0; index < environments_.size(); ++index)
        {
            environments_[index].set_course(course_stage_, course_difficulty_);
            environments_[index].reset(0x1000u + index * 7919u);
        }
        preview_.set_course(course_stage_, course_difficulty_);
        preview_.reset(0xDEADBEEFu);
    }

    void PpoTrainer::reset_policy(std::uint64_t seed)
    {
        policy_ = PolicyNetwork(seed);
        reset_training_state();
        controller_state_ = ControllerState::fresh;
    }

    void PpoTrainer::set_exploration(float standard_deviation) noexcept
    {
        policy_.set_exploration(standard_deviation);
    }

    std::string_view PpoTrainer::controller_state_name() const noexcept
    {
        switch (controller_state_)
        {
        case ControllerState::fresh: return "FRESH";
        case ControllerState::training: return "TRAINING";
        case ControllerState::resumed: return "RESUMED";
        case ControllerState::transferred: return "TRANSFERRED";
        }
        return "UNKNOWN";
    }

    float PpoTrainer::random_uniform() noexcept
    {
        return next_uniform(random_state_);
    }

    float PpoTrainer::random_normal() noexcept
    {
        return next_normal(random_state_);
    }

    std::array<float, sim::action_count> PpoTrainer::sample_action(
        const PolicyNetwork::Evaluation& evaluation,
        std::uint64_t& random_state,
        float& log_probability) const noexcept
    {
        const auto stddev = policy_.standard_deviation();
        std::array<float, sim::action_count> action{};
        for (std::size_t index = 0; index < action.size(); ++index)
            action[index] = clamp(evaluation.mean[index] + stddev[index] * next_normal(random_state), -1.0f, 1.0f);
        log_probability = policy_.log_probability(action, evaluation);
        return action;
    }

    void PpoTrainer::begin_update()
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


    bool PpoTrainer::restore_best_policy() noexcept
    {
        if (best_parameters_.size() != policy_.parameter_count())
            return false;
        policy_.parameters() = best_parameters_;
        adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.step = 0;
        preview_.reset(0xDEADBEEFu + metrics_.update);
        controller_state_ = ControllerState::resumed;
        return true;
    }

    void PpoTrainer::apply_adam(float learning_rate, float gradient_scale)
    {
        constexpr float beta1 = 0.9f;
        constexpr float beta2 = 0.999f;
        ++adam_.step;
        const float bias1 = 1.0f - std::pow(beta1, static_cast<float>(adam_.step));
        const float bias2 = 1.0f - std::pow(beta2, static_cast<float>(adam_.step));
        std::vector<float>& parameters = policy_.parameters();
        const std::vector<float>& gradients = policy_.gradients();
        for (std::size_t index = 0; index < parameters.size(); ++index)
        {
            const float gradient = gradients[index] * gradient_scale;
            adam_.first_moment[index] = beta1 * adam_.first_moment[index] + (1.0f - beta1) * gradient;
            adam_.second_moment[index] = beta2 * adam_.second_moment[index] + (1.0f - beta2) * gradient * gradient;
            const float first = adam_.first_moment[index] / bias1;
            const float second = adam_.second_moment[index] / bias2;
            parameters[index] -= learning_rate * first / (std::sqrt(second) + epsilon);
        }
    }

    void PpoTrainer::step_preview(float dt)
    {
        const auto action = policy_.deterministic_action(preview_.observation());
        if (preview_.step(action, dt).terminated)
            preview_.reset(0xDEADBEEFu + metrics_.update);
    }

    void PpoTrainer::reset_preview(std::uint64_t seed) noexcept
    {
        preview_.reset(seed);
    }

    void PpoTrainer::append_history(std::vector<float>& history, float value)
    {
        constexpr std::size_t maximum = 240;
        if (history.size() == maximum)
            history.erase(history.begin());
        history.push_back(value);
    }
}
