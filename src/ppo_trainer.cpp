#include "ppo.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <thread>
#include <vector>

namespace runner::rl
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

        [[nodiscard]] float skill_bootstrap_weight(std::uint64_t update,
            sim::CourseStage stage) noexcept
        {
            if (stage == sim::CourseStage::balance)
            {
                if (update < 200u)
                    return 0.66f;
                if (update < 1600u)
                    return lerp(0.66f, 0.20f,
                        static_cast<float>(update - 200u) / 1400.0f);
                if (update < 4500u)
                    return lerp(0.20f, 0.03f,
                        static_cast<float>(update - 1600u) / 2900.0f);
                return 0.0f;
            }
            if (stage == sim::CourseStage::duck_press
                || stage == sim::CourseStage::crouch_walk)
            {
                if (update < 300u)
                    return 0.70f;
                if (update < 2000u)
                    return lerp(0.70f, 0.24f,
                        static_cast<float>(update - 300u) / 1700.0f);
                if (update < 5200u)
                    return lerp(0.24f, 0.06f,
                        static_cast<float>(update - 2000u) / 3200.0f);
                return 0.04f;
            }
            if (stage == sim::CourseStage::ramps
                || stage == sim::CourseStage::duck_bars)
                return update < 1200u ? 0.36f : 0.10f;
            if (!sim::stage_requires_forward_gait(stage))
                return 0.0f;
            if (update < 400u)
                return 0.24f;
            if (update < 2200u)
                return lerp(0.24f, 0.10f,
                    static_cast<float>(update - 400u) / 1800.0f);
            if (update < 7000u)
                return lerp(0.10f, 0.02f,
                    static_cast<float>(update - 2200u) / 4800.0f);
            return 0.0f;
        }

        [[nodiscard]] std::array<float, sim::action_count> skill_bootstrap_action(
            const sim::Environment& environment, sim::CourseStage stage) noexcept
        {
            if (stage == sim::CourseStage::balance)
                return balance_teacher_action(environment);
            if (stage == sim::CourseStage::duck_press)
                return duck_teacher_action(environment);
            if (stage == sim::CourseStage::crouch_walk)
                return crouch_walk_teacher_action(environment);
            const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.25f;
            const float swing = std::sin(phase);
            const float lift_left = std::max(0.0f, swing);
            const float lift_right = std::max(0.0f, -swing);
            if (environment.blueprint().support_seed_count() <= 2u)
            {
                return {
                    0.52f * swing,
                    0.48f * lift_left - 0.10f,
                    -0.52f * swing,
                    0.48f * lift_right - 0.10f
                };
            }
            return {
                0.50f * swing,
                -0.50f * swing,
                -0.50f * swing,
                0.50f * swing
            };
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
            rollout_previous_actions_.assign(environment_count,
                std::array<float, sim::action_count>{});
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

                const float bootstrap = skill_bootstrap_weight(metrics_.update, course_stage_);
                const auto guided = skill_bootstrap_action(environment, course_stage_);
                std::array<float, sim::action_count>& previous_action
                    = rollout_previous_actions_[environment_index];
                for (std::size_t action_index = 0; action_index < transition.action.size(); ++action_index)
                {
                    const float guided_action = lerp(transition.action[action_index],
                        guided[action_index], bootstrap);
                    transition.action[action_index] = clamp(
                        lerp(previous_action[action_index], guided_action, 0.60f), -1.0f, 1.0f);
                    previous_action[action_index] = transition.action[action_index];
                }
                const MotorDiscoveryProbe probe = motor_discovery_probe(
                    environment, environment_index, metrics_.update, step);
                for (std::size_t action_index = 0; action_index < transition.action.size(); ++action_index)
                    transition.action[action_index] = lerp(transition.action[action_index],
                        probe.action[action_index], probe.weight);
                transition.action = effective_policy_action(
                    environment, transition.action, course_stage_);
                transition.log_probability = policy_.log_probability(transition.action, evaluation);
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
                    if (result.valid_motion)
                        ++totals.valid_episodes;
                    else
                        ++totals.invalid_episodes;
                    totals.total_distance += static_cast<double>(
                        std::max(0.0f, environment.distance_travelled()));
                    totals.alternating_steps += environment.gait_cycles();
                    totals.collisions += static_cast<std::uint64_t>(
                        std::max(0.0f, environment.collision_count()));
                    totals.powered_jumps += environment.powered_jumps();
                    totals.landed_jumps += environment.landed_jumps();
                    totals.landed_flips += environment.spin_landings();
                    totals.obstacles_passed += environment.obstacles_passed();
                    if (result.invalid_reason == sim::InvalidMotion::fallen
                        || result.invalid_reason == sim::InvalidMotion::collapsed_posture
                        || result.invalid_reason == sim::InvalidMotion::body_rolling)
                        ++totals.falls;
                    episode_rewards_[environment_index] = 0.0f;
                    episode_distances_[environment_index] = 0.0f;
                    rollout_previous_actions_[environment_index].fill(0.0f);
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
        for (auto& action : rollout_previous_actions_)
            action.fill(0.0f);
        metrics_.evaluation_reward = 0.0f;
        metrics_.evaluation_distance = 0.0f;
        metrics_.evaluation_speed = 0.0f;
        metrics_.evaluation_score = -std::numeric_limits<float>::infinity();
        metrics_.evaluation_survival = 0.0f;
        metrics_.evaluation_collisions = 0.0f;
        metrics_.evaluation_airborne_ratio = 0.0f;
        metrics_.evaluation_stride_events = 0.0f;
        metrics_.evaluation_duck_seconds = 0.0f;
        metrics_.evaluation_powered_jumps = 0.0f;
        metrics_.evaluation_jump_landings = 0.0f;
        metrics_.evaluation_spin_turns = 0.0f;
        metrics_.evaluation_spin_landings = 0.0f;
        metrics_.evaluation_obstacles_passed = 0.0f;
        metrics_.evaluation_stable_stance = 0.0f;
        metrics_.evaluation_longest_stance = 0.0f;
        metrics_.evaluation_duck_recoveries = 0.0f;
        metrics_.evaluation_max_joint_speed = 0.0f;
        metrics_.evaluation_quality_key = 0u;
        metrics_.evaluation_rejection_mask = 0u;
        metrics_.evaluation_invalid_runs = 0;
        metrics_.evaluation_valid = false;
        metrics_.learning_rate = 3.0e-4f;
        if (!preserve_best)
        {
            best_parameters_.clear();
            clear_self_imitation_prior();
            metrics_.best_evaluation_distance = -std::numeric_limits<float>::infinity();
            metrics_.best_evaluation_score = -std::numeric_limits<float>::infinity();
            metrics_.best_quality_key = 0u;
            metrics_.best_update = 0;
        }
    }

    void PpoTrainer::reset_training_state(bool clear_best) noexcept
    {
        adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.step = 0;
        const TrainingMetrics previous_metrics = metrics_;
        metrics_ = {};
        metrics_.total_updates = previous_metrics.total_updates;
        metrics_.total_environment_steps = previous_metrics.total_environment_steps;
        metrics_.total_episodes = previous_metrics.total_episodes;
        metrics_.total_valid_episodes = previous_metrics.total_valid_episodes;
        metrics_.total_invalid_episodes = previous_metrics.total_invalid_episodes;
        metrics_.total_resets = previous_metrics.total_resets + 1u;
        metrics_.total_alternating_steps = previous_metrics.total_alternating_steps;
        metrics_.total_falls = previous_metrics.total_falls;
        metrics_.total_collisions = previous_metrics.total_collisions;
        metrics_.total_powered_jumps = previous_metrics.total_powered_jumps;
        metrics_.total_landed_jumps = previous_metrics.total_landed_jumps;
        metrics_.total_landed_flips = previous_metrics.total_landed_flips;
        metrics_.total_obstacles_passed = previous_metrics.total_obstacles_passed;
        metrics_.total_distance = previous_metrics.total_distance;
        metrics_.total_training_seconds = previous_metrics.total_training_seconds;
        metrics_.evaluation_count = previous_metrics.evaluation_count;
        if (!clear_best)
        {
            metrics_.best_evaluation_distance = previous_metrics.best_evaluation_distance;
            metrics_.best_evaluation_score = previous_metrics.best_evaluation_score;
            metrics_.best_quality_key = previous_metrics.best_quality_key;
            metrics_.best_update = previous_metrics.best_update;
        }
        reward_history_.clear();
        speed_history_.clear();
        if (clear_best)
        {
            best_parameters_.clear();
            clear_self_imitation_prior();
        }
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (auto& action : rollout_previous_actions_)
            action.fill(0.0f);
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

    void PpoTrainer::begin_staged_update()
    {
        if (staged_update_active_)
            return;
        constexpr std::size_t horizon = rollout_horizon;
        const std::size_t environment_count = environments_.size();
        rollout_.clear();
        rollout_.resize(horizon * environment_count);

        const std::uint64_t update_seed = random_state_
            ^ (metrics_.update + 1u) * 0x9E3779B97F4A7C15ULL;
        staged_totals_ = {};
        if (rollout_workers_.empty())
        {
            staged_totals_ = collect_rollout_partition(0, 1, update_seed);
        }
        else
        {
            {
                std::scoped_lock lock(rollout_mutex_);
                rollout_completed_ = 0;
                rollout_update_seed_ = update_seed;
                rollout_active_worker_count_ = std::min(active_worker_count_, rollout_worker_count_);
                ++rollout_generation_;
            }
            rollout_start_cv_.notify_all();
            {
                std::unique_lock lock(rollout_mutex_);
                rollout_done_cv_.wait(lock, [this]
                {
                    return rollout_completed_ == rollout_worker_count_;
                });
            }
            for (const RolloutTotals& worker : rollout_worker_totals_)
            {
                staged_totals_.accumulated_speed += worker.accumulated_speed;
                staged_totals_.completed_reward += worker.completed_reward;
                staged_totals_.completed_distance += worker.completed_distance;
                staged_totals_.completed_episodes += worker.completed_episodes;
                staged_totals_.valid_episodes += worker.valid_episodes;
                staged_totals_.invalid_episodes += worker.invalid_episodes;
                staged_totals_.alternating_steps += worker.alternating_steps;
                staged_totals_.falls += worker.falls;
                staged_totals_.collisions += worker.collisions;
                staged_totals_.powered_jumps += worker.powered_jumps;
                staged_totals_.landed_jumps += worker.landed_jumps;
                staged_totals_.landed_flips += worker.landed_flips;
                staged_totals_.obstacles_passed += worker.obstacles_passed;
                staged_totals_.total_distance += worker.total_distance;
            }
        }
        random_state_ ^= update_seed + 0xA0761D6478BD642FULL;
        staged_update_active_ = true;
        staged_advantages_ready_ = false;
        staged_optimized_ = false;
    }

    void PpoTrainer::compute_staged_advantages()
    {
        if (!staged_update_active_ || staged_advantages_ready_)
            return;
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
        staged_advantages_ready_ = true;
    }

    void PpoTrainer::optimize_staged_update()
    {
        if (!staged_update_active_ || !staged_advantages_ready_ || staged_optimized_)
            return;
        update_policy();
        staged_optimized_ = true;
    }

    void PpoTrainer::finish_staged_update()
    {
        if (!staged_update_active_ || !staged_advantages_ready_ || !staged_optimized_)
            return;
        ++metrics_.update;
        metrics_.environment_steps += rollout_.size();
        ++metrics_.total_updates;
        metrics_.total_environment_steps += rollout_.size();
        metrics_.total_episodes += staged_totals_.completed_episodes;
        metrics_.total_valid_episodes += staged_totals_.valid_episodes;
        metrics_.total_invalid_episodes += staged_totals_.invalid_episodes;
        metrics_.total_resets += staged_totals_.completed_episodes;
        metrics_.total_alternating_steps += staged_totals_.alternating_steps;
        metrics_.total_falls += staged_totals_.falls;
        metrics_.total_collisions += staged_totals_.collisions;
        metrics_.total_powered_jumps += staged_totals_.powered_jumps;
        metrics_.total_landed_jumps += staged_totals_.landed_jumps;
        metrics_.total_landed_flips += staged_totals_.landed_flips;
        metrics_.total_obstacles_passed += staged_totals_.obstacles_passed;
        metrics_.total_distance += staged_totals_.total_distance;
        if (!environments_.empty())
        {
            metrics_.total_training_seconds += static_cast<double>(rollout_.size())
                / static_cast<double>(environments_.size()) / 60.0;
        }
        metrics_.mean_speed = staged_totals_.accumulated_speed
            / static_cast<float>(rollout_.size());
        if (staged_totals_.completed_episodes > 0)
        {
            metrics_.mean_reward = staged_totals_.completed_reward
                / static_cast<float>(staged_totals_.completed_episodes);
            metrics_.mean_episode_distance = staged_totals_.completed_distance
                / static_cast<float>(staged_totals_.completed_episodes);
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
        if (metrics_.update == 1 || metrics_.update % 5 == 0)
            evaluate_policy();
        staged_update_active_ = false;
        staged_advantages_ready_ = false;
        staged_optimized_ = false;
        staged_totals_ = {};
    }

    void PpoTrainer::train_one_update()
    {
        begin_staged_update();
        compute_staged_advantages();
        optimize_staged_update();
        finish_staged_update();
    }

    void PpoTrainer::evaluate_policy()
        {
            parallel_evaluate_policy();
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
        if (self_imitation_prior_.empty())
            refresh_self_imitation_prior();
        controller_state_ = ControllerState::resumed;
        return true;
    }

    void PpoTrainer::update_policy()
        {
            constexpr std::size_t optimization_passes = 2;
            constexpr std::size_t minibatch_size = 256;
            constexpr float clip_range = 0.12f;
            constexpr float value_coefficient = 0.42f;
            const float entropy_coefficient = 0.0012f
                * std::max(0.10f, 1.0f - static_cast<float>(metrics_.update) / 3500.0f);
            constexpr float max_gradient_norm = 0.38f;

            std::vector<std::size_t> indices(rollout_.size());
            std::iota(indices.begin(), indices.end(), 0);
            float total_policy_loss = 0.0f;
            float total_value_loss = 0.0f;
            float total_entropy = 0.0f;
            std::size_t sample_count = 0;

            for (std::size_t optimization_pass = 0;
                optimization_pass < optimization_passes; ++optimization_pass)
            {
                for (std::size_t index = indices.size(); index > 1; --index)
                {
                    const std::size_t other = static_cast<std::size_t>(
                        random_uniform() * static_cast<float>(index));
                    std::swap(indices[index - 1], indices[std::min(other, index - 1)]);
                }
                for (std::size_t begin = 0; begin < indices.size(); begin += minibatch_size)
                {
                    const std::size_t end = std::min(indices.size(), begin + minibatch_size);
                    float batch_policy_loss = 0.0f;
                    float batch_value_loss = 0.0f;
                    float batch_entropy = 0.0f;
                    parallel_accumulate_batch(
                        indices, begin, end, clip_range, value_coefficient, entropy_coefficient,
                        batch_policy_loss, batch_value_loss, batch_entropy);
                    apply_self_imitation_prior();

                    const float inverse_batch = 1.0f / static_cast<float>(end - begin);
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
                    total_policy_loss += batch_policy_loss;
                    total_value_loss += batch_value_loss;
                    total_entropy += batch_entropy;
                    sample_count += end - begin;
                }
            }
            const float inverse_samples = sample_count > 0
                ? 1.0f / static_cast<float>(sample_count)
                : 0.0f;
            if (best_parameters_.size() == policy_.parameter_count())
            {
                const float anchor = metrics_.update < 1500u ? 0.004f : 0.010f;
                std::vector<float>& current = policy_.parameters();
                for (std::size_t index = 0; index < current.size(); ++index)
                    current[index] = lerp(current[index], best_parameters_[index], anchor);
            }
            metrics_.policy_loss = total_policy_loss * inverse_samples;
            metrics_.value_loss = total_value_loss * inverse_samples;
            metrics_.entropy = total_entropy * inverse_samples;
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
        const auto raw_action = policy_.deterministic_action(preview_.observation());
        const auto action = effective_policy_action(preview_, raw_action, course_stage_);
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
