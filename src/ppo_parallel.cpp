#include "ppo.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <utility>

namespace epochrunner::rl
{
    struct PpoTrainer::ParallelState
    {
        enum class Job : std::uint8_t
        {
            none,
            gradient,
            evaluation
        };

        struct GradientTotals
        {
            float policy_loss{};
            float value_loss{};
            float entropy{};
            std::size_t samples{};
        };

        struct EvaluationTotals
        {
            float reward{};
            float distance{};
            float speed{};
            float survival{};
            float collisions{};
            float airborne{};
            float strides{};
            std::size_t speed_samples{};
            std::uint32_t invalid_runs{};
        };

        explicit ParallelState(PpoTrainer& trainer, std::size_t count)
            : owner(trainer), worker_count(count), local_policies(), gradient_totals(count),
              evaluation_totals(count)
        {
            local_policies.reserve(worker_count);
            for (std::size_t index = 0; index < worker_count; ++index)
                local_policies.emplace_back(0x5100u + static_cast<std::uint64_t>(index) * 131u);

            workers.reserve(worker_count);
            for (std::size_t index = 0; index < worker_count; ++index)
            {
                workers.emplace_back([this, index](std::stop_token stop_token)
                {
                    worker_main(index, stop_token);
                });
            }
        }

        ~ParallelState()
        {
            for (std::jthread& worker : workers)
                worker.request_stop();
            start_cv.notify_all();
            workers.clear();
        }

        ParallelState(const ParallelState&) = delete;
        ParallelState& operator=(const ParallelState&) = delete;

        void worker_main(std::size_t worker_index, std::stop_token stop_token)
        {
            std::uint64_t observed_generation{};
            while (!stop_token.stop_requested())
            {
                Job current_job{ Job::none };
                std::size_t current_active{};
                const std::vector<std::size_t>* current_indices{};
                std::size_t current_begin{};
                std::size_t current_end{};
                float current_clip{};
                float current_value_coefficient{};
                float current_entropy_coefficient{};
                sim::CourseStage current_stage{ sim::CourseStage::balance };
                float current_difficulty{};

                {
                    std::unique_lock lock(mutex);
                    start_cv.wait(lock, stop_token, [this, observed_generation]
                    {
                        return generation != observed_generation;
                    });
                    if (stop_token.stop_requested())
                        return;

                    observed_generation = generation;
                    current_job = job;
                    current_active = active_workers;
                    current_indices = indices;
                    current_begin = begin;
                    current_end = end;
                    current_clip = clip_range;
                    current_value_coefficient = value_coefficient;
                    current_entropy_coefficient = entropy_coefficient;
                    current_stage = stage;
                    current_difficulty = difficulty;
                }

                if (current_job == Job::gradient)
                {
                    GradientTotals totals{};
                    PolicyNetwork& local = local_policies[worker_index];
                    local.parameters() = owner.policy_.parameters();
                    local.zero_gradients();

                    if (worker_index < current_active && current_indices != nullptr)
                    {
                        for (std::size_t cursor = current_begin + worker_index;
                            cursor < current_end; cursor += current_active)
                        {
                            const Transition& transition = owner.rollout_[(*current_indices)[cursor]];
                            local.accumulate_gradient(
                                transition.observation,
                                transition.action,
                                transition.log_probability,
                                transition.advantage,
                                transition.return_value,
                                current_clip,
                                current_value_coefficient,
                                current_entropy_coefficient,
                                totals.policy_loss,
                                totals.value_loss,
                                totals.entropy);
                            ++totals.samples;
                        }
                    }
                    gradient_totals[worker_index] = totals;
                }
                else if (current_job == Job::evaluation)
                {
                    constexpr std::size_t evaluation_agents = 6;
                    constexpr int maximum_steps = 900;
                    EvaluationTotals totals{};
                    PolicyNetwork& local = local_policies[worker_index];
                    local.parameters() = owner.policy_.parameters();

                    if (worker_index < current_active)
                    {
                        for (std::size_t agent = worker_index; agent < evaluation_agents;
                            agent += current_active)
                        {
                            const std::uint64_t seed = 0xE000u
                                + static_cast<std::uint64_t>(agent) * 4099u;
                            sim::Environment environment{ owner.blueprint_, seed };
                            environment.set_course(current_stage, current_difficulty);
                            float episode_reward{};
                            for (int step = 0; step < maximum_steps; ++step)
                            {
                                const auto action = local.deterministic_action(environment.observation());
                                const sim::StepResult result = environment.step(action);
                                episode_reward += result.reward;
                                totals.speed += result.forward_speed;
                                ++totals.speed_samples;
                                if (result.terminated)
                                    break;
                            }

                            const bool gait_valid = current_stage == sim::CourseStage::balance
                                || environment.alternating_steps() >= 2;
                            if (!environment.valid_motion() || !gait_valid)
                                ++totals.invalid_runs;
                            totals.reward += episode_reward;
                            totals.distance += environment.distance_travelled();
                            totals.survival += environment.elapsed_seconds();
                            totals.collisions += environment.collision_count();
                            totals.airborne += environment.airborne_ratio();
                            totals.strides += static_cast<float>(environment.alternating_steps());
                        }
                    }
                    evaluation_totals[worker_index] = totals;
                }

                {
                    std::scoped_lock lock(mutex);
                    ++completed;
                }
                done_cv.notify_one();
            }
        }

        PpoTrainer& owner;
        std::size_t worker_count{};
        std::vector<PolicyNetwork> local_policies{};
        std::vector<GradientTotals> gradient_totals{};
        std::vector<EvaluationTotals> evaluation_totals{};
        std::vector<std::jthread> workers{};
        std::mutex mutex{};
        std::condition_variable_any start_cv{};
        std::condition_variable_any done_cv{};
        Job job{ Job::none };
        std::uint64_t generation{};
        std::size_t completed{};
        std::size_t active_workers{ 1 };
        const std::vector<std::size_t>* indices{};
        std::size_t begin{};
        std::size_t end{};
        float clip_range{};
        float value_coefficient{};
        float entropy_coefficient{};
        sim::CourseStage stage{ sim::CourseStage::balance };
        float difficulty{ 0.25f };
    };

    void PpoTrainer::initialize_parallel_workers()
    {
        if (rollout_worker_count_ == 0)
            return;
        parallel_ = std::make_shared<ParallelState>(*this, rollout_worker_count_);
    }

    void PpoTrainer::shutdown_parallel_workers() noexcept
    {
        parallel_.reset();
    }

    void PpoTrainer::set_cpu_mode(int mode) noexcept
    {
        cpu_mode_ = mode <= 1 ? 1 : (mode <= 2 ? 2 : 4);
        if (rollout_worker_count_ == 0)
        {
            active_worker_count_ = 1;
            return;
        }

        if (cpu_mode_ == 1)
            active_worker_count_ = std::max<std::size_t>(1, (rollout_worker_count_ + 2u) / 3u);
        else if (cpu_mode_ == 2)
            active_worker_count_ = std::max<std::size_t>(1, (rollout_worker_count_ * 2u + 2u) / 3u);
        else
            active_worker_count_ = rollout_worker_count_;
    }

    void PpoTrainer::dispatch_parallel_gradient_batch(
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
}
