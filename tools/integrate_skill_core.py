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
    '''        case CourseStage::balance: return "SPAWN STANCE";
        case CourseStage::walk: return "FLAT SAND PATROL";
        case CourseStage::ramps: return "SAND MOUNDS";
        case CourseStage::uneven: return "LOOSE / DEFORMED SAND";
        case CourseStage::hurdles: return "FLAT DEBRIS";
        case CourseStage::duck_bars: return "LOW-CLEARANCE DEBRIS";
        case CourseStage::moving_hazards: return "COMBAT TRAVERSAL";''',
    '''        case CourseStage::balance: return "1. STAND";
        case CourseStage::walk: return "2. DUCK / RECOVER";
        case CourseStage::ramps: return "3. JUMP / LAND";
        case CourseStage::uneven: return "4. WALK / RUN";
        case CourseStage::hurdles: return "5. MOVING DUCK / JUMP";
        case CourseStage::duck_bars: return "6. CONTROLLED FLIPS";
        case CourseStage::moving_hazards: return "7. MIXED GOAL COURSE";'''
)

replace_exact(
    "src/simulation.hpp",
    '''    inline constexpr std::size_t course_stage_count = 7;

    [[nodiscard]] inline std::string_view course_stage_name(CourseStage stage) noexcept''',
    '''    inline constexpr std::size_t course_stage_count = 7;

    [[nodiscard]] inline bool stage_requires_forward_gait(CourseStage stage) noexcept
    {
        return stage == CourseStage::uneven
            || stage == CourseStage::hurdles
            || stage == CourseStage::moving_hazards;
    }

    [[nodiscard]] inline bool stage_allows_powered_airtime(CourseStage stage) noexcept
    {
        return stage == CourseStage::ramps
            || stage == CourseStage::hurdles
            || stage == CourseStage::duck_bars
            || stage == CourseStage::moving_hazards;
    }

    [[nodiscard]] inline bool stage_allows_controlled_flips(CourseStage stage) noexcept
    {
        return stage == CourseStage::duck_bars
            || stage == CourseStage::moving_hazards;
    }

    [[nodiscard]] inline bool powered_joint_launch(CourseStage stage, float vertical_speed,
        float action_energy) noexcept
    {
        return stage_allows_powered_airtime(stage)
            && vertical_speed >= 0.85f
            && action_energy >= 0.055f;
    }

    [[nodiscard]] inline float allowed_airtime_for_stage(CourseStage stage,
        bool powered_launch) noexcept
    {
        if (!powered_launch)
            return 0.72f;
        if (stage == CourseStage::ramps)
            return 1.65f;
        if (stage == CourseStage::hurdles)
            return 1.85f;
        if (stage == CourseStage::duck_bars)
            return 2.75f;
        if (stage == CourseStage::moving_hazards)
            return 2.45f;
        return 0.72f;
    }

    [[nodiscard]] inline bool stage_skill_evidence(CourseStage stage,
        std::uint32_t alternating_steps, float duck_seconds,
        std::uint32_t landed_jumps, float maximum_spin_turns,
        std::uint32_t spin_landings, std::uint32_t obstacles_passed) noexcept
    {
        switch (stage)
        {
        case CourseStage::balance:
            return true;
        case CourseStage::walk:
            return duck_seconds >= 0.50f;
        case CourseStage::ramps:
            return landed_jumps >= 1u;
        case CourseStage::uneven:
            return alternating_steps >= 2u;
        case CourseStage::hurdles:
            return alternating_steps >= 2u && obstacles_passed >= 1u
                && (duck_seconds >= 0.25f || landed_jumps >= 1u);
        case CourseStage::duck_bars:
            return spin_landings >= 1u && maximum_spin_turns >= 0.75f;
        case CourseStage::moving_hazards:
            return alternating_steps >= 2u && obstacles_passed >= 1u
                && (duck_seconds >= 0.25f || landed_jumps >= 1u || spin_landings >= 1u);
        }
        return false;
    }

    [[nodiscard]] inline std::string_view course_stage_name(CourseStage stage) noexcept'''
)

replace_exact(
    "src/simulation.hpp",
    '''        foot_pivot_rolling,
        zero_progress,
        hazard_quiver''',
    '''        foot_pivot_rolling,
        zero_progress,
        excessive_spins,
        hazard_quiver'''
)
replace_exact(
    "src/simulation.hpp",
    '''        case InvalidMotion::zero_progress: return "ZERO MOVEMENT - RESET";
        case InvalidMotion::hazard_quiver: return "HAZARD QUIVER / NO LEG LIFT";''',
    '''        case InvalidMotion::zero_progress: return "ZERO MOVEMENT - RESET";
        case InvalidMotion::excessive_spins: return "MORE THAN 3 SPINS";
        case InvalidMotion::hazard_quiver: return "HAZARD QUIVER / NO LEG LIFT";'''
)
replace_exact(
    "src/simulation.hpp",
    '''    [[nodiscard]] inline InvalidMotion classify_motion_gate(float uprightness, float speed_kmh,
        Vec2 root_position, float airborne_seconds, float allowed_airtime,
        float micro_motion_seconds, bool fallen) noexcept
    {
        if (uprightness < -0.15f)
            return InvalidMotion::flipped;
        if (speed_kmh >= 50.0f)
            return InvalidMotion::overspeed;
        if (root_position.x < -8.0f || root_position.x > 300.0f || root_position.y > 14.0f)
            return InvalidMotion::out_of_bounds;
        if (airborne_seconds > allowed_airtime)
            return InvalidMotion::sustained_flight;
        if (micro_motion_seconds >= 3.0f)
            return InvalidMotion::micro_motion;
        if (fallen)
            return InvalidMotion::fallen;
        return InvalidMotion::none;
    }''',
    '''    [[nodiscard]] inline InvalidMotion classify_motion_gate(float uprightness, float speed_kmh,
        Vec2 root_position, float airborne_seconds, float allowed_airtime,
        float micro_motion_seconds, bool fallen,
        CourseStage stage = CourseStage::balance, float airborne_spin_turns = 0.0f) noexcept
    {
        if (std::abs(airborne_spin_turns) > 3.20f)
            return InvalidMotion::excessive_spins;
        if (uprightness < -0.15f && !stage_allows_controlled_flips(stage))
            return InvalidMotion::flipped;
        if (speed_kmh >= 50.0f)
            return InvalidMotion::overspeed;
        if (root_position.x < -8.0f || root_position.x > 300.0f || root_position.y > 14.0f)
            return InvalidMotion::out_of_bounds;
        if (airborne_seconds > allowed_airtime)
            return InvalidMotion::sustained_flight;
        if (micro_motion_seconds >= 3.0f)
            return InvalidMotion::micro_motion;
        if (fallen)
            return InvalidMotion::fallen;
        return InvalidMotion::none;
    }'''
)
replace_exact(
    "src/simulation.hpp",
    '''        [[nodiscard]] float course_speed() const noexcept
        {
            if (course_stage_ == CourseStage::balance)
                return 0.0f;
            if (static_cast<std::uint8_t>(course_stage_)
                < static_cast<std::uint8_t>(CourseStage::hurdles))
                return 0.68f + course_difficulty_ * 0.72f;
            return 1.05f + course_difficulty_ * 0.82f;
        }''',
    '''        [[nodiscard]] float course_speed() const noexcept
        {
            if (course_stage_ == CourseStage::balance
                || course_stage_ == CourseStage::walk
                || course_stage_ == CourseStage::ramps
                || course_stage_ == CourseStage::duck_bars)
                return 0.0f;
            if (course_stage_ == CourseStage::uneven)
                return 0.82f + course_difficulty_ * 0.88f;
            if (course_stage_ == CourseStage::hurdles)
                return 1.05f + course_difficulty_ * 0.95f;
            return 1.20f + course_difficulty_ * 1.05f;
        }'''
)
replace_exact(
    "src/simulation.hpp",
    '''        [[nodiscard]] float airborne_ratio() const noexcept;
        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }''',
    '''        [[nodiscard]] float airborne_ratio() const noexcept;
        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }
        [[nodiscard]] float duck_seconds() const noexcept { return duck_seconds_; }
        [[nodiscard]] bool duck_active() const noexcept { return duck_active_; }
        [[nodiscard]] std::uint32_t powered_jumps() const noexcept { return powered_jump_count_; }
        [[nodiscard]] std::uint32_t landed_jumps() const noexcept { return landed_jump_count_; }
        [[nodiscard]] float maximum_spin_turns() const noexcept { return maximum_spin_turns_; }
        [[nodiscard]] std::uint32_t spin_landings() const noexcept { return spin_landing_count_; }
        [[nodiscard]] std::uint32_t obstacles_passed() const noexcept { return obstacles_passed_; }'''
)
replace_exact(
    "src/simulation.hpp",
    '''        float airborne_seconds_{};
        float cumulative_airborne_{};
        float progress_window_seconds_{};''',
    '''        float airborne_seconds_{};
        float cumulative_airborne_{};
        float duck_seconds_{};
        float duck_depth_{};
        float current_airborne_rotation_{};
        float maximum_spin_turns_{};
        std::uint32_t powered_jump_count_{};
        std::uint32_t landed_jump_count_{};
        std::uint32_t spin_landing_count_{};
        std::uint32_t obstacles_passed_{};
        int last_passed_feature_sequence_{ course_safe_runway_markers - 1 };
        bool duck_active_{};
        bool powered_takeoff_{};
        bool powered_takeoff_this_step_{};
        bool powered_landing_this_step_{};
        bool spin_landing_this_step_{};
        bool passed_obstacle_this_step_{};
        bool collision_contact_active_{};
        bool collision_event_this_step_{};
        float progress_window_seconds_{};'''
)
replace_exact(
    "src/simulation.hpp",
    '''        if (stage == CourseStage::hurdles)
            return selector == 0 ? CourseFeatureKind::rock : CourseFeatureKind::hurdle;''',
    '''        if (stage == CourseStage::hurdles)
        {
            if (selector == 0)
                return CourseFeatureKind::rock;
            if (selector == 1)
                return CourseFeatureKind::hurdle;
            return CourseFeatureKind::overhead_bar;
        }'''
)

replace_exact(
    "src/simulation.cpp",
    '''        if (course_stage_ == CourseStage::balance || course_stage_ == CourseStage::walk)
            return 0.0f;''',
    '''        if (course_stage_ != CourseStage::moving_hazards)
            return 0.0f;'''
)
replace_exact(
    "src/simulation.cpp",
    '''        if (static_cast<std::uint8_t>(course_stage_)
            < static_cast<std::uint8_t>(CourseStage::hurdles))
            return;''',
    '''        if (course_stage_ != CourseStage::hurdles
            && course_stage_ != CourseStage::moving_hazards)
            return;'''
)
replace_exact(
    "src/simulation.cpp",
    '''        airborne_seconds_ = 0.0f;
        cumulative_airborne_ = 0.0f;
        progress_window_seconds_ = 0.0f;''',
    '''        airborne_seconds_ = 0.0f;
        cumulative_airborne_ = 0.0f;
        duck_seconds_ = 0.0f;
        duck_depth_ = 0.0f;
        current_airborne_rotation_ = 0.0f;
        maximum_spin_turns_ = 0.0f;
        powered_jump_count_ = 0;
        landed_jump_count_ = 0;
        spin_landing_count_ = 0;
        obstacles_passed_ = 0;
        last_passed_feature_sequence_ = course_safe_runway_markers - 1;
        duck_active_ = false;
        powered_takeoff_ = false;
        powered_takeoff_this_step_ = false;
        powered_landing_this_step_ = false;
        spin_landing_this_step_ = false;
        passed_obstacle_this_step_ = false;
        collision_contact_active_ = false;
        collision_event_this_step_ = false;
        progress_window_seconds_ = 0.0f;'''
)
replace_exact(
    "src/simulation.cpp",
    '''        const bool left = contact_supported(blueprint_.left_contact_node);
        const bool right = contact_supported(blueprint_.right_contact_node);
        const bool new_left = left && !previous_left_grounded_;
        const bool new_right = right && !previous_right_grounded_;''',
    '''        const bool left = contact_supported(blueprint_.left_contact_node);
        const bool right = contact_supported(blueprint_.right_contact_node);
        const bool was_supported = previous_left_grounded_ || previous_right_grounded_;
        const bool new_left = left && !previous_left_grounded_;
        const bool new_right = right && !previous_right_grounded_;'''
)
replace_exact(
    "src/simulation.cpp",
    '''        previous_left_grounded_ = left;
        previous_right_grounded_ = right;

        const float left_slip = left''',
    '''        const float left_slip = left'''
)
replace_exact(
    "src/simulation.cpp",
    '''        const float root_speed = valid_node(blueprint_.root_node)
            ? (particles_[blueprint_.root_node].position.x
                - particles_[blueprint_.root_node].previous.x) / std::max(dt, 1.0e-5f)
            : 0.0f;
        const float torso_angle = torso_roll_angle();
        torso_turn_speed_ = wrap_angle(torso_angle - previous_torso_angle_)
            / std::max(dt, 1.0e-5f);
        previous_torso_angle_ = torso_angle;
        non_foot_grounded_ = non_foot_ground_contact();
        const bool feet_supported = left || right;''',
    '''        const float root_speed = valid_node(blueprint_.root_node)
            ? (particles_[blueprint_.root_node].position.x
                - particles_[blueprint_.root_node].previous.x) / std::max(dt, 1.0e-5f)
            : 0.0f;
        const float root_vertical_speed = valid_node(blueprint_.root_node)
            ? (particles_[blueprint_.root_node].position.y
                - particles_[blueprint_.root_node].previous.y) / std::max(dt, 1.0e-5f)
            : 0.0f;
        const float torso_angle = torso_roll_angle();
        const float torso_delta = wrap_angle(torso_angle - previous_torso_angle_);
        torso_turn_speed_ = torso_delta / std::max(dt, 1.0e-5f);
        previous_torso_angle_ = torso_angle;
        non_foot_grounded_ = non_foot_ground_contact();
        const bool feet_supported = left || right;
        const bool airborne = !feet_supported;

        powered_takeoff_this_step_ = false;
        powered_landing_this_step_ = false;
        spin_landing_this_step_ = false;
        passed_obstacle_this_step_ = false;

        const float head_clearance = valid_node(blueprint_.head_node)
            ? particles_[blueprint_.head_node].position.y
                - ground_height_at(particles_[blueprint_.head_node].position.x)
            : 0.0f;
        const float rest_head_clearance = valid_node(blueprint_.head_node)
            ? blueprint_.nodes[blueprint_.head_node].y : 0.0f;
        duck_depth_ = std::max(0.0f, rest_head_clearance - head_clearance);
        duck_active_ = feet_supported && torso_uprightness() > 0.60f && duck_depth_ >= 0.48f;
        if (duck_active_)
            duck_seconds_ += dt;

        if (was_supported && airborne
            && powered_joint_launch(course_stage_, root_vertical_speed, action_energy))
        {
            powered_takeoff_ = true;
            powered_takeoff_this_step_ = true;
            current_airborne_rotation_ = 0.0f;
            ++powered_jump_count_;
        }

        if (airborne)
        {
            current_airborne_rotation_ += torso_delta;
            maximum_spin_turns_ = std::max(maximum_spin_turns_,
                std::abs(current_airborne_rotation_) / (2.0f * pi));
        }
        else if (!was_supported)
        {
            if (powered_takeoff_)
            {
                powered_landing_this_step_ = true;
                ++landed_jump_count_;
                const float landed_turns = std::abs(current_airborne_rotation_) / (2.0f * pi);
                maximum_spin_turns_ = std::max(maximum_spin_turns_, landed_turns);
                if (stage_allows_controlled_flips(course_stage_) && landed_turns >= 0.75f)
                {
                    spin_landing_this_step_ = true;
                    ++spin_landing_count_;
                }
            }
            powered_takeoff_ = false;
            current_airborne_rotation_ = 0.0f;
        }

        previous_left_grounded_ = left;
        previous_right_grounded_ = right;'''
)
replace_exact(
    "src/simulation.cpp",
    '''        if (course_stage_ != CourseStage::balance
            && wheel_sliding_motion(root_speed, left, right, stance_slip_speed_))''',
    '''        if (stage_requires_forward_gait(course_stage_)
            && wheel_sliding_motion(root_speed, left, right, stance_slip_speed_))'''
)
replace_exact(
    "src/simulation.cpp",
    '''        obstacle_lift_clearance_ = std::max(
            contact_cluster_clearance(blueprint_.left_contact_node),
            contact_cluster_clearance(blueprint_.right_contact_node));''',
    '''        obstacle_lift_clearance_ = std::max(
            contact_cluster_clearance(blueprint_.left_contact_node),
            contact_cluster_clearance(blueprint_.right_contact_node));

        int highest_passed_sequence = last_passed_feature_sequence_;
        for (const CourseFeature& feature : course_features_)
        {
            const float trailing_edge = feature.center.x + course_feature_half_width(feature);
            if (trailing_edge < root_x - 0.10f)
                highest_passed_sequence = std::max(highest_passed_sequence, feature.marker_sequence);
        }
        if (highest_passed_sequence > last_passed_feature_sequence_)
        {
            last_passed_feature_sequence_ = highest_passed_sequence;
            ++obstacles_passed_;
            passed_obstacle_this_step_ = true;
        }'''
)
replace_exact(
    "src/simulation.cpp",
    '''        if (course_stage_ != CourseStage::balance && foot_pivot_rolling_motion(root_speed,''',
    '''        if (stage_requires_forward_gait(course_stage_) && foot_pivot_rolling_motion(root_speed,'''
)
replace_exact(
    "src/simulation.cpp",
    '''            if (course_stage_ != CourseStage::balance && (high_energy_stall || inefficient_vibration))
                micro_motion_seconds_ += progress_window_seconds_;''',
    '''            if (stage_requires_forward_gait(course_stage_)
                && (high_energy_stall || inefficient_vibration))
                micro_motion_seconds_ += progress_window_seconds_;'''
)
replace_exact(
    "src/simulation.cpp",
    '''            const bool idle_window = course_stage_ != CourseStage::balance
                && elapsed_seconds_ > rolling_gate_warmup_end_seconds''',
    '''            const bool idle_window = stage_requires_forward_gait(course_stage_)
                && elapsed_seconds_ > rolling_gate_warmup_end_seconds'''
)
replace_exact(
    "src/simulation.cpp",
    '''        const float allowed_airtime = course_stage_ == CourseStage::hurdles ? 1.30f
            : course_stage_ == CourseStage::moving_hazards ? 1.05f
            : course_stage_ >= CourseStage::ramps ? 0.90f : 0.72f;''',
    '''        const float allowed_airtime = allowed_airtime_for_stage(
            course_stage_, powered_takeoff_);''',
    expected=2
)
replace_exact(
    "src/simulation.cpp",
    '''        if (collided_this_step_)
            collision_count_ += 1.0f;''',
    '''        collision_event_this_step_ = collided_this_step_ && !collision_contact_active_;
        if (collision_event_this_step_)
            collision_count_ += 1.0f;
        collision_contact_active_ = collided_this_step_;'''
)
replace_exact(
    "src/simulation.cpp",
    '''        if (!recovery_active_ && recovery_should_start(
            collided_this_step_, upright, geometric_fall, hard_fall))''',
    '''        const bool controlled_airborne_skill = powered_takeoff_
            && !supported && stage_allows_powered_airtime(course_stage_);
        if (!recovery_active_ && !controlled_airborne_skill && recovery_should_start(
            collided_this_step_, upright, geometric_fall, hard_fall))'''
)
replace_exact(
    "src/simulation.cpp",
    '''        invalidate(classify_motion_gate(gated_upright, maximum_speed_kmh_, pelvis_position,
            airborne_seconds_, allowed_airtime, micro_motion_seconds_, terminal_fall));''',
    '''        invalidate(classify_motion_gate(gated_upright, maximum_speed_kmh_, pelvis_position,
            airborne_seconds_, allowed_airtime, micro_motion_seconds_, terminal_fall,
            course_stage_, current_airborne_rotation_ / (2.0f * pi)));'''
)
replace_exact(
    "src/simulation.cpp",
    '''        const float collision_penalty = collided_this_step_ ? 0.070f : 0.0f;''',
    '''        const float collision_penalty = collision_event_this_step_ ? 0.070f : 0.0f;'''
)
replace_exact(
    "src/simulation.cpp",
    '''        if (course_stage_ == CourseStage::balance)
        {
            last_reward_ = std::max(0.0f, upright) * 0.030f
                + contact * 0.0030f
                - std::abs(forward_speed_) * 0.0040f
                - std::abs(distance_travelled_) * 0.0015f
                - action_energy * 0.0012f
                - body_contact_penalty;
        }
        else
        {
            last_reward_ = std::max(0.0f, safe_progress) * 1.65f * gait
                + std::max(0.0f, upright) * 0.012f
                + contact * 0.0006f
                + swing_reward
                + obstacle_lift_reward
                - std::max(0.0f, -safe_progress) * 0.45f
                - action_energy * 0.0010f
                - collision_penalty
                - knee_first_penalty
                - stance_slip_penalty
                - wheel_penalty
                - hazard_stall_penalty
                - body_contact_penalty;
        }''',
    '''        const float forward_gait_reward = std::max(0.0f, safe_progress) * 1.65f * gait;
        const float backward_penalty = std::max(0.0f, -safe_progress) * 0.45f;
        const float duck_reward = duck_active_
            ? 0.018f + clamp(duck_depth_ - 0.48f, 0.0f, 0.80f) * 0.012f : 0.0f;
        const float jump_reward = (powered_takeoff_this_step_ ? 0.10f : 0.0f)
            + (powered_landing_this_step_ ? 0.22f : 0.0f)
            + (powered_takeoff_ && !left_supported && !right_supported ? 0.0025f : 0.0f);
        const float spin_delta_turns = std::abs(torso_turn_speed_) * dt / (2.0f * pi);
        const float spin_reward = stage_allows_controlled_flips(course_stage_) && powered_takeoff_
            ? clamp(spin_delta_turns, 0.0f, 0.08f) * 0.65f : 0.0f;
        const float spin_landing_reward = spin_landing_this_step_
            ? 0.20f + clamp(maximum_spin_turns_, 0.0f, 3.0f) * 0.08f : 0.0f;
        const float pass_reward = passed_obstacle_this_step_ ? 0.18f : 0.0f;
        const float target_speed = 0.90f + course_difficulty_ * 1.30f;
        const float run_reward = stage_requires_forward_gait(course_stage_)
            ? clamp(forward_speed_ / target_speed, 0.0f, 1.0f) * 0.006f : 0.0f;

        switch (course_stage_)
        {
        case CourseStage::balance:
            last_reward_ = std::max(0.0f, upright) * 0.030f
                + contact * 0.0030f
                - std::abs(forward_speed_) * 0.0040f
                - std::abs(distance_travelled_) * 0.0015f
                - action_energy * 0.0012f
                - body_contact_penalty;
            break;
        case CourseStage::walk:
            last_reward_ = std::max(0.0f, upright) * 0.016f
                + contact * 0.0015f + duck_reward
                - std::abs(forward_speed_) * 0.0030f
                - action_energy * 0.0009f - body_contact_penalty;
            break;
        case CourseStage::ramps:
            last_reward_ = std::max(0.0f, upright) * 0.010f
                + contact * 0.0008f + jump_reward
                - std::abs(forward_speed_) * 0.0020f
                - action_energy * 0.0008f - body_contact_penalty;
            break;
        case CourseStage::uneven:
            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.012f
                + contact * 0.0006f + swing_reward + run_reward
                - backward_penalty - action_energy * 0.0010f
                - stance_slip_penalty - wheel_penalty - body_contact_penalty;
            break;
        case CourseStage::hurdles:
            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.011f
                + swing_reward + run_reward + duck_reward * 0.60f + jump_reward
                + obstacle_lift_reward + pass_reward - backward_penalty
                - action_energy * 0.0010f - collision_penalty - knee_first_penalty
                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - body_contact_penalty;
            break;
        case CourseStage::duck_bars:
            last_reward_ = std::max(0.0f, upright) * 0.008f
                + jump_reward + spin_reward + spin_landing_reward
                - std::abs(forward_speed_) * 0.0015f
                - action_energy * 0.0009f - body_contact_penalty;
            break;
        case CourseStage::moving_hazards:
            last_reward_ = forward_gait_reward + std::max(0.0f, upright) * 0.010f
                + swing_reward + run_reward + duck_reward * 0.45f + jump_reward
                + spin_reward + spin_landing_reward + obstacle_lift_reward + pass_reward
                - backward_penalty - action_energy * 0.0010f - collision_penalty
                - knee_first_penalty - stance_slip_penalty - wheel_penalty
                - hazard_stall_penalty - body_contact_penalty;
            break;
        }'''
)
replace_exact(
    "src/simulation.cpp",
    '''        const float timeout = course_stage_ == CourseStage::balance ? 12.0f
            : static_cast<std::uint8_t>(course_stage_) >= static_cast<std::uint8_t>(CourseStage::hurdles)
                ? 48.0f : 30.0f;''',
    '''        const float timeout = course_stage_ == CourseStage::balance ? 12.0f
            : course_stage_ == CourseStage::walk || course_stage_ == CourseStage::ramps
                || course_stage_ == CourseStage::duck_bars ? 20.0f
            : course_stage_ == CourseStage::moving_hazards ? 48.0f : 36.0f;'''
)

replace_exact(
    "tests/core_tests.cpp",
    '''    require(sim::classify_motion_gate(-0.2f, 0.0f, { 0.0f, 3.0f }, 0.0f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::flipped, "flip hard gate missing");''',
    '''    require(sim::classify_motion_gate(-0.2f, 0.0f, { 0.0f, 3.0f }, 0.0f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::flipped, "flip hard gate missing outside flip lessons");
    require(sim::classify_motion_gate(-0.2f, 0.0f, { 0.0f, 4.0f }, 0.4f, 2.7f, 0.0f,
            false, sim::CourseStage::duck_bars, 0.5f)
        == sim::InvalidMotion::none, "controlled flip lesson still rejects an airborne flip");
    require(sim::classify_motion_gate(0.4f, 0.0f, { 0.0f, 4.0f }, 1.2f, 2.7f, 0.0f,
            false, sim::CourseStage::duck_bars, 3.21f)
        == sim::InvalidMotion::excessive_spins, "more than three spins is not rejected");'''
)
replace_exact(
    "tests/core_tests.cpp",
    '''    require(sim::classify_motion_gate(1.0f, 0.0f, { 0.0f, 3.0f }, 0.8f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::sustained_flight, "flight hard gate missing");''',
    '''    require(sim::classify_motion_gate(1.0f, 0.0f, { 0.0f, 3.0f }, 0.8f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::sustained_flight, "unpowered flight hard gate missing");
    require(sim::powered_joint_launch(sim::CourseStage::ramps, 1.0f, 0.08f),
        "joint-powered jump is not recognized");
    require(!sim::powered_joint_launch(sim::CourseStage::walk, 1.0f, 0.08f),
        "duck lesson incorrectly enables flight");
    require(sim::allowed_airtime_for_stage(sim::CourseStage::duck_bars, true) > 2.0f,
        "controlled flip lesson does not allow bounded powered airtime");'''
)
replace_exact(
    "tests/core_tests.cpp",
    '''    require(sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 5)
            == sim::CourseFeatureKind::rock''',
    '''    require(sim::course_stage_name(sim::CourseStage::balance) == "1. STAND"
        && sim::course_stage_name(sim::CourseStage::walk) == "2. DUCK / RECOVER"
        && sim::course_stage_name(sim::CourseStage::ramps) == "3. JUMP / LAND"
        && sim::course_stage_name(sim::CourseStage::uneven) == "4. WALK / RUN"
        && sim::course_stage_name(sim::CourseStage::hurdles) == "5. MOVING DUCK / JUMP"
        && sim::course_stage_name(sim::CourseStage::duck_bars) == "6. CONTROLLED FLIPS"
        && sim::course_stage_name(sim::CourseStage::moving_hazards) == "7. MIXED GOAL COURSE",
        "skill curriculum is not ordered by prerequisite");
    require(sim::stage_skill_evidence(sim::CourseStage::walk, 0u, 0.6f, 0u, 0.0f, 0u, 0u),
        "duck evidence cannot complete the duck lesson");
    require(sim::stage_skill_evidence(sim::CourseStage::ramps, 0u, 0.0f, 1u, 0.0f, 0u, 0u),
        "landed jump cannot complete the jump lesson");
    require(sim::stage_skill_evidence(sim::CourseStage::duck_bars, 0u, 0.0f, 1u, 1.0f, 1u, 0u),
        "controlled landed flip cannot complete the flip lesson");
    require(!sim::stage_skill_evidence(sim::CourseStage::moving_hazards, 2u, 0.0f, 0u, 0.0f, 0u, 0u),
        "mixed goal lesson can complete without passing an obstacle");
    require(sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 5)
            == sim::CourseFeatureKind::rock'''
)

print("Integrated ordered skill simulation core.")
