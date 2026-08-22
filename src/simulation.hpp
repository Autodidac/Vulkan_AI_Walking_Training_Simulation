#pragma once

#include "math.hpp"
#include "deformable_terrain.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace runner::sim
{
    inline constexpr std::size_t anatomy_action_count = 8;
    inline constexpr std::size_t equipment_action_count = 3;
    inline constexpr std::size_t action_count =
        anatomy_action_count + equipment_action_count;
    inline constexpr std::size_t observation_count = 62;
    inline constexpr float foundational_gait_cadence_hz = 1.24f;

    enum class ShuttlePhase : std::uint8_t
    {
        traverse,
        braking,
        backing,
        turning
    };

    [[nodiscard]] inline std::string_view shuttle_phase_name(
        ShuttlePhase phase) noexcept
    {
        switch (phase)
        {
        case ShuttlePhase::traverse: return "FORWARD";
        case ShuttlePhase::braking: return "BRAKING";
        case ShuttlePhase::backing: return "BACKING";
        case ShuttlePhase::turning: return "TURNING";
        }
        return "UNKNOWN";
    }

    struct ShuttleState
    {
        ShuttlePhase phase{ ShuttlePhase::traverse };
        float facing_direction{ 1.0f };
        float locomotion_direction{ 1.0f };
        float phase_origin_x{};
        float phase_seconds{};
        std::uint32_t completed_turns{};
    };

    inline constexpr float shuttle_left_boundary = -2.0f;
    inline constexpr float shuttle_right_boundary = 10.0f;
    inline constexpr float shuttle_brake_seconds = 0.65f;
    inline constexpr float shuttle_brake_speed = 0.22f;
    inline constexpr float shuttle_backup_distance = 0.12f;
    inline constexpr float shuttle_backup_timeout_seconds = 0.75f;
    inline constexpr float shuttle_turn_seconds = 0.32f;

    [[nodiscard]] inline bool shuttle_dynamic_course_ready(
        std::uint32_t completed_turns, std::uint32_t gait_cycles,
        float stable_seconds) noexcept
    {
        return completed_turns >= 2u && gait_cycles >= 14u
            && std::isfinite(stable_seconds) && stable_seconds >= 2.0f;
    }
    [[nodiscard]] inline ShuttleState advance_shuttle_state(
        ShuttleState state, float root_x, float dt,
        float root_speed = 0.0f) noexcept
    {
        if (!std::isfinite(root_x) || !std::isfinite(dt) || dt <= 0.0f)
            return state;
        if (state.phase == ShuttlePhase::traverse)
        {
            state.phase_seconds += dt;
            const bool at_boundary = state.facing_direction > 0.0f
                ? root_x >= shuttle_right_boundary
                : root_x <= shuttle_left_boundary;
            if (at_boundary)
            {
                state.phase = ShuttlePhase::braking;
                state.locomotion_direction = 0.0f;
                state.phase_origin_x = root_x;
                state.phase_seconds = 0.0f;
            }
        }
        else if (state.phase == ShuttlePhase::braking)
        {
            state.phase_seconds += dt;
            if (state.phase_seconds >= shuttle_brake_seconds
                && std::isfinite(root_speed)
                && std::abs(root_speed) <= shuttle_brake_speed)
            {
                state.phase = ShuttlePhase::backing;
                state.locomotion_direction = -state.facing_direction;
                state.phase_origin_x = root_x;
                state.phase_seconds = 0.0f;
            }
        }
        else if (state.phase == ShuttlePhase::backing)
        {
            state.phase_seconds += dt;
            const float backed_distance = (root_x - state.phase_origin_x)
                * state.locomotion_direction;
            if (backed_distance >= shuttle_backup_distance
                || state.phase_seconds >= shuttle_backup_timeout_seconds)
            {
                state.phase = ShuttlePhase::turning;
                state.locomotion_direction = 0.0f;
                state.phase_seconds = 0.0f;
            }
        }
        else
        {
            state.phase_seconds += dt;
            if (state.phase_seconds >= shuttle_turn_seconds)
            {
                state.phase = ShuttlePhase::traverse;
                state.facing_direction = -state.facing_direction;
                state.locomotion_direction = state.facing_direction;
                state.phase_origin_x = root_x;
                state.phase_seconds = 0.0f;
                ++state.completed_turns;
            }
        }
        return state;
    }

    enum class CourseStage : std::uint8_t
    {
        balance,
        duck_press,
        uneven,
        crouch_walk,
        ramps,
        hurdles,
        duck_bars,
        moving_hazards,
        climb_descent,
        equipment_targets,
        combat_course,
        // Appended to preserve every persisted v0.7.35 numeric stage id.
        shuttle
    };

    inline constexpr std::size_t course_stage_count = 12;

    [[nodiscard]] inline constexpr std::size_t course_stage_curriculum_index(
        CourseStage stage) noexcept
    {
        switch (stage)
        {
        case CourseStage::balance: return 0u;
        case CourseStage::duck_press: return 1u;
        case CourseStage::uneven: return 2u;
        case CourseStage::shuttle: return 3u;
        case CourseStage::crouch_walk: return 4u;
        case CourseStage::ramps: return 5u;
        case CourseStage::hurdles: return 6u;
        case CourseStage::duck_bars: return 7u;
        case CourseStage::moving_hazards: return 8u;
        case CourseStage::climb_descent: return 9u;
        case CourseStage::equipment_targets: return 10u;
        case CourseStage::combat_course: return 11u;
        }
        return 0u;
    }

    [[nodiscard]] inline constexpr CourseStage next_course_stage(
        CourseStage stage) noexcept
    {
        switch (stage)
        {
        case CourseStage::balance: return CourseStage::duck_press;
        case CourseStage::duck_press: return CourseStage::uneven;
        case CourseStage::uneven: return CourseStage::shuttle;
        case CourseStage::shuttle: return CourseStage::crouch_walk;
        case CourseStage::crouch_walk: return CourseStage::ramps;
        case CourseStage::ramps: return CourseStage::hurdles;
        case CourseStage::hurdles: return CourseStage::duck_bars;
        case CourseStage::duck_bars: return CourseStage::moving_hazards;
        case CourseStage::moving_hazards: return CourseStage::climb_descent;
        case CourseStage::climb_descent: return CourseStage::equipment_targets;
        case CourseStage::equipment_targets: return CourseStage::combat_course;
        case CourseStage::combat_course: return CourseStage::combat_course;
        }
        return CourseStage::balance;
    }

    [[nodiscard]] inline bool stage_uses_deformable_terrain(CourseStage stage) noexcept
    {
        return stage == CourseStage::uneven
            || stage == CourseStage::crouch_walk
            || stage == CourseStage::hurdles
            || stage == CourseStage::moving_hazards
            || stage == CourseStage::combat_course;
    }

    [[nodiscard]] constexpr float terrain_sample_x(float world_x,
        float course_progress) noexcept
    {
        return world_x + course_progress;
    }

    [[nodiscard]] constexpr float terrain_world_x(float terrain_x,
        float course_progress) noexcept
    {
        return terrain_x - course_progress;
    }

    [[nodiscard]] constexpr float terrain_relative_distance(float world_x,
        float initial_world_x, float course_progress) noexcept
    {
        return terrain_sample_x(world_x, course_progress) - initial_world_x;
    }

    [[nodiscard]] constexpr float terrain_relative_frame_progress(
        float current_world_x, float previous_world_x,
        float course_speed, float dt) noexcept
    {
        return (current_world_x - previous_world_x) + course_speed * dt;
    }

    inline constexpr float odometer_speed_limit_mps = 50.0f / 3.6f;

    [[nodiscard]] inline float accepted_forward_odometer_progress(
        float world_displacement, float dt) noexcept
    {
        if (!std::isfinite(world_displacement) || !std::isfinite(dt) || dt <= 0.0f)
            return 0.0f;
        const float maximum_displacement = odometer_speed_limit_mps * dt;
        return world_displacement >= 0.0f && world_displacement <= maximum_displacement
            ? world_displacement : 0.0f;
    }

    [[nodiscard]] inline float accepted_directed_odometer_progress(
        float world_displacement, float dt, float direction) noexcept
    {
        if (!std::isfinite(direction) || std::abs(direction) < 0.5f)
            return 0.0f;
        return accepted_forward_odometer_progress(
            world_displacement * (direction < 0.0f ? -1.0f : 1.0f), dt);
    }

    [[nodiscard]] inline bool stage_requires_forward_gait(CourseStage stage) noexcept
    {
        return stage == CourseStage::uneven
            || stage == CourseStage::shuttle
            || stage == CourseStage::crouch_walk
            || stage == CourseStage::hurdles
            || stage == CourseStage::moving_hazards
            || stage == CourseStage::climb_descent
            || stage == CourseStage::combat_course;
    }

    [[nodiscard]] inline bool stage_allows_powered_airtime(CourseStage stage) noexcept
    {
        return stage == CourseStage::ramps
            || stage == CourseStage::hurdles
            || stage == CourseStage::duck_bars
            || stage == CourseStage::moving_hazards
            || stage == CourseStage::combat_course;
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
        if (stage == CourseStage::moving_hazards
            || stage == CourseStage::combat_course)
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
        case CourseStage::duck_press:
            return duck_seconds >= 0.75f && obstacles_passed >= 1u;
        case CourseStage::uneven:
            return alternating_steps >= 10u;
        case CourseStage::shuttle:
            return alternating_steps >= 12u;
        case CourseStage::crouch_walk:
            return alternating_steps >= 8u && duck_seconds >= 2.0f
                && obstacles_passed >= 3u;
        case CourseStage::ramps:
            return landed_jumps >= 1u;
        case CourseStage::hurdles:
            return alternating_steps >= 2u && obstacles_passed >= 1u
                && (duck_seconds >= 0.25f || landed_jumps >= 1u);
        case CourseStage::duck_bars:
            return spin_landings >= 1u && maximum_spin_turns >= 0.75f;
        case CourseStage::moving_hazards:
        case CourseStage::combat_course:
            return alternating_steps >= 2u && obstacles_passed >= 1u
                && (duck_seconds >= 0.25f || landed_jumps >= 1u || spin_landings >= 1u);
        case CourseStage::climb_descent:
        case CourseStage::equipment_targets:
            return true;
        }
        return false;
    }

    [[nodiscard]] inline std::string_view course_stage_name(CourseStage stage) noexcept
    {
        switch (stage)
        {
        case CourseStage::balance: return "1. STAND";
        case CourseStage::duck_press: return "2. STATIC CROUCH / HOLD / RECOVER";
        case CourseStage::uneven: return "3. WALK / RUN";
        case CourseStage::shuttle: return "4. BACK / TURN / RETURN";
        case CourseStage::crouch_walk: return "5. CROUCH WALK / UNEVEN AVOID";
        case CourseStage::ramps: return "6. JUMP / LAND";
        case CourseStage::hurdles: return "7. MOVING LOW BAR / HURDLE";
        case CourseStage::duck_bars: return "8. CONTROLLED FLIPS";
        case CourseStage::moving_hazards: return "9. MIXED GOAL COURSE";
        case CourseStage::climb_descent: return "10. CLIMB / BACKWARD DESCENT";
        case CourseStage::equipment_targets: return "11. EQUIPMENT / TARGETS";
        case CourseStage::combat_course: return "12. MOVE / AIM / FIRE";
        }
        return "UNKNOWN";
    }

    enum class CourseFeatureKind : std::uint8_t
    {
        hurdle,
        overhead_bar,
        duck_press,
        ledge,
        moving_hazard,
        rock,
        projectile
    };

    [[nodiscard]] inline std::string_view course_feature_name(CourseFeatureKind kind) noexcept
    {
        switch (kind)
        {
        case CourseFeatureKind::hurdle: return "HURDLE";
        case CourseFeatureKind::overhead_bar: return "LOW BAR";
        case CourseFeatureKind::duck_press: return "DUCK PRESS";
        case CourseFeatureKind::ledge: return "CLIMB LEDGE";
        case CourseFeatureKind::moving_hazard: return "MOVING HAZARD";
        case CourseFeatureKind::rock: return "ROCK";
        case CourseFeatureKind::projectile: return "THROWN OBJECT";
        }
        return "OBSTACLE";
    }

    struct CourseFeature
    {
        CourseFeatureKind kind{};
        Vec2 center{};
        Vec2 half_extent{};
        float radius{};
        Vec2 velocity{};
        int marker_sequence{ -1 };
    };

    [[nodiscard]] inline float course_feature_half_width(const CourseFeature& feature) noexcept
    {
        switch (feature.kind)
        {
        case CourseFeatureKind::moving_hazard:
        case CourseFeatureKind::rock:
        case CourseFeatureKind::projectile:
            return feature.radius;
        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
        case CourseFeatureKind::duck_press:
        case CourseFeatureKind::ledge:
            return feature.half_extent.x;
        }
        return 0.0f;
    }

    [[nodiscard]] inline float course_feature_top(const CourseFeature& feature) noexcept
    {
        switch (feature.kind)
        {
        case CourseFeatureKind::moving_hazard:
        case CourseFeatureKind::rock:
        case CourseFeatureKind::projectile:
            return feature.center.y + feature.radius;
        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
        case CourseFeatureKind::duck_press:
        case CourseFeatureKind::ledge:
            return feature.center.y + feature.half_extent.y;
        }
        return feature.center.y;
    }

    [[nodiscard]] inline bool knee_crosses_before_foot(float knee_front_x,
        float foot_front_x, float foot_top_y, const CourseFeature& feature) noexcept
    {
        if (feature.kind != CourseFeatureKind::rock
            && feature.kind != CourseFeatureKind::hurdle)
            return false;

        // Natural stepping often puts a bent knee slightly ahead of the foot.
        // Reject only an obvious body/joint-first shove: the knee must lead well
        // into the obstacle while the foot is both substantially behind it and
        // still below useful clearance. This remains guidance, not a hard gate.
        const float obstacle_front = feature.center.x + course_feature_half_width(feature);
        const float obstacle_top = course_feature_top(feature);
        const float knee_lead = knee_front_x - feature.center.x;
        const float foot_lag = obstacle_front - foot_front_x;
        const float clearance_deficit = obstacle_top + 0.015f - foot_top_y;
        return knee_lead > 0.24f
            && foot_lag > 0.16f
            && clearance_deficit > 0.08f;
    }

    [[nodiscard]] inline float gait_progress_multiplier(std::uint32_t alternating_steps,
        bool single_support, float swing_clearance) noexcept
    {
        if (alternating_steps == 0)
            return single_support && swing_clearance > 0.10f ? 0.12f : 0.0f;
        const float established = clamp(0.30f + static_cast<float>(alternating_steps) * 0.10f,
            0.30f, 1.0f);
        const float swing_bonus = single_support && swing_clearance > 0.10f ? 0.12f : 0.0f;
        return clamp(established + swing_bonus, 0.0f, 1.0f);
    }

    [[nodiscard]] inline bool sagittal_gait_evidence(
        std::uint32_t alternating_steps, std::uint32_t limb_crossings,
        float distance, float elapsed_seconds, float support_span_ratio) noexcept
    {
        return alternating_steps >= 10u
            && limb_crossings >= 8u
            && distance >= 6.0f
            && elapsed_seconds >= 8.0f
            && support_span_ratio >= 0.42f
            && support_span_ratio <= 1.45f;
    }

    [[nodiscard]] inline bool crab_walking_motion(
        std::uint32_t alternating_steps, std::uint32_t limb_crossings,
        float distance, float elapsed_seconds, float support_span_ratio) noexcept
    {
        const bool established_sagittal_crossing = alternating_steps >= 6u
            && limb_crossings >= 4u
            && static_cast<std::uint64_t>(limb_crossings) * 5u
                >= static_cast<std::uint64_t>(alternating_steps) * 3u;
        return elapsed_seconds >= 4.0f
            && distance >= 0.75f
            && !established_sagittal_crossing
            && (support_span_ratio > 1.55f
                || (alternating_steps >= 4u && limb_crossings < 2u));
    }

    [[nodiscard]] inline float sagittal_crossing_shaping_reward(
        bool forward_gait, bool paired_legs, bool crossing_this_step) noexcept
    {
        return forward_gait && paired_legs && crossing_this_step ? 0.250f : 0.0f;
    }

    [[nodiscard]] inline float lateral_crab_shaping_penalty(
        bool forward_gait, bool paired_legs, std::uint32_t alternating_steps,
        std::uint32_t limb_crossings, float distance, float elapsed_seconds,
        float support_span_ratio) noexcept
    {
        return forward_gait && paired_legs
            && crab_walking_motion(alternating_steps, limb_crossings,
                distance, elapsed_seconds, support_span_ratio)
            ? 0.012f : 0.0f;
    }

    [[nodiscard]] inline bool strict_segment_crossing(Vec2 a, Vec2 b,
        Vec2 c, Vec2 d) noexcept
    {
        const float ab_c = cross(b - a, c - a);
        const float ab_d = cross(b - a, d - a);
        const float cd_a = cross(d - c, a - c);
        const float cd_b = cross(d - c, b - c);
        constexpr float epsilon = 1.0e-5f;
        return ab_c * ab_d < -epsilon && cd_a * cd_b < -epsilon;
    }

    inline constexpr float forward_gait_quality_grace_seconds = 0.50f;

    [[nodiscard]] inline bool measure_forward_gait_faults(
        ShuttlePhase phase, float elapsed_seconds) noexcept
    {
        return phase == ShuttlePhase::traverse
            && std::isfinite(elapsed_seconds)
            && elapsed_seconds >= forward_gait_quality_grace_seconds;
    }

    inline constexpr float sustained_scissor_limit_seconds = 0.34f;
    inline constexpr float backward_brace_activation_ratio = 0.24f;
    inline constexpr float sustained_backward_brace_limit_seconds = 0.80f;

    [[nodiscard]] inline float directional_backward_brace_ratio(
        Vec2 authored_axis, Vec2 current_axis, float travel_direction) noexcept
    {
        const float authored_length = length(authored_axis);
        const float current_length = length(current_axis);
        if (!std::isfinite(authored_length) || !std::isfinite(current_length)
            || !std::isfinite(travel_direction) || authored_length <= 1.0e-5f
            || current_length <= 1.0e-5f || std::abs(travel_direction) < 0.5f)
            return 0.0f;
        const Vec2 current = current_axis / current_length;
        const float direction = travel_direction < 0.0f ? -1.0f : 1.0f;
        // Authored geometry determines segment length and neutral joint shape,
        // but it may not waive ground-relative posture truth. An edited rest
        // axis that already leans backward must still be trained upright.
        return std::max(0.0f, -current.x * direction);
    }

    [[nodiscard]] inline float contiguous_condition_seconds(bool active,
        float prior_seconds, float dt) noexcept
    {
        if (!active || !std::isfinite(prior_seconds) || !std::isfinite(dt)
            || prior_seconds < 0.0f || dt <= 0.0f)
            return 0.0f;
        return prior_seconds + dt;
    }

    [[nodiscard]] inline bool awkward_paired_passing_pose(
        float knee_span, float foot_span, float leg_length) noexcept
    {
        if (!std::isfinite(knee_span) || !std::isfinite(foot_span)
            || !std::isfinite(leg_length) || leg_length <= 0.01f)
            return false;
        const float knee_ratio = std::abs(knee_span) / leg_length;
        const float foot_ratio = std::abs(foot_span) / leg_length;
        return knee_ratio > 0.52f && foot_ratio < 0.12f;
    }

    struct CasualGaitEvidence
    {
        float quality{};
        float support_ratio{};
        float clearance_ratio{};
        bool relaxed{};
        bool tiny_shuffle{};
        bool high_march{};
        bool overstride{};
        bool cadence_fault{};
        bool backward_brace{};
    };

    [[nodiscard]] inline float bounded_casual_window(float value,
        float outer_low, float inner_low, float inner_high,
        float outer_high) noexcept
    {
        if (!std::isfinite(value) || value <= outer_low || value >= outer_high
            || !(outer_low < inner_low && inner_low <= inner_high
                && inner_high < outer_high))
            return 0.0f;
        if (value >= inner_low && value <= inner_high)
            return 1.0f;
        if (value < inner_low)
            return clamp((value - outer_low) / (inner_low - outer_low),
                0.0f, 1.0f);
        return clamp((outer_high - value) / (outer_high - inner_high),
            0.0f, 1.0f);
    }

    [[nodiscard]] inline CasualGaitEvidence casual_gait_evidence(
        float authored_leg_length, float support_separation,
        float swing_clearance, float step_cadence_hz,
        float backward_brace_ratio) noexcept
    {
        CasualGaitEvidence evidence{};
        if (!std::isfinite(authored_leg_length)
            || !std::isfinite(support_separation)
            || !std::isfinite(swing_clearance)
            || !std::isfinite(step_cadence_hz)
            || !std::isfinite(backward_brace_ratio)
            || authored_leg_length <= 0.01f || support_separation < 0.0f
            || swing_clearance < 0.0f || step_cadence_hz < 0.0f)
            return evidence;

        evidence.support_ratio = support_separation / authored_leg_length;
        evidence.clearance_ratio = swing_clearance / authored_leg_length;
        evidence.tiny_shuffle = evidence.support_ratio < 0.08f;
        evidence.overstride = evidence.support_ratio > 0.62f;
        evidence.high_march = evidence.clearance_ratio > 0.26f;
        evidence.cadence_fault = step_cadence_hz < 0.60f
            || step_cadence_hz > 3.20f;
        evidence.backward_brace = backward_brace_ratio
            > backward_brace_activation_ratio;

        const float stance_quality = bounded_casual_window(
            evidence.support_ratio, 0.08f, 0.16f, 0.44f, 0.62f);
        const float clearance_quality = bounded_casual_window(
            evidence.clearance_ratio, 0.025f, 0.055f, 0.17f, 0.26f);
        const float cadence_quality = bounded_casual_window(
            step_cadence_hz, 0.60f, 0.95f, 2.40f, 3.20f);
        const float posture_quality = 1.0f - clamp((backward_brace_ratio - 0.04f)
            / 0.20f, 0.0f, 1.0f);
        evidence.quality = std::min(std::min(stance_quality, clearance_quality),
            std::min(cadence_quality, posture_quality));
        evidence.relaxed = evidence.quality >= 0.75f
            && !evidence.tiny_shuffle && !evidence.high_march
            && !evidence.overstride && !evidence.cadence_fault
            && !evidence.backward_brace;
        return evidence;
    }


    [[nodiscard]] inline bool pathological_lower_leg_crossing(
        bool strict_crossing, bool left_swinging, bool right_swinging,
        bool awkward_pose) noexcept
    {
        const bool single_support_pass = left_swinging != right_swinging;
        return awkward_pose || (strict_crossing && !single_support_pass);
    }

    [[nodiscard]] inline float lower_leg_scissor_shaping_penalty(
        bool forward_gait, bool paired_legs, float contiguous_seconds) noexcept
    {
        return forward_gait && paired_legs
            ? clamp(contiguous_seconds - 0.08f, 0.0f, 0.50f) * 0.060f
            : 0.0f;
    }

    [[nodiscard]] inline bool friction_driven_shuffle(float root_speed,
        bool left_supported, bool right_supported, float stance_slip_speed,
        std::uint32_t gait_cycles, float swing_clearance) noexcept
    {
        return left_supported && right_supported
            && gait_cycles == 0u && swing_clearance < 0.06f
            && std::abs(root_speed) > 0.35f && stance_slip_speed > 0.24f;
    }

    [[nodiscard]] inline bool duck_ground_contact_allowed(bool duck_active,
        bool non_foot_grounded) noexcept
    {
        return !duck_active || !non_foot_grounded;
    }

    [[nodiscard]] inline bool controlled_somersault_allowed(CourseStage stage,
        float spin_turns, float torso_turn_speed, bool airborne_or_landing) noexcept
    {
        return stage_allows_controlled_flips(stage)
            && airborne_or_landing
            && std::abs(torso_turn_speed) >= 0.45f
            && std::abs(spin_turns) <= 3.0f;
    }

    [[nodiscard]] inline bool forward_prone_allowed(CourseStage stage,
        bool non_foot_grounded, bool head_faces_forward, float uprightness,
        float forward_speed) noexcept
    {
        const bool recovery_stage = stage == CourseStage::uneven
            || stage == CourseStage::ramps
            || stage == CourseStage::hurdles
            || stage == CourseStage::duck_bars
            || stage == CourseStage::moving_hazards
            || stage == CourseStage::combat_course;
        return recovery_stage && non_foot_grounded && head_faces_forward
            && uprightness <= 0.42f && forward_speed >= -0.15f;
    }

    [[nodiscard]] inline bool rolling_body_motion(float root_speed, float torso_turn_speed,
        float uprightness, bool feet_supported, bool non_foot_grounded) noexcept
    {
        return non_foot_grounded
            && (std::abs(torso_turn_speed) > 0.45f || uprightness < 0.55f)
            && (!feet_supported || std::abs(root_speed) > 0.08f);
    }

    [[nodiscard]] inline float ground_contact_offset(bool traction_contact,
        float particle_radius) noexcept
    {
        return traction_contact ? std::min(particle_radius, 0.065f) : particle_radius;
    }

    [[nodiscard]] inline bool foot_pivot_rolling_motion(float root_speed,
        bool left_supported, bool right_supported, float stance_slip_speed,
        float maximum_foot_clearance, float torso_turn_speed,
        std::size_t authored_support_count = 2u,
        std::size_t meaningfully_lifted_supports = 0u,
        bool recent_authored_support_transfer = false,
        std::size_t independent_support_clusters = 2u) noexcept
    {
        return left_supported && right_supported
            && independent_support_clusters >= 2u
            && std::abs(root_speed) > 0.085f
            && stance_slip_speed < 0.080f
            && maximum_foot_clearance < 0.085f
            && !recent_authored_support_transfer
            && (authored_support_count <= 2u || meaningfully_lifted_supports == 0u)
            && (std::abs(torso_turn_speed) > 0.12f || std::abs(root_speed) > 0.18f);
    }

    [[nodiscard]] inline float unsupported_locomotion_penalty(
        bool locomotion_required, bool powered_takeoff,
        float airborne_seconds) noexcept
    {
        if (!locomotion_required || powered_takeoff
            || !std::isfinite(airborne_seconds))
            return 0.0f;
        return std::max(0.0f, airborne_seconds - 0.12f) * 0.16f;
    }

    inline constexpr float rolling_gate_activation_seconds = 1.35f;
    inline constexpr float rolling_gate_warmup_end_seconds = 2.60f;

    [[nodiscard]] inline bool rolling_gate_active(float elapsed_seconds) noexcept
    {
        return elapsed_seconds >= rolling_gate_activation_seconds;
    }

    [[nodiscard]] inline float body_rolling_limit(CourseStage stage,
        float elapsed_seconds) noexcept
    {
        if (elapsed_seconds < rolling_gate_warmup_end_seconds)
            return stage == CourseStage::balance ? 0.78f : 0.55f;
        return stage == CourseStage::balance ? 0.55f : 0.32f;
    }

    [[nodiscard]] inline float head_contact_limit(float elapsed_seconds) noexcept
    {
        return elapsed_seconds < rolling_gate_warmup_end_seconds ? 0.38f : 0.24f;
    }

    [[nodiscard]] inline float foot_pivot_rolling_limit(float elapsed_seconds) noexcept
    {
        return elapsed_seconds < rolling_gate_warmup_end_seconds ? 0.68f : 0.42f;
    }

    [[nodiscard]] inline bool zero_progress_window(float net_progress,
        std::uint32_t new_steps, float useful_foot_lift, bool recovering) noexcept
    {
        return !recovering && net_progress < 0.045f
            && new_steps == 0u && useful_foot_lift < 0.11f;
    }

    [[nodiscard]] inline bool micro_motion_window(float average_energy,
        float net_progress, float root_path, std::uint32_t new_gait_events,
        bool locomotion_required, bool recovery_or_terrain_step) noexcept
    {
        if (!locomotion_required || recovery_or_terrain_step
            || new_gait_events > 0u)
            return false;
        const bool high_energy_stall = average_energy > 0.10f
            && net_progress < 0.05f;
        const bool inefficient_vibration = average_energy > 0.16f
            && net_progress < 0.12f
            && root_path > std::max(0.08f, net_progress * 2.5f);
        return high_energy_stall || inefficient_vibration;
    }

    [[nodiscard]] inline float update_zero_progress_seconds(float previous_seconds,
        bool zero_progress, float window_seconds) noexcept
    {
        return zero_progress
            ? previous_seconds + window_seconds
            : std::max(0.0f, previous_seconds - window_seconds * 2.0f);
    }

    inline constexpr float zero_progress_reset_seconds = 1.80f;

    [[nodiscard]] inline bool ground_clearance_hazard(CourseFeatureKind kind) noexcept
    {
        return kind == CourseFeatureKind::rock || kind == CourseFeatureKind::hurdle;
    }

    [[nodiscard]] inline float hazard_approach_weight(float distance_ahead) noexcept
    {
        if (distance_ahead <= -0.20f || distance_ahead >= 2.60f)
            return 0.0f;
        if (distance_ahead <= 0.45f)
            return 1.0f;
        return clamp((2.60f - distance_ahead) / 2.15f, 0.0f, 1.0f);
    }

    [[nodiscard]] inline float duck_obstacle_approach_weight(float distance_ahead) noexcept
    {
        if (distance_ahead <= -1.25f || distance_ahead >= 8.0f)
            return 0.0f;
        if (distance_ahead <= 2.25f)
            return 1.0f;
        return clamp((8.0f - distance_ahead) / 5.75f, 0.0f, 1.0f);
    }

    struct DuckPressProfile
    {
        float bottom_y{};
        float vertical_velocity{};
        bool descending{};
        bool holding{};
        bool retracting{};
    };

    [[nodiscard]] inline DuckPressProfile duck_press_profile(float elapsed_seconds,
        float difficulty, float standing_head_top,
        bool horizontal_body_plan = false) noexcept
    {
        const float settle_end = horizontal_body_plan ? 2.75f : 2.50f;
        const float descend_end = horizontal_body_plan ? 6.25f : 5.00f;
        const float hold_end = horizontal_body_plan ? 8.25f : 7.00f;
        const float retract_end = horizontal_body_plan ? 10.75f : 9.50f;
        const float cycle = horizontal_body_plan ? 12.25f : 11.0f;
        float local = std::fmod(std::max(0.0f, elapsed_seconds), cycle);
        if (local < 0.0f)
            local += cycle;
        const float start = standing_head_top + 1.10f;
        const float crouch_drop = horizontal_body_plan
            ? clamp(standing_head_top * 0.070f, 0.20f, 0.28f)
                + clamp(difficulty, 0.0f, 1.0f) * 0.020f
            : clamp(standing_head_top * 0.16f, 0.78f, 0.86f)
                + clamp(difficulty, 0.0f, 1.0f) * 0.08f;
        const float target = standing_head_top - crouch_drop;
        if (local < settle_end)
            return { start, 0.0f, false, false, false };
        if (local < descend_end)
        {
            const float duration = descend_end - settle_end;
            const float t = (local - settle_end) / duration;
            const float smooth = t * t * (3.0f - 2.0f * t);
            const float derivative = 6.0f * t * (1.0f - t) / duration;
            return { lerp(start, target, smooth),
                (target - start) * derivative, true, false, false };
        }
        if (local < hold_end)
            return { target, 0.0f, false, true, false };
        if (local < retract_end)
        {
            const float duration = retract_end - hold_end;
            const float t = (local - hold_end) / duration;
            const float smooth = t * t * (3.0f - 2.0f * t);
            const float derivative = 6.0f * t * (1.0f - t) / duration;
            return { lerp(target, start, smooth),
                (start - target) * derivative, false, false, true };
        }
        return { start, 0.0f, false, false, false };
    }

    [[nodiscard]] inline bool hazard_quiver_motion(float distance_ahead, float root_speed,
        float lifted_foot_clearance, float target_clearance, float action_energy) noexcept
    {
        return hazard_approach_weight(distance_ahead) > 0.35f
            && std::abs(root_speed) < 0.16f
            && lifted_foot_clearance < target_clearance * 0.55f
            && action_energy > 0.075f;
    }

    inline constexpr float terrain_cycle_length_m = 56.0f;

    [[nodiscard]] inline bool course_zone_is_flat(float course_distance) noexcept
    {
        float local = std::fmod(std::max(0.0f, course_distance), terrain_cycle_length_m);
        if (local < 0.0f)
            local += terrain_cycle_length_m;
        return local < 28.0f || local >= 44.0f;
    }

    [[nodiscard]] inline bool obstacles_require_flat_zone(CourseStage stage,
        float difficulty) noexcept
    {
        return (stage != CourseStage::moving_hazards
            && stage != CourseStage::combat_course) || difficulty < 0.70f;
    }

    [[nodiscard]] inline float course_feature_observation_size(
        const CourseFeature& feature) noexcept
    {
        switch (feature.kind)
        {
        case CourseFeatureKind::moving_hazard:
        case CourseFeatureKind::rock:
        case CourseFeatureKind::projectile:
            return feature.radius;
        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
        case CourseFeatureKind::duck_press:
        case CourseFeatureKind::ledge:
            return std::max(feature.half_extent.x, feature.half_extent.y);
        }
        return 0.0f;
    }

    struct CrouchPostureEvidence
    {
        bool paired_leg_chains{};
        bool horizontal_body{};
        bool feet_supported{};
        bool non_foot_grounded{};
        float pelvis_drop{};
        float left_knee_flex{};
        float right_knee_flex{};
        float torso_pitch{};
        float support_margin{ -1.0f };
    };

    [[nodiscard]] inline bool crouch_posture_qualified(
        const CrouchPostureEvidence& evidence) noexcept
    {
        if (!evidence.feet_supported || evidence.non_foot_grounded)
            return false;
        if (evidence.paired_leg_chains)
        {
            return evidence.pelvis_drop >= 0.30f
                && evidence.left_knee_flex >= 0.16f
                && evidence.right_knee_flex >= 0.16f
                && evidence.torso_pitch <= 0.55f
                && evidence.support_margin >= -0.08f;
        }
        if (evidence.horizontal_body)
        {
            return evidence.pelvis_drop >= 0.12f
                && evidence.torso_pitch <= 0.80f
                && evidence.support_margin >= -0.22f;
        }
        return evidence.pelvis_drop >= 0.22f
            && evidence.torso_pitch <= 0.65f
            && evidence.support_margin >= -0.10f;
    }

    enum class InvalidMotion : std::uint8_t
    {
        none,
        fallen,
        flipped,
        overspeed,
        out_of_bounds,
        sustained_flight,
        micro_motion,
        wheel_sliding,
        body_rolling,
        foot_pivot_rolling,
        zero_progress,
        collapsed_posture,
        excessive_spins,
        hazard_quiver,
        robotic_torso_swing,
        press_penetration,
        duck_body_contact,
        buried_no_escape,
        duck_hip_hinge,
        structural_compression
    };

    [[nodiscard]] inline std::string_view invalid_motion_name(InvalidMotion reason) noexcept
    {
        switch (reason)
        {
        case InvalidMotion::none: return "VALID";
        case InvalidMotion::fallen: return "FALLEN";
        case InvalidMotion::flipped: return "FLIPPED";
        case InvalidMotion::overspeed: return "OVER 50 KM/H";
        case InvalidMotion::out_of_bounds: return "OUT OF BOUNDS";
        case InvalidMotion::sustained_flight: return "FLYING";
        case InvalidMotion::micro_motion: return "MICRO-MOTION EXPLOIT";
        case InvalidMotion::wheel_sliding: return "WHEEL-SLIDING EXPLOIT";
        case InvalidMotion::body_rolling: return "HEAD / TAIL / BODY ROLLING";
        case InvalidMotion::foot_pivot_rolling: return "FOOT-NODE SKATING / ROLLING";
        case InvalidMotion::zero_progress: return "ZERO MOVEMENT - RESET";
        case InvalidMotion::collapsed_posture: return "COLLAPSED / UNSUPPORTED POSTURE";
        case InvalidMotion::excessive_spins: return "MORE THAN 3 SPINS";
        case InvalidMotion::hazard_quiver: return "HAZARD QUIVER / NO LEG LIFT";
        case InvalidMotion::robotic_torso_swing: return "ROBOTIC TORSO / SHOULDER SWING";
        case InvalidMotion::press_penetration: return "DUCK PRESS PENETRATION";
        case InvalidMotion::duck_body_contact: return "DUCK CONTACT - FEET ONLY";
        case InvalidMotion::buried_no_escape: return "BURIED / NO ESCAPE SPACE";
        case InvalidMotion::duck_hip_hinge: return "HIP HINGE - NOT A CROUCH";
        case InvalidMotion::structural_compression: return "BONE LENGTH ERROR";
        }
        return "INVALID";
    }

    [[nodiscard]] inline InvalidMotion classify_motion_gate(float uprightness, float speed_kmh,
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
    }

    [[nodiscard]] inline bool recovery_should_start(bool collided,
        float uprightness, bool geometric_fall, bool hard_fall) noexcept
    {
        static_cast<void>(collided);
        constexpr float independent_recovery_uprightness = 0.72f;
        return !hard_fall
            && (uprightness < independent_recovery_uprightness || geometric_fall);
    }

    [[nodiscard]] inline bool recovery_terminal_fall(bool geometric_fall,
        bool hard_fall, bool recovery_active) noexcept
    {
        return hard_fall || (geometric_fall && !recovery_active);
    }

    [[nodiscard]] inline bool qualifies_alternating_step(int previous_side, int strike_side,
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
            && std::abs(root_displacement) >= 0.045f
            && swing_air_seconds >= 0.06f
            && swing_clearance >= 0.015f;
    }

    [[nodiscard]] inline bool qualifies_monoped_support_transfer(
        int previous_side, int strike_side, float seconds_since_previous,
        float root_displacement, float swing_air_seconds,
        float swing_clearance) noexcept
    {
        // A monoped's two contact seeds are heel/toe edges of one compact foot.
        // Keep the real airborne-contact and displacement contract, scaled to
        // that authored rocker span instead of a full humanoid stride.
        return qualifies_alternating_step(previous_side, strike_side,
            seconds_since_previous, root_displacement)
            && std::abs(root_displacement) >= 0.035f
            && swing_air_seconds >= 0.05f
            && swing_clearance >= 0.035f;
    }

    [[nodiscard]] inline bool qualifies_topology_support_transfer(
        bool horizontal_multi_support, bool new_landing,
        float swing_air_seconds, float swing_clearance,
        float seconds_since_transfer, float landing_displacement,
        std::size_t previous_phase, std::size_t current_phase) noexcept
    {
        return horizontal_multi_support && new_landing
            && swing_air_seconds >= 0.05f
            && swing_clearance >= 0.015f
            && seconds_since_transfer >= 0.08f
            && std::abs(landing_displacement) >= 0.018f
            && previous_phase != std::numeric_limits<std::size_t>::max()
            && previous_phase != current_phase;
    }

    [[nodiscard]] inline float ground_velocity_retention(bool traction_contact,
        float vertical_speed) noexcept
    {
        static_cast<void>(vertical_speed);
        return traction_contact ? 0.0f : 0.985f;
    }

    [[nodiscard]] inline float foot_friction_retention(float horizontal_speed,
        float firmness, float looseness, bool static_lesson,
        bool toe_contact) noexcept
    {
        firmness = clamp(firmness, 0.0f, 1.0f);
        looseness = clamp(looseness, 0.0f, 1.0f);
        const float static_limit = std::max(0.035f,
            0.08f + firmness * 0.18f - looseness * 0.06f);
        if (std::abs(horizontal_speed) <= static_limit)
            return 0.0f;
        float retention = 0.30f - firmness * 0.22f + looseness * 0.10f;
        if (static_lesson)
            retention *= 0.35f;
        if (toe_contact)
            retention = std::max(retention, 0.060f);
        return clamp(retention, 0.0f, 0.42f);
    }

    inline constexpr float moving_contact_slop_m = 0.032f;
    // A support must unlatch as soon as an articulated gait deliberately
    // raises it. Waiting for root-level jump velocity pins both feet and lets
    // the body translate by stance slip without producing a real step.
    inline constexpr float moving_contact_release_speed_mps = 0.035f;

    [[nodiscard]] inline bool support_contact_release_requested(bool semantic_support,
        bool static_support, bool powered_release, float upward_speed) noexcept
    {
        if (!semantic_support || upward_speed <= 0.0f)
            return false;
        return powered_release || (!static_support
            && upward_speed > moving_contact_release_speed_mps);
    }

    [[nodiscard]] inline bool planted_contact_persists(bool contact_latched,
        bool semantic_support, bool static_support, float separation,
        float upward_speed, bool release_requested) noexcept
    {
        if (!contact_latched || !semantic_support || release_requested)
            return false;
        if (static_support)
            return true;
        return separation > 0.0025f
            && separation <= moving_contact_slop_m
            && upward_speed <= moving_contact_release_speed_mps;
    }

    [[nodiscard]] inline bool completes_side_view_crossing(
        bool began_behind, float swing_center_x, float stance_center_x,
        float swing_clearance) noexcept
    {
        return began_behind
            && swing_clearance >= 0.065f
            && swing_center_x >= stance_center_x + 0.035f;
    }

    [[nodiscard]] inline bool qualifies_crossing_step(int previous_side,
        int strike_side, float seconds_since_previous, float root_displacement,
        float swing_air_seconds, float swing_clearance, bool swing_crossed,
        bool crossing_required) noexcept
    {
        return (!crossing_required || swing_crossed)
            && qualifies_supported_step(previous_side, strike_side,
                seconds_since_previous, root_displacement,
                swing_air_seconds, swing_clearance);
    }

    [[nodiscard]] inline bool advanced_material_pressure_ready(
        float distance, float required_distance, std::uint32_t gait_cycles,
        std::uint32_t limb_crossings, bool paired_leg_chains) noexcept
    {
        return std::abs(distance) >= required_distance
            && gait_cycles >= 2u
            && (!paired_leg_chains || limb_crossings >= 2u);
    }
    inline constexpr float course_marker_spacing_m = 8.0f;
    inline constexpr int course_safe_runway_markers = 5;
    inline constexpr int course_feature_cycle_length = 5;

    [[nodiscard]] inline int first_course_feature_sequence(float root_x, float course_progress,
        float spacing = course_marker_spacing_m, float trailing_distance = 6.0f) noexcept
    {
        return static_cast<int>(std::ceil(
            (root_x + course_progress - trailing_distance) / spacing));
    }

    [[nodiscard]] inline float course_feature_world_x(int sequence, float course_progress,
        float spacing = course_marker_spacing_m) noexcept
    {
        return static_cast<float>(sequence) * spacing - course_progress;
    }

    [[nodiscard]] inline float course_marker_distance_m(int sequence,
        float spacing = course_marker_spacing_m) noexcept
    {
        return static_cast<float>(sequence) * spacing;
    }

    [[nodiscard]] inline CourseFeatureKind scheduled_course_feature(CourseStage stage,
        int marker_sequence) noexcept
    {
        const int relative = std::max(0, marker_sequence - course_safe_runway_markers);
        const int selector = relative % course_feature_cycle_length;
        if (stage == CourseStage::duck_press)
            return CourseFeatureKind::duck_press;
        if (stage == CourseStage::ramps || stage == CourseStage::uneven)
            return CourseFeatureKind::rock;
        if (stage == CourseStage::hurdles)
        {
            if (selector == 0)
                return CourseFeatureKind::rock;
            if (selector == 1)
                return CourseFeatureKind::hurdle;
            return CourseFeatureKind::overhead_bar;
        }
        if (stage == CourseStage::duck_bars)
        {
            if (selector == 0)
                return CourseFeatureKind::rock;
            if (selector == 1)
                return CourseFeatureKind::hurdle;
            return CourseFeatureKind::overhead_bar;
        }
        switch (selector)
        {
        case 0: return CourseFeatureKind::rock;
        case 1: return CourseFeatureKind::hurdle;
        case 2: return CourseFeatureKind::overhead_bar;
        case 3: return CourseFeatureKind::moving_hazard;
        default: return CourseFeatureKind::projectile;
        }
    }

    enum class RigTestPattern : std::uint8_t
    {
        manual,
        crouch,
        gait
    };

    [[nodiscard]] inline float rig_test_motor_input(RigTestPattern pattern,
        std::size_t motor_index, float phase, float manual_input) noexcept
    {
        if (pattern == RigTestPattern::manual)
            return clamp(manual_input, -1.0f, 1.0f);
        if (pattern == RigTestPattern::crouch)
        {
            constexpr std::array<float, anatomy_action_count> crouch{
                -0.22f, 0.70f, 0.22f, -0.70f, 0.0f, 0.0f, 0.0f, 0.0f
            };
            return crouch[std::min(motor_index, crouch.size() - 1u)];
        }
        const float swing = std::sin(phase);
        const std::array<float, anatomy_action_count> gait{
            0.58f * swing,
            0.48f * std::max(0.0f, swing),
            -0.58f * swing,
            -0.48f * std::max(0.0f, -swing),
            -0.16f * swing, 0.08f * swing,
            0.16f * swing, -0.08f * swing
        };
        return gait[std::min(motor_index, gait.size() - 1u)];
    }

    enum class FootContactPhase : std::uint8_t
    {
        airborne,
        heel_strike,
        flat,
        toe_off
    };

    [[nodiscard]] inline std::string_view foot_contact_phase_name(
        FootContactPhase phase) noexcept
    {
        switch (phase)
        {
        case FootContactPhase::airborne: return "AIR";
        case FootContactPhase::heel_strike: return "HEEL";
        case FootContactPhase::flat: return "FLAT";
        case FootContactPhase::toe_off: return "TOE";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] inline FootContactPhase classify_foot_contact_phase(
        bool heel, bool ball, bool toe) noexcept
    {
        if (!heel && !ball && !toe)
            return FootContactPhase::airborne;
        if (heel && !toe)
            return FootContactPhase::heel_strike;
        if (toe && !heel)
            return FootContactPhase::toe_off;
        return FootContactPhase::flat;
    }

    struct Particle
    {
        Vec2 position{};
        Vec2 previous{};
        float inverse_mass{ 1.0f };
        float radius{ 0.12f };
        bool grounded{};
    };

    enum class MaterialKind : std::uint8_t
    {
        sand,
        dirt,
        rock,
        debris
    };

    struct MaterialParticle
    {
        MaterialKind kind{ MaterialKind::sand };
        Vec2 position{};
        Vec2 velocity{};
        float radius{ 0.08f };
        float density{ 0.45f };
        bool active{ true };
    };

    enum class EquipmentState : std::uint8_t
    {
        unarmed,
        safe_carry,
        ready,
        disarmed,
        dropped
    };

    [[nodiscard]] inline std::string_view equipment_state_name(
        EquipmentState state) noexcept
    {
        switch (state)
        {
        case EquipmentState::unarmed: return "UNARMED";
        case EquipmentState::safe_carry: return "SAFE CARRY";
        case EquipmentState::ready: return "READY";
        case EquipmentState::disarmed: return "DISARMED";
        case EquipmentState::dropped: return "DROPPED";
        }
        return "UNKNOWN";
    }

    enum class WeaponClass : std::uint8_t
    {
        none,
        sidearm,
        carbine,
        launcher
    };

    [[nodiscard]] inline std::string_view weapon_class_name(
        WeaponClass weapon) noexcept
    {
        switch (weapon)
        {
        case WeaponClass::none: return "NONE";
        case WeaponClass::sidearm: return "SIDEARM";
        case WeaponClass::carbine: return "CARBINE";
        case WeaponClass::launcher: return "LAUNCHER";
        }
        return "UNKNOWN";
    }

    struct WeaponProfile
    {
        float projectile_speed{};
        float cooldown_seconds{};
        float projectile_radius{};
        float gravity{};
        float recoil{};
        float minimum_engagement_distance{};
        float maximum_engagement_distance{};
        float aim_tolerance{};
    };

    [[nodiscard]] inline WeaponProfile weapon_profile(WeaponClass weapon) noexcept
    {
        switch (weapon)
        {
        case WeaponClass::sidearm:
            return { 12.0f, 0.42f, 0.065f, 0.0f, 0.12f, 2.5f, 10.0f, 0.12f };
        case WeaponClass::carbine:
            return { 17.0f, 0.20f, 0.050f, 0.0f, 0.08f, 5.0f, 18.0f, 0.09f };
        case WeaponClass::launcher:
            return { 8.5f, 0.90f, 0.110f, 4.5f, 0.20f, 7.0f, 24.0f, 0.14f };
        case WeaponClass::none: break;
        }
        return {};
    }

    struct EquipmentProjectile
    {
        WeaponClass weapon{ WeaponClass::none };
        Vec2 position{};
        Vec2 velocity{};
        float radius{ 0.05f };
        std::uint32_t sequence{};
        bool active{ true };
    };

    struct EquipmentTarget
    {
        Vec2 position{};
        float radius{ 0.32f };
        std::uint32_t sequence{};
        bool active{};
    };

    inline constexpr std::size_t equipment_state_action =
        anatomy_action_count;
    inline constexpr std::size_t equipment_aim_action =
        anatomy_action_count + 1u;
    inline constexpr std::size_t equipment_trigger_action =
        anatomy_action_count + 2u;

    struct DistanceConstraint
    {
        std::uint16_t a{};
        std::uint16_t b{};
        float rest_length{};
        // Authored stiffness remains part of the physical rig. v0.7.24
        // applies a separate exact projection only to the primary walking-leg
        // chains, preventing telescoping without freezing compliant braces.
        float stiffness{ 1.0f };
    };

    [[nodiscard]] inline float bone_length_error_ratio(
        float current_length, float rest_length) noexcept
    {
        if (rest_length <= 1.0e-6f)
            return std::numeric_limits<float>::infinity();
        return std::abs(current_length - rest_length) / rest_length;
    }

    struct MotorConstraint
    {
        std::uint16_t a{};
        std::uint16_t pivot{};
        std::uint16_t c{};
        float minimum_angle{ -1.2f };
        float maximum_angle{ 1.2f };
        float neutral_angle{};
        float strength{ 0.055f };
        bool enabled{ true };
    };

    enum class CreatureSpecies : std::uint8_t
    {
        custom,
        human,
        chicken,
        dog,
        hexapod
    };

    [[nodiscard]] inline std::string_view creature_species_slug(
        CreatureSpecies species) noexcept
    {
        switch (species)
        {
        case CreatureSpecies::human: return "human";
        case CreatureSpecies::chicken: return "chicken";
        case CreatureSpecies::dog: return "dog";
        case CreatureSpecies::hexapod: return "hexapod";
        case CreatureSpecies::custom: return "custom";
        }
        return "custom";
    }

    [[nodiscard]] inline std::optional<CreatureSpecies> creature_species_from_slug(
        std::string_view slug) noexcept
    {
        if (slug == "human") return CreatureSpecies::human;
        if (slug == "chicken") return CreatureSpecies::chicken;
        if (slug == "dog") return CreatureSpecies::dog;
        if (slug == "hexapod") return CreatureSpecies::hexapod;
        if (slug == "custom") return CreatureSpecies::custom;
        return std::nullopt;
    }

    [[nodiscard]] inline std::filesystem::path creature_species_rig_filename(
        CreatureSpecies species)
    {
        return std::filesystem::path{ creature_species_slug(species) }
            .replace_extension(".rig");
    }

    struct CreatureSpeciesPaths
    {
        std::filesystem::path rig{};
        std::filesystem::path autosave_checkpoint{};
        std::filesystem::path evolved_rig{};
        std::filesystem::path autonomy_state{};
    };

    [[nodiscard]] inline CreatureSpeciesPaths creature_species_paths(
        CreatureSpecies species)
    {
        const std::string slug{ creature_species_slug(species) };
        const std::string state_prefix = "runner-v0743-" + slug;
        return CreatureSpeciesPaths{
            .rig = creature_species_rig_filename(species),
            .autosave_checkpoint = state_prefix + "-autosave.eppo",
            .evolved_rig = state_prefix + "-evolved.rig",
            .autonomy_state = state_prefix + "-autonomy.state"
        };
    }

    struct CoupledMotorConstraint
    {
        MotorConstraint motor{};
        std::uint8_t source_action{};
        float action_scale{ 1.0f };
    };

    [[nodiscard]] inline float motor_target_angle(const MotorConstraint& motor, float action) noexcept
    {
        action = clamp(action, -1.0f, 1.0f);
        const float negative_span = std::max(0.0f, motor.neutral_angle - motor.minimum_angle);
        const float positive_span = std::max(0.0f, motor.maximum_angle - motor.neutral_angle);
        const float target = action < 0.0f
            ? motor.neutral_angle + action * negative_span
            : motor.neutral_angle + action * positive_span;
        return clamp(target, motor.minimum_angle, motor.maximum_angle);
    }

    [[nodiscard]] inline float toe_command_slew_rate(bool supported,
        CourseStage stage) noexcept
    {
        if (stage == CourseStage::balance)
            return 0.55f;
        if (stage == CourseStage::duck_press)
            return 0.80f;
        if (supported)
            return stage_allows_powered_airtime(stage) ? 1.55f : 1.25f;
        return stage_allows_powered_airtime(stage) ? 2.20f : 1.80f;
    }

    [[nodiscard]] inline float rate_limited_toe_command(float previous,
        float desired, float dt, bool supported, CourseStage stage) noexcept
    {
        desired = clamp(desired, -1.0f, 1.0f);
        if (std::abs(desired) < 0.055f)
            desired = 0.0f;
        const float maximum_delta = toe_command_slew_rate(supported, stage)
            * clamp(dt, 1.0f / 240.0f, 1.0f / 30.0f);
        float next = previous + clamp(desired - previous,
            -maximum_delta, maximum_delta);
        if (std::abs(next) < 0.025f && desired == 0.0f)
            next = 0.0f;
        return clamp(next, -1.0f, 1.0f);
    }

    [[nodiscard]] inline float toe_angular_rate_limit(bool supported,
        CourseStage stage) noexcept
    {
        constexpr float radians_per_degree = pi / 180.0f;
        if (stage == CourseStage::balance)
            return 38.0f * radians_per_degree;
        if (stage == CourseStage::duck_press)
            return 58.0f * radians_per_degree;
        if (supported)
            return (stage_allows_powered_airtime(stage) ? 112.0f : 88.0f)
                * radians_per_degree;
        return (stage_allows_powered_airtime(stage) ? 168.0f : 138.0f)
            * radians_per_degree;
    }

    struct CreatureBlueprint
    {
        // RUNRIG 5+ owns its species identity. Geometry may evolve within the
        // species contract without silently changing persistence, presentation,
        // curriculum, or checkpoint ownership. Legacy rigs leave this empty and
        // are classified from topology once when they are migrated.
        std::optional<CreatureSpecies> species_identity{};
        std::vector<Vec2> nodes{};
        std::vector<float> radii{};
        std::vector<DistanceConstraint> bones{};
        std::array<MotorConstraint, anatomy_action_count> motors{};
        std::size_t active_motor_count{ 4 };

        std::uint16_t root_node{};
        std::uint16_t torso_node{ 1 };
        std::uint16_t head_node{ 2 };
        std::uint16_t left_contact_node{ 4 };
        std::uint16_t right_contact_node{ 6 };
        std::vector<std::uint16_t> additional_left_contact_nodes{};
        std::vector<std::uint16_t> additional_right_contact_nodes{};

        [[nodiscard]] bool is_left_support_seed(std::size_t node) const noexcept
        {
            if (node == left_contact_node)
                return true;
            return std::ranges::find(additional_left_contact_nodes,
                static_cast<std::uint16_t>(node)) != additional_left_contact_nodes.end();
        }
        [[nodiscard]] bool is_right_support_seed(std::size_t node) const noexcept
        {
            if (node == right_contact_node)
                return true;
            return std::ranges::find(additional_right_contact_nodes,
                static_cast<std::uint16_t>(node)) != additional_right_contact_nodes.end();
        }
        [[nodiscard]] bool is_support_seed(std::size_t node) const noexcept
        {
            return is_left_support_seed(node) || is_right_support_seed(node);
        }
        [[nodiscard]] std::size_t support_seed_count() const noexcept
        {
            return 2u + additional_left_contact_nodes.size()
                + additional_right_contact_nodes.size();
        }
        [[nodiscard]] std::uint8_t support_branch_mask(
            const MotorConstraint& motor) const noexcept
        {
            if (!motor.enabled || motor.pivot >= nodes.size()
                || motor.c >= nodes.size() || nodes.size() > 128u)
                return 0u;
            std::array<bool, 128> visited{};
            std::array<std::uint16_t, 128> stack{};
            std::size_t stack_size = 0u;
            visited[motor.pivot] = true;
            visited[motor.c] = true;
            stack[stack_size++] = motor.c;
            std::uint8_t mask = 0u;
            while (stack_size > 0u)
            {
                const std::uint16_t current = stack[--stack_size];
                if (is_left_support_seed(current))
                    mask = static_cast<std::uint8_t>(mask | 0x1u);
                if (is_right_support_seed(current))
                    mask = static_cast<std::uint8_t>(mask | 0x2u);
                for (const DistanceConstraint& bone : bones)
                {
                    if (bone.stiffness < 0.20f)
                        continue;
                    std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                    if (bone.a == current)
                        next = bone.b;
                    else if (bone.b == current)
                        next = bone.a;
                    if (next < nodes.size() && !visited[next])
                    {
                        visited[next] = true;
                        stack[stack_size++] = next;
                    }
                }
            }
            return mask;
        }
        [[nodiscard]] float support_branch_center_x(
            const MotorConstraint& motor) const noexcept
        {
            if (!motor.enabled || motor.pivot >= nodes.size()
                || motor.c >= nodes.size() || nodes.size() > 128u)
                return std::numeric_limits<float>::quiet_NaN();
            std::array<bool, 128> visited{};
            std::array<std::uint16_t, 128> stack{};
            std::size_t stack_size = 0u;
            visited[motor.pivot] = true;
            visited[motor.c] = true;
            stack[stack_size++] = motor.c;
            float support_x = 0.0f;
            std::size_t support_count = 0u;
            while (stack_size > 0u)
            {
                const std::uint16_t current = stack[--stack_size];
                if (is_support_seed(current))
                {
                    support_x += nodes[current].x;
                    ++support_count;
                }
                for (const DistanceConstraint& bone : bones)
                {
                    if (bone.stiffness < 0.20f)
                        continue;
                    std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                    if (bone.a == current)
                        next = bone.b;
                    else if (bone.b == current)
                        next = bone.a;
                    if (next < nodes.size() && !visited[next])
                    {
                        visited[next] = true;
                        stack[stack_size++] = next;
                    }
                }
            }
            return support_count > 0u
                ? support_x / static_cast<float>(support_count)
                : std::numeric_limits<float>::quiet_NaN();
        }
        [[nodiscard]] std::size_t support_branch_longitudinal_band(
            const MotorConstraint& motor) const noexcept
        {
            const float center_x = support_branch_center_x(motor);
            if (!std::isfinite(center_x))
                return 0u;
            std::size_t preceding_supports = 0u;
            for (std::size_t node = 0; node < nodes.size(); ++node)
            {
                if (is_support_seed(node) && nodes[node].x < center_x - 0.02f)
                    ++preceding_supports;
            }
            // Side-view multi-support rigs author near/far supports in pairs.
            // Counting longitudinal pairs yields diagonal four-leg and tripod
            // six-leg phases without relying on action-slot numbering.
            return preceding_supports / 2u;
        }
        [[nodiscard]] std::uint8_t node_support_mask(std::size_t node) const noexcept
        {
            std::uint8_t mask = is_left_support_seed(node) ? 0x1u : 0u;
            if (is_right_support_seed(node))
                mask = static_cast<std::uint8_t>(mask | 0x2u);
            for (std::size_t index = 0; index < active_motor_count; ++index)
            {
                const MotorConstraint& motor = motors[index];
                if (node == motor.pivot || node == motor.c)
                    mask = static_cast<std::uint8_t>(mask | support_branch_mask(motor));
            }
            for (const CoupledMotorConstraint& coupled : coupled_support_motors())
            {
                if (!coupled.motor.enabled)
                    continue;
                if (node == coupled.motor.pivot || node == coupled.motor.c)
                    mask = static_cast<std::uint8_t>(
                        mask | support_branch_mask(coupled.motor));
            }
            return mask;
        }

        [[nodiscard]] bool monopedal_gait() const noexcept
        {
            return support_seed_count() == 2u
                && active_motor_count >= 4u
                && motors[2].enabled && motors[3].enabled
                && motors[2].a == motors[3].a
                && motors[2].pivot == motors[3].pivot;
        }
        [[nodiscard]] bool paired_leg_chains() const noexcept
        {
            return support_seed_count() == 2u
                && !monopedal_gait() && active_motor_count >= 4u
                && motors[0].enabled && motors[1].enabled
                && motors[2].enabled && motors[3].enabled
                && motors[0].pivot == motors[2].pivot
                && motors[1].a == motors[0].pivot
                && motors[3].a == motors[2].pivot;
        }
        [[nodiscard]] bool avian_gait() const noexcept
        {
            if (!paired_leg_chains() || root_node >= nodes.size()
                || torso_node >= nodes.size() || head_node >= nodes.size())
                return false;
            const float head_reach = std::abs(
                nodes[head_node].x - nodes[torso_node].x);
            bool rear_counterweight = false;
            for (std::size_t node = 0; node < nodes.size(); ++node)
            {
                if (!is_support_seed(node)
                    && nodes[node].x < nodes[root_node].x - 0.45f)
                {
                    rear_counterweight = true;
                    break;
                }
            }
            return head_reach >= 0.55f && rear_counterweight;
        }
        [[nodiscard]] bool paired_manipulator_chains() const noexcept
        {
            std::size_t chains{};
            for (std::size_t proximal_index = 0;
                proximal_index < active_motor_count; ++proximal_index)
            {
                const MotorConstraint& proximal = motors[proximal_index];
                if (!proximal.enabled || proximal.pivot >= nodes.size()
                    || proximal.c >= nodes.size()
                    || support_branch_mask(proximal) != 0u)
                    continue;
                for (std::size_t distal_index = 0;
                    distal_index < active_motor_count; ++distal_index)
                {
                    const MotorConstraint& distal = motors[distal_index];
                    if (distal_index == proximal_index || !distal.enabled
                        || distal.pivot != proximal.c
                        || distal.c >= nodes.size()
                        || support_branch_mask(distal) != 0u)
                        continue;
                    ++chains;
                    break;
                }
            }
            return chains >= 2u;
        }
        [[nodiscard]] bool human_casual_gait_plan() const noexcept
        {
            return paired_leg_chains() && !avian_gait()
                && !horizontal_body_plan() && paired_manipulator_chains();
        }
        [[nodiscard]] bool horizontal_body_plan() const noexcept
        {
            if (root_node >= nodes.size() || head_node >= nodes.size())
                return false;
            const Vec2 head_offset = nodes[head_node] - nodes[root_node];
            return std::abs(head_offset.x) >= std::abs(head_offset.y) * 0.72f;
        }
        [[nodiscard]] bool horizontal_multi_support_plan() const noexcept
        {
            return !monopedal_gait() && support_seed_count() >= 4u;
        }
        [[nodiscard]] CreatureSpecies inferred_species() const noexcept
        {
            if (human_casual_gait_plan())
                return CreatureSpecies::human;
            if (avian_gait())
                return CreatureSpecies::chicken;
            if (horizontal_multi_support_plan() && support_seed_count() >= 6u)
                return CreatureSpecies::hexapod;
            if (horizontal_multi_support_plan())
                return CreatureSpecies::dog;
            return CreatureSpecies::custom;
        }
        [[nodiscard]] bool topology_compatible_with_species(
            CreatureSpecies species) const noexcept
        {
            switch (species)
            {
            case CreatureSpecies::human:
                return paired_leg_chains() && !avian_gait()
                    && !horizontal_body_plan() && paired_manipulator_chains();
            case CreatureSpecies::chicken:
                return paired_leg_chains() && avian_gait();
            case CreatureSpecies::dog:
                return horizontal_multi_support_plan()
                    && support_seed_count() >= 4u && support_seed_count() < 6u;
            case CreatureSpecies::hexapod:
                return horizontal_multi_support_plan()
                    && support_seed_count() >= 6u;
            case CreatureSpecies::custom:
                return true;
            }
            return false;
        }
        [[nodiscard]] CreatureSpecies presentation_species() const noexcept
        {
            return species_identity.value_or(inferred_species());
        }
        [[nodiscard]] std::array<CoupledMotorConstraint, 4>
            coupled_support_motors() const noexcept
        {
            std::array<CoupledMotorConstraint, 4> result{};
            for (CoupledMotorConstraint& coupled : result)
                coupled.motor.enabled = false;
            if (presentation_species() != CreatureSpecies::hexapod
                || nodes.size() < 12u)
                return result;

            const auto make_motor = [&](std::uint16_t a, std::uint16_t pivot,
                std::uint16_t c, std::uint8_t source_action,
                float action_scale) noexcept
            {
                CoupledMotorConstraint coupled{};
                coupled.motor = MotorConstraint{ a, pivot, c };
                const float neutral = signed_angle(
                    nodes[a] - nodes[pivot], nodes[c] - nodes[pivot]);
                const float travel = 44.0f * 0.01745329251994329577f;
                coupled.motor.minimum_angle = neutral - travel;
                coupled.motor.maximum_angle = neutral + travel;
                coupled.motor.neutral_angle = neutral;
                coupled.motor.strength = 0.043f;
                coupled.motor.enabled = true;
                coupled.source_action = source_action;
                coupled.action_scale = action_scale;
                return coupled;
            };

            // Fixed-width policy, complete six-leg body: the middle pair derives
            // its two-link actuation from the opposite outer tripod. Existing
            // eight-channel checkpoints remain compatible while all six authored
            // supports participate in locomotion instead of dragging passively.
            result[0] = make_motor(0u, 1u, 8u, 2u, 0.92f);
            result[1] = make_motor(1u, 8u, 9u, 3u, 0.88f);
            result[2] = make_motor(2u, 1u, 10u, 0u, 0.92f);
            result[3] = make_motor(1u, 10u, 11u, 1u, 0.88f);
            return result;
        }

        [[nodiscard]] static CreatureBlueprint scaffold();
        [[nodiscard]] static CreatureBlueprint chicken();
        [[nodiscard]] static CreatureBlueprint biped();
        [[nodiscard]] static CreatureBlueprint humanoid();
        [[nodiscard]] static CreatureBlueprint quadruped();
        [[nodiscard]] static CreatureBlueprint crawler4();
        [[nodiscard]] static CreatureBlueprint hexapod();
        [[nodiscard]] static CreatureBlueprint monoped();
        [[nodiscard]] static CreatureBlueprint for_species(CreatureSpecies species);
        [[nodiscard]] static CreatureBlueprint load_owned_or_default(
            CreatureSpecies species, const std::filesystem::path& owned_path,
            const std::filesystem::path& legacy_path, std::string& source_note);
        [[nodiscard]] bool human_paired_limb_topology() const noexcept;
        [[nodiscard]] bool enforce_human_paired_segment_lengths(
            std::size_t edited_node = std::numeric_limits<std::size_t>::max()) noexcept;
        void rebuild_rest_lengths() noexcept;
        void calibrate_motor(std::size_t motor_index, float negative_degrees = 30.0f,
            float positive_degrees = 30.0f, float power = 0.055f) noexcept;
        void calibrate_all_motors(float degrees = 30.0f, float power = 0.055f) noexcept;
        [[nodiscard]] float rest_joint_angle(std::size_t motor_index) const noexcept;
        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::uint64_t signature() const noexcept;
        [[nodiscard]] bool save(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] static CreatureBlueprint load(const std::filesystem::path& path, std::string& error);
        [[nodiscard]] static std::optional<CreatureBlueprint> load_for_species(
            const std::filesystem::path& path, CreatureSpecies expected,
            std::string& error);
    };

    [[nodiscard]] inline float authored_foundational_gait_cadence_hz(
        const CreatureBlueprint& blueprint) noexcept
    {
        if (blueprint.paired_leg_chains())
            return foundational_gait_cadence_hz;
        if (blueprint.support_seed_count() >= 6u)
            return 1.20f;
        float support_height = std::numeric_limits<float>::infinity();
        for (std::size_t node = 0; node < blueprint.nodes.size(); ++node)
        {
            if (blueprint.is_support_seed(node))
                support_height = std::min(support_height,
                    blueprint.nodes[node].y);
        }
        const float root_clearance = blueprint.root_node < blueprint.nodes.size()
                && std::isfinite(support_height)
            ? blueprint.nodes[blueprint.root_node].y - support_height : 2.0f;
        return root_clearance >= 1.65f ? 1.30f : 1.44f;
    }

    [[nodiscard]] inline bool single_support_rocker_motor(
        const CreatureBlueprint& blueprint, std::size_t motor_index) noexcept
    {
        if (!blueprint.monopedal_gait()
            || motor_index >= blueprint.active_motor_count)
            return false;
        const MotorConstraint& motor = blueprint.motors[motor_index];
        for (std::size_t sibling = 0; sibling < blueprint.active_motor_count; ++sibling)
        {
            if (sibling == motor_index)
                continue;
            const MotorConstraint& candidate = blueprint.motors[sibling];
            if (candidate.a == motor.a && candidate.pivot == motor.pivot)
                return true;
        }
        return false;
    }

    [[nodiscard]] inline bool single_support_compression_motor(
        const CreatureBlueprint& blueprint, std::size_t motor_index) noexcept
    {
        if (!blueprint.monopedal_gait()
            || motor_index >= blueprint.active_motor_count)
            return false;
        const MotorConstraint& motor = blueprint.motors[motor_index];
        for (std::size_t rocker = 0; rocker < blueprint.active_motor_count; ++rocker)
        {
            if (single_support_rocker_motor(blueprint, rocker)
                && motor.c == blueprint.motors[rocker].pivot)
                return true;
        }
        return false;
    }

    [[nodiscard]] inline float single_support_motor_action_limit(
        const CreatureBlueprint& blueprint, std::size_t motor_index) noexcept
    {
        if (!blueprint.monopedal_gait())
            return 1.0f;
        if (single_support_rocker_motor(blueprint, motor_index))
            return 0.27f;
        if (single_support_compression_motor(blueprint, motor_index))
            return 0.59f;
        return 0.34f;
    }

    [[nodiscard]] inline float single_support_motor_slew_rate(
        const CreatureBlueprint& blueprint, std::size_t motor_index) noexcept
    {
        if (!blueprint.monopedal_gait())
            return std::numeric_limits<float>::infinity();
        if (single_support_rocker_motor(blueprint, motor_index))
            return 30.00f;
        if (single_support_compression_motor(blueprint, motor_index))
            return 30.00f;
        return 30.00f;
    }

    [[nodiscard]] inline float conditioned_single_support_motor_action(
        const CreatureBlueprint& blueprint, std::size_t motor_index,
        float previous_action, float desired_action, float dt) noexcept
    {
        const float limit = single_support_motor_action_limit(blueprint, motor_index);
        const float limited = clamp(desired_action, -limit, limit);
        if (desired_action == limited)
            return desired_action;
        const float slew_rate = single_support_motor_slew_rate(blueprint, motor_index);
        if (!std::isfinite(slew_rate))
            return limited;
        const float bounded_dt = clamp(dt, 1.0f / 240.0f, 1.0f / 30.0f);
        const float maximum_delta = slew_rate * bounded_dt;
        return previous_action
            + clamp(limited - previous_action, -maximum_delta, maximum_delta);
    }

    struct StepResult
    {
        float reward{};
        float forward_speed{};
        float forward_odometer_progress{};
        bool terminated{};
        bool valid_motion{ true };
        InvalidMotion invalid_reason{ InvalidMotion::none };
    };

    struct MotorDiagnostic
    {
        bool available{};
        bool enabled{};
        bool support_branch{};
        std::uint16_t parent{};
        std::uint16_t pivot{};
        std::uint16_t driven{};
        std::size_t action_slot{};
        float authored_neutral{};
        float minimum{};
        float maximum{};
        float current_angle{};
        float angular_velocity{};
        float applied_action{};
        float target_angle{};
    };

    struct EnvironmentTestAccess;

    class Environment
    {
    public:
        Environment();
        explicit Environment(const CreatureBlueprint& blueprint, std::uint64_t seed = 1);

        void set_blueprint(const CreatureBlueprint& blueprint);
        void set_course(CourseStage stage, float difficulty = 0.25f);
        void configure_equipment(WeaponClass weapon, float target_distance = 8.0f);
        void clear_equipment() noexcept;
        void disarm_equipment() noexcept;
        void set_diagnostic_rigid_rotation(float radians) noexcept;
        void reset(std::uint64_t seed = 0);
        [[nodiscard]] StepResult step(std::span<const float, action_count> actions, float dt = 1.0f / 60.0f);
        [[nodiscard]] std::array<float, observation_count> observation() const noexcept;
        [[nodiscard]] MotorDiagnostic motor_diagnostic(
            std::size_t motor_index) const noexcept;

        [[nodiscard]] const std::vector<Particle>& particles() const noexcept { return particles_; }
        [[nodiscard]] const CreatureBlueprint& blueprint() const noexcept { return blueprint_; }
        [[nodiscard]] std::span<const CourseFeature> course_features() const noexcept { return course_features_; }
        [[nodiscard]] std::span<const MaterialParticle> material_particles() const noexcept
        {
            return material_particles_;
        }
        [[nodiscard]] std::span<const EquipmentProjectile> equipment_projectiles() const noexcept
        {
            return equipment_projectiles_;
        }
        [[nodiscard]] const EquipmentTarget& equipment_target() const noexcept
        {
            return equipment_target_;
        }
        [[nodiscard]] EquipmentState equipment_state() const noexcept
        {
            return equipment_state_;
        }
        [[nodiscard]] WeaponClass weapon_class() const noexcept { return weapon_class_; }
        [[nodiscard]] float equipment_aim_angle() const noexcept { return equipment_aim_angle_; }
        [[nodiscard]] float equipment_cooldown() const noexcept
        {
            return equipment_cooldown_seconds_;
        }
        [[nodiscard]] std::uint32_t shots_fired() const noexcept { return shots_fired_; }
        [[nodiscard]] std::uint32_t target_hits() const noexcept { return target_hits_; }
        [[nodiscard]] std::uint32_t equipment_hit_goal() const noexcept;
        [[nodiscard]] bool equipment_engagement_ready() const noexcept;
        [[nodiscard]] std::uint32_t equipment_transitions() const noexcept
        {
            return equipment_transition_count_;
        }
        [[nodiscard]] Vec2 equipment_mount_position() const noexcept;
        [[nodiscard]] Vec2 equipment_display_position() const noexcept;
        [[nodiscard]] std::uint32_t hand_ledge_contacts() const noexcept
        {
            return hand_ledge_contacts_;
        }
        [[nodiscard]] std::uint32_t climb_support_transfers() const noexcept
        {
            return climb_support_transfers_;
        }
        [[nodiscard]] std::uint32_t ledge_climbs() const noexcept { return ledge_climbs_; }
        [[nodiscard]] std::uint32_t controlled_descents() const noexcept
        {
            return controlled_descents_;
        }
        [[nodiscard]] CourseStage course_stage() const noexcept { return course_stage_; }
        [[nodiscard]] float course_difficulty() const noexcept { return course_difficulty_; }
        void set_course_motion_enabled(bool enabled) noexcept
        {
            course_motion_enabled_ = enabled;
        }
        [[nodiscard]] bool course_motion_enabled() const noexcept
        {
            return course_motion_enabled_;
        }
        [[nodiscard]] bool shuttle_enabled() const noexcept
        {
            return course_stage_ == CourseStage::shuttle;
        }
        [[nodiscard]] ShuttlePhase shuttle_phase() const noexcept
        {
            return shuttle_state_.phase;
        }
        [[nodiscard]] float facing_direction() const noexcept
        {
            return shuttle_enabled() ? shuttle_state_.facing_direction : 1.0f;
        }
        [[nodiscard]] float locomotion_direction() const noexcept
        {
            return shuttle_enabled() ? shuttle_state_.locomotion_direction : 1.0f;
        }
        [[nodiscard]] std::uint32_t completed_shuttle_turns() const noexcept
        {
            return shuttle_state_.completed_turns;
        }
        [[nodiscard]] float shuttle_phase_seconds() const noexcept
        {
            return shuttle_state_.phase_seconds;
        }
        [[nodiscard]] float elapsed_seconds() const noexcept { return elapsed_seconds_; }
        [[nodiscard]] float distance_travelled() const noexcept { return distance_travelled_; }
        [[nodiscard]] float forward_speed() const noexcept { return forward_speed_; }
        [[nodiscard]] bool fallen() const noexcept { return fallen_; }
        [[nodiscard]] float ground_height() const noexcept { return 0.0f; }
        [[nodiscard]] float ground_height_at(float x) const noexcept;
        [[nodiscard]] float terrain_firmness_at(float x) const noexcept;
        [[nodiscard]] float terrain_looseness_at(float x) const noexcept;
        [[nodiscard]] TerrainRegion terrain_region_at(float x) const noexcept
        {
            return stage_uses_deformable_terrain(course_stage_)
                ? terrain_.region_at(terrain_sample_x(x, course_progress()))
                : TerrainRegion::firm;
        }
        [[nodiscard]] sandhybrid::Material terrain_surface_material_at(
            float x) const noexcept
        {
            return stage_uses_deformable_terrain(course_stage_)
                ? terrain_.surface_material_at(terrain_sample_x(x, course_progress()))
                : sandhybrid::Material::dirt;
        }
        [[nodiscard]] float water_depth_at(float x) const noexcept
        {
            return stage_uses_deformable_terrain(course_stage_)
                ? terrain_.water_depth_at(terrain_sample_x(x, course_progress()))
                : 0.0f;
        }
        [[nodiscard]] float water_surface_at(float x) const noexcept
        {
            return stage_uses_deformable_terrain(course_stage_)
                ? terrain_.water_surface_at(terrain_sample_x(x, course_progress()))
                : ground_height_at(x);
        }
        [[nodiscard]] const DeformableTerrain& terrain() const noexcept { return terrain_; }
        [[nodiscard]] float burial_depth() const noexcept { return burial_depth_; }
        [[nodiscard]] float water_depth() const noexcept { return water_depth_; }
        [[nodiscard]] float water_submersion() const noexcept { return water_submersion_; }
        [[nodiscard]] float free_space_direction() const noexcept { return free_space_direction_; }
        [[nodiscard]] Vec2 incoming_material_velocity() const noexcept { return incoming_material_velocity_; }
        [[nodiscard]] float incoming_time_to_impact() const noexcept { return incoming_time_to_impact_; }
        [[nodiscard]] float incoming_material_density() const noexcept { return incoming_material_density_; }
        [[nodiscard]] std::uint32_t material_event_count() const noexcept { return material_event_sequence_; }
        [[nodiscard]] bool granular_hazard_active() const noexcept
        {
            return granular_hazard_hold_seconds_ > 0.0f
                || std::ranges::any_of(material_particles_,
                    [](const MaterialParticle& particle) { return particle.active; })
                || (granular_block_present_ && !granular_block_safe_);
        }
        [[nodiscard]] bool granular_hazard_safe() const noexcept
        {
            return material_event_sequence_ > 0u && !granular_hazard_active()
                && granular_hazard_safe_seconds_ >= 0.75f;
        }
        [[nodiscard]] bool granular_block_present() const noexcept
        {
            return granular_block_present_;
        }
        [[nodiscard]] std::uint8_t obstruction_mask() const noexcept { return obstruction_mask_; }
        [[nodiscard]] float course_speed() const noexcept
        {
            if (!course_motion_enabled_)
                return 0.0f;
            if (course_stage_ == CourseStage::balance
                || course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::shuttle
                || course_stage_ == CourseStage::ramps
                || course_stage_ == CourseStage::duck_bars
                || course_stage_ == CourseStage::climb_descent
                || course_stage_ == CourseStage::equipment_targets)
                return 0.0f;
            if (course_stage_ == CourseStage::crouch_walk)
                return 0.58f + course_difficulty_ * 0.18f;
            if (course_stage_ == CourseStage::uneven)
                return 0.82f + course_difficulty_ * 0.88f;
            if (course_stage_ == CourseStage::hurdles)
                return 1.05f + course_difficulty_ * 0.95f;
            return 1.20f + course_difficulty_ * 1.05f;
        }
        [[nodiscard]] float course_progress() const noexcept
        {
            return elapsed_seconds_ * course_speed();
        }
        [[nodiscard]] bool recovering() const noexcept { return recovery_active_; }
        [[nodiscard]] std::uint32_t recovery_events() const noexcept { return recovery_events_; }
        [[nodiscard]] std::uint32_t recovery_successes() const noexcept { return recovery_successes_; }
        [[nodiscard]] float collision_count() const noexcept { return collision_count_; }
        [[nodiscard]] float airborne_ratio() const noexcept;
        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }
        [[nodiscard]] std::uint32_t limb_crossings() const noexcept { return limb_crossings_; }
        [[nodiscard]] std::uint32_t heel_strikes() const noexcept { return heel_strike_count_; }
        [[nodiscard]] std::uint32_t toe_offs() const noexcept { return toe_off_count_; }
        [[nodiscard]] FootContactPhase left_foot_phase() const noexcept
        {
            return left_foot_phase_;
        }
        [[nodiscard]] FootContactPhase right_foot_phase() const noexcept
        {
            return right_foot_phase_;
        }
        [[nodiscard]] std::uint32_t gait_cycles() const noexcept
        {
            return blueprint_.monopedal_gait()
                ? std::max(alternating_steps_, single_leg_cycles_)
                : alternating_steps_;
        }
        [[nodiscard]] float duck_seconds() const noexcept { return duck_seconds_; }
        [[nodiscard]] float crouch_walk_seconds() const noexcept { return crouch_walk_seconds_; }
        [[nodiscard]] float crouch_walk_distance() const noexcept { return crouch_walk_distance_; }
        [[nodiscard]] bool duck_active() const noexcept { return duck_active_; }
        [[nodiscard]] float duck_obstacle_weight() const noexcept
        {
            return duck_obstacle_weight_;
        }
        [[nodiscard]] float duck_clearance_margin() const noexcept
        {
            return duck_clearance_margin_;
        }
        [[nodiscard]] CrouchPostureEvidence current_crouch_posture() const noexcept;
        [[nodiscard]] bool crouch_posture_valid() const noexcept
        {
            return crouch_posture_qualified(current_crouch_posture());
        }
        [[nodiscard]] float longest_valid_crouch_seconds() const noexcept
        {
            return longest_valid_crouch_seconds_;
        }
        [[nodiscard]] bool duck_press_contact() const noexcept { return duck_press_contact_this_step_; }
        [[nodiscard]] bool duck_press_completed() const noexcept { return duck_press_completed_; }
        [[nodiscard]] float duck_press_penetration() const noexcept { return duck_press_max_penetration_; }
        [[nodiscard]] float torso_swing_seconds() const noexcept { return torso_swing_seconds_; }
        [[nodiscard]] std::uint32_t powered_jumps() const noexcept { return powered_jump_count_; }
        [[nodiscard]] std::uint32_t landed_jumps() const noexcept { return landed_jump_count_; }
        [[nodiscard]] float maximum_flip_turns() const noexcept { return maximum_spin_turns_; }
        [[nodiscard]] float maximum_spin_turns() const noexcept { return uncontrolled_spin_turns_; }
        [[nodiscard]] float uncontrolled_spin_turns() const noexcept { return uncontrolled_spin_turns_; }
        [[nodiscard]] std::uint32_t spin_landings() const noexcept { return spin_landing_count_; }
        [[nodiscard]] std::uint32_t obstacles_passed() const noexcept { return obstacles_passed_; }
        [[nodiscard]] std::uint32_t knee_first_faults() const noexcept { return knee_first_faults_; }
        [[nodiscard]] float stance_slip_speed() const noexcept { return stance_slip_speed_; }
        [[nodiscard]] bool non_foot_grounded() const noexcept { return non_foot_grounded_; }
        [[nodiscard]] float body_rolling_seconds() const noexcept { return body_rolling_seconds_; }
        [[nodiscard]] float foot_pivot_rolling_seconds() const noexcept { return foot_pivot_rolling_seconds_; }
        [[nodiscard]] float zero_progress_seconds() const noexcept { return zero_progress_seconds_; }
        [[nodiscard]] float hazard_stall_seconds() const noexcept { return hazard_stall_seconds_; }
        [[nodiscard]] float obstacle_lift_clearance() const noexcept { return obstacle_lift_clearance_; }
        [[nodiscard]] float stable_stance_seconds() const noexcept { return stable_stance_seconds_; }
        [[nodiscard]] float longest_stable_stance_seconds() const noexcept
        {
            return longest_stable_stance_seconds_;
        }
        [[nodiscard]] std::uint32_t duck_recoveries() const noexcept
        {
            return duck_recovery_count_;
        }
        [[nodiscard]] float maximum_joint_speed() const noexcept { return maximum_joint_speed_; }
        [[nodiscard]] float maximum_speed_kmh() const noexcept { return maximum_speed_kmh_; }
        [[nodiscard]] float maximum_upper_body_motor_deviation() const noexcept;
        [[nodiscard]] float primary_support_span_ratio() const noexcept;
        [[nodiscard]] float maximum_lower_leg_scissor_seconds() const noexcept
        {
            return maximum_lower_leg_scissor_seconds_;
        }
        [[nodiscard]] float backward_brace_ratio() const noexcept
        {
            return backward_brace_ratio_;
        }
        [[nodiscard]] float backward_brace_seconds() const noexcept
        {
            return backward_brace_seconds_;
        }
        [[nodiscard]] float maximum_backward_brace_seconds() const noexcept
        {
            return maximum_backward_brace_seconds_;
        }
        [[nodiscard]] float posture_failure_seconds() const noexcept
        {
            return posture_failure_seconds_;
        }
        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }
        [[nodiscard]] InvalidMotion invalid_reason() const noexcept { return invalid_reason_; }
        [[nodiscard]] float uprightness() const noexcept { return torso_uprightness(); }
        [[nodiscard]] bool body_integrity_valid() const noexcept;
        [[nodiscard]] float maximum_bone_length_error_ratio() const noexcept;
        [[nodiscard]] bool current_display_posture_valid() const noexcept;
        [[nodiscard]] bool left_supported() const noexcept
        {
            return contact_supported(blueprint_.left_contact_node);
        }
        [[nodiscard]] bool right_supported() const noexcept
        {
            return contact_supported(blueprint_.right_contact_node);
        }

    private:
        friend struct EnvironmentTestAccess;

        void solve_distance(const DistanceConstraint& constraint) noexcept;
        void project_structure_rigid(float dt) noexcept;
        void separate_support_clusters() noexcept;
        void stabilize_passive_appendages() noexcept;
        void stabilize_balance_posture() noexcept;
        void stabilize_duck_posture() noexcept;
        [[nodiscard]] bool articulated_toe_motor(bool left,
            MotorConstraint& motor) const noexcept;
        void update_articulated_toe_commands(
            std::span<const float, anatomy_action_count> actions, float dt) noexcept;
        void solve_articulated_toes() noexcept;
        void limit_articulated_toe_rates(float dt) noexcept;
        void solve_motor(const MotorConstraint& motor, float action) noexcept;
        void solve_ground(float dt) noexcept;
        void solve_course(float dt = 1.0f / 60.0f) noexcept;
        void apply_water_forces(float dt) noexcept;
        void apply_support_pressure(float dt) noexcept;
        void update_materials(float dt) noexcept;
        void clear_dynamic_materials() noexcept;
        void append_dynamic_material_features();
        void update_material_metrics(float dt) noexcept;
        void rebuild_course_features() noexcept;
        void mirror_rig_about_root() noexcept;
        void update_shuttle(float root_x, float root_speed, float dt) noexcept;
        void reset_equipment() noexcept;
        void update_equipment(std::span<const float, action_count> actions,
            float dt) noexcept;
        void update_climb_metrics(float dt) noexcept;
        [[nodiscard]] std::uint16_t equipment_mount_node() const noexcept;
        void update_gait_metrics(float dt, float action_energy) noexcept;
        void invalidate(InvalidMotion reason) noexcept;
        [[nodiscard]] float joint_angle(const MotorConstraint& motor) const noexcept;
        [[nodiscard]] float torso_uprightness() const noexcept;
        [[nodiscard]] float authored_standing_head_clearance() const noexcept;
        [[nodiscard]] float random_unit() noexcept;
        [[nodiscard]] bool valid_node(std::uint16_t index) const noexcept;
        [[nodiscard]] bool contact_cluster_contains(std::uint16_t contact_node,
            std::size_t particle_index) const noexcept;
        [[nodiscard]] std::size_t support_seed_grounded_count(bool left) const noexcept;
        [[nodiscard]] std::size_t support_seed_lifted_count(
            float minimum_clearance) const noexcept;
        [[nodiscard]] bool contact_supported(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] bool non_foot_ground_contact() const noexcept;
        [[nodiscard]] bool head_ground_contact() const noexcept;
        [[nodiscard]] float torso_roll_angle() const noexcept;
        [[nodiscard]] float contact_cluster_front_x(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] float contact_cluster_top_y(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] float contact_cluster_horizontal_speed(std::uint16_t contact_node,
            float dt) const noexcept;
        [[nodiscard]] float contact_cluster_center_x(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] FootContactPhase detect_foot_contact_phase(bool left) const noexcept;
        [[nodiscard]] float contact_cluster_clearance(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] bool knee_before_foot_fault() const noexcept;

        CreatureBlueprint blueprint_{};
        std::vector<Particle> particles_{};
        std::vector<std::uint8_t> support_contact_latch_{};
        std::vector<float> support_contact_anchor_x_{};
        std::vector<CourseFeature> course_features_{};
        DeformableTerrain terrain_{};
        std::vector<MaterialParticle> material_particles_{};
        std::vector<EquipmentProjectile> equipment_projectiles_{};
        EquipmentTarget equipment_target_{};
        EquipmentState equipment_state_{ EquipmentState::unarmed };
        WeaponClass weapon_class_{ WeaponClass::none };
        WeaponClass configured_weapon_class_{ WeaponClass::none };
        bool equipment_override_{};
        float configured_target_distance_{ 8.0f };
        float equipment_aim_angle_{};
        float equipment_cooldown_seconds_{};
        Vec2 dropped_equipment_position_{};
        Vec2 dropped_equipment_velocity_{};
        std::uint32_t equipment_projectile_sequence_{};
        std::uint32_t shots_fired_{};
        std::uint32_t target_hits_{};
        std::uint32_t equipment_transition_count_{};
        bool target_hit_this_step_{};
        std::array<std::uint16_t, 2> ledge_grasp_nodes_{
            std::numeric_limits<std::uint16_t>::max(),
            std::numeric_limits<std::uint16_t>::max() };
        std::array<Vec2, 2> ledge_grasp_anchors_{};
        std::uint32_t hand_ledge_contacts_{};
        std::uint32_t climb_support_transfers_{};
        std::uint32_t ledge_climbs_{};
        std::uint32_t controlled_descents_{};
        bool ledge_climbed_{};
        bool ledge_descending_{};
        float ledge_top_height_{};
        float ledge_left_edge_{};
        float previous_root_height_{};
        std::uint64_t random_state_{ 1 };
        std::uint64_t course_layout_seed_{ 1 };
        bool course_layout_initialized_{};
        std::array<float, anatomy_action_count> previous_angles_{};
        std::array<float, anatomy_action_count> angular_velocities_{};
        std::array<float, anatomy_action_count> previous_applied_actions_{};
        std::array<float, 2> articulated_toe_commands_{};
        std::array<float, 2> previous_articulated_toe_angles_{};
        Vec2 previous_pelvis_{};
        float elapsed_seconds_{};
        float last_step_dt_{ 1.0f / 60.0f };
        float distance_travelled_{};
        float forward_speed_{};
        float last_reward_{};
        bool fallen_{};

        CourseStage course_stage_{ CourseStage::balance };
        float course_difficulty_{ 0.25f };
        bool course_motion_enabled_{ true };
        ShuttleState shuttle_state_{};
        float shuttle_distance_travelled_{};
        float collision_count_{};
        float airborne_seconds_{};
        float cumulative_airborne_{};
        float duck_seconds_{};
        float duck_depth_{};
        float duck_obstacle_weight_{};
        float duck_clearance_margin_{};
        float duck_press_hold_seconds_{};
        float duck_body_contact_seconds_{};
        float duck_posture_failure_seconds_{};
        float current_valid_crouch_seconds_{};
        float longest_valid_crouch_seconds_{};
        float duck_press_max_penetration_{};
        float duck_walk_started_seconds_{};
        float crouch_walk_seconds_{};
        float crouch_walk_distance_{};
        float torso_swing_seconds_{};
        float current_duck_hold_seconds_{};
        float stable_stance_seconds_{};
        float longest_stable_stance_seconds_{};
        float stance_failure_grace_seconds_{};
        float posture_failure_seconds_{};
        float maximum_joint_speed_{};
        std::uint32_t duck_recovery_count_{};
        bool duck_cycle_qualified_{};
        bool duck_press_contact_this_step_{};
        bool duck_press_contact_seen_{};
        bool duck_press_hold_qualified_{};
        bool duck_press_completed_{};
        float current_airborne_rotation_{};
        float maximum_spin_turns_{};
        float uncontrolled_spin_turns_{};
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
        float progress_window_seconds_{};
        float progress_window_start_x_{};
        float micro_motion_seconds_{};
        float action_energy_window_{};
        float root_path_window_{};
        Vec2 previous_root_for_path_{};
        float last_step_time_{ -100.0f };
        float last_step_x_{};
        float left_swing_seconds_{};
        float right_swing_seconds_{};
        float left_swing_clearance_{};
        float right_swing_clearance_{};
        bool left_swing_started_behind_{};
        bool right_swing_started_behind_{};
        bool left_swing_crossed_{};
        bool right_swing_crossed_{};
        std::uint32_t limb_crossings_{};
        bool lower_leg_scissored_this_step_{};
        float lower_leg_scissor_seconds_{};
        float maximum_lower_leg_scissor_seconds_{};
        float backward_brace_ratio_{};
        float backward_brace_seconds_{};
        float maximum_backward_brace_seconds_{};
        std::uint32_t heel_strike_count_{};
        std::uint32_t toe_off_count_{};
        FootContactPhase left_foot_phase_{ FootContactPhase::airborne };
        FootContactPhase right_foot_phase_{ FootContactPhase::airborne };
        float action_change_energy_{};
        bool alternating_step_this_step_{};
        bool single_leg_cycle_this_step_{};
        bool limb_crossing_this_step_{};
        float maximum_speed_kmh_{};
        std::uint32_t alternating_steps_{};
        std::uint32_t single_leg_cycles_{};
        float last_single_leg_landing_x_{};
        std::uint32_t progress_window_start_steps_{};
        std::uint32_t knee_first_faults_{};
        float wheel_sliding_seconds_{};
        float body_rolling_seconds_{};
        float foot_pivot_rolling_seconds_{};
        float zero_progress_seconds_{};
        float head_contact_seconds_{};
        float previous_torso_angle_{};
        float last_support_transfer_seconds_{ -100.0f };
        float torso_turn_speed_{};
        float stance_slip_speed_{};
        float hazard_stall_seconds_{};
        float obstacle_approach_weight_{};
        float obstacle_lift_clearance_{};
        float obstacle_clearance_target_{ 0.20f };
        bool non_foot_grounded_{};
        bool knee_first_this_step_{};
        int last_contact_side_{};
        bool previous_left_grounded_{};
        bool previous_right_grounded_{};
        std::vector<std::uint8_t> previous_support_grounded_{};
        std::vector<float> support_swing_seconds_{};
        std::vector<float> support_swing_clearance_{};
        std::size_t last_topology_support_phase_{ std::numeric_limits<std::size_t>::max() };
        float last_topology_transfer_seconds_{ -100.0f };
        float last_topology_transfer_x_{};
        bool collided_this_step_{};
        bool recovery_active_{};
        float recovery_started_seconds_{};
        float recovery_best_upright_{ 1.0f };
        std::uint32_t recovery_events_{};
        std::uint32_t recovery_successes_{};
        float next_material_event_seconds_{ 9.0f };
        std::uint32_t material_event_sequence_{};
        CourseFeature granular_block_{};
        bool granular_block_present_{};
        bool granular_block_safe_{};
        float granular_block_settled_seconds_{};
        float granular_hazard_hold_seconds_{};
        float granular_hazard_safe_seconds_{};
        float terrain_firmness_{ 1.0f };
        float terrain_looseness_{};
        float water_depth_{};
        float water_submersion_{};
        float burial_depth_{};
        float previous_burial_depth_{};
        float buried_no_escape_seconds_{};
        float free_space_direction_{};
        Vec2 incoming_material_velocity_{};
        float incoming_time_to_impact_{ 10.0f };
        float incoming_material_density_{};
        std::uint8_t obstruction_mask_{};
        InvalidMotion invalid_reason_{ InvalidMotion::none };
    };
}
