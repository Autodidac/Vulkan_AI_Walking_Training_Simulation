from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_exact(path: str, old: str, new: str, expected: int = 1) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"{path}: expected {expected} matches, found {count}")
    target.write_text(text.replace(old, new), encoding="utf-8")


replace_exact(
    "src/simulation.hpp",
    "inline constexpr std::size_t observation_count = 30;",
    "inline constexpr std::size_t observation_count = 32;"
)

replace_exact(
    "src/simulation.hpp",
    '''    [[nodiscard]] inline bool qualifies_alternating_step(int previous_side, int strike_side,
        float seconds_since_previous, float root_displacement) noexcept
    {
        return previous_side != 0 && strike_side != 0 && strike_side != previous_side
            && seconds_since_previous >= 0.12f && std::abs(root_displacement) >= 0.025f;
    }''',
    '''    [[nodiscard]] inline bool qualifies_alternating_step(int previous_side, int strike_side,
        float seconds_since_previous, float root_displacement) noexcept
    {
        return previous_side != 0 && strike_side != 0 && strike_side != previous_side
            && seconds_since_previous >= 0.12f && std::abs(root_displacement) >= 0.025f;
    }

    [[nodiscard]] inline bool qualifies_supported_step(int previous_side, int strike_side,
        float seconds_since_previous, float root_displacement,
        float swing_air_seconds, float swing_clearance) noexcept
    {
        return qualifies_alternating_step(previous_side, strike_side,
            seconds_since_previous, root_displacement)
            && std::abs(root_displacement) >= 0.055f
            && swing_air_seconds >= 0.10f
            && swing_clearance >= 0.075f;
    }'''
)

replace_exact(
    "src/simulation.hpp",
    '''    [[nodiscard]] inline bool foot_pivot_rolling_motion(float root_speed,
        bool left_supported, bool right_supported, float stance_slip_speed,
        float maximum_foot_clearance, float torso_turn_speed) noexcept
    {
        return left_supported && right_supported
            && std::abs(root_speed) > 0.10f
            && stance_slip_speed < 0.065f
            && maximum_foot_clearance < 0.075f
            && std::abs(torso_turn_speed) > 0.20f;
    }''',
    '''    [[nodiscard]] inline bool foot_pivot_rolling_motion(float root_speed,
        bool left_supported, bool right_supported, float stance_slip_speed,
        float maximum_foot_clearance, float torso_turn_speed) noexcept
    {
        return left_supported && right_supported
            && std::abs(root_speed) > 0.085f
            && stance_slip_speed < 0.080f
            && maximum_foot_clearance < 0.085f
            && (std::abs(torso_turn_speed) > 0.12f || std::abs(root_speed) > 0.18f);
    }'''
)
replace_exact(
    "src/simulation.hpp",
    'case InvalidMotion::foot_pivot_rolling: return "FOOT-NODE ROLLING";',
    'case InvalidMotion::foot_pivot_rolling: return "FOOT-NODE SKATING / ROLLING";'
)

replace_exact(
    "src/simulation.hpp",
    '''        std::array<float, action_count> previous_angles_{};
        std::array<float, action_count> angular_velocities_{};''',
    '''        std::array<float, action_count> previous_angles_{};
        std::array<float, action_count> angular_velocities_{};
        std::array<float, action_count> previous_applied_actions_{};'''
)
replace_exact(
    "src/simulation.hpp",
    '''        float last_step_time_{ -100.0f };
        float last_step_x_{};
        float maximum_speed_kmh_{};''',
    '''        float last_step_time_{ -100.0f };
        float last_step_x_{};
        float left_swing_seconds_{};
        float right_swing_seconds_{};
        float left_swing_clearance_{};
        float right_swing_clearance_{};
        float action_change_energy_{};
        bool alternating_step_this_step_{};
        float maximum_speed_kmh_{};'''
)

replace_exact(
    "src/simulation.cpp",
    '''        previous_angles_.fill(0.0f);
        angular_velocities_.fill(0.0f);''',
    '''        previous_angles_.fill(0.0f);
        angular_velocities_.fill(0.0f);
        previous_applied_actions_.fill(0.0f);'''
)
replace_exact(
    "src/simulation.cpp",
    '''        last_step_time_ = -100.0f;
        last_step_x_ = previous_pelvis_.x;
        maximum_speed_kmh_ = 0.0f;''',
    '''        last_step_time_ = -100.0f;
        last_step_x_ = previous_pelvis_.x;
        left_swing_seconds_ = 0.0f;
        right_swing_seconds_ = 0.0f;
        left_swing_clearance_ = 0.0f;
        right_swing_clearance_ = 0.0f;
        action_change_energy_ = 0.0f;
        alternating_step_this_step_ = false;
        maximum_speed_kmh_ = 0.0f;'''
)

replace_exact(
    "src/simulation.cpp",
    '''        const bool left = contact_supported(blueprint_.left_contact_node);
        const bool right = contact_supported(blueprint_.right_contact_node);
        const bool was_supported = previous_left_grounded_ || previous_right_grounded_;
        const bool new_left = left && !previous_left_grounded_;
        const bool new_right = right && !previous_right_grounded_;
        const int strike_side = new_left == new_right ? 0 : (new_left ? -1 : 1);
        const float root_x = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position.x : 0.0f;
        if (strike_side != 0)
        {
            if (last_contact_side_ == 0)
            {
                last_contact_side_ = strike_side;
                last_step_time_ = elapsed_seconds_;
                last_step_x_ = root_x;
            }
            else if (qualifies_alternating_step(last_contact_side_, strike_side,
                elapsed_seconds_ - last_step_time_, root_x - last_step_x_))
            {
                ++alternating_steps_;
                last_contact_side_ = strike_side;
                last_step_time_ = elapsed_seconds_;
                last_step_x_ = root_x;
            }
        }
        const float left_slip = left''',
    '''        const bool left = contact_supported(blueprint_.left_contact_node);
        const bool right = contact_supported(blueprint_.right_contact_node);
        const bool was_supported = previous_left_grounded_ || previous_right_grounded_;
        const bool new_left = left && !previous_left_grounded_;
        const bool new_right = right && !previous_right_grounded_;
        const int strike_side = new_left == new_right ? 0 : (new_left ? -1 : 1);
        const float root_x = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float left_clearance = contact_cluster_clearance(blueprint_.left_contact_node);
        const float right_clearance = contact_cluster_clearance(blueprint_.right_contact_node);
        if (!left)
        {
            left_swing_seconds_ += dt;
            left_swing_clearance_ = std::max(left_swing_clearance_, left_clearance);
        }
        if (!right)
        {
            right_swing_seconds_ += dt;
            right_swing_clearance_ = std::max(right_swing_clearance_, right_clearance);
        }

        alternating_step_this_step_ = false;
        if (strike_side != 0)
        {
            const float swing_air_seconds = new_left ? left_swing_seconds_ : right_swing_seconds_;
            const float swing_clearance = new_left ? left_swing_clearance_ : right_swing_clearance_;
            if (last_contact_side_ == 0)
            {
                last_contact_side_ = strike_side;
                last_step_time_ = elapsed_seconds_;
                last_step_x_ = root_x;
            }
            else if (qualifies_supported_step(last_contact_side_, strike_side,
                elapsed_seconds_ - last_step_time_, root_x - last_step_x_,
                swing_air_seconds, swing_clearance))
            {
                ++alternating_steps_;
                alternating_step_this_step_ = true;
                last_contact_side_ = strike_side;
                last_step_time_ = elapsed_seconds_;
                last_step_x_ = root_x;
            }
        }
        if (left)
        {
            left_swing_seconds_ = 0.0f;
            left_swing_clearance_ = 0.0f;
        }
        if (right)
        {
            right_swing_seconds_ = 0.0f;
            right_swing_clearance_ = 0.0f;
        }

        const float left_slip = left'''
)

replace_exact(
    "src/simulation.cpp",
    '''        std::array<float, action_count> applied_actions{};
        for (std::size_t index = 0; index < action_count; ++index)
            applied_actions[index] = clamp(actions[index], -1.0f, 1.0f) * control_ramp;
        constexpr Vec2 gravity{ 0.0f, -22.0f };''',
    '''        std::array<float, action_count> applied_actions{};
        action_change_energy_ = 0.0f;
        for (std::size_t index = 0; index < action_count; ++index)
        {
            applied_actions[index] = clamp(actions[index], -1.0f, 1.0f) * control_ramp;
            const float action_delta = applied_actions[index] - previous_applied_actions_[index];
            action_change_energy_ += action_delta * action_delta;
            previous_applied_actions_[index] = applied_actions[index];
        }
        constexpr Vec2 gravity{ 0.0f, -22.0f };'''
)

replace_exact(
    "src/simulation.cpp",
    '''        const float target_speed = 0.90f + course_difficulty_ * 1.30f;
        const float run_reward = stage_requires_forward_gait(course_stage_)
            ? clamp(forward_speed_ / target_speed, 0.0f, 1.0f) * 0.006f : 0.0f;
''',
    '''        const float target_speed = 0.90f + course_difficulty_ * 1.30f;
        const float run_reward = stage_requires_forward_gait(course_stage_)
            ? clamp(forward_speed_ / target_speed, 0.0f, 1.0f) * 0.006f : 0.0f;
        const float real_step_reward = alternating_step_this_step_ ? 0.070f : 0.0f;
        const float unearned_progress_penalty = alternating_steps_ == 0u
            ? std::max(0.0f, safe_progress) * 0.80f : 0.0f;
        const float double_support_shuffle_penalty = left_supported && right_supported
            && std::abs(raw_speed) > 0.08f && obstacle_lift_clearance_ < 0.085f
            ? 0.028f : 0.0f;
        const float action_change_penalty = action_change_energy_ * 0.0025f;
'''
)

replace_exact(
    "src/simulation.cpp",
    '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.012f
                + contact * 0.0006f + swing_reward + run_reward
                - backward_penalty - action_energy * 0.0010f
                - stance_slip_penalty - wheel_penalty - body_contact_penalty;''',
    '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.012f
                + contact * 0.0006f + swing_reward + run_reward + real_step_reward
                - backward_penalty - unearned_progress_penalty
                - double_support_shuffle_penalty - action_energy * 0.0010f
                - action_change_penalty - stance_slip_penalty - wheel_penalty
                - body_contact_penalty;'''
)
replace_exact(
    "src/simulation.cpp",
    '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.011f
                + swing_reward + run_reward + duck_reward * 0.60f + jump_reward
                + obstacle_lift_reward + pass_reward - backward_penalty
                - action_energy * 0.0010f - collision_penalty - knee_first_penalty
                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - body_contact_penalty;''',
    '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.011f
                + swing_reward + run_reward + real_step_reward
                + duck_reward * 0.60f + jump_reward + obstacle_lift_reward
                + pass_reward - backward_penalty - unearned_progress_penalty
                - double_support_shuffle_penalty - action_energy * 0.0010f
                - action_change_penalty - collision_penalty - knee_first_penalty
                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - body_contact_penalty;'''
)
replace_exact(
    "src/simulation.cpp",
    '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.010f
                + swing_reward + run_reward + duck_reward * 0.45f + jump_reward
                + spin_reward + spin_landing_reward + obstacle_lift_reward + pass_reward
                - backward_penalty - action_energy * 0.0010f - collision_penalty
                - knee_first_penalty - stance_slip_penalty - wheel_penalty
                - hazard_stall_penalty - body_contact_penalty;''',
    '''            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.010f
                + swing_reward + run_reward + real_step_reward
                + duck_reward * 0.45f + jump_reward + spin_reward
                + spin_landing_reward + obstacle_lift_reward + pass_reward
                - backward_penalty - unearned_progress_penalty
                - double_support_shuffle_penalty - action_energy * 0.0010f
                - action_change_penalty - collision_penalty - knee_first_penalty
                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - body_contact_penalty;'''
)

replace_exact(
    "src/simulation.cpp",
    '''        result[28] = static_cast<float>(course_stage_) / static_cast<float>(course_stage_count - 1);
        result[29] = course_difficulty_;
        return result;''',
    '''        result[28] = static_cast<float>(course_stage_) / static_cast<float>(course_stage_count - 1);
        result[29] = course_difficulty_;
        const float gait_phase = elapsed_seconds_ * 2.0f * pi * 1.25f;
        result[30] = std::sin(gait_phase);
        result[31] = std::cos(gait_phase);
        return result;'''
)

replace_exact(
    "src/ppo.hpp",
    '''    [[nodiscard]] inline bool elite_motion_eligible(sim::CourseStage stage, bool valid_motion,
        std::uint32_t alternating_steps, float distance, float survival_seconds,
        float duck_seconds = 0.0f, std::uint32_t landed_jumps = 0u,
        float maximum_spin_turns = 0.0f, std::uint32_t spin_landings = 0u,
        std::uint32_t obstacles_passed = 0u) noexcept''',
    '''    [[nodiscard]] inline bool policy_regression_guard(float best_score,
        float current_score, bool current_valid) noexcept
    {
        if (!std::isfinite(best_score))
            return false;
        if (!current_valid || !std::isfinite(current_score))
            return true;
        const float allowed_drop = std::max(0.12f, std::abs(best_score) * 0.08f);
        return current_score < best_score - allowed_drop;
    }

    [[nodiscard]] inline bool elite_motion_eligible(sim::CourseStage stage, bool valid_motion,
        std::uint32_t alternating_steps, float distance, float survival_seconds,
        float duck_seconds = 0.0f, std::uint32_t landed_jumps = 0u,
        float maximum_spin_turns = 0.0f, std::uint32_t spin_landings = 0u,
        std::uint32_t obstacles_passed = 0u) noexcept'''
)
replace_exact(
    "src/ppo.hpp",
    '''        std::vector<float> episode_rewards_{};
        std::vector<float> episode_distances_{};''',
    '''        std::vector<float> episode_rewards_{};
        std::vector<float> episode_distances_{};
        std::vector<std::array<float, sim::action_count>> rollout_previous_actions_{};'''
)

replace_exact(
    "src/ppo_network.cpp",
    '''        constexpr float log_two_pi = 1.83787706640934548356f;
        constexpr float epsilon = 1.0e-8f;''',
    '''        constexpr float log_two_pi = 1.83787706640934548356f;
        constexpr float epsilon = 1.0e-8f;
        constexpr float minimum_log_standard_deviation = -4.0f;
        constexpr float maximum_log_standard_deviation = -1.51412773f;'''
)
replace_exact(
    "src/ppo_network.cpp",
    '''            result[index] = std::exp(std::clamp(parameters_[layout_.log_std + index], -4.0f, 0.0f));''',
    '''            result[index] = std::exp(std::clamp(parameters_[layout_.log_std + index],
                minimum_log_standard_deviation, maximum_log_standard_deviation));'''
)
replace_exact(
    "src/ppo_network.cpp",
    '''        const float value = std::log(clamp(standard_deviation, 0.02f, 0.80f));''',
    '''        const float value = std::log(clamp(standard_deviation, 0.02f, 0.22f));'''
)
replace_exact(
    "src/ppo_network.cpp",
    '''            const float log_std = std::clamp(parameters_[layout_.log_std + index], -4.0f, 0.0f);''',
    '''            const float log_std = std::clamp(parameters_[layout_.log_std + index],
                minimum_log_standard_deviation, maximum_log_standard_deviation);'''
)
replace_exact(
    "src/ppo_network.cpp",
    '''            const float log_std = std::clamp(parameters_[layout_.log_std + output], -4.0f, 0.0f);''',
    '''            const float log_std = std::clamp(parameters_[layout_.log_std + output],
                minimum_log_standard_deviation, maximum_log_standard_deviation);'''
)

replace_exact(
    "src/ppo_trainer.cpp",
    '''        [[nodiscard]] float next_normal(std::uint64_t& state) noexcept
        {
            const float u1 = next_uniform(state);
            const float u2 = next_uniform(state);
            return std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * pi * u2);
        }
    }''',
    '''        [[nodiscard]] float next_normal(std::uint64_t& state) noexcept
        {
            const float u1 = next_uniform(state);
            const float u2 = next_uniform(state);
            return std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * pi * u2);
        }

        [[nodiscard]] float gait_bootstrap_weight(std::uint64_t update,
            sim::CourseStage stage) noexcept
        {
            if (!sim::stage_requires_forward_gait(stage))
                return 0.0f;
            if (update < 400u)
                return 0.28f;
            if (update < 2200u)
            {
                const float t = static_cast<float>(update - 400u) / 1800.0f;
                return lerp(0.28f, 0.13f, t);
            }
            if (update < 7000u)
            {
                const float t = static_cast<float>(update - 2200u) / 4800.0f;
                return lerp(0.13f, 0.025f, t);
            }
            return 0.0f;
        }

        [[nodiscard]] std::array<float, sim::action_count> gait_bootstrap_action(
            const sim::Environment& environment) noexcept
        {
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
    }'''
)

replace_exact(
    "src/ppo_trainer.cpp",
    '''            episode_rewards_.assign(environment_count, 0.0f);
            episode_distances_.assign(environment_count, 0.0f);''',
    '''            episode_rewards_.assign(environment_count, 0.0f);
            episode_distances_.assign(environment_count, 0.0f);
            rollout_previous_actions_.assign(environment_count, {});'''
)
replace_exact(
    "src/ppo_trainer.cpp",
    '''                transition.value = evaluation.value;
                transition.action = sample_action(evaluation, local_random, transition.log_probability);
                const sim::StepResult result = environment.step(transition.action);''',
    '''                transition.value = evaluation.value;
                transition.action = sample_action(evaluation, local_random, transition.log_probability);

                const float bootstrap = gait_bootstrap_weight(metrics_.update, course_stage_);
                const auto guided = gait_bootstrap_action(environment);
                std::array<float, sim::action_count>& previous_action
                    = rollout_previous_actions_[environment_index];
                for (std::size_t action_index = 0; action_index < transition.action.size(); ++action_index)
                {
                    const float guided_action = lerp(transition.action[action_index],
                        guided[action_index], bootstrap);
                    transition.action[action_index] = clamp(
                        lerp(previous_action[action_index], guided_action, 0.42f), -1.0f, 1.0f);
                    previous_action[action_index] = transition.action[action_index];
                }
                transition.log_probability = policy_.log_probability(transition.action, evaluation);
                const sim::StepResult result = environment.step(transition.action);'''
)
replace_exact(
    "src/ppo_trainer.cpp",
    '''                    episode_rewards_[environment_index] = 0.0f;
                    episode_distances_[environment_index] = 0.0f;
                    environment.reset''',
    '''                    episode_rewards_[environment_index] = 0.0f;
                    episode_distances_[environment_index] = 0.0f;
                    rollout_previous_actions_[environment_index].fill(0.0f);
                    environment.reset'''
)
replace_exact(
    "src/ppo_trainer.cpp",
    '''        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);''',
    '''        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (auto& action : rollout_previous_actions_)
            action.fill(0.0f);''',
    expected=2
)

replace_exact(
    "src/ppo_trainer.cpp",
    '''            constexpr std::size_t epochs = 4;
            constexpr std::size_t minibatch_size = 256;
            constexpr float clip_range = 0.20f;
            constexpr float value_coefficient = 0.50f;
            constexpr float entropy_coefficient = 0.0020f;
            constexpr float max_gradient_norm = 0.70f;''',
    '''            constexpr std::size_t epochs = 2;
            constexpr std::size_t minibatch_size = 256;
            constexpr float clip_range = 0.12f;
            constexpr float value_coefficient = 0.42f;
            const float entropy_coefficient = 0.0012f
                * std::max(0.10f, 1.0f - static_cast<float>(metrics_.update) / 3500.0f);
            constexpr float max_gradient_norm = 0.38f;'''
)
replace_exact(
    "src/ppo_trainer.cpp",
    '''            metrics_.policy_loss = total_policy_loss * inverse_samples;
            metrics_.value_loss = total_value_loss * inverse_samples;
            metrics_.entropy = total_entropy * inverse_samples;''',
    '''            if (best_parameters_.size() == policy_.parameter_count())
            {
                const float anchor = metrics_.update < 1500u ? 0.004f : 0.010f;
                std::vector<float>& current = policy_.parameters();
                for (std::size_t index = 0; index < current.size(); ++index)
                    current[index] = lerp(current[index], best_parameters_[index], anchor);
            }
            metrics_.policy_loss = total_policy_loss * inverse_samples;
            metrics_.value_loss = total_value_loss * inverse_samples;
            metrics_.entropy = total_entropy * inverse_samples;'''
)

replace_exact(
    "src/ppo_parallel.cpp",
    '''        ++metrics_.evaluation_count;

        if (metrics_.evaluation_valid
            && (best_parameters_.empty() || metrics_.evaluation_score > metrics_.best_evaluation_score))
        {
            best_parameters_ = policy_.parameters();
            metrics_.best_evaluation_distance = metrics_.evaluation_distance;
            metrics_.best_evaluation_score = metrics_.evaluation_score;
            metrics_.best_update = metrics_.update;
            refresh_self_imitation_prior();
        }''',
    '''        ++metrics_.evaluation_count;

        const bool regressed = !best_parameters_.empty()
            && policy_regression_guard(metrics_.best_evaluation_score,
                metrics_.evaluation_score, metrics_.evaluation_valid);
        if (regressed)
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
        else
        {
            const float improvement_margin = best_parameters_.empty() ? 0.0f
                : std::max(0.015f, std::abs(metrics_.best_evaluation_score) * 0.004f);
            if (metrics_.evaluation_valid
                && (best_parameters_.empty()
                    || metrics_.evaluation_score > metrics_.best_evaluation_score + improvement_margin))
            {
                best_parameters_ = policy_.parameters();
                metrics_.best_evaluation_distance = metrics_.evaluation_distance;
                metrics_.best_evaluation_score = metrics_.evaluation_score;
                metrics_.best_update = metrics_.update;
                refresh_self_imitation_prior();
            }
        }'''
)

replace_exact(
    "tests/core_tests.cpp",
    '''    require(sim::qualifies_alternating_step(-1, 1, 0.30f, 0.08f),
        "real spaced alternating step was rejected");''',
    '''    require(sim::qualifies_alternating_step(-1, 1, 0.30f, 0.08f),
        "real spaced alternating step was rejected");
    require(!sim::qualifies_supported_step(-1, 1, 0.30f, 0.08f, 0.03f, 0.02f),
        "tiny contact wiggle still counts as a supported walking step");
    require(sim::qualifies_supported_step(-1, 1, 0.30f, 0.08f, 0.16f, 0.12f),
        "real lifted swing and landing is rejected as a walking step");'''
)
replace_exact(
    "tests/core_tests.cpp",
    '''    require(!sim::foot_pivot_rolling_motion(0.24f, true, false, 0.01f, 0.18f, 0.50f),
        "single-support lifted-foot walking is incorrectly rejected as foot-node rolling");''',
    '''    require(!sim::foot_pivot_rolling_motion(0.24f, true, false, 0.01f, 0.18f, 0.50f),
        "single-support lifted-foot walking is incorrectly rejected as foot-node rolling");
    require(sim::foot_pivot_rolling_motion(0.22f, true, true, 0.01f, 0.02f, 0.02f),
        "straight double-supported skating around planted feet is not rejected");'''
)
replace_exact(
    "tests/core_tests.cpp",
    '''    require(rl::self_imitation_prior_weight(0, 0) == 0.0f,
        "empty imitation memory still changes PPO gradients");''',
    '''    require(rl::self_imitation_prior_weight(0, 0) == 0.0f,
        "empty imitation memory still changes PPO gradients");
    require(rl::policy_regression_guard(10.0f, 8.5f, true),
        "large valid-policy degradation does not restore the champion");
    require(rl::policy_regression_guard(10.0f, 10.5f, false),
        "invalid policy does not restore the champion");
    require(!rl::policy_regression_guard(10.0f, 9.4f, true),
        "small exploration change triggers an unnecessary champion rollback");'''
)

replace_exact(
    "MISSIONS.md",
    '''- Evaluation, self-imitation eligibility, telemetry, and deterministic tests use duck, jump, landing, spin, and obstacle-pass evidence.
- The four-action controller remains intact for this pass.''',
    '''- Evaluation, self-imitation eligibility, telemetry, and deterministic tests use duck, jump, landing, spin, and obstacle-pass evidence.
- Real walking steps require measurable swing airtime and foot clearance; contact wiggles may not count as gait.
- Straight double-supported skating and pivot rolling around planted semantic feet are invalid.
- Policy actions are smoothed, early gait exploration receives a decaying periodic guide, and phase observations make repeatable cadence learnable before 10,000 updates.
- The best valid policy is a protected champion: substantial or invalid evaluation regression immediately restores it, reduces learning rate/exploration, and prevents the 15,000-update collapse.
- PPO uses a smaller clip range, fewer epochs, lower gradient norm, decaying entropy, bounded exploration, and a light champion anchor.
- The four-action controller remains intact for this pass.'''
)

replace_exact(
    "README.md",
    '''Training now advances through stand, duck/recover, jump/land, walk/run, moving duck/jump, controlled flips, and a mixed goal course. Hazard contact is legal: collision applies physics and a bounded event penalty, while passing the obstacle earns progress. Joint-powered launches receive bounded airtime, controlled airborne flips may reach three spins, and a fourth spin, ground rolling, hovering, or unpowered sustained flight remains invalid.''',
    '''Training now advances through stand, duck/recover, jump/land, walk/run, moving duck/jump, controlled flips, and a mixed goal course. Hazard contact is legal: collision applies physics and a bounded event penalty, while passing the obstacle earns progress. Joint-powered launches receive bounded airtime, controlled airborne flips may reach three spins, and a fourth spin, ground rolling, hovering, or unpowered sustained flight remains invalid.

Walking convergence now uses phase observations, temporally smoothed actions, and a decaying early gait guide. A step counts only after real swing airtime and clearance, so foot-contact wiggles and double-supported skating cannot masquerade as gait. PPO updates are less destructive, exploration is bounded, and every substantial or invalid regression restores the best valid champion immediately instead of allowing late training to degrade beyond it.'''
)

print("Integrated faster gait learning, anti-skating gates, and champion regression protection.")
