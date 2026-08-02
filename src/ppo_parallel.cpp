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
            float duck_seconds{};
            float powered_jumps{};
            float jump_landings{};
            float spin_turns{};
            float spin_landings{};
            float obstacles_passed{};
            float stable_stance{};
            float longest_stance{};
            float duck_recoveries{};
            float maximum_joint_speed{};
            std::uint64_t minimum_quality{ std::numeric_limits<std::uint64_t>::max() };
            std::uint32_t rejection_mask{};
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
                    const int maximum_steps = static_cast<std::uint8_t>(current_stage)
                        >= static_cast<std::uint8_t>(sim::CourseStage::hurdles) ? 2400 : 1200;
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
                                const auto raw_action = local.deterministic_action(
                                    environment.observation());
                                const auto action = effective_policy_action(
                                    environment, raw_action, current_stage);
                                const sim::StepResult result = environment.step(action);
                                episode_reward += result.reward;
                                totals.speed += result.forward_speed;
                                ++totals.speed_samples;
                                if (current_stage == sim::CourseStage::balance
                                    && environment.valid_motion()
                                    && environment.longest_stable_stance_seconds() >= 3.0f)
                                    break;
                                if (result.terminated)
                                    break;
                            }

                            const StageMotionQualification qualification =
                                stage_motion_qualification(current_stage, environment);
                            if (!qualification.valid)
                            {
                                ++totals.invalid_runs;
                                totals.rejection_mask |= qualification.rejection_mask;
                            }
                            else
                            {
                                totals.minimum_quality = std::min(
                                    totals.minimum_quality, qualification.quality_key);
                            }
                            totals.reward += episode_reward;
                            totals.distance += environment.distance_travelled();
                            totals.survival += environment.elapsed_seconds();
                            totals.collisions += environment.collision_count();
                            totals.airborne += environment.airborne_ratio();
                            totals.strides += static_cast<float>(environment.alternating_steps());
                            totals.duck_seconds += environment.duck_seconds();
                            totals.powered_jumps += static_cast<float>(environment.powered_jumps());
                            totals.jump_landings += static_cast<float>(environment.landed_jumps());
                            totals.spin_turns += environment.maximum_spin_turns();
                            totals.spin_landings += static_cast<float>(environment.spin_landings());
                            totals.obstacles_passed += static_cast<float>(environment.obstacles_passed());
                            totals.stable_stance += environment.stable_stance_seconds();
                            totals.longest_stance += environment.longest_stable_stance_seconds();
                            totals.duck_recoveries += static_cast<float>(environment.duck_recoveries());
                            totals.maximum_joint_speed = std::max(
                                totals.maximum_joint_speed, environment.maximum_joint_speed());
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
        std::condition_variable done_cv{};
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

    void PpoTrainer::parallel_accumulate_batch(
        const std::vector<std::size_t>& batch_indices,
        std::size_t batch_begin,
        std::size_t batch_end,
        float batch_clip_range,
        float batch_value_coefficient,
        float batch_entropy_coefficient,
        float& policy_loss,
        float& value_loss,
        float& entropy)
    {
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
                    policy_loss,
                    value_loss,
                    entropy);
            }
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
        {
            std::unique_lock lock(state.mutex);
            state.done_cv.wait(lock, [&state]
            {
                return state.completed == state.worker_count;
            });
        }

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

    void PpoTrainer::parallel_evaluate_policy()
    {
        constexpr std::size_t evaluation_agents = 6;
        if (!parallel_ || parallel_->worker_count == 0)
            return;

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
        {
            std::unique_lock lock(state.mutex);
            state.done_cv.wait(lock, [&state]
            {
                return state.completed == state.worker_count;
            });
        }

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
            totals.duck_seconds += local.duck_seconds;
            totals.powered_jumps += local.powered_jumps;
            totals.jump_landings += local.jump_landings;
            totals.spin_turns += local.spin_turns;
            totals.spin_landings += local.spin_landings;
            totals.obstacles_passed += local.obstacles_passed;
            totals.stable_stance += local.stable_stance;
            totals.longest_stance += local.longest_stance;
            totals.duck_recoveries += local.duck_recoveries;
            totals.maximum_joint_speed = std::max(
                totals.maximum_joint_speed, local.maximum_joint_speed);
            totals.minimum_quality = std::min(totals.minimum_quality, local.minimum_quality);
            totals.rejection_mask |= local.rejection_mask;
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
        metrics_.evaluation_duck_seconds = totals.duck_seconds * inverse_agents;
        metrics_.evaluation_powered_jumps = totals.powered_jumps * inverse_agents;
        metrics_.evaluation_jump_landings = totals.jump_landings * inverse_agents;
        metrics_.evaluation_spin_turns = totals.spin_turns * inverse_agents;
        metrics_.evaluation_spin_landings = totals.spin_landings * inverse_agents;
        metrics_.evaluation_obstacles_passed = totals.obstacles_passed * inverse_agents;
        metrics_.evaluation_stable_stance = totals.stable_stance * inverse_agents;
        metrics_.evaluation_longest_stance = totals.longest_stance * inverse_agents;
        metrics_.evaluation_duck_recoveries = totals.duck_recoveries * inverse_agents;
        metrics_.evaluation_max_joint_speed = totals.maximum_joint_speed;
        constexpr std::uint32_t robust_balance_failures_allowed = 2u;
        const std::uint32_t allowed_invalid_runs = course_stage_ == sim::CourseStage::balance
            ? robust_balance_failures_allowed : 0u;
        metrics_.evaluation_invalid_runs = totals.invalid_runs;
        metrics_.evaluation_valid = totals.invalid_runs <= allowed_invalid_runs
            && totals.minimum_quality != std::numeric_limits<std::uint64_t>::max();
        metrics_.evaluation_rejection_mask = metrics_.evaluation_valid
            ? 0u : totals.rejection_mask;
        metrics_.evaluation_quality_key = metrics_.evaluation_valid
            ? totals.minimum_quality : 0u;

        if (!metrics_.evaluation_valid)
        {
            metrics_.evaluation_score = -1000.0f
                - static_cast<float>(totals.invalid_runs) * 100.0f;
        }
        else
        {
            switch (course_stage_)
            {
            case sim::CourseStage::balance:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_stable_stance * 0.30f
                    + metrics_.evaluation_longest_stance * 0.12f
                    + metrics_.evaluation_survival * 0.04f
                    - metrics_.evaluation_max_joint_speed * 0.015f
                    - std::abs(metrics_.evaluation_distance) * 0.20f;
                break;
            case sim::CourseStage::walk:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_duck_seconds * 0.30f
                    + metrics_.evaluation_survival * 0.03f
                    - std::abs(metrics_.evaluation_distance) * 0.10f;
                break;
            case sim::CourseStage::ramps:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_jump_landings * 0.60f
                    + metrics_.evaluation_powered_jumps * 0.15f
                    - metrics_.evaluation_airborne_ratio * 0.10f;
                break;
            case sim::CourseStage::uneven:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_distance * 0.75f
                    + metrics_.evaluation_stride_events * 0.04f
                    + metrics_.evaluation_speed * 0.12f;
                break;
            case sim::CourseStage::hurdles:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_distance * 0.70f
                    + metrics_.evaluation_obstacles_passed * 0.45f
                    + metrics_.evaluation_jump_landings * 0.25f
                    + metrics_.evaluation_duck_seconds * 0.12f
                    - metrics_.evaluation_collisions * 0.10f;
                break;
            case sim::CourseStage::duck_bars:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_spin_landings * 0.90f
                    + std::min(metrics_.evaluation_spin_turns, 3.0f) * 0.40f
                    + metrics_.evaluation_jump_landings * 0.20f;
                break;
            case sim::CourseStage::moving_hazards:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_distance * 0.70f
                    + metrics_.evaluation_obstacles_passed * 0.55f
                    + metrics_.evaluation_stride_events * 0.03f
                    + metrics_.evaluation_jump_landings * 0.20f
                    + metrics_.evaluation_spin_landings * 0.30f
                    + metrics_.evaluation_duck_seconds * 0.08f
                    - metrics_.evaluation_collisions * 0.10f;
                break;
            }
        }
        ++metrics_.evaluation_count;

        const bool has_best = !best_parameters_.empty();
        const bool quality_regressed = has_best
            && (!metrics_.evaluation_valid
                || metrics_.evaluation_quality_key < metrics_.best_quality_key);
        const bool score_regressed = has_best && metrics_.evaluation_valid
            && metrics_.evaluation_quality_key == metrics_.best_quality_key
            && policy_regression_guard(metrics_.best_evaluation_score,
                metrics_.evaluation_score, true);
        if (quality_regressed || score_regressed)
        {
            policy_.parameters() = best_parameters_;
            adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
            adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
            adam_.step = 0;
            metrics_.learning_rate = std::max(4.0e-5f, metrics_.learning_rate * 0.72f);
            policy_.set_exploration(std::max(0.035f, policy_.mean_exploration() * 0.82f));
            preview_.reset(0xDEADBEEFu + metrics_.update);
            controller_state_ = ControllerState::resumed;
        }
        else if (metrics_.evaluation_valid
            && policy_candidate_better(metrics_.evaluation_quality_key,
                metrics_.evaluation_score, metrics_.best_quality_key,
                metrics_.best_evaluation_score, has_best))
        {
            best_parameters_ = policy_.parameters();
            metrics_.best_evaluation_distance = metrics_.evaluation_distance;
            metrics_.best_evaluation_score = metrics_.evaluation_score;
            metrics_.best_quality_key = metrics_.evaluation_quality_key;
            metrics_.best_update = metrics_.update;
            refresh_self_imitation_prior();
        }
    }
}
