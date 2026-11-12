#pragma once

#include "simulation.hpp"
#include "locomotion_strategy.hpp"
#include "raw_learning_provenance.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace runner::rl
{
    struct RawLearningTestAccess;
    inline constexpr std::uint32_t training_semantics_version = 0x0007'5115u;

    struct RawPlayerIntentCurriculum
    {
        float direction{};
        bool stop{};
    };

    [[nodiscard]] inline std::array<float, sim::observation_count>
    encode_foundational_human_support_observation(
        std::array<float, sim::observation_count> observation,
        const sim::HumanSupportFeedback& feedback, float facing) noexcept
    {
        observation = raw_policy_observation(observation);
        const bool finite = std::isfinite(facing)
            && std::isfinite(feedback.center_of_mass.x)
            && std::isfinite(feedback.center_of_mass.y)
            && std::isfinite(feedback.center_of_mass_velocity.x)
            && std::isfinite(feedback.center_of_mass_velocity.y)
            && std::isfinite(feedback.aggregate_cop_x)
            && std::isfinite(feedback.capture_point_x)
            && std::isfinite(feedback.capture_error_x)
            && std::isfinite(feedback.total_normal_impulse)
            && std::isfinite(feedback.load_asymmetry);
        if (!feedback.valid || !finite
            || feedback.total_normal_impulse <= 1.0e-7f
            || std::abs(facing) < 0.5f)
            return observation;
        const float facing_sign = facing < 0.0f ? -1.0f : 1.0f;
        observation[elapsed_phase_sine_observation_index] = clamp(
            feedback.capture_error_x * facing_sign, -2.0f, 2.0f);
        observation[elapsed_phase_cosine_observation_index] = clamp(
            feedback.load_asymmetry, -1.0f, 1.0f);
        return observation;
    }

    [[nodiscard]] inline std::array<float, sim::observation_count>
    foundational_policy_observation(const sim::Environment& environment,
        sim::CourseStage stage, sim::GaitTask gait_task,
        sim::GuidanceMode guidance_mode) noexcept
    {
        const auto observation = environment.observation();
        if (stage == sim::CourseStage::uneven
            && gait_task == sim::GaitTask::walk
            && environment.blueprint().human_casual_gait_plan())
        {
            return encode_foundational_human_support_observation(observation,
                environment.human_support_feedback(),
                environment.facing_direction());
        }
        if (guidance_mode == sim::GuidanceMode::raw_policy_audit)
            return raw_policy_observation(observation);
        return observation;
    }

    [[nodiscard]] inline constexpr RawPlayerIntentCurriculum
    raw_player_intent_curriculum(std::size_t environment_index,
        std::size_t rollout_step) noexcept
    {
        const float direction = (environment_index & 1u) == 0u
            ? 1.0f : -1.0f;
        const bool stop = (environment_index & 2u) != 0u
            && (rollout_step & 127u) >= 64u;
        return { stop ? 0.0f : direction, stop };
    }

    using ActionMask = std::array<bool, sim::action_count>;

    [[nodiscard]] inline constexpr ActionMask default_action_mask() noexcept
    {
        ActionMask result{};
        for (std::size_t index = 0; index < sim::anatomy_action_count; ++index)
            result[index] = true;
        return result;
    }

    [[nodiscard]] inline ActionMask action_mask_for(
        const sim::Environment& environment, sim::CourseStage stage) noexcept
    {
        ActionMask result{};
        for (std::size_t index = 0; index < sim::anatomy_action_count; ++index)
            result[index] = environment.anatomy_action_active(index);
        const bool equipment_active = environment.equipment_capable()
            && (environment.equipment_directive()
                    != sim::EquipmentDirective::passive
                || stage == sim::CourseStage::equipment_targets
                || stage == sim::CourseStage::combat_course);
        for (std::size_t index = sim::anatomy_action_count;
            index < sim::action_count; ++index)
            result[index] = equipment_active;
        return result;
    }

    [[nodiscard]] inline constexpr std::uint32_t action_mask_bits(
        const ActionMask& mask) noexcept
    {
        std::uint32_t result{};
        for (std::size_t index = 0; index < mask.size(); ++index)
            if (mask[index])
                result |= std::uint32_t{ 1u } << index;
        return result;
    }

    [[nodiscard]] inline constexpr std::uint64_t evaluation_seed(
        std::size_t agent, std::uint64_t sequence) noexcept
    {
        return 0xE000u + static_cast<std::uint64_t>(agent) * 4099u
            + sequence * 104729u;
    }

    inline void configure_policy_evaluation_environment(
        sim::Environment& environment, sim::CourseStage stage,
        float difficulty, sim::GaitTask gait_task,
        sim::GuidanceMode guidance_mode,
        float physical_guidance_authority = 1.0f) noexcept
    {
        environment.set_course(stage, difficulty);
        environment.set_gait_task(gait_task);
        environment.set_course_motion_enabled(false);
        environment.set_guidance_mode(guidance_mode);
        environment.set_physical_guidance_authority(
            physical_guidance_authority);
    }

    [[nodiscard]] inline bool motor_drives_support_branch(
        const sim::CreatureBlueprint& rig,
        const sim::MotorConstraint& motor) noexcept;

    [[nodiscard]] inline std::array<float, sim::action_count> balance_teacher_action(
        const sim::Environment& environment) noexcept
    {
        static_assert(sim::observation_count == 84u);
        const auto observation = environment.observation();
        std::array<float, sim::action_count> action{};
        const sim::CreatureBlueprint& rig = environment.blueprint();

        for (std::size_t index = 0; index < rig.active_motor_count; ++index)
        {
            const float joint_offset = observation[
                sim::joint_angle_observation_index(index)];
            const float joint_speed = observation[
                sim::joint_velocity_observation_index(index)];
            action[index] = clamp(-0.16f * joint_offset - 0.055f * joint_speed,
                -0.28f, 0.28f);
        }

        if (rig.paired_leg_chains())
        {
            if (!environment.left_supported())
            {
                action[0] = clamp(action[0] - 0.010f, -0.34f, 0.34f);
                action[1] = clamp(action[1] + 0.016f, -0.36f, 0.36f);
            }
            if (!environment.right_supported())
            {
                action[2] = clamp(action[2] + 0.010f, -0.34f, 0.34f);
                action[3] = clamp(action[3] - 0.016f, -0.36f, 0.36f);
            }
            const bool foundational_human = environment.raw_policy_audit()
                && rig.human_casual_gait_plan()
                && (environment.course_stage() == sim::CourseStage::uneven
                    || environment.course_stage() == sim::CourseStage::shuttle);
            const Vec2 authored_body = rig.root_node < rig.nodes.size()
                    && rig.torso_node < rig.nodes.size()
                ? normalized(rig.nodes[rig.torso_node] - rig.nodes[rig.root_node],
                    { 0.0f, 1.0f })
                : Vec2{ 0.0f, 1.0f };
            const float body_error = observation[0]
                - (foundational_human ? authored_body.x : 0.0f);
            const float correction = clamp(body_error * 0.32f
                + observation[2] * 0.05f
                + (foundational_human
                    ? observation[sim::torso_turn_rate_observation] * 0.42f
                    : 0.0f),
                -0.18f, 0.18f);
            action[0] = clamp(action[0] - correction, -0.44f, 0.44f);
            action[1] = clamp(action[1] + correction * 0.16f,
                -0.44f, 0.44f);
            action[2] = clamp(action[2] - correction, -0.44f, 0.44f);
            action[3] = clamp(action[3] - correction * 0.16f,
                -0.44f, 0.44f);
        }

        const bool six_foot_plate_topology =
            rig.additional_left_contact_nodes.size() == 3u
            && rig.additional_right_contact_nodes.size() == 1u;
        if (six_foot_plate_topology)
        {
            // The authored-pose Stand guide is the demonstration for the
            // horizontal six-foot rig. Residual angle damping continually
            // wound its three rigid plates against one another and prevented
            // the low-joint-speed stance timer from ever starting.
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
                action[index] = 0.0f;
        }

        const bool support_loaded = environment.left_supported()
            || environment.right_supported();
        const float upper_body_authority = support_loaded
            && environment.stable_stance_seconds() >= 0.75f ? 0.35f : 0.10f;
        for (std::size_t index = 0; index < rig.active_motor_count; ++index)
        {
            if (!motor_drives_support_branch(rig, rig.motors[index]))
                action[index] *= upper_body_authority;
        }
        return action;
    }

    struct MotorDiscoveryProbe
    {
        std::array<float, sim::action_count> action{};
        float weight{};
    };

    [[nodiscard]] inline std::size_t motor_discovery_lane_count(
        const sim::CreatureBlueprint& rig) noexcept
    {
        return std::min<std::size_t>(2u * rig.active_motor_count + 8u, 28u);
    }

    [[nodiscard]] inline MotorDiscoveryProbe motor_discovery_probe(
        const sim::Environment& environment, std::size_t environment_index,
        std::uint64_t update, std::size_t rollout_step) noexcept
    {
        MotorDiscoveryProbe probe{};
        const std::size_t active = environment.blueprint().active_motor_count;
        const std::size_t lane_count = motor_discovery_lane_count(environment.blueprint());
        if (active == 0u || environment_index >= lane_count || update >= 480u)
            return probe;
        const std::size_t half_cycle = (rollout_step / 24u) & 1u;
        const float progress = clamp(static_cast<float>(update) / 480.0f, 0.0f, 1.0f);
        const float amplitude = lerp(0.20f, 0.48f, progress);
        const std::size_t lane = environment_index % lane_count;
        if (half_cycle != 0u)
        {
            probe.weight = 0.88f;
            return probe;
        }
        if (lane < active)
            probe.action[lane] = amplitude;
        else if (lane < active * 2u)
            probe.action[lane - active] = -amplitude;
        else
        {
            const std::size_t pattern = lane - active * 2u;
            if (pattern == 0u || pattern == 1u)
            {
                const float sign = pattern == 0u ? 1.0f : -1.0f;
                for (std::size_t index = 0; index < active; ++index)
                    probe.action[index] = amplitude * sign;
            }
            else if (pattern == 2u)
            {
                for (std::size_t index = 0; index < active; ++index)
                    probe.action[index] = ((index / 2u) & 1u) == 0u
                        ? amplitude : -amplitude;
            }
            else if (pattern == 3u)
            {
                for (std::size_t index = 0; index < active; ++index)
                    probe.action[index] = (index & 1u) == 0u
                        ? amplitude : -amplitude;
            }
            else if (active >= 4u)
            {
                // Anatomy-aware simultaneous lanes: left chain, right chain,
                // bilateral crouch, and bilateral extension. These explicitly
                // teach that hip and knee joints may move in one policy step.
                if (pattern == 4u || pattern == 6u)
                {
                    probe.action[0] = -amplitude;
                    probe.action[1] = amplitude;
                }
                if (pattern == 5u || pattern == 6u)
                {
                    probe.action[2] = amplitude;
                    probe.action[3] = -amplitude;
                }
                if (pattern == 7u)
                {
                    probe.action[0] = amplitude;
                    probe.action[1] = -amplitude;
                    probe.action[2] = -amplitude;
                    probe.action[3] = amplitude;
                }
            }
        }
        // Support discovery may use the full authored range. Manipulator
        // discovery stays near its authored rest so exploration cannot teach
        // a balancing T-pose before locomotion begins.
        const sim::CreatureBlueprint& rig = environment.blueprint();
        for (std::size_t index = 0; index < active; ++index)
        {
            if (!motor_drives_support_branch(rig, rig.motors[index]))
                probe.action[index] = clamp(probe.action[index], -0.18f, 0.18f);
        }
        probe.weight = 0.88f;
        return probe;
    }

    [[nodiscard]] inline float motor_action_for_target_angle(
        const sim::MotorConstraint& motor, float target_angle) noexcept
    {
        // signed_angle() is canonicalized to [-pi, pi], while an authored
        // neutral and its calibrated travel interval may legitimately straddle
        // that branch cut. Unwrap the target around the authored neutral before
        // clamping so equivalent +pi/-pi poses do not become full-scale torque.
        target_angle = motor.neutral_angle
            + wrap_angle(target_angle - motor.neutral_angle);
        target_angle = clamp(target_angle, motor.minimum_angle, motor.maximum_angle);
        if (target_angle < motor.neutral_angle)
        {
            const float span = std::max(1.0e-5f,
                motor.neutral_angle - motor.minimum_angle);
            return clamp((target_angle - motor.neutral_angle) / span, -1.0f, 0.0f);
        }
        const float span = std::max(1.0e-5f,
            motor.maximum_angle - motor.neutral_angle);
        return clamp((target_angle - motor.neutral_angle) / span, 0.0f, 1.0f);
    }

    [[nodiscard]] inline float authored_joint_flexion_direction(
        const sim::MotorConstraint& motor) noexcept
    {
        // A joint's authored neutral can sit on either side of the signed-angle
        // branch cut. Flexion is the calibrated direction that folds the two
        // links toward one another, not a hard-coded left/right motor sign.
        const float neutral = wrap_angle(motor.neutral_angle);
        if (std::abs(neutral) <= 1.0e-4f)
            return 1.0f;
        const float target = motor.neutral_angle
            - std::copysign(std::min(0.12f, std::abs(neutral)), neutral);
        const float action = motor_action_for_target_angle(motor, target);
        if (std::abs(action) <= 1.0e-4f)
            return -std::copysign(1.0f, neutral);
        return std::copysign(1.0f, action);
    }

    [[nodiscard]] inline std::uint8_t motor_support_mask(
        const sim::CreatureBlueprint& rig,
        const sim::MotorConstraint& motor) noexcept
    {
        return rig.support_branch_mask(motor);
    }
    [[nodiscard]] inline bool motor_drives_support_branch(
        const sim::CreatureBlueprint& rig,
        const sim::MotorConstraint& motor) noexcept
    {
        return motor_support_mask(rig, motor) != 0u;
    }

    [[nodiscard]] inline bool rig_has_manipulator_motors(
        const sim::CreatureBlueprint& rig) noexcept
    {
        for (std::size_t index = 0; index < rig.active_motor_count; ++index)
        {
            if (!motor_drives_support_branch(rig, rig.motors[index]))
                return true;
        }
        return false;
    }

    [[nodiscard]] inline bool motor_drives_manipulator_chain(
        const sim::CreatureBlueprint& rig, std::size_t motor_index) noexcept
    {
        if (motor_index >= rig.active_motor_count)
            return false;
        const sim::MotorConstraint& motor = rig.motors[motor_index];
        if (!motor.enabled || motor_drives_support_branch(rig, motor))
            return false;
        if (motor.a == rig.torso_node)
        {
            for (std::size_t distal_index = 0;
                distal_index < rig.active_motor_count; ++distal_index)
            {
                const sim::MotorConstraint& distal = rig.motors[distal_index];
                if (distal.enabled && !motor_drives_support_branch(rig, distal)
                    && distal.pivot == motor.c)
                    return true;
            }
            return false;
        }
        for (std::size_t proximal_index = 0;
            proximal_index < rig.active_motor_count; ++proximal_index)
        {
            const sim::MotorConstraint& proximal = rig.motors[proximal_index];
            if (proximal.enabled && !motor_drives_support_branch(rig, proximal)
                && proximal.a == rig.torso_node && proximal.c == motor.pivot)
                return true;
        }
        return false;
    }

    inline void settle_manipulator_actions_to_rest(
        const sim::CreatureBlueprint& rig,
        std::array<float, sim::action_count>& action, float strength) noexcept
    {
        strength = clamp(strength, 0.0f, 1.0f);
        for (std::size_t index = 0; index < rig.active_motor_count; ++index)
        {
            if (!motor_drives_manipulator_chain(rig, index))
                continue;
            const float rest = motor_action_for_target_angle(
                rig.motors[index], rig.rest_joint_angle(index));
            action[index] = lerp(action[index], rest, strength);
        }
    }
    inline void constrain_human_manipulator_actions_to_body_envelope(
        const sim::Environment& environment,
        std::array<float, sim::action_count>& action) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const auto particles = environment.particles();
        if (!rig.human_casual_gait_plan()
            || rig.root_node >= particles.size()
            || rig.root_node >= rig.nodes.size())
            return;

        const Vec2 root = particles[rig.root_node].position;
        for (std::size_t shoulder_index = 0;
            shoulder_index < rig.active_motor_count; ++shoulder_index)
        {
            const sim::MotorConstraint& shoulder = rig.motors[shoulder_index];
            if (!motor_drives_manipulator_chain(rig, shoulder_index)
                || shoulder.a != rig.torso_node)
                continue;

            for (std::size_t elbow_index = 0;
                elbow_index < rig.active_motor_count; ++elbow_index)
            {
                const sim::MotorConstraint& elbow = rig.motors[elbow_index];
                if (elbow_index == shoulder_index
                    || !motor_drives_manipulator_chain(rig, elbow_index)
                    || elbow.pivot != shoulder.c
                    || elbow.c >= particles.size()
                    || elbow.c >= rig.nodes.size())
                    continue;

                const float rest_radius = length(
                    rig.nodes[elbow.c] - rig.nodes[rig.root_node]);
                const float integrity_radius = std::max(
                    1.80f, rest_radius * 3.0f + 0.80f);
                const float soft_radius = std::max(
                    rest_radius + 0.35f, integrity_radius - 0.45f);
                const float hard_radius = std::max(
                    soft_radius + 0.05f, integrity_radius - 0.18f);
                const float current_radius = length(
                    particles[elbow.c].position - root);
                const float normalized = clamp(
                    (current_radius - soft_radius)
                        / std::max(0.05f, hard_radius - soft_radius),
                    0.0f, 1.0f);
                const float strength = normalized * normalized
                    * (3.0f - 2.0f * normalized);
                if (strength <= 0.0f)
                    break;

                const float shoulder_rest = motor_action_for_target_angle(
                    shoulder, rig.rest_joint_angle(shoulder_index));
                const float elbow_rest = motor_action_for_target_angle(
                    elbow, rig.rest_joint_angle(elbow_index));
                action[shoulder_index] = lerp(
                    action[shoulder_index], shoulder_rest, strength);
                action[elbow_index] = lerp(
                    action[elbow_index], elbow_rest, strength);
                break;
            }
        }
    }

    [[nodiscard]] inline bool rig_has_driven_two_link_support_chains(
        const sim::CreatureBlueprint& rig) noexcept
    {
        std::size_t chain_count{};
        for (std::size_t proximal_index = 0;
            proximal_index < rig.active_motor_count; ++proximal_index)
        {
            const sim::MotorConstraint& proximal = rig.motors[proximal_index];
            const std::uint8_t support_mask = motor_support_mask(rig, proximal);
            if (!proximal.enabled || support_mask == 0u
                || proximal.pivot >= rig.nodes.size()
                || proximal.c >= rig.nodes.size())
                continue;
            for (std::size_t distal_index = 0;
                distal_index < rig.active_motor_count; ++distal_index)
            {
                const sim::MotorConstraint& distal = rig.motors[distal_index];
                if (distal_index == proximal_index || !distal.enabled
                    || distal.pivot != proximal.c
                    || distal.c >= rig.nodes.size()
                    || motor_support_mask(rig, distal) != support_mask)
                    continue;
                ++chain_count;
                break;
            }
        }
        return chain_count >= 2u;
    }
    [[nodiscard]] inline std::array<float, sim::action_count> compact_support_teacher_action(
        const sim::Environment& environment, float pressure) noexcept
    {
        auto action = balance_teacher_action(environment);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        pressure = clamp(pressure, 0.0f, 1.0f);
        for (std::size_t index = 0; index < rig.active_motor_count; ++index)
        {
            const sim::MotorConstraint& motor = rig.motors[index];
            if (!motor.enabled || motor.a >= rig.nodes.size()
                || motor.pivot >= rig.nodes.size() || motor.c >= rig.nodes.size()
                || !motor_drives_support_branch(rig, motor))
                continue;
            const Vec2 reference = rig.nodes[motor.a] - rig.nodes[motor.pivot];
            const Vec2 driven = rig.nodes[motor.c] - rig.nodes[motor.pivot];
            if (length(reference) <= 1.0e-5f || length(driven) <= 1.0e-5f)
                continue;
            Vec2 compact = driven;
            compact.x *= 1.0f + pressure * 0.22f;
            compact.y *= 1.0f - pressure * 0.36f;
            const float target = signed_angle(reference, compact);
            const float desired = motor_action_for_target_angle(motor, target);
            action[index] = lerp(action[index], desired, 0.78f);
        }
        return action;
    }

    [[nodiscard]] inline std::array<float, sim::action_count> bilateral_joint_synergy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> action,
        sim::CourseStage stage) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        if (!rig.paired_leg_chains())
        {
            for (float& value : action)
                value = clamp(value, -1.0f, 1.0f);
            return action;
        }

        if (stage == sim::CourseStage::duck_press)
        {
            const float left_hip_direction =
                authored_joint_flexion_direction(rig.motors[0]);
            const float left_knee_direction =
                authored_joint_flexion_direction(rig.motors[1]);
            const float right_hip_direction =
                authored_joint_flexion_direction(rig.motors[2]);
            const float right_knee_direction =
                authored_joint_flexion_direction(rig.motors[3]);
            const float shared_hip_flex = 0.5f
                * (std::max(0.0f, action[0] * left_hip_direction)
                    + std::max(0.0f, action[2] * right_hip_direction));
            const float shared_knee_flex = 0.5f
                * (std::max(0.0f, action[1] * left_knee_direction)
                    + std::max(0.0f, action[3] * right_knee_direction));
            constexpr float chain_strength = 0.88f;
            action[0] = lerp(action[0],
                left_hip_direction * shared_hip_flex, chain_strength);
            action[1] = lerp(action[1],
                left_knee_direction * shared_knee_flex, chain_strength);
            action[2] = lerp(action[2],
                right_hip_direction * shared_hip_flex, chain_strength);
            action[3] = lerp(action[3],
                right_knee_direction * shared_knee_flex, chain_strength);
        }
        else if (stage != sim::CourseStage::balance)
        {
            // Shuttle support timing already comes from the opposed gait teacher.
            // Re-projecting those joints here can erase the contact transfer.
            const float pair_strength = stage == sim::CourseStage::shuttle
                ? 0.0f
                : stage == sim::CourseStage::crouch_walk ? 0.18f : 0.10f;
            const float left_hip_direction =
                authored_joint_flexion_direction(rig.motors[0]);
            const float left_knee_direction =
                authored_joint_flexion_direction(rig.motors[1]);
            const float right_hip_direction =
                authored_joint_flexion_direction(rig.motors[2]);
            const float right_knee_direction =
                authored_joint_flexion_direction(rig.motors[3]);
            const float hip_phase = 0.5f
                * (action[0] * left_hip_direction
                    - action[2] * right_hip_direction);
            const float knee_phase = 0.5f
                * (action[1] * left_knee_direction
                    - action[3] * right_knee_direction);
            action[0] = lerp(action[0],
                left_hip_direction * hip_phase, pair_strength);
            action[2] = lerp(action[2],
                -right_hip_direction * hip_phase, pair_strength);
            action[1] = lerp(action[1],
                left_knee_direction * knee_phase, pair_strength);
            action[3] = lerp(action[3],
                -right_knee_direction * knee_phase, pair_strength);
        }

        std::array<std::size_t, 2> shoulder_indices{};
        std::array<std::size_t, 2> elbow_indices{};
        std::size_t manipulator_chain_count{};
        if (rig.paired_leg_chains())
        {
            for (std::size_t shoulder_index = 0;
                shoulder_index < rig.active_motor_count
                    && manipulator_chain_count < shoulder_indices.size();
                ++shoulder_index)
            {
                const sim::MotorConstraint& shoulder = rig.motors[shoulder_index];
                if (motor_drives_support_branch(rig, shoulder)
                    || shoulder.a != rig.torso_node)
                    continue;
                for (std::size_t elbow_index = 0;
                    elbow_index < rig.active_motor_count; ++elbow_index)
                {
                    const sim::MotorConstraint& elbow = rig.motors[elbow_index];
                    if (elbow_index == shoulder_index
                        || motor_drives_support_branch(rig, elbow)
                        || elbow.pivot != shoulder.c)
                        continue;
                    shoulder_indices[manipulator_chain_count] = shoulder_index;
                    elbow_indices[manipulator_chain_count] = elbow_index;
                    ++manipulator_chain_count;
                    break;
                }
            }
        }
        if (manipulator_chain_count == 2u)
        {
            const bool preserve_human_opposed_swing = rig.human_casual_gait_plan()
                && (stage == sim::CourseStage::uneven
                    || stage == sim::CourseStage::shuttle);
            const float arm_pair_strength = preserve_human_opposed_swing ? 0.0f
                : (stage == sim::CourseStage::uneven
                    || stage == sim::CourseStage::shuttle)
                ? 0.18f : sim::stage_allows_controlled_flips(stage)
                    ? 0.24f : 0.10f;
            const std::size_t left_shoulder = shoulder_indices[0];
            const std::size_t right_shoulder = shoulder_indices[1];
            const std::size_t left_elbow = elbow_indices[0];
            const std::size_t right_elbow = elbow_indices[1];
            const float left_shoulder_direction =
                authored_joint_flexion_direction(rig.motors[left_shoulder]);
            const float right_shoulder_direction =
                authored_joint_flexion_direction(rig.motors[right_shoulder]);
            const float left_elbow_direction =
                authored_joint_flexion_direction(rig.motors[left_elbow]);
            const float right_elbow_direction =
                authored_joint_flexion_direction(rig.motors[right_elbow]);
            const float shoulder = 0.5f
                * (action[left_shoulder] * left_shoulder_direction
                    - action[right_shoulder] * right_shoulder_direction);
            const float elbow = 0.5f
                * (action[left_elbow] * left_elbow_direction
                    - action[right_elbow] * right_elbow_direction);
            action[left_shoulder] = lerp(action[left_shoulder],
                left_shoulder_direction * shoulder, arm_pair_strength);
            action[right_shoulder] = lerp(action[right_shoulder],
                -right_shoulder_direction * shoulder, arm_pair_strength);
            action[left_elbow] = lerp(action[left_elbow],
                left_elbow_direction * elbow, arm_pair_strength);
            action[right_elbow] = lerp(action[right_elbow],
                -right_elbow_direction * elbow, arm_pair_strength);
        }

        for (float& value : action)
            value = clamp(value, -1.0f, 1.0f);
        return action;
    }

    [[nodiscard]] inline std::array<float, sim::action_count> duck_teacher_action(
        const sim::Environment& environment) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        float pressure = environment.duck_press_completed()
            ? 0.0f : environment.duck_obstacle_weight();
        if (environment.duck_active())
            pressure *= 0.55f;
        auto action = compact_support_teacher_action(
            environment, pressure * 0.48f);
        if (rig.paired_leg_chains() && !environment.duck_press_completed())
        {
            const float span_ratio = environment.primary_support_span_ratio();
            const float span_brake = clamp((span_ratio - 1.02f) * 0.34f,
                0.0f, 0.24f);
            const sim::CrouchPostureEvidence posture =
                environment.current_crouch_posture();
            const float drop_deficit = clamp(
                (0.42f - posture.pelvis_drop) / 0.42f, 0.0f, 1.0f);
            const float hip_flex = std::max(0.025f,
                0.10f * pressure - span_brake);
            const float knee_flex = (0.60f + drop_deficit * 0.10f) * pressure;
            const float left_hip_direction =
                authored_joint_flexion_direction(rig.motors[0]);
            const float left_knee_direction =
                authored_joint_flexion_direction(rig.motors[1]);
            const float right_hip_direction =
                authored_joint_flexion_direction(rig.motors[2]);
            const float right_knee_direction =
                authored_joint_flexion_direction(rig.motors[3]);
            action[0] = clamp(action[0]
                + left_hip_direction * hip_flex, -0.62f, 0.62f);
            action[1] = clamp(action[1]
                + left_knee_direction * knee_flex, -0.82f, 0.82f);
            action[2] = clamp(action[2]
                + right_hip_direction * hip_flex, -0.62f, 0.62f);
            action[3] = clamp(action[3]
                + right_knee_direction * knee_flex, -0.82f, 0.82f);
        }
        for (std::size_t index = 0; index < rig.active_motor_count; ++index)
        {
            if (!motor_drives_support_branch(rig, rig.motors[index]))
                action[index] = 0.0f;
        }
        return bilateral_joint_synergy_action(environment, action,
            sim::CourseStage::duck_press);
    }

    [[nodiscard]] inline locomotion::Signals locomotion_signals(
        const sim::Environment& environment) noexcept
    {
        locomotion::Signals signals{};
        const auto& rig = environment.blueprint();
        const auto particles = environment.particles();
        if (!rig.valid() || particles.empty() || rig.root_node >= particles.size()
            || rig.left_contact_node >= particles.size()
            || rig.right_contact_node >= particles.size())
            return signals;

        const Vec2 root = particles[rig.root_node].position;
        const float ground = environment.ground_height_at(root.x);
        const float requested_direction = environment.locomotion_direction();
        const float lookahead_direction = std::abs(requested_direction) >= 0.5f
            ? requested_direction : environment.facing_direction();
        signals.uprightness = environment.uprightness();
        signals.root_x = root.x;
        signals.left_support_x = particles[rig.left_contact_node].position.x;
        signals.right_support_x = particles[rig.right_contact_node].position.x;
        signals.left_supported = environment.left_supported();
        signals.right_supported = environment.right_supported();
        signals.near_rise = environment.ground_height_at(root.x + 0.65f * lookahead_direction) - ground;
        signals.mid_rise = environment.ground_height_at(root.x + 1.50f * lookahead_direction) - ground;
        signals.far_rise = environment.ground_height_at(root.x + 3.00f * lookahead_direction) - ground;
        signals.left_escape_rise = environment.ground_height_at(root.x - 0.85f) - ground;
        signals.right_escape_rise = environment.ground_height_at(root.x + 0.85f) - ground;
        signals.slope = environment.terrain().slope_at(
            sim::terrain_sample_x(root.x, environment.course_progress()))
            * lookahead_direction;
        signals.forward_speed = environment.forward_speed();
        signals.recovering = environment.recovering();
        signals.non_foot_grounded = environment.non_foot_grounded();
        signals.burial_depth = environment.burial_depth();
        signals.obstruction_mask = environment.obstruction_mask();
        signals.free_space_direction = environment.free_space_direction();
        signals.incoming_velocity_x = environment.incoming_material_velocity().x;
        signals.incoming_time_to_impact = environment.incoming_time_to_impact();
        signals.incoming_density = environment.incoming_material_density();
        signals.gait_cycles = environment.gait_cycles();
        signals.requested_direction = requested_direction;
        signals.turning = environment.shuttle_phase() == sim::ShuttlePhase::turning
            || environment.shuttle_phase() == sim::ShuttlePhase::settling;
        signals.dynamic_hazard_active = environment.granular_hazard_active();
        signals.dynamic_hazard_safe = environment.granular_hazard_safe();
        signals.zero_progress_seconds = environment.zero_progress_seconds();
        signals.micro_motion_seconds = environment.micro_motion_seconds();
        signals.hazard_stall_seconds = environment.hazard_stall_seconds();
        const float material_probe_x = root.x + lookahead_direction * 0.75f;
        signals.terrain_firmness = std::min(
            environment.terrain_firmness_at(root.x),
            environment.terrain_firmness_at(material_probe_x));
        signals.terrain_looseness = std::max(environment.terrain_looseness_at(root.x),
            environment.terrain_looseness_at(material_probe_x));
        signals.water_depth = environment.water_depth();
        signals.water_submersion = environment.water_submersion();
        signals.water_depth_ahead = environment.water_depth_at(
            root.x + lookahead_direction * 0.75f);
        signals.saturated_depth_ahead = environment.water_depth_at(
            root.x + lookahead_direction
                * locomotion::shore_exit_lookahead_m);
        signals.shore_exit_ahead = signals.water_depth
                > locomotion::wading_depth_threshold
            && environment.water_depth_at(
                root.x + lookahead_direction
                    * locomotion::shore_exit_lookahead_m)
                <= locomotion::wading_depth_threshold;
        signals.weapon_equipped = environment.weapon_class()
            != sim::WeaponClass::none;

        for (const sim::CourseFeature& feature : environment.course_features())
        {
            const bool granular_block = feature.marker_sequence >= 50'000;
            if (feature.kind != sim::CourseFeatureKind::moving_hazard
                && feature.kind != sim::CourseFeatureKind::projectile
                && !granular_block)
                continue;
            const float dx = feature.center.x - root.x;
            const float relative_velocity = feature.velocity.x
                - environment.forward_speed();
            const float closing_speed = dx > 0.0f
                ? std::max(0.0f, -relative_velocity)
                : std::max(0.0f, relative_velocity);
            if (closing_speed <= 0.05f)
                continue;
            const float time = std::abs(dx) / closing_speed;
            if (time < signals.incoming_time_to_impact)
            {
                signals.incoming_time_to_impact = time;
                signals.incoming_velocity_x = relative_velocity;
                signals.incoming_density = granular_block ? 0.95f
                    : feature.kind == sim::CourseFeatureKind::moving_hazard
                        ? 0.90f : 0.65f;
            }
        }
        return signals;
    }

    [[nodiscard]] inline locomotion::Plan current_locomotion_plan(
        const sim::Environment& environment) noexcept
    {
        return locomotion::plan(locomotion_signals(environment));
    }

    [[nodiscard]] inline float locomotion_gait_seconds(
        const sim::Environment& environment) noexcept
    {
        // Each planted shuttle phase owns a deterministic local gait clock.
        // This keeps hold/turn transitions in their authored rest pose while
        // preserving frame-rate-independent timing within every phase.
        const float phase_seconds = environment.shuttle_enabled()
            ? environment.shuttle_phase_seconds()
            : environment.elapsed_seconds();
        return environment.blueprint().human_casual_gait_plan()
            ? sim::policy_control_phase_seconds(phase_seconds)
            : phase_seconds;
    }

    struct MultiSupportTeacherParameters
    {
        float cadence_hz{};
        float amplitude{};
        float phase_offset{};
        float stance_backstroke{};
        float swing_lift_ratio{};
    };

    [[nodiscard]] inline std::size_t multi_support_phase_group(
        const sim::CreatureBlueprint& rig,
        const sim::MotorConstraint& motor) noexcept
    {
        const std::uint8_t mask = motor_support_mask(rig, motor);
        const std::size_t side = mask == 0x2u ? 1u : 0u;
        return (rig.support_branch_longitudinal_band(motor) + side) & 1u;
    }

    [[nodiscard]] inline float multi_support_release_distance(
        const sim::CreatureBlueprint& rig) noexcept
    {
        return rig.support_seed_count() >= 6u ? 14.0f : 10.0f;
    }

    [[nodiscard]] inline float multi_support_release_stride_events(
        const sim::CreatureBlueprint& rig) noexcept
    {
        // Count only completed topology-aware support transfers. The strict replay
        // still requires full survival and useful distance, so 18/22 transfers
        // reject the old two-step plateau without penalizing rough repeated seeds.
        return rig.support_seed_count() >= 6u ? 22.0f : 18.0f;
    }

    [[nodiscard]] inline bool multi_support_progress_truth(float distance,
        std::uint32_t gait_cycles, float survival_seconds) noexcept
    {
        if (!std::isfinite(distance) || !std::isfinite(survival_seconds)
            || distance < 0.35f || gait_cycles < 2u || survival_seconds < 0.75f)
            return false;
        return distance / static_cast<float>(gait_cycles) >= 0.045f;
    }

    [[nodiscard]] inline float multi_support_gait_authority(
        const locomotion::Plan& movement) noexcept
    {
        if (movement.intent == locomotion::Intent::recover)
            return 0.18f;
        if (movement.intent == locomotion::Intent::crawl)
            return 0.30f;
        const float reserve_authority = clamp(
            (movement.balance_reserve - 0.25f) / 0.45f, 0.35f, 1.0f);
        return movement.brake ? std::min(reserve_authority, 0.55f)
            : reserve_authority;
    }

    [[nodiscard]] inline std::array<float, sim::action_count>
    multi_support_teacher_action(const sim::Environment& environment,
        MultiSupportTeacherParameters parameters) noexcept
    {
        auto action = balance_teacher_action(environment);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const locomotion::Plan movement = current_locomotion_plan(environment);
        const float local_direction = movement.direction
            * environment.facing_direction();
        const float learned_gait_authority = multi_support_gait_authority(movement);
        const float topology_gait_floor = movement.intent == locomotion::Intent::recover
            || movement.intent == locomotion::Intent::crawl
            ? 0.0f
            : (rig.support_seed_count() >= 6u ? 0.86f : 0.72f);
        const float gait_authority = std::max(learned_gait_authority, topology_gait_floor);
        const float phase = locomotion_gait_seconds(environment) * 2.0f * pi
            * parameters.cadence_hz + parameters.phase_offset;
        for (std::size_t index = 0; index < rig.active_motor_count; ++index)
        {
            const sim::MotorConstraint& motor = rig.motors[index];
            const std::uint8_t mask = motor_support_mask(rig, motor);
            if (mask == 0u || motor.a >= rig.nodes.size()
                || motor.pivot >= rig.nodes.size() || motor.c >= rig.nodes.size())
                continue;
            const float phase_sign = multi_support_phase_group(rig, motor) == 0u
                ? 1.0f : -1.0f;
            const float swing = std::sin(phase) * phase_sign;
            const float drive = swing * local_direction * gait_authority;
            const Vec2 reference = rig.nodes[motor.a] - rig.nodes[motor.pivot];
            Vec2 desired = rig.nodes[motor.c] - rig.nodes[motor.pivot];
            const float segment_length = length(desired);
            desired.x += parameters.amplitude * segment_length * drive;
            if (rig.support_seed_count() >= 6u)
            {
                desired.x -= parameters.stance_backstroke * segment_length
                    * local_direction * gait_authority;
            }
            else if (swing < 0.0f)
                desired.x -= parameters.stance_backstroke * segment_length
                    * local_direction * gait_authority * -swing;
            desired.y += parameters.swing_lift_ratio * segment_length
                * std::max(0.0f, swing) * gait_authority;
            const float target = signed_angle(reference, desired);
            action[index] = motor_action_for_target_angle(motor, target);
        }
        // Leg and arm IK above already establishes authored bend direction,
        // opposed phase, and the saved side-rest origin. Return that solved cycle
        // directly; effective_policy_action applies the generic bilateral filter
        // once at the final composition boundary.
        for (float& value : action)
            value = clamp(value, -1.0f, 1.0f);
        return action;
    }

    struct TwoLinkSagittalSolution
    {
        Vec2 upper{};
        Vec2 lower{};
        bool valid{};
    };

    [[nodiscard]] inline TwoLinkSagittalSolution solve_two_link_sagittal(
        float upper_length, float lower_length, Vec2 target,
        float bend_direction) noexcept
    {
        if (!std::isfinite(upper_length) || !std::isfinite(lower_length)
            || !std::isfinite(target.x) || !std::isfinite(target.y)
            || upper_length <= 0.001f || lower_length <= 0.001f)
            return {};
        const float target_length = length(target);
        if (!std::isfinite(target_length) || target_length <= 0.001f)
            return {};
        const float reach = clamp(target_length,
            std::abs(upper_length - lower_length) + 0.001f,
            upper_length + lower_length - 0.001f);
        target *= reach / target_length;
        const float target_direction = std::atan2(target.y, target.x);
        const float alpha = std::acos(clamp(
            (upper_length * upper_length + reach * reach
                - lower_length * lower_length)
                / (2.0f * upper_length * reach), -1.0f, 1.0f));
        const float upper_angle = target_direction
            + (bend_direction < 0.0f ? -alpha : alpha);
        const Vec2 upper{
            std::cos(upper_angle) * upper_length,
            std::sin(upper_angle) * upper_length
        };
        return { upper, target - upper, true };
    }

    [[nodiscard]] inline Vec2 authored_opposed_swing_target(
        Vec2 authored_endpoint, float phase, float horizontal_amplitude,
        float vertical_lift, float direction = 1.0f) noexcept
    {
        if (!std::isfinite(authored_endpoint.x)
            || !std::isfinite(authored_endpoint.y)
            || !std::isfinite(phase)
            || !std::isfinite(horizontal_amplitude)
            || !std::isfinite(vertical_lift)
            || !std::isfinite(direction))
            return authored_endpoint;
        const float swing = std::sin(phase);
        authored_endpoint.x += clamp(horizontal_amplitude, 0.0f, 2.0f)
            * swing * clamp(direction, -1.0f, 1.0f);
        authored_endpoint.y += clamp(vertical_lift, 0.0f, 2.0f)
            * std::max(0.0f, -std::cos(phase));
        return authored_endpoint;
    }

    [[nodiscard]] inline float human_contralateral_arm_swing_offset(
        float leg_phase, float horizontal_amplitude,
        float direction = 1.0f) noexcept
    {
        if (!std::isfinite(leg_phase)
            || !std::isfinite(horizontal_amplitude)
            || !std::isfinite(direction))
            return 0.0f;
        // Heel strike is the visible phase landmark: when a leg reaches
        // forward, its same-side arm is aft. The opposite limb uses phase+pi.
        return -std::cos(leg_phase)
            * clamp(horizontal_amplitude, 0.0f, 2.0f)
            * clamp(direction, -1.0f, 1.0f);
    }

    [[nodiscard]] inline float sagittal_step_x(float step_length,
        float progress, bool swing_phase,
        bool smooth_contact_transitions = false,
        float endpoint_tangent = -0.50f) noexcept
    {
        if (!std::isfinite(step_length) || !std::isfinite(progress))
            return 0.0f;
        const float bounded_progress = clamp(progress, 0.0f, 1.0f);
        if (!smooth_contact_transitions)
        {
            const float linear = 0.5f - bounded_progress;
            return step_length * (swing_phase ? -linear : linear);
        }
        // A casual foot must meet the floor with approximately zero world-X
        // speed, not zero pelvis-relative speed. Cubic Hermite endpoints keep
        // both phases continuous with the same modest backward relative speed:
        // the stance stays planted and the swing foot settles before impact.
        const float smooth_start = swing_phase ? -0.5f : 0.5f;
        const float end = -smooth_start;
        endpoint_tangent = clamp(endpoint_tangent, -2.0f, 0.0f);
        const float t = bounded_progress;
        const float t2 = t * t;
        const float t3 = t2 * t;
        return step_length * ((2.0f * t3 - 3.0f * t2 + 1.0f) * smooth_start
            + (t3 - 2.0f * t2 + t) * endpoint_tangent
            + (-2.0f * t3 + 3.0f * t2) * end
            + (t3 - t2) * endpoint_tangent);
    }

    [[nodiscard]] inline std::array<float, sim::action_count>
    multi_support_two_link_teacher_action(const sim::Environment& environment,
        MultiSupportTeacherParameters parameters) noexcept
    {
        auto action = multi_support_teacher_action(environment, parameters);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const locomotion::Plan movement = current_locomotion_plan(environment);
        const float local_direction = movement.direction
            * environment.facing_direction();
        const float learned_gait_authority = multi_support_gait_authority(movement);
        const float topology_gait_floor = movement.intent == locomotion::Intent::recover
            || movement.intent == locomotion::Intent::crawl
            ? 0.0f
            : (rig.support_seed_count() >= 6u ? 0.86f : 0.42f);
        const float gait_authority = std::max(
            learned_gait_authority, topology_gait_floor);
        const float base_phase = locomotion_gait_seconds(environment) * 2.0f * pi
            * parameters.cadence_hz + parameters.phase_offset;
        for (std::size_t proximal_index = 0;
            proximal_index < rig.active_motor_count; ++proximal_index)
        {
            const sim::MotorConstraint& proximal = rig.motors[proximal_index];
            if (!motor_drives_support_branch(rig, proximal)
                || proximal.pivot >= rig.nodes.size()
                || proximal.c >= rig.nodes.size())
                continue;
            for (std::size_t distal_index = 0;
                distal_index < rig.active_motor_count; ++distal_index)
            {
                const sim::MotorConstraint& distal = rig.motors[distal_index];
                if (distal_index == proximal_index
                    || distal.pivot != proximal.c
                    || distal.c >= rig.nodes.size()
                    || !motor_drives_support_branch(rig, distal)
                    || motor_support_mask(rig, proximal)
                        != motor_support_mask(rig, distal))
                    continue;
                const float upper_length = length(
                    rig.nodes[proximal.c] - rig.nodes[proximal.pivot]);
                const float lower_length = length(
                    rig.nodes[distal.c] - rig.nodes[distal.pivot]);
                const Vec2 authored_upper = rig.nodes[proximal.c]
                    - rig.nodes[proximal.pivot];
                const Vec2 authored_lower = rig.nodes[distal.c]
                    - rig.nodes[distal.pivot];
                Vec2 target = rig.nodes[distal.c] - rig.nodes[proximal.pivot];
                float phase = std::fmod(base_phase
                    + (multi_support_phase_group(rig, proximal) == 0u
                        ? 0.0f : pi), 2.0f * pi);
                if (phase < 0.0f)
                    phase += 2.0f * pi;
                const bool swing_phase = phase >= pi;
                const float progress = swing_phase
                    ? (phase - pi) / pi : phase / pi;
                target.x += sagittal_step_x(parameters.amplitude, progress,
                    swing_phase) * local_direction * gait_authority;
                if (swing_phase)
                {
                    target.y += (upper_length + lower_length) * parameters.swing_lift_ratio
                        * std::sin(progress * pi) * gait_authority;
                }
                const float authored_cross = authored_upper.x * authored_lower.y
                    - authored_upper.y * authored_lower.x;
                const float bend_direction = authored_cross >= 0.0f ? -1.0f : 1.0f;
                const TwoLinkSagittalSolution solution = solve_two_link_sagittal(
                    upper_length, lower_length, target, bend_direction);
                if (!solution.valid)
                    break;
                const Vec2 proximal_reference = rig.nodes[proximal.a]
                    - rig.nodes[proximal.pivot];
                action[proximal_index] = motor_action_for_target_angle(proximal,
                    signed_angle(proximal_reference, solution.upper));
                action[distal_index] = motor_action_for_target_angle(distal,
                    signed_angle(-1.0f * solution.upper, solution.lower));
                break;
            }
        }
        // Leg and arm IK above already establishes authored bend direction,
        // opposed phase, and the saved side-rest origin. Return that solved cycle
        // directly; effective_policy_action applies the generic bilateral filter
        // once at the final composition boundary.
        for (float& value : action)
            value = clamp(value, -1.0f, 1.0f);
        return action;
    }

    struct BipedGaitParameters
    {
        float cadence_hz{ sim::foundational_gait_cadence_hz };
        float step_length{ 0.72f };
        float swing_lift{ 0.20f };
        float leg_height{ 2.30f };
        float direction{ 1.0f };
        float phase_offset{};
        float stance_center_x{};
        float transition_flex{};
        float stance_height_ratio{ 1.0f };
        float arm_swing_ratio{ 0.025f };
        float startup_seconds{ 0.15f };
        float stance_lowering_seconds{ 0.15f };
        bool smooth_contact_transitions{};
        float endpoint_tangent{ -0.50f };
        // Optional explicit controller phase. A finite value is part of the
        // caller-visible gait state; NaN preserves the legacy analytic clock.
        float controller_phase_radians{
            std::numeric_limits<float>::quiet_NaN() };
    };

    inline constexpr float biped_leg_reach_reserve = 0.010f;
    inline constexpr float human_authored_stance_step = 0.80f;
    [[nodiscard]] inline float authored_gait_startup_blend(
        float gait_seconds, float startup_seconds = 0.15f) noexcept
    {
        if (!std::isfinite(gait_seconds))
            return 0.0f;
        // Preserve the authored rest pose at spawn, but reach the proven
        // locomotion envelope before short failed episodes can repeatedly
        // restart inside the blend and teach the policy to remain still.
        const float ratio = clamp(gait_seconds /
            std::max(0.05f, startup_seconds), 0.0f, 1.0f);
        return ratio * ratio * (3.0f - 2.0f * ratio);
    }

    [[nodiscard]] inline float reachable_biped_leg_height(
        float chain_length, float horizontal_extent,
        float desired_height) noexcept
    {
        if (!std::isfinite(chain_length) || !std::isfinite(horizontal_extent)
            || !std::isfinite(desired_height) || chain_length <= 0.01f)
            return 0.0f;
        const float maximum_reach = chain_length
            * (1.0f - biped_leg_reach_reserve);
        const float horizontal = std::abs(horizontal_extent);
        const float maximum_height = std::sqrt(std::max(0.0f,
            maximum_reach * maximum_reach - horizontal * horizontal));
        return std::max(0.01f, std::min(std::abs(desired_height), maximum_height));
    }

    [[nodiscard]] inline float biped_stance_vertical_reach(
        float horizontal_extent, float chain_length) noexcept
    {
        if (!std::isfinite(horizontal_extent) || !std::isfinite(chain_length)
            || chain_length <= 0.01f)
            return 0.0f;
        const float maximum_reach = chain_length
            * (1.0f - biped_leg_reach_reserve);
        const float horizontal = std::min(
            std::abs(horizontal_extent), maximum_reach);
        return std::sqrt(std::max(0.0f,
            maximum_reach * maximum_reach - horizontal * horizontal));
    }

    [[nodiscard]] inline Vec2 human_casual_arm_rest_target(
        float chain_length) noexcept
    {
        if (!std::isfinite(chain_length) || chain_length <= 0.01f)
            return {};
        return { 0.0f, -0.94f * chain_length };
    }

    [[nodiscard]] inline float human_flare_vertical_lift(
        float minimum_clearance, float mean_sole_velocity_y,
        float target_clearance, float target_velocity_y,
        float phase_blend) noexcept
    {
        if (!std::isfinite(minimum_clearance)
            || !std::isfinite(mean_sole_velocity_y)
            || !std::isfinite(target_clearance)
            || !std::isfinite(target_velocity_y)
            || !std::isfinite(phase_blend))
            return 0.0f;
        const float position_lift = std::max(0.0f,
            target_clearance - minimum_clearance);
        // The Human leg target is slewed at one action-unit per second, so a
        // late low-gain correction cannot arrest the observed sole before it
        // reaches the floor. This remains a bounded endpoint-height request
        // through the same finite hip/knee motors; it supplies no collision or
        // body impulse.
        const float velocity_lift = std::max(0.0f,
            0.280f * (target_velocity_y - mean_sole_velocity_y));
        return clamp(position_lift + velocity_lift, 0.0f, 0.24f)
            * clamp(phase_blend, 0.0f, 1.0f);
    }

    struct HumanCaptureSupportWeights
    {
        float left{};
        float right{};
    };

    [[nodiscard]] inline HumanCaptureSupportWeights
    human_capture_support_weights(const sim::HumanSupportFeedback& feedback,
        bool left_supported, bool right_supported,
        bool exclude_left = false, bool exclude_right = false) noexcept
    {
        if (!feedback.valid || !std::isfinite(feedback.total_normal_impulse)
            || feedback.total_normal_impulse <= 1.0e-7f
            || !std::isfinite(feedback.load_asymmetry))
            return {};

        HumanCaptureSupportWeights weights{
            0.5f * (1.0f - clamp(feedback.load_asymmetry, -1.0f, 1.0f)),
            0.5f * (1.0f + clamp(feedback.load_asymmetry, -1.0f, 1.0f))
        };
        const bool left_eligible = left_supported && !exclude_left;
        const bool right_eligible = right_supported && !exclude_right;
        if (!left_eligible)
            weights.left = 0.0f;
        if (!right_eligible)
            weights.right = 0.0f;
        const float active_load = weights.left + weights.right;
        if (!std::isfinite(active_load))
            return {};
        if (active_load <= 1.0e-7f)
        {
            const float eligible_count = static_cast<float>(left_eligible)
                + static_cast<float>(right_eligible);
            if (eligible_count <= 0.0f)
                return {};
            return { static_cast<float>(left_eligible) / eligible_count,
                static_cast<float>(right_eligible) / eligible_count };
        }
        weights.left /= active_load;
        weights.right /= active_load;
        return weights;
    }

    [[nodiscard]] inline float human_capture_endpoint_shift(
        const sim::HumanSupportFeedback& feedback, float facing,
        float leg_scale, float sole_span) noexcept
    {
        if (!feedback.valid || !std::isfinite(feedback.capture_error_x)
            || !std::isfinite(feedback.total_normal_impulse)
            || feedback.total_normal_impulse <= 1.0e-7f
            || !std::isfinite(facing) || std::abs(facing) < 0.5f
            || !std::isfinite(leg_scale) || leg_scale <= 0.01f
            || !std::isfinite(sole_span) || sole_span <= 0.01f)
            return 0.0f;
        const float facing_sign = facing < 0.0f ? -1.0f : 1.0f;
        const float maximum_shift = 0.45f * std::abs(sole_span);
        return clamp(feedback.capture_error_x * facing_sign * leg_scale,
            -maximum_shift, maximum_shift);
    }

    inline constexpr float human_flare_target_clearance = 0.030f;
    inline constexpr float human_flare_target_velocity_y = -0.15f;
    inline constexpr float human_departure_leg_shortening = 0.045f;

    [[nodiscard]] inline float human_walk_loaded_departure_shortening(
        const sim::HumanGaitCoordinatorState& gait,
        const sim::HumanSupportFeedback& support, bool opposite_supported,
        float leg_length) noexcept
    {
        if (!gait.active || gait.completed_handoffs == 0u
            || gait.stage != sim::HumanGaitCoordinatorStage::departure
            || !opposite_supported || !support.valid
            || !std::isfinite(gait.stage_seconds) || !std::isfinite(leg_length)
            || !std::isfinite(support.load_asymmetry)
            || !std::isfinite(support.total_normal_impulse)
            || leg_length <= 0.01f || support.total_normal_impulse <= 0.001f)
            return human_departure_leg_shortening;
        // The established stance, not the swing ankle, must carry the load
        // before asking the two-link swing leg to retract. Let a delayed
        // release raise the entire sole instead of curling its distal toe
        // farther while the ball of the foot stays on the terrain.
        const float stance_share = 0.5f * (1.0f
            + (gait.outgoing_side < 0 ? 1.0f : -1.0f) * support.load_asymmetry);
        const float load_blend = clamp((stance_share - 0.55f) / 0.25f, 0.0f, 1.0f);
        const float wait_blend = clamp(gait.stage_seconds / 0.22f, 0.0f, 1.0f);
        return human_departure_leg_shortening
            + std::min(0.10f, 0.047f * leg_length) * wait_blend * load_blend;
    }

    [[nodiscard]] inline float finite_sole_alignment_correction(
        Vec2 current_span, Vec2 desired_span,
        Vec2 relative_step_displacement) noexcept
    {
        const float span_squared = length_squared(current_span);
        if (!std::isfinite(current_span.x) || !std::isfinite(current_span.y)
            || !std::isfinite(desired_span.x)
            || !std::isfinite(desired_span.y)
            || !std::isfinite(relative_step_displacement.x)
            || !std::isfinite(relative_step_displacement.y)
            || span_squared <= 1.0e-8f
            || length_squared(desired_span) <= 1.0e-8f)
            return 0.0f;
        const float position_error = signed_angle(current_span, desired_span);
        const float angular_step = cross(current_span,
            relative_step_displacement) / span_squared;
        // A bounded discrete PD target spends two parts on the remaining plane
        // error and one part braking the measured one-step angular motion. The
        // request still flows only through the finite MTP motor; it never
        // translates or contacts a foot node.
        constexpr float proportional_gain = 2.0f;
        constexpr float derivative_gain = 1.0f;
        return clamp(position_error * proportional_gain
                - angular_step * derivative_gain,
            -0.12f, 0.12f);
    }

    [[nodiscard]] inline float human_flare_clearance_target(
        const sim::HumanGaitCoordinatorState& coordinator) noexcept
    {
        if (!coordinator.active
            || !std::isfinite(coordinator.flare_clearance_guard))
            return human_flare_target_clearance;
        return clamp(coordinator.flare_clearance_guard,
            human_flare_target_clearance, 0.100f);
    }

    [[nodiscard]] inline float human_flare_horizontal_correction(
        float mean_sole_velocity_x, float phase_blend) noexcept
    {
        if (!std::isfinite(mean_sole_velocity_x)
            || !std::isfinite(phase_blend))
            return 0.0f;
        return clamp(mean_sole_velocity_x * 0.24f, -0.18f, 0.18f)
            * clamp(phase_blend, 0.0f, 1.0f);
    }

    [[nodiscard]] inline float human_landing_alignment_blend(
        const sim::HumanGaitCoordinatorState& coordinator,
        bool left) noexcept
    {
        if (!coordinator.active || !std::isfinite(coordinator.phase)
            || left != (coordinator.outgoing_side < 0))
            return 0.0f;
        if (coordinator.stage == sim::HumanGaitCoordinatorStage::flare
            || coordinator.stage == sim::HumanGaitCoordinatorStage::landing)
            return 1.0f;
        if (coordinator.stage != sim::HumanGaitCoordinatorStage::descent)
            return 0.0f;
        const float boundary = left ? pi : 0.0f;
        const float progress = clamp((coordinator.phase - boundary) / pi,
            0.0f, 1.0f);
        const float blend = clamp((progress - 0.50f) / 0.30f,
            0.0f, 1.0f);
        return blend * blend * (3.0f - 2.0f * blend);
    }

    [[nodiscard]] inline float human_terminal_sole_alignment_blend(
        const sim::HumanGaitCoordinatorState& coordinator,
        bool left) noexcept
    {
        if (!coordinator.active || !std::isfinite(coordinator.phase)
            || left != (coordinator.outgoing_side < 0))
            return 0.0f;
        if (coordinator.stage == sim::HumanGaitCoordinatorStage::apex
            || coordinator.stage == sim::HumanGaitCoordinatorStage::descent
            || coordinator.stage == sim::HumanGaitCoordinatorStage::flare
            || coordinator.stage == sim::HumanGaitCoordinatorStage::landing)
            return 1.0f;
        if (coordinator.stage != sim::HumanGaitCoordinatorStage::swing)
            return 0.0f;
        const float boundary = left ? pi : 0.0f;
        const float swing_progress = clamp(
            (coordinator.phase - boundary) / (0.50f * pi), 0.0f, 1.0f);
        return swing_progress * swing_progress
            * (3.0f - 2.0f * swing_progress);
    }

    [[nodiscard]] inline float human_landing_completion_blend(
        const sim::HumanGaitCoordinatorState& coordinator,
        bool left) noexcept
    {
        if (!coordinator.active || !std::isfinite(coordinator.phase)
            || left != (coordinator.outgoing_side < 0)
            || coordinator.stage
                != sim::HumanGaitCoordinatorStage::landing)
            return 0.0f;
        const float boundary = left ? pi : 0.0f;
        const float progress = clamp((coordinator.phase - boundary) / pi,
            0.0f, 1.0f);
        const float blend = clamp((progress - 0.80f) / 0.20f,
            0.0f, 1.0f);
        return blend * blend * (3.0f - 2.0f * blend);
    }

    [[nodiscard]] inline Vec2 bounded_biped_leg_target(
        Vec2 desired_target, float chain_length) noexcept
    {
        if (!std::isfinite(desired_target.x)
            || !std::isfinite(desired_target.y)
            || !std::isfinite(chain_length)
            || chain_length <= 0.01f)
            return {};
        const float maximum_reach = chain_length
            * (1.0f - biped_leg_reach_reserve);
        const float radius = length(desired_target);
        if (!std::isfinite(radius))
            return {};
        if (radius <= maximum_reach + 1.0e-6f)
            return desired_target;
        if (radius <= 1.0e-6f)
            return {};
        return (maximum_reach / radius) * desired_target;
    }

    [[nodiscard]] inline bool biped_leg_target_within_reach(
        Vec2 target, float chain_length) noexcept
    {
        const float radius = length(target);
        return std::isfinite(target.x) && std::isfinite(target.y)
            && std::isfinite(chain_length)
            && chain_length > 0.01f && std::isfinite(radius)
            && radius <= chain_length * (1.0f - biped_leg_reach_reserve)
                + 1.0e-5f;
    }

    [[nodiscard]] inline float biped_gait_maximum_target_radius(
        const BipedGaitParameters& parameters) noexcept
    {
        const float horizontal_extent = std::abs(parameters.stance_center_x)
            + 0.5f * std::abs(parameters.step_length);
        return std::hypot(horizontal_extent, std::abs(parameters.leg_height));
    }

    [[nodiscard]] inline bool biped_gait_target_within_reach(
        const BipedGaitParameters& parameters, float chain_length) noexcept
    {
        const float radius = biped_gait_maximum_target_radius(parameters);
        return std::isfinite(chain_length) && chain_length > 0.01f
            && std::isfinite(radius)
            && radius <= chain_length * (1.0f - biped_leg_reach_reserve)
                + 1.0e-5f;
    }

    [[nodiscard]] inline BipedGaitParameters anatomy_scaled_foundational_gait(
        const sim::CreatureBlueprint& rig,
        sim::GaitTask gait_task = sim::GaitTask::walk,
        float transition_seconds = 0.0f) noexcept
    {
        float minimum_leg_length = std::numeric_limits<float>::infinity();
        for (const std::size_t hip_index : { 0u, 2u })
        {
            const std::size_t knee_index = hip_index + 1u;
            if (knee_index >= rig.active_motor_count)
                continue;
            const sim::MotorConstraint& hip = rig.motors[hip_index];
            const sim::MotorConstraint& knee = rig.motors[knee_index];
            if (hip.pivot >= rig.nodes.size() || hip.c >= rig.nodes.size()
                || knee.pivot >= rig.nodes.size() || knee.c >= rig.nodes.size())
                continue;
            const float chain_length = length(rig.nodes[hip.c] - rig.nodes[hip.pivot])
                + length(rig.nodes[knee.c] - rig.nodes[knee.pivot]);
            if (!std::isfinite(chain_length) || chain_length <= 0.01f)
                continue;
            minimum_leg_length = std::min(minimum_leg_length, chain_length);
        }
        const float leg_length = std::isfinite(minimum_leg_length)
            ? minimum_leg_length : 2.30f;
        // Human stride scale is anatomical and must not change when arm nodes
        // are edited, disabled, or absent from a diagnostic copy.
        const bool human_stride = rig.presentation_species()
            == sim::CreatureSpecies::human;
        const float gait_mix = human_stride
            ? sim::gait_task_transition_mix(gait_task, transition_seconds) : 0.0f;
        const bool casual_walk = gait_task == sim::GaitTask::walk;
        const bool speed_walk = gait_task == sim::GaitTask::speed_walk;
        const float walk_step = human_stride ? 0.50f
            : clamp(leg_length * 0.34f, 0.62f, 0.82f);
        const float run_step = clamp(leg_length * 0.58f, 1.12f, 1.28f);
        const float speed_walk_step = std::lerp(
            clamp(leg_length * 0.38f, 0.76f, 0.88f), run_step, gait_mix);
        const float step_length = !human_stride ? walk_step
            : casual_walk ? walk_step
            : speed_walk ? speed_walk_step
            : std::lerp(walk_step, run_step, gait_mix);
        // The Human's authored boot is a three-seed rigid contact plate. A
        // short attainable lift unloads heel, centre and toe without the high
        // marching step that made casual Walk read as Speed Walk. Alternating
        // landing and lake-exit gates prove that this is not a dragging sole.
        const float walk_lift = human_stride ? 0.22f
            : clamp(leg_length * 0.085f, 0.16f, 0.24f);
        const float run_lift = clamp(leg_length * 0.22f, 0.44f, 0.50f);
        const float speed_walk_lift = std::lerp(
            clamp(leg_length * 0.135f, 0.27f, 0.31f), run_lift, gait_mix);
        const float swing_lift = !human_stride ? walk_lift
            : casual_walk ? walk_lift
            : speed_walk ? speed_walk_lift
            : std::lerp(walk_lift, run_lift, gait_mix);
        // Keep the complete phase envelope inside the authored two-link
        // reach. The compact saved Human no longer has the old 1.88 m
        // minimum extension; forcing it saturated the IK and produced glide.
        const float leg_height = reachable_biped_leg_height(leg_length,
            0.5f * step_length, leg_length * 0.95f);
        // The Human cadence remains casual while providing a complete planted
        // stance transfer instead of compensating with short, rapid shuffles.
        // The small reserve also keeps contact-led replay over the strict walk
        // floor without altering other species.
        BipedGaitParameters parameters{
            human_stride ? sim::gait_task_nominal_cadence_hz(gait_task,
                transition_seconds) : sim::foundational_gait_cadence_hz,
            step_length,
            swing_lift,
            leg_height,
            1.0f
        };
        // Reach a nearly straight planted leg at mid-stance without driving both
        // feet to maximum reach during contact exchange. Flexing only the phase
        // endpoints prevents the old permanent crouch and the newer hop/flip.
        const float run_flex = clamp(leg_length * 0.052f, 0.105f, 0.125f);
        parameters.transition_flex = !human_stride ? 0.0f
            : casual_walk ? 0.055f
            : speed_walk ? std::lerp(
                clamp(leg_length * 0.032f, 0.065f, 0.080f),
                run_flex, gait_mix)
            : std::lerp(0.055f, run_flex, gait_mix);
        parameters.arm_swing_ratio = !human_stride ? 0.040f
            : casual_walk ? 0.012f
            : speed_walk ? std::lerp(0.012f, 0.040f, gait_mix)
            : std::lerp(0.012f, 0.040f, gait_mix);
        if (human_stride && rig.active_motor_count >= 4u
            && rig.motors[0].pivot < rig.nodes.size()
            && rig.motors[1].c < rig.nodes.size()
            && rig.motors[2].pivot < rig.nodes.size()
            && rig.motors[3].c < rig.nodes.size())
        {
            // The two-link IK controls each shank endpoint (the anatomical
            // ankle), not the heel contact behind it. Derive the phase origin
            // in that same joint frame; heel/ball/toe remain physical contact
            // evidence and never shift a motor target implicitly.
            const float left_offset = rig.nodes[rig.motors[1].c].x
                - rig.nodes[rig.motors[0].pivot].x;
            const float right_offset = rig.nodes[rig.motors[3].c].x
                - rig.nodes[rig.motors[2].pivot].x;
            // Start from the saved authored stance: the left support is aft
            // while the right support is forward. Beginning half a cycle out
            // of phase avoids commanding an immediate leg swap and preserves
            // equal clearance after the rig is mirrored for return travel.

            parameters.stance_center_x = 0.5f * (left_offset + right_offset);
            const float authored_left_offset = left_offset
                - parameters.stance_center_x;
            const float authored_phase_step = casual_walk
                ? human_authored_stance_step : parameters.step_length;
            const float authored_progress = clamp(
                0.5f - authored_left_offset
                    / std::max(authored_phase_step, 0.001f),
                0.02f, 0.98f);
            // Match the saved 0.80 m rest phase while allowing the physical
            // casual stride to extend past it, then converge to the task's
            // exact phase reference through Speed Walk and Run.
            parameters.phase_offset = pi * authored_progress;
        }
        return parameters;
    }

    inline void configure_raw_human_foundational_walk(
        BipedGaitParameters& parameters) noexcept
    {
        // Single-source the shipping Raw Human demonstration envelope so its
        // phase/support selection and its emitted joint targets cannot drift
        // between production, diagnostics, and endpoint-authority tests.
        parameters.cadence_hz = 0.64f;
        parameters.step_length = 0.50f;
        parameters.swing_lift = 0.22f;
        parameters.transition_flex = 0.0f;
        parameters.smooth_contact_transitions = true;
    }

    [[nodiscard]] inline std::array<float, sim::action_count>
    biped_gait_teacher_action(const sim::Environment& environment,
        BipedGaitParameters parameters = {}) noexcept
    {
        auto action = balance_teacher_action(environment);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const float gait_seconds = locomotion_gait_seconds(environment);
        const float startup_blend = authored_gait_startup_blend(
            gait_seconds, parameters.startup_seconds);
        const sim::HumanSupportFeedback support_feedback =
            environment.human_support_feedback();
        float authored_sole_span{};
        float authored_leg_scale{};
        std::size_t authored_leg_count{};
        for (const std::size_t hip_index : { 0u, 2u })
        {
            const std::size_t knee_index = hip_index + 1u;
            if (knee_index >= rig.active_motor_count)
                continue;
            const sim::MotorConstraint& hip = rig.motors[hip_index];
            const sim::MotorConstraint& knee = rig.motors[knee_index];
            if (!hip.enabled || !knee.enabled
                || hip.pivot >= rig.nodes.size() || hip.c >= rig.nodes.size()
                || knee.pivot >= rig.nodes.size() || knee.c >= rig.nodes.size())
                continue;
            authored_leg_scale += length(
                    rig.nodes[hip.c] - rig.nodes[hip.pivot])
                + length(rig.nodes[knee.c] - rig.nodes[knee.pivot]);
            ++authored_leg_count;
        }
        if (authored_leg_count > 0u)
            authored_leg_scale /= static_cast<float>(authored_leg_count);
        if (sim::human_left_heel_node < rig.nodes.size()
            && sim::human_left_toe_node < rig.nodes.size()
            && sim::human_right_heel_node < rig.nodes.size()
            && sim::human_right_toe_node < rig.nodes.size())
        {
            authored_sole_span = 0.5f * (
                std::abs(rig.nodes[sim::human_left_toe_node].x
                    - rig.nodes[sim::human_left_heel_node].x)
                + std::abs(rig.nodes[sim::human_right_toe_node].x
                    - rig.nodes[sim::human_right_heel_node].x));
        }
        const auto solve_leg = [&](bool left, float phase)
        {
            phase = std::fmod(phase, 2.0f * pi);
            if (phase < 0.0f)
                phase += 2.0f * pi;
            float progress{};
            float x{};
            float lift{};
            const bool stance_phase = phase < pi;
            if (phase < pi)
            {
                progress = phase / pi;
                x = sagittal_step_x(parameters.step_length, progress, false,
                    parameters.smooth_contact_transitions,
                    parameters.endpoint_tangent);
            }
            else
            {
                progress = (phase - pi) / pi;
                x = sagittal_step_x(parameters.step_length, progress, true,
                    parameters.smooth_contact_transitions,
                    parameters.endpoint_tangent);
                const float lift_profile = std::sin(progress * pi);
                lift = parameters.swing_lift * lift_profile;
            }
            x *= parameters.direction;
            const std::size_t hip_index = left ? 0u : 2u;
            const std::size_t knee_index = left ? 1u : 3u;
            const sim::MotorConstraint& hip = rig.motors[hip_index];
            const sim::MotorConstraint& knee = rig.motors[knee_index];
            const float upper_length = length(
                rig.nodes[hip.c] - rig.nodes[hip.pivot]);
            const float lower_length = length(
                rig.nodes[knee.c] - rig.nodes[knee.pivot]);
            float stance_x = x + parameters.stance_center_x;
            const bool casual_human = rig.presentation_species()
                == sim::CreatureSpecies::human;
            const bool smooth_human_reach = casual_human
                && parameters.smooth_contact_transitions;
            const sim::HumanGaitCoordinatorState& gait_coordinator =
                environment.human_gait_coordinator();
            const bool coordinator_outgoing = left
                == (gait_coordinator.outgoing_side < 0);
            const float capture_endpoint_shift = smooth_human_reach
                ? human_capture_endpoint_shift(support_feedback,
                    environment.facing_direction(), authored_leg_scale,
                    authored_sole_span)
                : 0.0f;
            if (smooth_human_reach && gait_coordinator.active
                && gait_coordinator.stage
                    != sim::HumanGaitCoordinatorStage::settle)
            {
                const bool bilateral_purchase = gait_coordinator.stage
                    == sim::HumanGaitCoordinatorStage::purchase;
                const HumanCaptureSupportWeights support_weights =
                    human_capture_support_weights(support_feedback,
                        environment.left_supported(),
                        environment.right_supported(),
                        !bilateral_purchase
                            && gait_coordinator.outgoing_side < 0,
                        !bilateral_purchase
                            && gait_coordinator.outgoing_side > 0);
                const float support_weight = left
                    ? support_weights.left : support_weights.right;
                stance_x += support_weight * capture_endpoint_shift;
            }
            const bool coordinated_departure = gait_coordinator.active
                && coordinator_outgoing
                && gait_coordinator.stage
                    == sim::HumanGaitCoordinatorStage::departure;
            const bool coordinated_apex = gait_coordinator.active
                && coordinator_outgoing
                && gait_coordinator.stage
                    == sim::HumanGaitCoordinatorStage::apex;
            const bool coordinated_landing_approach = gait_coordinator.active
                && coordinator_outgoing
                && (coordinated_apex || gait_coordinator.stage
                        == sim::HumanGaitCoordinatorStage::descent
                    || gait_coordinator.stage
                        == sim::HumanGaitCoordinatorStage::flare
                    || gait_coordinator.stage
                        == sim::HumanGaitCoordinatorStage::landing);
            const bool coordinated_flare = gait_coordinator.active
                && coordinator_outgoing
                && gait_coordinator.stage
                    == sim::HumanGaitCoordinatorStage::flare;
            const bool coordinated_landing = gait_coordinator.active
                && coordinator_outgoing
                && gait_coordinator.stage
                    == sim::HumanGaitCoordinatorStage::landing;
            const float landing_alignment = human_landing_alignment_blend(
                gait_coordinator, left);
            const float terminal_sole_alignment =
                human_terminal_sole_alignment_blend(gait_coordinator, left);
            const float landing_completion = human_landing_completion_blend(
                gait_coordinator, left);
            const float flare_clearance = human_flare_clearance_target(
                gait_coordinator);
            float minimum_clearance{};
            float sole_velocity_x{};
            float sole_velocity_y{};
            bool physical_swing{};
            float physical_descent_blend{};
            float physical_landing_phase_blend{};
            float horizontal_landing_correction{};
            if (smooth_human_reach)
            {
                const auto gait_observation = environment.observation();
                const std::size_t support_index = left ? 20u : 21u;
                const std::size_t other_support_index = left ? 21u : 20u;
                const std::size_t clearance_index = left
                    ? sim::left_sole_clearance_observation
                    : sim::right_sole_clearance_observation;
                const std::size_t velocity_x_index = left
                    ? sim::left_sole_velocity_x_observation
                    : sim::right_sole_velocity_x_observation;
                const std::size_t velocity_y_index = left
                    ? sim::left_sole_velocity_y_observation
                    : sim::right_sole_velocity_y_observation;
                minimum_clearance = gait_observation[clearance_index] * 0.25f;
                sole_velocity_x = gait_observation[velocity_x_index] * 6.0f;
                sole_velocity_y = gait_observation[velocity_y_index] * 6.0f;
                physical_swing = gait_observation[support_index] < 0.5f
                    && gait_observation[other_support_index] >= 0.5f;
                if (physical_swing)
                {
                    const float phase_ready = stance_phase ? 1.0f : clamp(
                        (progress - 0.55f) / 0.20f, 0.0f, 1.0f);
                    const float phase_blend = phase_ready * phase_ready
                        * (3.0f - 2.0f * phase_ready);
                    // Descent is authored by the continuous leg phase. Making
                    // it depend on the sign of measured sole velocity toggles
                    // the flare target by roughly nine centimetres at every
                    // zero crossing and creates a self-sustaining bounce.
                    physical_descent_blend = phase_blend;
                    physical_landing_phase_blend = phase_blend;
                    if (coordinated_landing_approach)
                        horizontal_landing_correction =
                            human_flare_horizontal_correction(
                                sole_velocity_x,
                                coordinated_apex ? 1.0f : phase_blend);
                }
            }
            const float swing_profile = std::sin(progress * pi);
            if (smooth_human_reach && coordinated_departure)
            {
                // Shorten the ordinary two-link leg enough to unload the whole
                // heel/ball/toe plate before SWING is allowed to advance. This
                // remains a hip/knee target; it never edits a contact or node.
                const bool foundational_walk = rig.human_casual_gait_plan()
                    && environment.course_stage() == sim::CourseStage::uneven
                    && environment.gait_task() == sim::GaitTask::walk;
                const float shortening = foundational_walk
                    ? human_walk_loaded_departure_shortening(gait_coordinator,
                        support_feedback, left ? environment.right_supported()
                            : environment.left_supported(), upper_length + lower_length)
                    : human_departure_leg_shortening;
                lift = std::max(lift, shortening);
            }
            // Raw Human contact exchange must not retract the load-bearing leg.
            // Keep flex on the free half of the cycle, with the same exact zero
            // at both exchange boundaries as the swing lift. Other bipeds retain
            // their established endpoint-flex envelope.
            const float transition_flex = std::max(0.0f,
                parameters.transition_flex) * (smooth_human_reach
                    ? (stance_phase ? 0.0f : swing_profile * swing_profile)
                    : std::abs(std::cos(progress * pi)));
            const float lowering_blend = authored_gait_startup_blend(
                gait_seconds, parameters.stance_lowering_seconds);
            const float stance_height_ratio = std::lerp(1.0f,
                clamp(parameters.stance_height_ratio, 0.55f, 1.0f), lowering_blend);
            const Vec2 authored_target = rig.nodes[knee.c]
                - rig.nodes[hip.pivot];
            const float authored_reach = length(authored_target);
            // The saved root-to-ankle reach is already a finite, physically
            // attainable two-link pose. Preserve that radius throughout Raw
            // stance instead of applying the generic one-percent reserve, which
            // lifted the nominal support ankle by about four centimetres. Swing
            // lift/flex only shorten the free leg and cannot pole-vault stance.
            const float vertical_reach = smooth_human_reach
                ? std::sqrt(std::max(0.0f,
                    authored_reach * authored_reach - stance_x * stance_x))
                    * stance_height_ratio
                : casual_human
                    ? biped_stance_vertical_reach(stance_x,
                        upper_length + lower_length) * stance_height_ratio
                : parameters.leg_height;
            if (smooth_human_reach)
            {
                const float clearance_hold = !stance_phase
                    ? 1.0f - physical_descent_blend : 0.0f;
                if (physical_swing)
                    lift = std::max(lift,
                        parameters.swing_lift * clearance_hold);
                if (!stance_phase || physical_swing)
                {
                    const float target_clearance = coordinated_landing
                        ? std::lerp(flare_clearance, 0.005f,
                            landing_completion)
                        : coordinated_flare
                        ? flare_clearance
                        : coordinated_landing_approach
                        ? std::lerp(0.12f,
                            flare_clearance, landing_alignment)
                        : !stance_phase ? std::lerp(0.12f, 0.005f,
                            physical_descent_blend) : 0.005f;
                    const float target_vertical_velocity = coordinated_landing
                        ? std::lerp(human_flare_target_velocity_y, -0.30f,
                            landing_completion)
                        : coordinated_flare
                        ? human_flare_target_velocity_y
                        : coordinated_landing_approach
                        ? std::lerp(0.0f,
                            human_flare_target_velocity_y, landing_alignment)
                        : !stance_phase ? std::lerp(0.0f, -0.30f,
                            physical_descent_blend) : -0.20f;
                    const float vertical_feedback =
                        coordinated_landing_approach && physical_swing
                            && physical_landing_phase_blend > 0.0f
                        ? human_flare_vertical_lift(minimum_clearance,
                            sole_velocity_y, target_clearance,
                            target_vertical_velocity,
                            physical_landing_phase_blend)
                        : clamp(target_clearance - minimum_clearance
                                + 0.220f * (target_vertical_velocity
                                    - sole_velocity_y),
                            0.0f, 0.24f);
                    lift += vertical_feedback;
                }
            }
            const Vec2 nominal_cyclic_target{ stance_x,
                -vertical_reach + lift + transition_flex };
            Vec2 cyclic_target = nominal_cyclic_target;
            cyclic_target.x -= horizontal_landing_correction;
            if (length(cyclic_target) >= upper_length + lower_length - 0.001f)
                cyclic_target = nominal_cyclic_target;
            // Preserve the exact authored rest endpoint while the actuator
            // ramp is zero. Clamping the blended target to the locomotion
            // reserve first shortened both nominally planted legs even at
            // startup_blend==0, producing a bilateral flex/hop before policy
            // control. Only the cyclic endpoint needs the reach reserve.
            const Vec2 bounded_cyclic_target = smooth_human_reach
                ? cyclic_target : bounded_biped_leg_target(
                    cyclic_target, upper_length + lower_length);
            const Vec2 target = authored_target + startup_blend
                * (bounded_cyclic_target - authored_target);
            const Vec2 authored_upper = rig.nodes[hip.c]
                - rig.nodes[hip.pivot];
            const Vec2 authored_lower = rig.nodes[knee.c]
                - rig.nodes[knee.pivot];
            const float authored_cross = authored_upper.x * authored_lower.y
                - authored_upper.y * authored_lower.x;
            const float bend_direction = authored_cross >= 0.0f ? -1.0f : 1.0f;
            const TwoLinkSagittalSolution solution = solve_two_link_sagittal(
                upper_length, lower_length, target, bend_direction);
            if (!solution.valid)
                return;
            const Vec2 hip_reference = rig.nodes[hip.a]
                - rig.nodes[hip.pivot];
            const float hip_target = signed_angle(hip_reference, solution.upper);
            const float knee_target = signed_angle(-1.0f * solution.upper,
                solution.lower);
            action[hip_index] = motor_action_for_target_angle(hip, hip_target);
            action[knee_index] = motor_action_for_target_angle(knee, knee_target);

            const std::size_t ankle_index = left
                ? sim::human_left_ankle_action
                : sim::human_right_ankle_action;
            if (rig.presentation_species() == sim::CreatureSpecies::human
                && ankle_index < rig.active_motor_count
                && rig.motors[ankle_index].enabled)
            {
                const sim::MotorConstraint& ankle = rig.motors[ankle_index];
                const Vec2 authored_foot = normalized(
                    rig.nodes[ankle.c] - rig.nodes[ankle.pivot],
                    { parameters.direction, 0.0f });
                const float toe_up = stance_phase ? 0.0f
                    : 0.12f * std::sin(progress * pi);
                // The ankle motor drives ankle->ball, not the heel->ball sole
                // edge. A level sole therefore retains the authored downward
                // ankle offset; targeting a horizontal ankle->ball vector
                // lifts the complete rear-foot triangle and pole-vaults the
                // pelvis at every stance transition. Rotate that anatomical
                // vector only by the explicit swing toe-up command.
                const Vec2 cyclic_foot = normalized(
                    rotate(authored_foot, parameters.direction * toe_up),
                    authored_foot);
                const Vec2 desired_foot = normalized(authored_foot
                        + startup_blend * (cyclic_foot - authored_foot),
                    authored_foot);
                // Ankle actions are explicit policy outputs. The teacher asks
                // the rear-foot triangle to retain its level sole in stance and with a
                // small toe-up swing angle; no contact phase is consulted by
                // Environment and the passive toe hinge remains unpowered.
                Vec2 ankle_reference = -1.0f * solution.lower;
                Vec2 resolved_foot = desired_foot;
                float target_facing = 1.0f;
                const bool coordinated_landing_preparation =
                    terminal_sole_alignment > 0.0f;
                if ((stance_phase || coordinated_landing_preparation)
                    && smooth_human_reach)
                {
                    // A planted ankle must level its physical rear-foot from
                    // the shank that actually arrived at the floor. Referencing
                    // the planned IK shank lets hip/knee tracking lag rotate the
                    // complete heel/ball plate even though the ankle motor has
                    // reached its relative target. Sample the same heel-to-ball
                    // footprint against physical terrain and express the result
                    // only as this explicit ankle action; swing remains tied to
                    // the continuous planned shank and toe-up target above.
                    const auto body = environment.particles();
                    const std::uint16_t knee_node = ankle.a;
                    const std::uint16_t ankle_node = ankle.pivot;
                    const std::uint16_t heel_node = left
                        ? sim::human_left_heel_node : sim::human_right_heel_node;
                    const std::uint16_t ball_node = left
                        ? sim::human_left_ball_node : sim::human_right_ball_node;
                    if (knee_node < body.size() && ankle_node < body.size()
                        && heel_node < body.size() && ball_node < body.size())
                    {
                        const float facing = environment.facing_direction();
                        bool used_measured_plate{};
                        if (coordinated_landing_preparation)
                        {
                            const Vec2 actual_sole = body[ball_node].position
                                - body[heel_node].position;
                            const Vec2 desired_sole{
                                actual_sole.x,
                                environment.ground_height_at(
                                    body[ball_node].position.x)
                                    + sim::ground_contact_offset(true,
                                        body[ball_node].radius)
                                    - environment.ground_height_at(
                                        body[heel_node].position.x)
                                    - sim::ground_contact_offset(true,
                                        body[heel_node].radius) };
                            const Vec2 actual_driven =
                                body[ball_node].position
                                - body[ankle_node].position;
                            if (length(actual_sole) > 1.0e-5f
                                && length(desired_sole) > 1.0e-5f
                                && length(actual_driven) > 1.0e-5f)
                            {
                                const float plate_error = signed_angle(
                                    actual_sole, desired_sole);
                                ankle_reference = body[knee_node].position
                                    - body[ankle_node].position;
                                resolved_foot = rotate(actual_driven,
                                    plate_error);
                                target_facing = facing;
                                used_measured_plate = true;
                            }
                        }
                        if (!used_measured_plate)
                        {
                            const float heel_x = body[ankle_node].position.x
                                + facing * (rig.nodes[heel_node].x
                                    - rig.nodes[ankle_node].x);
                            const float ball_x = body[ankle_node].position.x
                                + facing * (rig.nodes[ball_node].x
                                    - rig.nodes[ankle_node].x);
                            const Vec2 authored_sole = rig.nodes[ball_node]
                                - rig.nodes[heel_node];
                            const float foot_from_sole = signed_angle(
                                authored_sole, authored_foot);
                            const Vec2 ground_sole = normalized(Vec2{
                                    ball_x - heel_x,
                                    environment.ground_height_at(ball_x)
                                        - environment.ground_height_at(heel_x) },
                                { facing, 0.0f });
                            ankle_reference = body[knee_node].position
                                - body[ankle_node].position;
                            resolved_foot = normalized(rotate(ground_sole,
                                    facing * foot_from_sole),
                                desired_foot);
                            target_facing = facing;
                        }
                    }
                }
                const float planned_ankle_target = signed_angle(
                    -1.0f * solution.lower, desired_foot);
                const float terrain_ankle_target = signed_angle(
                    ankle_reference, resolved_foot) * target_facing;
                const float terrain_blend = stance_phase
                    ? 1.0f : terminal_sole_alignment;
                action[ankle_index] = std::lerp(
                    motor_action_for_target_angle(ankle,
                        planned_ankle_target),
                    motor_action_for_target_angle(ankle,
                        terrain_ankle_target),
                    terrain_blend);
            }
            const std::size_t toe_index = left
                ? sim::human_left_toe_action : sim::human_right_toe_action;
            if (rig.presentation_species() == sim::CreatureSpecies::human
                && toe_index < rig.active_motor_count
                && rig.motors[toe_index].enabled)
            {
                const sim::HumanGaitCoordinatorState& coordinator =
                    environment.human_gait_coordinator();
                const bool outgoing = left
                    == (coordinator.outgoing_side < 0);
                const auto terrain_toe_action = [&]() noexcept
                {
                    float result = -0.14f;
                    const auto body = environment.particles();
                    const std::uint16_t heel_node = left
                        ? sim::human_left_heel_node
                        : sim::human_right_heel_node;
                    const std::uint16_t ball_node = left
                        ? sim::human_left_ball_node
                        : sim::human_right_ball_node;
                    const std::uint16_t toe_node = left
                        ? sim::human_left_toe_node
                        : sim::human_right_toe_node;
                    if (heel_node < body.size() && ball_node < body.size()
                        && toe_node < body.size()
                        && toe_node < rig.nodes.size()
                        && toe_node < rig.radii.size()
                        && ball_node < rig.radii.size())
                    {
                        const float facing = environment.facing_direction();
                        const float toe_x = body[ball_node].position.x
                            + facing * std::abs(rig.nodes[toe_node].x
                                - rig.nodes[ball_node].x);
                        const Vec2 reference = body[heel_node].position
                            - body[ball_node].position;
                        const Vec2 ground_tangent = normalized(Vec2{
                                toe_x - body[ball_node].position.x,
                                environment.ground_height_at(toe_x)
                                    - environment.ground_height_at(
                                        body[ball_node].position.x)
                                    + sim::ground_contact_offset(true,
                                        rig.radii[toe_node])
                                    - sim::ground_contact_offset(true,
                                        rig.radii[ball_node]) },
                            { facing, 0.0f });
                        const Vec2 current_span = body[toe_node].position
                            - body[ball_node].position;
                        const Vec2 relative_step_displacement =
                            (body[toe_node].position
                                - body[toe_node].previous)
                            - (body[ball_node].position
                                - body[ball_node].previous);
                        const float plane_correction =
                            finite_sole_alignment_correction(current_span,
                                ground_tangent, relative_step_displacement);
                        const Vec2 commanded_span = rotate(current_span,
                            plane_correction);
                        const float commanded_target = signed_angle(
                            reference, commanded_span) * facing;
                        result = motor_action_for_target_angle(
                            rig.motors[toe_index], commanded_target);
                    }
                    return result;
                };
                float toe_action = -0.14f;
                if (!coordinator.active
                    || coordinator.stage
                        == sim::HumanGaitCoordinatorStage::settle)
                    toe_action = 0.0f;
                else if (outgoing)
                {
                    switch (coordinator.stage)
                    {
                    case sim::HumanGaitCoordinatorStage::departure:
                        toe_action = 0.34f;
                        break;
                    case sim::HumanGaitCoordinatorStage::swing:
                    case sim::HumanGaitCoordinatorStage::apex:
                        toe_action = std::lerp(0.34f,
                            terrain_toe_action(), terminal_sole_alignment);
                        break;
                    case sim::HumanGaitCoordinatorStage::descent:
                    case sim::HumanGaitCoordinatorStage::flare:
                    case sim::HumanGaitCoordinatorStage::landing:
                        // Blend the distal edge toward the sampled ground from
                        // the observed descent state, giving the finite toe
                        // actuator time to converge before FLARE evaluates its
                        // all-three-node plane. FLARE/LANDING retain the exact
                        // target until physical purchase.
                        toe_action = std::lerp(0.34f,
                            terrain_toe_action(), terminal_sole_alignment);
                        break;
                    case sim::HumanGaitCoordinatorStage::purchase:
                        // Preserve the same measured terrain target until the
                        // coordinator has proved a sustained two-sole purchase
                        // and actually swaps sides. Dropping to the generic
                        // stance curl here lifted the just-landed distal plate
                        // before the physical purchase window could complete.
                        toe_action = terrain_toe_action();
                        break;
                    case sim::HumanGaitCoordinatorStage::settle:
                    case sim::HumanGaitCoordinatorStage::count:
                        toe_action = 0.0f;
                        break;
                    }
                }
                action[toe_index] = toe_action * startup_blend;
            }
        };
        const float phase = std::isfinite(parameters.controller_phase_radians)
            ? parameters.controller_phase_radians
            : gait_seconds * 2.0f * pi * parameters.cadence_hz
                + parameters.phase_offset;
        solve_leg(true, phase);
        solve_leg(false, phase + pi);
        std::array<std::size_t, sim::anatomy_action_count> shoulder_motors{};
        std::array<std::size_t, sim::anatomy_action_count> elbow_motors{};
        std::size_t arm_chain_count{};
        for (std::size_t index = 0; index < rig.active_motor_count
            && arm_chain_count < shoulder_motors.size(); ++index)
        {
            const sim::MotorConstraint& shoulder = rig.motors[index];
            if (motor_drives_support_branch(rig, shoulder)
                || shoulder.a != rig.torso_node
                || shoulder.pivot >= rig.nodes.size()
                || shoulder.c >= rig.nodes.size())
                continue;
            for (std::size_t distal = 0; distal < rig.active_motor_count; ++distal)
            {
                const sim::MotorConstraint& elbow = rig.motors[distal];
                if (distal == index || motor_drives_support_branch(rig, elbow)
                    || elbow.pivot != shoulder.c
                    || elbow.c >= rig.nodes.size())
                    continue;
                shoulder_motors[arm_chain_count] = index;
                elbow_motors[arm_chain_count] = distal;
                ++arm_chain_count;
                break;
            }
        }
        for (std::size_t chain = 0; chain < arm_chain_count; ++chain)
        {
            const std::size_t shoulder_index = shoulder_motors[chain];
            const std::size_t elbow_index = elbow_motors[chain];
            const sim::MotorConstraint& shoulder = rig.motors[shoulder_index];
            const sim::MotorConstraint& elbow = rig.motors[elbow_index];
            const float upper_length = length(
                rig.nodes[shoulder.c] - rig.nodes[shoulder.pivot]);
            const float lower_length = length(
                rig.nodes[elbow.c] - rig.nodes[elbow.pivot]);
            const float chain_length = upper_length + lower_length;
            // Motor topology owns left/right identity in the same order as the
            // paired support chains. Shoulder screen-X can cross in a relaxed
            // side view and must never swap the arm-to-leg phase assignment.
            const float same_side_leg_phase = phase
                + ((chain & 1u) == 0u ? 0.0f : pi);
            const Vec2 authored_endpoint = rig.nodes[elbow.c]
                - rig.nodes[shoulder.pivot];
            const bool casual_human = rig.presentation_species()
                == sim::CreatureSpecies::human;
            const Vec2 gait_origin = casual_human
                ? human_casual_arm_rest_target(chain_length)
                : authored_endpoint;
            Vec2 cyclic_target = gait_origin;
            if (casual_human)
            {
                cyclic_target.x += human_contralateral_arm_swing_offset(
                    same_side_leg_phase, chain_length * parameters.arm_swing_ratio,
                    parameters.direction);
            }
            else
            {
                cyclic_target = authored_opposed_swing_target(
                    gait_origin, same_side_leg_phase + pi,
                    chain_length * 0.040f, 0.0f, parameters.direction);
            }
            const Vec2 desired_target = authored_endpoint
                + startup_blend * (cyclic_target - authored_endpoint);
            const Vec2 target = bounded_biped_leg_target(
                desired_target, chain_length);
            const Vec2 authored_upper = rig.nodes[shoulder.c]
                - rig.nodes[shoulder.pivot];
            const Vec2 authored_lower = rig.nodes[elbow.c]
                - rig.nodes[elbow.pivot];
            const float authored_cross = authored_upper.x * authored_lower.y
                - authored_upper.y * authored_lower.x;
            const float bend_direction = authored_cross >= 0.0f ? -1.0f : 1.0f;
            const TwoLinkSagittalSolution solution = solve_two_link_sagittal(
                upper_length, lower_length, target, bend_direction);
            if (!solution.valid)
                continue;
            const Vec2 shoulder_reference = rig.nodes[shoulder.a]
                - rig.nodes[shoulder.pivot];
            action[shoulder_index] = motor_action_for_target_angle(shoulder,
                signed_angle(shoulder_reference, solution.upper));
            action[elbow_index] = motor_action_for_target_angle(elbow,
                signed_angle(-1.0f * solution.upper, solution.lower));
        }
        // Leg and arm IK above already establishes authored bend direction,
        // opposed phase, and the saved side-rest origin. Return that solved cycle
        // directly; effective_policy_action applies the generic bilateral filter
        // once at the final composition boundary.
        for (float& value : action)
            value = clamp(value, -1.0f, 1.0f);
        return action;
    }

    struct RawHumanSupportBalanceApplication
    {
        std::array<float, sim::action_count> action{};
        Vec2 stance_endpoint_before{};
        Vec2 stance_endpoint_after{};
        float endpoint_error{};
        float signed_balance{};
        int support_side{};
        bool applied{};
    };

    [[nodiscard]] inline int raw_human_balance_support_side(
        const sim::Environment& environment,
        const BipedGaitParameters& parameters,
        bool require_raw_policy = true) noexcept
    {
        static_cast<void>(parameters);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        if (!rig.human_casual_gait_plan()
            || rig.root_node >= rig.nodes.size()
            || rig.torso_node >= rig.nodes.size()
            || (require_raw_policy && !environment.raw_policy_audit()))
            return 0;

        const bool left = environment.left_supported();
        const bool right = environment.right_supported();
        if (left != right)
            return left ? -1 : 1;
        // During double support there is no contact-observable stance side.
        // Choosing one from the private gait clock made equivalent scrubbed
        // policy inputs map to opposing ankle targets. Leave this loose until
        // one sole actually unloads; airborne and double-support states add no
        // balance target.
        return 0;
    }

    [[nodiscard]] inline RawHumanSupportBalanceApplication
    apply_raw_human_support_balance(
        const sim::Environment& environment,
        const BipedGaitParameters& parameters,
        const std::array<float, sim::action_count>& base_action,
        bool require_raw_policy = true) noexcept
    {
        RawHumanSupportBalanceApplication result{};
        result.action = base_action;
        const sim::CreatureBlueprint& rig = environment.blueprint();
        result.support_side = raw_human_balance_support_side(
            environment, parameters, require_raw_policy);
        if (result.support_side == 0
            || !rig.human_casual_gait_plan()
            || rig.root_node >= rig.nodes.size()
            || rig.torso_node >= rig.nodes.size())
            return result;

        const auto observation = environment.observation();
        const Vec2 authored_body = normalized(
            rig.nodes[rig.torso_node] - rig.nodes[rig.root_node],
            { 0.0f, 1.0f });
        // Positive means the torso is falling behind its authored local-facing
        // attitude. Both signals are explicit observations available to the
        // retained network; this is a curriculum action target, never a body
        // transform, force, or runtime servo.
        constexpr float angle_gain = 0.48f;
        constexpr float rate_gain = 0.62f;
        constexpr float maximum_correction = 0.34f;
        result.signed_balance = clamp(
            (authored_body.x - observation[0]) * angle_gain
                + observation[sim::torso_turn_rate_observation] * rate_gain,
            -maximum_correction, maximum_correction);
        const bool left = result.support_side < 0;
        const std::size_t hip_index = left ? 0u : 2u;
        const std::size_t knee_index = hip_index + 1u;
        const std::size_t ankle_index = left
            ? sim::human_left_ankle_action : sim::human_right_ankle_action;
        if (knee_index >= rig.active_motor_count
            || ankle_index >= rig.active_motor_count
            || !rig.motors[hip_index].enabled
            || !rig.motors[knee_index].enabled
            || !rig.motors[ankle_index].enabled)
            return result;

        const auto anatomy = [](const std::array<float, sim::action_count>& action)
        {
            return std::span<const float, sim::anatomy_action_count>{
                action.data(), sim::anatomy_action_count };
        };
        result.stance_endpoint_before = sim::requested_leg_endpoint(
            rig, left, anatomy(result.action), environment.facing_direction());

        // A nonsingular planar hip/knee pair consumes both degrees of freedom
        // positioning the ankle, so it has no nonzero endpoint-preserving
        // attitude null space. Keep those two policy targets byte-for-byte and
        // put feedback only on the explicit downstream ankle motor. The real
        // planted sole may resist that torque through finite contact impulses;
        // this helper never edits a particle, contact, root, or course state.
        // The two sagittal chains use reflected support-side reactions around
        // the shared pelvis. Map the canonical observation error once at the
        // selected physical stance ankle.
        const float stance_action_sign = static_cast<float>(
            result.support_side);
        result.action[ankle_index] = clamp(
            result.action[ankle_index]
                + stance_action_sign * result.signed_balance,
            -1.0f, 1.0f);
        result.stance_endpoint_after = sim::requested_leg_endpoint(
            rig, left, anatomy(result.action), environment.facing_direction());
        result.endpoint_error = length(
            result.stance_endpoint_after - result.stance_endpoint_before);
        result.applied = true;
        return result;
    }

    [[nodiscard]] inline std::array<float, sim::action_count>
    raw_human_torso_joint_balance_correction(
        const sim::Environment& environment) noexcept
    {
        BipedGaitParameters parameters = anatomy_scaled_foundational_gait(
            environment.blueprint(), environment.gait_task(),
            environment.elapsed_seconds());
        configure_raw_human_foundational_walk(parameters);
        const std::array<float, sim::action_count> zero{};
        const auto applied = apply_raw_human_support_balance(
            environment, parameters, zero);
        return applied.action;
    }

    [[nodiscard]] inline float human_walk_stance_hip_trim(
        const sim::HumanGaitCoordinatorState& gait,
        const sim::HumanSupportFeedback& support, int stance_side,
        float local_pitch_sine, float local_pitch_rate) noexcept
    {
        if (!gait.active || gait.completed_handoffs == 0u
            || (stance_side != -1 && stance_side != 1) || !support.valid
            || !std::isfinite(support.load_asymmetry)
            || !std::isfinite(support.total_normal_impulse)
            || !std::isfinite(local_pitch_sine) || !std::isfinite(local_pitch_rate)
            || !std::isfinite(gait.stage_seconds) || support.total_normal_impulse <= 0.001f)
            return 0.0f;
        const float stance_share = 0.5f * (1.0f
            + static_cast<float>(stance_side) * support.load_asymmetry);
        const float load_blend = clamp((stance_share - 0.56f) / 0.24f, 0.0f, 1.0f);
        const float onset = gait.stage == sim::HumanGaitCoordinatorStage::departure
            ? clamp(gait.stage_seconds / 0.12f, 0.0f, 1.0f) : 1.0f;
        // Both sagittal hips have the same measured torque-to-forward-pitch
        // sign. Use a small, rate-damped motor request on the stance side;
        // the reciprocal joint and ground reaction own the actual response.
        constexpr float forward_lean = 0.065f;
        const float demand = -0.76f * (local_pitch_sine - forward_lean)
            - 0.072f * local_pitch_rate;
        return clamp(demand, -0.20f, 0.20f) * load_blend * onset;
    }

    [[nodiscard]] inline int human_walk_dominant_support_side(
        const sim::HumanSupportFeedback& support, bool left, bool right) noexcept
    {
        if (!support.valid || !std::isfinite(support.load_asymmetry))
            return 0;
        if (left != right)
            return left ? -1 : 1;
        // A tiny toe reaction can keep the trailing foot's contact bit on
        // throughout a transfer. Prefer the measured load, not the private
        // gait clock, and leave genuine double support unselected.
        if (!left || std::abs(support.load_asymmetry) < 0.55f)
            return 0;
        return support.load_asymmetry < 0.0f ? -1 : 1;
    }

    [[nodiscard]] inline std::array<float, sim::action_count>
    human_walk_heel_preparation(const sim::Environment& environment,
        std::array<float, sim::action_count> action) noexcept
    {
        const auto& rig = environment.blueprint();
        const auto& gait = environment.human_gait_coordinator();
        if (!rig.human_casual_gait_plan()
            || environment.course_stage() != sim::CourseStage::uneven
            || environment.gait_task() != sim::GaitTask::walk || !gait.active
            || gait.stage < sim::HumanGaitCoordinatorStage::swing
            || gait.stage > sim::HumanGaitCoordinatorStage::landing)
            return action;

        const bool left = gait.outgoing_side < 0;
        const auto heel = left ? sim::human_left_heel_node : sim::human_right_heel_node;
        const auto ball = left ? sim::human_left_ball_node : sim::human_right_ball_node;
        const auto ankle_slot = left ? sim::human_left_ankle_action : sim::human_right_ankle_action;
        const auto& ankle = rig.motors[ankle_slot];
        const auto body = environment.particles();
        const float progress = clamp((gait.phase - (left ? pi : 0.0f)) / pi, 0.0f, 1.0f);
        constexpr float heel_preparation_start = 0.35f;
        if (progress <= heel_preparation_start || body[heel].grounded)
            return action;

        const auto support_height = [&](std::uint16_t node)
        {
            const auto& particle = body[node];
            const float x = particle.position.x;
            return environment.ground_height_at(x)
                + sim::ground_contact_offset(true, particle.radius)
                - sim::support_contact_soil_sink(environment.terrain_firmness_at(x));
        };
        const Vec2 sole = body[ball].position - body[heel].position;
        const Vec2 flat{ sole.x, support_height(ball) - support_height(heel) };
        const float facing = environment.facing_direction();
        const Vec2 reference = body[ankle.a].position - body[ankle.pivot].position;
        const Vec2 current = body[ankle.c].position - body[ankle.pivot].position;
        constexpr float heel_lead_radians = 0.12f;
        const Vec2 desired = rotate(flat, facing * heel_lead_radians);
        const float angle = signed_angle(reference,
            rotate(current, signed_angle(sole, desired))) * facing;
        const float blend = clamp((progress - heel_preparation_start) / 0.13f, 0.0f, 1.0f);
        action[ankle_slot] = std::lerp(action[ankle_slot],
            motor_action_for_target_angle(ankle, angle), blend);
        // Start unflexing the swing knee while the sole is still unloaded.
        // Don't drive past authored neutral or override a grounded heel.
        const std::size_t knee_slot = left ? 1u : 3u;
        const float knee = action[knee_slot];
        action[knee_slot] = std::lerp(knee, std::max(knee, std::min(0.0f, knee + 0.40f)),
            blend * clamp((progress - 0.52f) / 0.18f, 0.0f, 1.0f));
        return action;
    }

    [[nodiscard]] inline std::array<float, sim::action_count>
    raw_human_foundational_teacher_action(
        const sim::Environment& environment,
        BipedGaitParameters parameters,
        float target_response = 1.0f,
        float joint_damping = 0.0f) noexcept
    {
        parameters.smooth_contact_transitions = true;
        auto action = biped_gait_teacher_action(environment, parameters);
        const auto observation = environment.observation();
        target_response = clamp(target_response, 0.0f, 1.0f);
        joint_damping = clamp(joint_damping, 0.0f, 1.0f);
        // The raw curriculum is an observable joint-space PD demonstration.
        // It cannot translate a body node: the target, current joint angle and
        // angular rate are all inputs available to the feed-forward policy,
        // and retained raw inference never calls this helper. Torso recovery
        // is deliberately not blended toward the magnitude-only static balance
        // teacher: the signed correction below is the sole attitude response,
        // so opposite falls cannot request the same pose-return action.
        for (std::size_t index = 0;
            index < environment.blueprint().active_motor_count; ++index)
        {
            const float current = observation[
                sim::joint_angle_observation_index(index)];
            const float speed = observation[
                sim::joint_velocity_observation_index(index)];
            action[index] = clamp(current
                    + target_response * (action[index] - current)
                    - joint_damping * speed,
                -1.0f, 1.0f);
        }
        const auto balanced = apply_raw_human_support_balance(environment, parameters, action, false);
        action = balanced.action;
        if (environment.course_stage() == sim::CourseStage::uneven
            && environment.gait_task() == sim::GaitTask::walk
            && !environment.non_foot_grounded())
        {
            const auto& support = environment.human_support_feedback();
            const int side = human_walk_dominant_support_side(support,
                environment.left_supported(), environment.right_supported());
            if (side != 0)
            {
                const std::size_t stance_hip = side < 0 ? 0u : 2u;
                action[stance_hip] = clamp(action[stance_hip]
                    + human_walk_stance_hip_trim(environment.human_gait_coordinator(),
                        support, side, observation[0],
                        -6.0f * observation[sim::torso_turn_rate_observation]), -1.0f, 1.0f);
            }
        }
        return human_walk_heel_preparation(environment, action);
    }

    [[nodiscard]] inline std::array<float, sim::action_count>
    avian_gait_teacher_action(const sim::Environment& environment,
        float direction, float cadence_hz) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        if (!rig.avian_gait() || rig.active_motor_count < 4u)
            return balance_teacher_action(environment);

        float total_chain_length{};
        std::size_t valid_chains{};
        for (const std::size_t hip_index : { 0u, 2u })
        {
            const sim::MotorConstraint& hip = rig.motors[hip_index];
            const sim::MotorConstraint& knee = rig.motors[hip_index + 1u];
            if (hip.pivot >= rig.nodes.size() || hip.c >= rig.nodes.size()
                || knee.pivot >= rig.nodes.size() || knee.c >= rig.nodes.size())
                continue;
            total_chain_length += length(
                rig.nodes[hip.c] - rig.nodes[hip.pivot])
                + length(rig.nodes[knee.c] - rig.nodes[knee.pivot]);
            ++valid_chains;
        }
        if (valid_chains == 0u)
            return balance_teacher_action(environment);

        const float chain_length = total_chain_length
            / static_cast<float>(valid_chains);
        // Avian legs use the same physical plant/swing IK contract as paired
        // legs, but every dimension comes from their short authored chains.
        // Reusing humanoid minimums made the feet cross the entire body and
        // flung the trunk; preserving an authored rest endpoint as the stride
        // centre instead produced a double-support shuffle.
        BipedGaitParameters parameters{};
        parameters.cadence_hz = clamp(cadence_hz * 1.30f, 1.55f, 1.74f);
        parameters.step_length = chain_length * 0.699f;
        parameters.swing_lift = chain_length * 0.28f;
        // The planted target must preserve the authored root-to-talon reach.
        // Shortening it to 0.88 raised both supports and created false skating.
        parameters.leg_height = chain_length * 0.98f;
        parameters.direction = direction < 0.0f ? -1.0f : 1.0f;
        parameters.stance_center_x = 0.5f * (
            (rig.nodes[rig.left_contact_node].x
                - rig.nodes[rig.motors[0].pivot].x)
            + (rig.nodes[rig.right_contact_node].x
                - rig.nodes[rig.motors[2].pivot].x));
        // The authored chicken rests with its first support behind the root and
        // its second support ahead. Match that physical stance on the first
        // sample instead of commanding both legs through an immediate crossing.
        parameters.phase_offset = pi;
        return biped_gait_teacher_action(environment, parameters);
    }

    [[nodiscard]] inline std::array<float, sim::action_count>
    monoped_gait_teacher_action(const sim::Environment& environment,
        float direction, float cadence_hz) noexcept
    {
        auto action = balance_teacher_action(environment);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        if (!rig.monopedal_gait() || rig.active_motor_count < 4u)
            return action;

        const sim::MotorConstraint& hip = rig.motors[0];
        const sim::MotorConstraint& knee = rig.motors[1];
        if (hip.a >= rig.nodes.size() || hip.pivot >= rig.nodes.size()
            || hip.c >= rig.nodes.size()
            || knee.pivot >= rig.nodes.size() || knee.c >= rig.nodes.size())
            return action;

        const float gait_seconds = locomotion_gait_seconds(environment);
        const float phase = gait_seconds * 2.0f * pi * cadence_hz;
        const float cycle = std::sin(phase);
        const float compression = std::max(0.0f, -cycle);
        const float extension = std::max(0.0f, cycle);
        const float startup = clamp((gait_seconds - 0.30f) / 0.45f, 0.0f, 1.0f);

        // A monoped is not a degenerate biped. Drive small offsets around its
        // calibrated authored stance so both foot plates begin planted and the
        // single knee compresses before it extends. The forward set is needed
        // because a symmetric rocker has no preferred travel direction under
        // finite Coulomb traction and reproducibly walks the authored foot
        // backward. This remains a joint target: the root receives no travel,
        // anchor, or course displacement. Solving a new two-link pose at t=0
        // changed the IK branch immediately and pole-vaulted the body.
        const float hip_target = rig.rest_joint_angle(0u)
            + startup * direction * (0.34f + 0.26f * cycle);
        const float knee_target = rig.rest_joint_angle(1u)
            + startup * (0.46f * compression - 0.180f * extension);
        action[0] = motor_action_for_target_angle(hip, hip_target);
        action[1] = motor_action_for_target_angle(knee, knee_target);

        // The paired plates are the heel/toe of one physical foot, not two
        // independent legs. A visible rocker transfer must unload one plate
        // before the opposite edge can establish the next supported hop.
        const float rocker = startup * direction * cycle * 0.145f;
        action[2] = motor_action_for_target_angle(rig.motors[2],
            rig.rest_joint_angle(2u) + rocker);
        action[3] = motor_action_for_target_angle(rig.motors[3],
            rig.rest_joint_angle(3u) - rocker);
        return action;
    }

    [[nodiscard]] inline std::array<float, sim::action_count> raw_walking_teacher_action(
        const sim::Environment& environment) noexcept
    {
        auto action = balance_teacher_action(environment);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const locomotion::Plan movement = current_locomotion_plan(environment);
        // Turning is a planted hold, but braking begins while the rig still has
        // travel momentum. Continue a short opposed support transfer until the
        // shuttle reaches its backing threshold; dropping straight to static
        // balance at the boundary lets the torso roll over the planted boots.
        const bool shuttle_braking = environment.shuttle_enabled()
            && environment.shuttle_phase() == sim::ShuttlePhase::braking;
        const float local_direction = shuttle_braking
            ? -environment.facing_direction()
            : movement.direction * environment.facing_direction();
        if (movement.intent == locomotion::Intent::hold && !shuttle_braking)
        {
            if (rig.human_casual_gait_plan() && environment.shuttle_enabled()
                && (environment.shuttle_phase() == sim::ShuttlePhase::turning
                    || environment.shuttle_phase() == sim::ShuttlePhase::settling))
                settle_manipulator_actions_to_rest(rig, action, 1.0f);
            return action;
        }
        if (rig.avian_gait())
            return avian_gait_teacher_action(environment,
                local_direction, movement.cadence_hz);
        if (!rig.paired_leg_chains())
        {
            if (rig.monopedal_gait())
            {
                const float cadence = movement.aquatic
                    ? movement.cadence_hz
                    : sim::authored_foundational_gait_cadence_hz(rig);
                return monoped_gait_teacher_action(environment,
                    local_direction, cadence);
            }
            if (rig.support_seed_count() < 4u)
                return action;
            // Four-support rigs can enter a deterministic launch while every
            // limb continues its normal gait. Reduce stride authority while the
            // body is both overspeed and losing uprightness, but keep the support
            // transfer active so the brake cannot strand the rig in a static pose.
            const float directed_speed = environment.forward_speed()
                * local_direction;
            const bool four_support_overspeed_recovery =
                rig.support_seed_count() == 4u && directed_speed > 1.90f
                && environment.uprightness() < 0.92f;

            float rest_support_height = std::numeric_limits<float>::infinity();
            for (std::size_t node = 0; node < rig.nodes.size(); ++node)
            {
                if (rig.is_support_seed(node))
                    rest_support_height = std::min(rest_support_height,
                        rig.nodes[node].y);
            }
            const float root_clearance = rig.root_node < rig.nodes.size()
                && std::isfinite(rest_support_height)
                ? rig.nodes[rig.root_node].y - rest_support_height
                : 2.0f;
            const bool six_supports = rig.support_seed_count() >= 6u;
            const float anatomy_scaled_amplitude = std::clamp(
                root_clearance * (six_supports ? 1.55f : 0.79f),
                six_supports ? 0.95f : 0.69f,
                six_supports ? 1.42f : 0.79f);
            const float topology_cadence_hz = six_supports
                ? sim::authored_foundational_gait_cadence_hz(rig)
                : std::min(sim::authored_foundational_gait_cadence_hz(rig), 1.12f);
            const bool aquatic = movement.aquatic;

            const MultiSupportTeacherParameters multi_parameters{
                aquatic ? movement.cadence_hz : topology_cadence_hz,
                four_support_overspeed_recovery
                    ? anatomy_scaled_amplitude * 0.68f
                    : anatomy_scaled_amplitude
                        * (aquatic ? movement.intent == locomotion::Intent::swim
                            ? 0.82f : 0.68f : 1.0f),
                pi * 1.5f,
                aquatic ? 0.04f : six_supports ? 0.08f : 0.0f,
                aquatic ? 0.52f : six_supports ? 0.27f : 0.28f
            };
            return rig_has_driven_two_link_support_chains(rig)
                ? multi_support_two_link_teacher_action(environment, multi_parameters)
                : multi_support_teacher_action(environment, multi_parameters);
        }

        const float phase = locomotion_gait_seconds(environment) * 2.0f * pi
            * movement.cadence_hz;
        const float swing = std::sin(phase);
        const float directed_swing = swing * local_direction;
        const float left_lift = std::max(0.0f, swing);
        const float right_lift = std::max(0.0f, -swing);

        if (movement.intent == locomotion::Intent::crawl
            && environment.course_stage() != sim::CourseStage::uneven)
        {
            action[0] = clamp(action[0] - 0.24f + 0.20f * directed_swing,
                -0.82f, 0.82f);
            action[1] = clamp(action[1] + 0.58f + 0.18f * left_lift,
                -0.90f, 0.90f);
            action[2] = clamp(action[2] + 0.24f - 0.20f * directed_swing,
                -0.82f, 0.82f);
            action[3] = clamp(action[3] - 0.58f - 0.18f * right_lift,
                -0.90f, 0.90f);
            std::array<std::size_t, 2> crawl_shoulders{};
            std::array<std::size_t, 2> crawl_elbows{};
            std::size_t crawl_arm_count{};
            for (std::size_t shoulder_index = 0;
                shoulder_index < rig.active_motor_count
                    && crawl_arm_count < crawl_shoulders.size(); ++shoulder_index)
            {
                const sim::MotorConstraint& shoulder = rig.motors[shoulder_index];
                if (motor_drives_support_branch(rig, shoulder)
                    || shoulder.a != rig.torso_node)
                    continue;
                for (std::size_t elbow_index = 0;
                    elbow_index < rig.active_motor_count; ++elbow_index)
                {
                    const sim::MotorConstraint& elbow = rig.motors[elbow_index];
                    if (elbow_index == shoulder_index
                        || motor_drives_support_branch(rig, elbow)
                        || elbow.pivot != shoulder.c)
                        continue;
                    crawl_shoulders[crawl_arm_count] = shoulder_index;
                    crawl_elbows[crawl_arm_count] = elbow_index;
                    ++crawl_arm_count;
                    break;
                }
            }
            if (crawl_arm_count == 2u)
            {
                action[crawl_shoulders[0]] = clamp(action[crawl_shoulders[0]]
                    - 0.34f * directed_swing, -0.70f, 0.70f);
                action[crawl_elbows[0]] = clamp(action[crawl_elbows[0]]
                    + 0.18f * left_lift, -0.55f, 0.55f);
                action[crawl_shoulders[1]] = clamp(action[crawl_shoulders[1]]
                    + 0.34f * directed_swing, -0.70f, 0.70f);
                action[crawl_elbows[1]] = clamp(action[crawl_elbows[1]]
                    - 0.18f * right_lift, -0.55f, 0.55f);
            }
            return action;
        }

        const bool foundational_walk = environment.course_stage()
                == sim::CourseStage::uneven
            || environment.course_stage() == sim::CourseStage::shuttle;
        const auto body = environment.particles();
        const float root_x = rig.root_node < body.size()
            ? body[rig.root_node].position.x : 0.0f;
        const float gait_probe_x = root_x + local_direction * 0.75f;
        const bool yielding_ground = foundational_walk && !movement.aquatic
            && (std::min(environment.terrain_firmness_at(root_x),
                    environment.terrain_firmness_at(gait_probe_x)) < 0.58f
                || std::max(environment.terrain_looseness_at(root_x),
                    environment.terrain_looseness_at(gait_probe_x)) > 0.45f);
        const sim::GaitTask physical_gait_task = yielding_ground
            ? sim::GaitTask::walk : environment.gait_task();
        BipedGaitParameters biped_parameters = foundational_walk
            ? anatomy_scaled_foundational_gait(rig, physical_gait_task,
                environment.elapsed_seconds())
            : BipedGaitParameters{
                movement.intent == locomotion::Intent::flee ? 1.40f
                    : movement.intent == locomotion::Intent::recover ? 0.90f : 1.20f,
                movement.step_up ? 0.40f : 0.50f,
                movement.step_up ? 0.66f : 0.50f,
                movement.step_up ? 2.20f : 2.30f,
                local_direction
            };

        biped_parameters.direction = local_direction;
        if (foundational_walk && physical_gait_task == sim::GaitTask::walk
            && rig.presentation_species() == sim::CreatureSpecies::human)
        {
            // One bounded, contact-qualified demonstration teaches the no-rail
            // Human on both sides of the authority boundary. Guidance changes
            // whether this target executes, never which target imitation saw:
            // raw inference receives no phase controller or teacher call.
            // Match the phase clock to the mandatory 1.0 action-unit/second
            // Raw actuator slew. At 0.84 Hz the swing target reverses before
            // the explicit hip/ankle commands reach it, so the foot touches
            // down with forward velocity and receives a backward impulse.
            configure_raw_human_foundational_walk(biped_parameters);
            biped_parameters.controller_phase_radians =
                environment.human_gait_coordinator().phase;
            // Retain the anatomy-scaled casual clearance. The old 0.05 m
            // pre-ankle experiment cannot unload the canonical heel/ball/toe
            // plate: its swing heel lands while the opposite stance has only
            // a toe contact, reversing the body through a real but wrong
            // collision impulse.
            biped_parameters.swing_lift = 0.22f;
            // The two phases must meet at the same flexed vertical target.
            // Dynamic near-full stance extension plus endpoint shortening made
            // both feet unload at exchange before the swing sole landed.
            biped_parameters.transition_flex = 0.0f;
        }
        if (foundational_walk
            && rig.presentation_species() == sim::CreatureSpecies::human)
        {
            const auto gait_observation = environment.observation();
            const float local_pelvis_speed = gait_observation[2] * 6.0f;
            const float directed_speed = local_pelvis_speed
                * biped_parameters.direction;
            const float target_speed = std::max(0.10f,
                movement.target_speed);
            const float landing_speed = std::max(target_speed + 0.45f,
                directed_speed);
            const float trajectory_speed_scale = std::max(0.05f,
                2.0f * biped_parameters.cadence_hz
                    * biped_parameters.step_length);
            if (physical_gait_task != sim::GaitTask::walk)
            {
                biped_parameters.smooth_contact_transitions = true;
                biped_parameters.endpoint_tangent = -clamp(
                    landing_speed / trajectory_speed_scale, 0.25f, 2.0f);
            }
        }
        if (yielding_ground && movement.intent == locomotion::Intent::escape)
        {
            biped_parameters.cadence_hz = 0.68f;
            biped_parameters.step_length = std::min(biped_parameters.step_length, 0.48f);
            biped_parameters.swing_lift = std::max(biped_parameters.swing_lift,
                std::clamp(environment.burial_depth() + 0.12f, 0.18f, 0.28f));
            biped_parameters.transition_flex = std::max(
                biped_parameters.transition_flex, 0.08f);
        }
        if (movement.aquatic)
        {
            const bool human_aquatic = rig.presentation_species()
                    == sim::CreatureSpecies::human
                && rig.paired_leg_chains();
            // Keep the Human support clock continuous across the wet/dry
            // boundary. Recomputing total-time phase at 0.62 Hz in water and
            // 0.90 Hz on shore instantaneously exchanged the planted and swing
            // legs, physically throwing an upright rig back into the lake.
            // Aquatic speed still comes from the shorter stride, bounded lift,
            // finite drag and contact traction; no body/root pose is moved.
            if (!human_aquatic)
                biped_parameters.cadence_hz = movement.cadence_hz;
            biped_parameters.step_length *= movement.intent
                    == locomotion::Intent::shore_exit
                ? human_aquatic ? 0.60f : 0.72f
                : 0.48f;
            if (human_aquatic)
            {
                // The generic aquatic plan expresses clearance for compact and
                // multi-support topologies. A Human's accepted 0.14 m casual
                // lift must remain anatomical: clear the measured water, but
                // never turn a knee-deep wade into a 0.82 m marching step.
                const bool shore_exit = movement.intent
                    == locomotion::Intent::shore_exit;
                const float water_clearance = std::clamp(
                    environment.water_depth() + (shore_exit ? 0.10f : 0.07f),
                    shore_exit ? 0.24f : 0.18f,
                    shore_exit ? 0.42f : 0.36f);
                biped_parameters.swing_lift = std::max(
                    biped_parameters.swing_lift, water_clearance);
            }
            else
                biped_parameters.swing_lift = std::max(
                    biped_parameters.swing_lift, movement.swing_lift);
            biped_parameters.stance_height_ratio = movement.intent
                    == locomotion::Intent::swim ? 0.86f : 0.94f;
            biped_parameters.transition_flex = movement.intent
                    == locomotion::Intent::shore_exit ? 0.14f : 0.08f;
            biped_parameters.arm_swing_ratio = movement.intent
                    == locomotion::Intent::swim ? 0.34f
                    : human_aquatic ? 0.012f : 0.08f;
        }
        if (shuttle_braking)
        {
            // Use a low, short counter-step while momentum is being arrested.
            // The phase remains deterministic and restarts at the boundary, so
            // the rig plants instead of marching or being dragged backward.
            biped_parameters.cadence_hz *= 0.62f;
            biped_parameters.step_length *= 0.46f;
            biped_parameters.swing_lift *= 0.56f;
        }
        else if (environment.shuttle_phase() == sim::ShuttlePhase::backing)
        {
            // Backing is a real locomotion skill. Keep the learned opposed
            // gait, but shorten and slow it so the rig can unload its leading
            // support before the explicit turn instead of reaching the
            // boundary by sustained backward bracing.
            biped_parameters.cadence_hz *= 0.84f;
            biped_parameters.step_length *= 0.78f;
            biped_parameters.swing_lift *= 0.82f;
        }
        if (foundational_walk
            && rig.presentation_species() == sim::CreatureSpecies::human
            && rig.root_node < rig.nodes.size()
            && rig.torso_node < rig.nodes.size())
        {
            return raw_human_foundational_teacher_action(
                environment, biped_parameters, 0.80f, 0.010f);
        }
        return biped_gait_teacher_action(environment, biped_parameters);
    }

    [[nodiscard]] inline std::array<float, sim::action_count> walking_teacher_action(
        const sim::Environment& environment) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const auto raw = raw_walking_teacher_action(environment);
        auto filtered = bilateral_joint_synergy_action(environment,
            raw, environment.course_stage());
        // Topology IK already solves each support chain in its authored frame.
        // Bilateral regularization is useful for relaxed body/manipulator motion,
        // but processing the contact clock a second time turns return travel into
        // backward bracing. Keep the exact support targets at every public action
        // boundary so direct probes and the production code brain agree.
        for (std::size_t index = 0; index < rig.active_motor_count; ++index)
        {
            if (motor_drives_support_branch(rig, rig.motors[index]))
                filtered[index] = raw[index];
        }
        constrain_human_manipulator_actions_to_body_envelope(
            environment, filtered);
        return filtered;
    }
    [[nodiscard]] inline std::array<float, sim::action_count> crouch_walk_teacher_action(
        const sim::Environment& environment) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        if (rig.presentation_species() == sim::CreatureSpecies::human
            && rig.paired_leg_chains())
        {
            BipedGaitParameters parameters = anatomy_scaled_foundational_gait(
                rig, sim::GaitTask::walk, environment.elapsed_seconds());
            parameters.cadence_hz = 0.92f;
            // Lower through paired-leg IK, not a translated pelvis rail. Ease
            // the upper body vertically while retaining the validated walk
            // cycle in both legs. The resulting shorter hip-to-foot distance
            // produces knee flex without changing bone lengths or teaching a
            // separate backward-drifting endpoint convention.
            const float lower_ratio = clamp(environment.elapsed_seconds()
                / 1.50f, 0.0f, 1.0f);
            const float lower_blend = lower_ratio * lower_ratio
                * (3.0f - 2.0f * lower_ratio);
            // The shortened hip-to-foot distance consumes clearance; retain
            // enough ankle lift for the complete heel/ball/toe plate to leave
            // contact instead of shuffling through long double support.
            parameters.swing_lift += 0.18f * lower_blend;
            parameters.step_length *= 0.82f;
            // Flex the planted chain slightly as part of IK. This supplies the
            // missing crouch depth at high physics rates without applying a
            // larger external upper-body correction.
            parameters.stance_height_ratio = std::lerp(1.0f, 0.949f, lower_blend);
            parameters.stance_lowering_seconds = 1.50f;
            parameters.transition_flex = 0.06f * lower_blend;
            parameters.arm_swing_ratio = 0.006f;
            return biped_gait_teacher_action(environment, parameters);
        }
        const float pressure = std::max(0.72f, environment.duck_obstacle_weight());
        const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.05f;
        const float swing = std::sin(phase);
        auto action = compact_support_teacher_action(environment, pressure);

        if (rig.paired_leg_chains())
        {
            const float left_hip_direction =
                authored_joint_flexion_direction(rig.motors[0]);
            const float left_knee_direction =
                authored_joint_flexion_direction(rig.motors[1]);
            const float right_hip_direction =
                authored_joint_flexion_direction(rig.motors[2]);
            const float right_knee_direction =
                authored_joint_flexion_direction(rig.motors[3]);
            action[0] = clamp(action[0] + left_hip_direction
                * (0.24f * pressure + 0.34f * swing), -0.82f, 0.82f);
            action[1] = clamp(action[1] + left_knee_direction
                * (0.50f * pressure + 0.34f * std::max(0.0f, swing)),
                -0.90f, 0.90f);
            action[2] = clamp(action[2] + right_hip_direction
                * (0.24f * pressure - 0.34f * swing), -0.82f, 0.82f);
            action[3] = clamp(action[3] + right_knee_direction
                * (0.50f * pressure + 0.34f * std::max(0.0f, -swing)),
                -0.90f, 0.90f);
        }
        else
        {
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
            {
                const std::uint8_t mask = motor_support_mask(rig, rig.motors[index]);
                if (mask == 0u)
                    continue;
                const float gait_direction = mask == 0x1u ? swing
                    : mask == 0x2u ? -swing
                    : ((index & 1u) == 0u ? swing : -swing);
                action[index] = clamp(action[index] + gait_direction * 0.24f,
                    -0.86f, 0.86f);
            }
        }
        for (std::size_t index = 0; index < rig.active_motor_count; ++index)
        {
            if (!motor_drives_support_branch(rig, rig.motors[index]))
                action[index] = 0.0f;
        }
        return bilateral_joint_synergy_action(environment, action,
            sim::CourseStage::crouch_walk);
    }

    [[nodiscard]] inline std::uint64_t foundational_walk_teacher_handoff_update(
        const sim::CreatureBlueprint& blueprint) noexcept
    {
        if (!blueprint.paired_leg_chains() || blueprint.avian_gait())
            return 900u;
        return rig_has_manipulator_motors(blueprint) ? 900u : 500u;
    }

    [[nodiscard]] inline std::uint64_t foundational_walk_teacher_fade_begin_update(
        const sim::CreatureBlueprint& blueprint) noexcept
    {
        if (!blueprint.paired_leg_chains() || blueprint.avian_gait())
            return 600u;
        return rig_has_manipulator_motors(blueprint) ? 600u : 300u;
    }

    [[nodiscard]] inline float foundational_walk_teacher_authority(
        std::uint64_t update, const sim::CreatureBlueprint& blueprint) noexcept
    {
        const std::uint64_t handoff =
            foundational_walk_teacher_handoff_update(blueprint);
        const std::uint64_t fade_begin =
            foundational_walk_teacher_fade_begin_update(blueprint);
        if (update < fade_begin)
            return 1.0f;
        if (update < handoff)
            return 1.0f - static_cast<float>(update - fade_begin)
                / static_cast<float>(handoff - fade_begin);
        return 0.0f;
    }

    inline constexpr std::uint64_t foundational_walk_consolidation_updates = 300u;

    [[nodiscard]] inline bool foundational_walk_consolidation_active(
        std::uint64_t update, sim::CourseStage stage,
        const sim::CreatureBlueprint& blueprint) noexcept
    {
        if (stage != sim::CourseStage::uneven)
            return false;
        const std::uint64_t handoff =
            foundational_walk_teacher_handoff_update(blueprint);
        return update >= handoff
            && update < handoff + foundational_walk_consolidation_updates;
    }

    [[nodiscard]] inline std::size_t foundational_walk_raw_rollout_count(
        std::uint64_t update, sim::CourseStage stage,
        const sim::CreatureBlueprint& blueprint,
        std::size_t cohort_size) noexcept
    {
        if (cohort_size == 0u || stage != sim::CourseStage::uneven
            || !blueprint.human_casual_gait_plan())
            return 0u;
        const std::uint64_t handoff =
            foundational_walk_teacher_handoff_update(blueprint);
        const std::uint64_t fade_begin =
            foundational_walk_teacher_fade_begin_update(blueprint);
        if (update <= fade_begin)
            return 0u;
        if (update >= handoff)
            return cohort_size;
        const std::uint64_t fade_duration = handoff - fade_begin;
        const std::uint64_t elapsed = update - fade_begin;
        const std::uint64_t numerator = elapsed
            * static_cast<std::uint64_t>(cohort_size)
            + fade_duration - 1u;
        // Keep at least one Assisted origin until the explicit handoff. The
        // complement supplies demonstrations while Raw PPO authority grows.
        return std::min<std::size_t>(cohort_size - 1u,
            static_cast<std::size_t>(numerator
                / fade_duration));
    }

    [[nodiscard]] inline bool foundational_walk_raw_rollout_environment(
        std::uint64_t update, sim::CourseStage stage,
        const sim::CreatureBlueprint& blueprint,
        std::size_t environment_index, std::size_t cohort_size) noexcept
    {
        const std::size_t raw_count = foundational_walk_raw_rollout_count(
            update, stage, blueprint, cohort_size);
        return raw_count > 0u && cohort_size > 0u
            && (environment_index + static_cast<std::size_t>(update % cohort_size))
                % cohort_size < raw_count;
    }

    [[nodiscard]] inline bool foundational_walk_uses_raw_evaluation(
        std::uint64_t update, sim::CourseStage stage,
        const sim::CreatureBlueprint& blueprint) noexcept
    {
        return stage == sim::CourseStage::uneven
            && blueprint.human_casual_gait_plan()
            && update >= foundational_walk_teacher_handoff_update(blueprint);
    }

    inline constexpr std::uint64_t crouch_teacher_fade_begin_update = 60u;
    inline constexpr std::uint64_t crouch_teacher_handoff_update = 200u;
    inline constexpr std::uint64_t crouch_walk_teacher_fade_begin_update = 300u;
    inline constexpr std::uint64_t crouch_walk_teacher_handoff_update = 5200u;

    [[nodiscard]] inline float crouch_teacher_authority(
        std::uint64_t lesson_update) noexcept
    {
        if (lesson_update < crouch_teacher_fade_begin_update)
            return 1.0f;
        if (lesson_update < crouch_teacher_handoff_update)
            return 1.0f - static_cast<float>(
                lesson_update - crouch_teacher_fade_begin_update)
                / static_cast<float>(crouch_teacher_handoff_update
                    - crouch_teacher_fade_begin_update);
        return 0.0f;
    }

    [[nodiscard]] inline float crouch_walk_teacher_authority(
        std::uint64_t lesson_update) noexcept
    {
        if (lesson_update < crouch_walk_teacher_fade_begin_update)
            return 1.0f;
        if (lesson_update < crouch_walk_teacher_handoff_update)
            return 1.0f - static_cast<float>(
                lesson_update - crouch_walk_teacher_fade_begin_update)
                / static_cast<float>(crouch_walk_teacher_handoff_update
                    - crouch_walk_teacher_fade_begin_update);
        return 0.0f;
    }

    [[nodiscard]] inline float lesson_teacher_authority(
        std::uint64_t lesson_update, sim::CourseStage stage,
        const sim::CreatureBlueprint& blueprint) noexcept
    {
        if (stage == sim::CourseStage::duck_press)
            return crouch_teacher_authority(lesson_update);
        if (stage == sim::CourseStage::crouch_walk)
            return crouch_walk_teacher_authority(lesson_update);
        if (stage == sim::CourseStage::uneven
            || stage == sim::CourseStage::shuttle)
            return foundational_walk_teacher_authority(lesson_update, blueprint);
        return 1.0f;
    }

    struct TopologyReflexAuthority
    {
        float support{};
        float body{};
    };

    [[nodiscard]] inline TopologyReflexAuthority topology_runtime_reflex_authority(
        const sim::CreatureBlueprint& rig, sim::CourseStage stage) noexcept
    {
        if (stage != sim::CourseStage::uneven
            && stage != sim::CourseStage::shuttle)
            return {};

        if (rig.monopedal_gait())
            return { 0.92f, 0.82f };
        if (rig.avian_gait())
            return { 0.88f, 0.50f };
        // Human Uneven is the foundational learned-policy handoff. Its authored
        // gait is a finite demonstration whose lesson authority decays to zero;
        // a persistent topology reflex here would silently replace every
        // learned joint target until Raw mode removed it in one discontinuous
        // step. Shuttle remains the separately declared hybrid code-brain task.
        if (stage == sim::CourseStage::uneven && rig.human_casual_gait_plan())
            return {};

        // Back / Turn / Return is a hybrid code-brain skill for every body
        // plan. Once the learned policy is past curriculum handoff it still
        // needs a persistent, faced-direction support clock; otherwise a
        // forward-specialized residual can brace against the return command.
        if (stage == sim::CourseStage::shuttle)
        {
            if (rig.paired_leg_chains())
                return { 1.0f, 1.0f };
            if (rig.support_seed_count() >= 6u)
                return { 0.98f, 0.88f };
            if (rig.support_seed_count() >= 4u)
                return { 0.96f, 0.76f };
        }
        if (rig.support_seed_count() >= 6u)
            return { 0.98f, 0.84f };
        if (rig.support_seed_count() >= 4u)
            return { 0.94f, 0.70f };
        return {};
    }

    struct RuntimeSafetyAuthority
    {
        float support{};
        float body{};
    };

    [[nodiscard]] inline RuntimeSafetyAuthority runtime_safety_authority(
        const locomotion::Plan& plan) noexcept
    {
        if (plan.intent == locomotion::Intent::escape)
            return { 0.0f, 0.0f };
        if (plan.intent == locomotion::Intent::crawl)
            return { 0.78f, 0.60f };
        if (plan.intent == locomotion::Intent::flee)
            return { 0.58f, 0.34f };
        return {};
    }

    struct GuidanceAuthorityReport
    {
        sim::GuidanceMode mode{ sim::GuidanceMode::assisted };
        float lesson_teacher{};
        float topology_support{};
        float topology_body{};
        float safety_support{};
        float safety_body{};
        float posture_guide{};
        float swing_clearance{};
        float physical_contact{ 1.0f };
        float mandatory_joint_cluster{};

        [[nodiscard]] bool optional_guidance_active() const noexcept
        {
            return lesson_teacher > 0.0f || topology_support > 0.0f
                || topology_body > 0.0f || safety_support > 0.0f
                || safety_body > 0.0f || posture_guide > 0.0f
                || swing_clearance > 0.0f;
        }
    };

    [[nodiscard]] inline GuidanceAuthorityReport guidance_authority_report(
        const sim::Environment& environment, sim::CourseStage stage,
        float lesson_authority) noexcept
    {
        GuidanceAuthorityReport result{};
        result.mode = environment.guidance_mode();
        result.mandatory_joint_cluster =
            environment.blueprint().human_casual_gait_plan() ? 1.0f : 0.0f;
        if (result.mode == sim::GuidanceMode::raw_policy_audit)
            return result;

        result.lesson_teacher = clamp(lesson_authority, 0.0f, 1.0f);
        const bool foundational_human = stage == sim::CourseStage::uneven
            && environment.blueprint().human_casual_gait_plan();
        const bool physical_crouch = stage == sim::CourseStage::crouch_walk;
        if (physical_crouch)
            result.lesson_teacher = std::min(result.lesson_teacher,
                environment.effective_physical_guidance_authority());
        const float composite_authority = foundational_human || physical_crouch
            ? result.lesson_teacher : 1.0f;
        const TopologyReflexAuthority topology =
            topology_runtime_reflex_authority(environment.blueprint(), stage);
        result.topology_support = topology.support * composite_authority;
        result.topology_body = topology.body * composite_authority;
        if (stage == sim::CourseStage::crouch_walk
            && environment.blueprint().human_casual_gait_plan())
        {
            result.topology_support = composite_authority;
            result.topology_body = 0.95f * composite_authority;
            result.swing_clearance = composite_authority;
        }
        const RuntimeSafetyAuthority safety = runtime_safety_authority(
            current_locomotion_plan(environment));
        result.safety_support = safety.support * composite_authority;
        result.safety_body = safety.body * composite_authority;
        const bool posture_guided = stage == sim::CourseStage::balance
            || stage == sim::CourseStage::duck_press
            || stage == sim::CourseStage::crouch_walk
            || stage == sim::CourseStage::ramps
            || sim::stage_requires_forward_gait(stage);
        result.posture_guide = posture_guided
            ? stage == sim::CourseStage::balance
                ? std::min(composite_authority,
                    environment.effective_physical_guidance_authority())
                : composite_authority
            : 0.0f;
        return result;
    }

    [[nodiscard]] inline std::array<float, sim::action_count>
    effective_policy_action_with_unguided_baseline(
        const sim::Environment& environment,
        std::array<float, sim::action_count> policy_action,
        const std::array<float, sim::action_count>& unguided_policy_action,
        sim::CourseStage stage,
        float lesson_authority = 1.0f,
        sim::GuidanceMode guidance_mode = sim::GuidanceMode::assisted) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const std::size_t active = rig.active_motor_count;
        const ActionMask active_actions = action_mask_for(environment, stage);
        std::array<float, sim::action_count> bounded_network_action =
            unguided_policy_action;
        for (std::size_t index = 0; index < sim::action_count; ++index)
            bounded_network_action[index] = !active_actions[index]
                ? 0.0f : clamp(bounded_network_action[index], -1.0f, 1.0f);
        if (guidance_mode == sim::GuidanceMode::raw_policy_audit
            || environment.raw_policy_audit())
        {
            // Raw audit preserves motors, constraints, gravity, collision, and
            // species-owned local articulation. It adds no teacher, topology
            // reflex, posture guide, swing projection, bilateral synergy, or
            // locomotion-producing manipulator guidance.
            return bounded_network_action;
        }
        const bool foundational_human_composite_fade =
            stage == sim::CourseStage::uneven && rig.human_casual_gait_plan();
        const bool crouch_walk_composite_fade =
            stage == sim::CourseStage::crouch_walk;
        const bool composite_guidance_fade = foundational_human_composite_fade
            || crouch_walk_composite_fade;
        const float composite_authority = crouch_walk_composite_fade
            ? std::min(clamp(lesson_authority, 0.0f, 1.0f),
                environment.effective_physical_guidance_authority())
            : foundational_human_composite_fade
                ? clamp(lesson_authority, 0.0f, 1.0f) : 1.0f;
        if (composite_guidance_fade)
        {
            for (std::size_t index = 0; index < sim::action_count; ++index)
                policy_action[index] = !active_actions[index]
                    ? 0.0f : clamp(policy_action[index], -1.0f, 1.0f);
        }
        std::array<float, sim::action_count> hard_support_teacher{};
        bool hard_support_reflex{};
        auto support_motor = [&rig](std::size_t index) noexcept
        {
            return index < rig.active_motor_count
                && motor_drives_support_branch(rig, rig.motors[index]);
        };
        auto blend_teacher = [&](const std::array<float, sim::action_count>& teacher,
            float support_assist, float body_assist) noexcept
        {
            for (std::size_t index = 0; index < active; ++index)
            {
                const float assist = support_motor(index) ? support_assist : body_assist;
                policy_action[index] = lerp(policy_action[index], teacher[index], assist);
            }
        };
        auto neutralize_non_support = [&](float amount) noexcept
        {
            for (std::size_t index = 0; index < active; ++index)
            {
                if (!support_motor(index))
                    policy_action[index] = lerp(policy_action[index], 0.0f, amount);
            }
        };
        auto apply_equipment_arm_pose = [&](float desired_angle,
            bool shoulder_seated) noexcept
        {
            const float facing = environment.facing_direction() < 0.0f
                ? -1.0f : 1.0f;
            const float local_desired_angle = wrap_angle(facing < 0.0f
                ? pi - desired_angle : desired_angle);
            policy_action[sim::equipment_aim_action] = clamp(
                local_desired_angle / (pi * 0.42f), -1.0f, 1.0f);
            const Vec2 weapon_axis{
                std::cos(desired_angle), std::sin(desired_angle) };
            struct ArmChain
            {
                std::size_t shoulder_index{};
                std::size_t elbow_index{};
                std::uint16_t shoulder_node{};
                std::uint16_t elbow_node{};
                std::uint16_t hand_node{};
                float upper_length{};
                float lower_length{};
            };
            std::array<ArmChain, 2u> chains{};
            std::size_t chain_count{};
            for (std::size_t shoulder_index = 0;
                shoulder_index < rig.active_motor_count; ++shoulder_index)
            {
                const sim::MotorConstraint& shoulder = rig.motors[shoulder_index];
                if (motor_drives_support_branch(rig, shoulder)
                    || shoulder.a != rig.torso_node
                    || shoulder.pivot >= rig.nodes.size()
                    || shoulder.c >= rig.nodes.size())
                    continue;
                for (std::size_t elbow_index = 0;
                    elbow_index < rig.active_motor_count; ++elbow_index)
                {
                    const sim::MotorConstraint& elbow = rig.motors[elbow_index];
                    if (elbow_index == shoulder_index
                        || motor_drives_support_branch(rig, elbow)
                        || elbow.pivot != shoulder.c
                        || elbow.c >= rig.nodes.size())
                        continue;
                    if (chain_count < chains.size())
                    {
                        chains[chain_count++] = ArmChain{
                            shoulder_index, elbow_index, shoulder.pivot,
                            shoulder.c, elbow.c,
                            length(rig.nodes[shoulder.c]
                                - rig.nodes[shoulder.pivot]),
                            length(rig.nodes[elbow.c]
                                - rig.nodes[elbow.pivot]) };
                    }
                    break;
                }
            }
            if (chain_count == 0u)
                return;

            const std::uint16_t physical_mount =
                environment.equipment_mount_node_index();
            std::size_t primary{};
            for (std::size_t index = 0; index < chain_count; ++index)
            {
                if (chains[index].hand_node == physical_mount)
                {
                    primary = index;
                    break;
                }
            }
            const std::size_t secondary = primary == 0u ? 1u : 0u;
            const auto& particles = environment.particles();
            auto live_position = [&](std::uint16_t node) noexcept
            {
                return node < particles.size()
                    ? particles[node].position : rig.nodes[node];
            };
            auto pose_motor = [&](std::size_t index) noexcept
            {
                return rig.presentation_species() == sim::CreatureSpecies::human
                    ? sim::human_equipment_motor_envelope(rig.motors[index], index)
                    : rig.motors[index];
            };
            auto set_chain_targets = [&](const ArmChain& chain,
                Vec2 upper, Vec2 lower) noexcept
            {
                if (length(upper) <= 1.0e-5f || length(lower) <= 1.0e-5f)
                    return;
                const sim::MotorConstraint shoulder =
                    pose_motor(chain.shoulder_index);
                const sim::MotorConstraint elbow = pose_motor(chain.elbow_index);
                const Vec2 shoulder_reference = live_position(shoulder.a)
                    - live_position(shoulder.pivot);
                const float shoulder_world = signed_angle(
                    shoulder_reference, upper);
                const float elbow_world = signed_angle(-1.0f * upper, lower);
                policy_action[chain.shoulder_index] =
                    motor_action_for_target_angle(shoulder,
                        shoulder_world * facing);
                policy_action[chain.elbow_index] =
                    motor_action_for_target_angle(elbow,
                        elbow_world * facing);
            };

            if (chain_count < 2u || primary == secondary)
                return;
            const Vec2 body_up = normalized(
                live_position(rig.torso_node) - live_position(rig.root_node),
                { 0.0f, 1.0f });
            const Vec2 body_forward = facing
                * Vec2{ body_up.y, -body_up.x };
            const sim::WeaponGeometry geometry = sim::weapon_geometry(
                environment.weapon_class());
            const ArmChain& rear_chain = chains[primary];
            const ArmChain& front_chain = chains[secondary];
            const Vec2 rear_shoulder = live_position(rear_chain.shoulder_node);
            Vec2 rear_grip_target{};
            float grip_span = geometry.rear_to_foregrip;
            if (shoulder_seated)
            {
                rear_grip_target = geometry.shouldered
                    ? rear_shoulder + body_forward * 0.04f
                        - body_up * 0.10f
                        + weapon_axis * geometry.butt_to_rear_grip
                    : rear_shoulder + body_forward * 0.28f
                        - body_up * 0.16f;
            }
            else
            {
                rear_grip_target = rear_shoulder
                    + body_forward * 0.02f - body_up * 0.42f;
                grip_span = std::min(0.22f, geometry.rear_to_foregrip);
            }
            const Vec2 front_grip_target = rear_grip_target
                + weapon_axis * grip_span;

            auto solve_endpoint = [&](const ArmChain& chain,
                Vec2 endpoint) noexcept
            {
                const Vec2 shoulder = live_position(chain.shoulder_node);
                const Vec2 target = endpoint - shoulder;
                const TwoLinkSagittalSolution negative =
                    solve_two_link_sagittal(chain.upper_length,
                        chain.lower_length, target, -1.0f);
                const TwoLinkSagittalSolution positive =
                    solve_two_link_sagittal(chain.upper_length,
                        chain.lower_length, target, 1.0f);
                if (!negative.valid && !positive.valid)
                    return;
                const Vec2 current_upper = live_position(chain.elbow_node)
                    - shoulder;
                const Vec2 current_lower = live_position(chain.hand_node)
                    - live_position(chain.elbow_node);
                auto continuity_score = [&](const TwoLinkSagittalSolution& solution)
                {
                    if (!solution.valid)
                        return std::numeric_limits<float>::infinity();
                    return std::abs(signed_angle(current_upper, solution.upper))
                        + std::abs(signed_angle(current_lower, solution.lower));
                };
                const float negative_score = continuity_score(negative);
                const float positive_score = continuity_score(positive);
                const bool choose_positive = positive_score + 1.0e-5f
                        < negative_score
                    || (std::abs(positive_score - negative_score) <= 1.0e-5f
                        && dot(positive.upper, body_up)
                            < dot(negative.upper, body_up));
                const TwoLinkSagittalSolution& solution = choose_positive
                    ? positive : negative;
                set_chain_targets(chain, solution.upper, solution.lower);
            };
            solve_endpoint(rear_chain, rear_grip_target);
            solve_endpoint(front_chain, front_grip_target);
        };
        auto apply_equipment_teacher = [&]() noexcept
        {
            for (std::size_t index = sim::anatomy_action_count;
                index < sim::action_count; ++index)
                policy_action[index] = 0.0f;

            if (environment.weapon_class() == sim::WeaponClass::none)
                return;
            sim::EquipmentDirective directive = environment.equipment_directive();
            if (directive == sim::EquipmentDirective::passive
                && (stage == sim::CourseStage::equipment_targets
                    || stage == sim::CourseStage::combat_course))
                directive = sim::EquipmentDirective::fire_and_correct;
            policy_action[sim::equipment_trigger_action] = -1.0f;
            if (directive == sim::EquipmentDirective::passive)
                return;
            const float facing_angle = environment.facing_direction() < 0.0f
                ? pi : 0.0f;
            const bool facing_left = environment.facing_direction() < 0.0f;
            const auto local_angle = [facing_angle, facing_left](
                float angle) noexcept
            {
                return wrap_angle(facing_left
                    ? facing_angle - angle : angle);
            };
            if (directive == sim::EquipmentDirective::safe_carry_walk)
            {
                apply_equipment_arm_pose(local_angle(-0.72f), false);
                policy_action[sim::equipment_state_action] = -0.30f;
                return;
            }
            if (directive == sim::EquipmentDirective::low_ready_walk
                || directive == sim::EquipmentDirective::break_contact)
            {
                apply_equipment_arm_pose(local_angle(-0.28f), true);
                policy_action[sim::equipment_state_action] = 0.45f;
                return;
            }

            if (directive == sim::EquipmentDirective::fire_and_correct
                && (!environment.equipment_target().active
                    || environment.target_hits()
                        >= environment.equipment_hit_goal()))
            {
                // The engagement goal is complete. Return the real arm chain
                // to safe carry and leave the locomotion teacher selected by
                // the course intact so a combat-course NPC walks on physically.
                apply_equipment_arm_pose(local_angle(-0.72f), false);
                policy_action[sim::equipment_state_action] = -0.30f;
                return;
            }

            // A combatant first arrests the same physical body used for
            // locomotion. No target lock or projectile steering can substitute
            // for establishing a supported gun stance.
            const auto hold = balance_teacher_action(environment);
            blend_teacher(hold, 0.96f, 0.92f);
            policy_action[sim::equipment_trigger_action] = -1.0f;
            if (directive == sim::EquipmentDirective::stop_and_plant)
            {
                apply_equipment_arm_pose(local_angle(-0.62f), false);
                policy_action[sim::equipment_state_action] = 0.62f;
                return;
            }
            if (directive == sim::EquipmentDirective::gun_stance)
            {
                apply_equipment_arm_pose(local_angle(-0.12f), true);
                policy_action[sim::equipment_state_action] = 0.66f;
                return;
            }
            policy_action[sim::equipment_state_action] =
                directive == sim::EquipmentDirective::fire_and_correct
                ? 1.0f : 0.82f;
            if (!environment.equipment_stopped())
            {
                apply_equipment_arm_pose(local_angle(-0.28f), true);
                return;
            }

            if (environment.equipment_state() == sim::EquipmentState::reloading)
            {
                apply_equipment_arm_pose(local_angle(-0.12f), true);
                return;
            }

            const sim::EquipmentTarget& target = environment.equipment_target();
            if (!target.active)
                return;
            const float desired_angle = environment.equipment_recommended_aim_angle();
            const float local_desired_angle = wrap_angle(facing_left
                ? pi - desired_angle : desired_angle);
            policy_action[sim::equipment_aim_action] = clamp(
                local_desired_angle / (pi * 0.42f), -1.0f, 1.0f);

            // Both finite hand endpoints establish the one physical grip frame
            // consumed by aim, rendering, ballistics, and recoil.
            apply_equipment_arm_pose(desired_angle, true);
            policy_action[sim::equipment_trigger_action] =
                directive == sim::EquipmentDirective::fire_and_correct
                    && environment.equipment_engagement_ready() ? 1.0f : -1.0f;
        };
        if (stage == sim::CourseStage::balance)
        {
            const auto teacher = balance_teacher_action(environment);
            const bool established = environment.stable_stance_seconds() >= 0.75f;
            blend_teacher(teacher, established ? 0.46f : 0.72f, established ? 0.52f : 0.78f);
        }
        else if (stage == sim::CourseStage::duck_press)
        {
            const auto teacher = duck_teacher_action(environment);
            const float authority = clamp(lesson_authority, 0.0f, 1.0f);
            blend_teacher(teacher,
                (0.76f + environment.duck_obstacle_weight() * 0.16f) * authority,
                0.0f);
            neutralize_non_support(0.995f * authority);
        }
        else if (stage == sim::CourseStage::uneven
            || stage == sim::CourseStage::shuttle)
        {
            const auto teacher = raw_walking_teacher_action(environment);
            // Human Uneven owns one outer composite fade below. Build the full
            // finite-motor Assisted demonstration here so teacher influence is
            // linear rather than accidentally authority-squared.
            const float authority = foundational_human_composite_fade
                ? 1.0f : clamp(lesson_authority, 0.0f, 1.0f);
            // Fragile authored body plans ship a deterministic support reflex as
            // their code brain. Learned weights still control the bounded
            // residual, while lesson authority remains curriculum-only.
            const TopologyReflexAuthority reflex =
                topology_runtime_reflex_authority(rig, stage);
            const float support_assist = std::max(0.995f * authority, reflex.support);
            const float body_assist = std::max(0.995f * authority, reflex.body);
            blend_teacher(teacher, support_assist, body_assist);
            if (reflex.support >= 1.0f)
            {
                // The deterministic topology teacher is already solved in the
                // authored joint frame. Preserve its exact support clock after
                // regularizing only the learned residual below.
                hard_support_teacher = teacher;
                hard_support_reflex = true;
            }
        }
        else if (stage == sim::CourseStage::crouch_walk)
        {
            const auto teacher = crouch_walk_teacher_action(environment);
            const bool human_crouch = rig.human_casual_gait_plan();
            blend_teacher(teacher,
                human_crouch ? 1.0f
                    : 0.58f + environment.duck_obstacle_weight() * 0.24f,
                human_crouch ? 0.95f : 0.0f);
            if (human_crouch)
            {
                hard_support_teacher = teacher;
                hard_support_reflex = true;
            }
            else
            {
                neutralize_non_support(0.98f);
            }
        }
        else if (stage == sim::CourseStage::ramps)
        {
            const auto teacher = balance_teacher_action(environment);
            blend_teacher(teacher, 0.26f, 0.88f);
        }
        else if (stage == sim::CourseStage::hurdles
            || stage == sim::CourseStage::moving_hazards
            || stage == sim::CourseStage::climb_descent
            || stage == sim::CourseStage::combat_course)
        {
            const auto teacher = raw_walking_teacher_action(environment);
            const locomotion::Plan movement = current_locomotion_plan(environment);
            const float support_assist = movement.intent == locomotion::Intent::crawl ? 0.78f : movement.intent == locomotion::Intent::flee ? 0.52f : movement.intent == locomotion::Intent::recover ? 0.62f : movement.step_up ? 0.48f : 0.24f;
            const float body_assist = movement.intent == locomotion::Intent::crawl ? 0.60f : movement.intent == locomotion::Intent::flee ? 0.34f : 0.20f;
            blend_teacher(teacher, support_assist, body_assist);
        }
        else if (stage == sim::CourseStage::equipment_targets)
        {
            const auto teacher = balance_teacher_action(environment);
            blend_teacher(teacher, 0.58f, 0.66f);
        }

        if (stage != sim::CourseStage::equipment_targets
            && stage != sim::CourseStage::combat_course)
        {
            for (std::size_t index = sim::anatomy_action_count;
                index < sim::action_count; ++index)
                policy_action[index] = 0.0f;
        }
        const locomotion::Plan runtime_plan = current_locomotion_plan(environment);
        if (runtime_plan.intent == locomotion::Intent::escape
            || runtime_plan.intent == locomotion::Intent::flee
            || runtime_plan.intent == locomotion::Intent::crawl)
        {
            const auto safety_teacher = raw_walking_teacher_action(environment);
            const RuntimeSafetyAuthority authority =
                runtime_safety_authority(runtime_plan);
            blend_teacher(safety_teacher, authority.support, authority.body);
        }
        else if (runtime_plan.intent == locomotion::Intent::hold
            && environment.granular_hazard_active()
            && !environment.granular_hazard_safe())
        {
            const auto safety_teacher = balance_teacher_action(environment);
            blend_teacher(safety_teacher, 0.72f, 0.78f);
        }

        if ((stage == sim::CourseStage::duck_press
                || stage == sim::CourseStage::uneven
                || stage == sim::CourseStage::shuttle)
            && lesson_authority <= 0.0f
            && !foundational_human_composite_fade)
        {
            for (std::size_t index = 0; index < active; ++index)
            {
                if (!support_motor(index))
                    policy_action[index] = clamp(policy_action[index], -0.32f, 0.32f);
            }
        }
        if (environment.longest_stable_stance_seconds() < 1.0f
            && !sim::stage_allows_controlled_flips(stage)
            && !hard_support_reflex)
        {
            for (std::size_t index = 0; index < active; ++index)
            {
                if (!support_motor(index))
                    policy_action[index] *= 0.08f;
            }
        }
        policy_action = bilateral_joint_synergy_action(
            environment, policy_action, stage);
        if (rig.paired_leg_chains()
            && (stage == sim::CourseStage::balance
                || sim::stage_requires_forward_gait(stage))
            && !sim::stage_allows_controlled_flips(stage))
        {
            const float limit = stage == sim::CourseStage::balance ? 0.24f : 0.36f;
            for (std::size_t index = 0; index < active; ++index)
            {
                if (!support_motor(index))
                    policy_action[index] = clamp(policy_action[index], -limit, limit);
            }
        }
        // A full support reflex is the locomotion code brain for fragile plans;
        // restore it after generic synergy so learned residuals cannot erase the
        // opposed contact-transfer clock or accidentally process it twice.
        if (hard_support_reflex)
        {
            for (std::size_t index = 0; index < active; ++index)
            {
                if (support_motor(index))
                    policy_action[index] = hard_support_teacher[index];
            }
        }
        if (rig.human_casual_gait_plan() && environment.shuttle_enabled())
        {
            const sim::ShuttlePhase phase = environment.shuttle_phase();
            if (phase == sim::ShuttlePhase::turning
                || phase == sim::ShuttlePhase::settling)
                settle_manipulator_actions_to_rest(rig, policy_action, 1.0f);
        }
        constrain_human_manipulator_actions_to_body_envelope(
            environment, policy_action);
        if (guidance_mode == sim::GuidanceMode::assisted
            && environment.weapon_class() != sim::WeaponClass::none)
            // The selected Director task owns the final physical arm chain.
            apply_equipment_teacher();
        if (composite_guidance_fade)
        {
            for (std::size_t index = 0; index < sim::action_count; ++index)
            {
                const float full_assisted = !active_actions[index]
                    ? 0.0f : clamp(policy_action[index], -1.0f, 1.0f);
                policy_action[index] = lerp(bounded_network_action[index],
                    full_assisted, composite_authority);
            }
        }
        for (std::size_t index = 0; index < sim::action_count; ++index)
            if (!active_actions[index])
                policy_action[index] = 0.0f;
        return policy_action;
    }

    [[nodiscard]] inline std::array<float, sim::action_count> effective_policy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> policy_action,
        sim::CourseStage stage,
        float lesson_authority = 1.0f,
        sim::GuidanceMode guidance_mode = sim::GuidanceMode::assisted) noexcept
    {
        return effective_policy_action_with_unguided_baseline(environment,
            policy_action, policy_action, stage, lesson_authority,
            guidance_mode);
    }

    struct TrainingMetrics
    {
        std::uint64_t update{};
        std::uint64_t environment_steps{};
        std::uint64_t total_updates{};
        std::uint64_t total_environment_steps{};
        std::uint64_t total_episodes{};
        std::uint64_t total_valid_episodes{};
        std::uint64_t total_invalid_episodes{};
        std::uint64_t total_resets{};
        std::uint64_t total_alternating_steps{};
        std::uint64_t total_falls{};
        std::uint64_t total_collisions{};
        std::uint64_t total_powered_jumps{};
        std::uint64_t total_landed_jumps{};
        std::uint64_t total_landed_flips{};
        std::uint64_t total_obstacles_passed{};
        double total_distance{};
        double total_training_seconds{};
        float mean_reward{};
        float mean_episode_distance{};
        float mean_speed{};
        float policy_loss{};
        float value_loss{};
        float entropy{};
        float learning_rate{ 3.0e-4f };

        float evaluation_reward{};
        float evaluation_distance{};
        float evaluation_speed{};
        float evaluation_score{ -std::numeric_limits<float>::infinity() };
        float evaluation_survival{};
        float evaluation_collisions{};
        float evaluation_airborne_ratio{};
        float evaluation_stride_events{};
        float evaluation_duck_seconds{};
        float evaluation_powered_jumps{};
        float evaluation_jump_landings{};
        float evaluation_spin_turns{};
        float evaluation_spin_landings{};
        float evaluation_obstacles_passed{};
        float evaluation_stable_stance{};
        float evaluation_longest_stance{};
        float evaluation_duck_recoveries{};
        float evaluation_max_joint_speed{};
        float evaluation_max_backward_brace_seconds{};
        float evaluation_hand_contacts{};
        float evaluation_climb_transfers{};
        float evaluation_climbs{};
        float evaluation_descents{};
        float evaluation_shots{};
        float evaluation_target_hits{};
        float evaluation_equipment_transitions{};
        std::uint64_t evaluation_quality_key{};
        std::uint32_t evaluation_rejection_mask{};
        std::uint32_t evaluation_invalid_runs{};
        sim::InvalidMotion evaluation_invalid_reason{ sim::InvalidMotion::none };
        bool evaluation_valid{};

        float best_evaluation_distance{ -std::numeric_limits<float>::infinity() };
        float best_evaluation_score{ -std::numeric_limits<float>::infinity() };
        std::uint64_t best_quality_key{};
        std::uint64_t best_update{};
        std::uint64_t evaluation_count{};
        std::uint32_t imitation_samples{};
        float imitation_weight{};
        float imitation_source_score{ -std::numeric_limits<float>::infinity() };
    };

    [[nodiscard]] inline float guided_rollout_imitation_weight(
        std::uint64_t update, sim::CourseStage stage,
        const sim::CreatureBlueprint* blueprint = nullptr) noexcept
    {
        if (stage == sim::CourseStage::duck_press)
        {
            if (update < crouch_teacher_fade_begin_update)
                return 64.0f;
            if (update < crouch_teacher_handoff_update)
                return lerp(64.0f, 0.0f,
                    static_cast<float>(update - crouch_teacher_fade_begin_update)
                        / static_cast<float>(crouch_teacher_handoff_update
                            - crouch_teacher_fade_begin_update));
            return 0.0f;
        }
        if (stage == sim::CourseStage::crouch_walk)
            return 64.0f * crouch_walk_teacher_authority(update);
        if (!sim::stage_requires_forward_gait(stage))
            return 0.0f;
        if (stage == sim::CourseStage::uneven && blueprint != nullptr)
        {
            const float authority = foundational_walk_teacher_authority(
                update, *blueprint);
            if (authority > 0.0f)
                return 64.0f * authority;
            if (foundational_walk_consolidation_active(
                    update, stage, *blueprint))
            {
                const std::uint64_t handoff =
                    foundational_walk_teacher_handoff_update(*blueprint);
                const float progress = static_cast<float>(update - handoff)
                    / static_cast<float>(foundational_walk_consolidation_updates);
                const bool fragile_support_topology = blueprint->monopedal_gait()
                    || blueprint->avian_gait();
                return fragile_support_topology
                    ? lerp(48.0f, 8.0f, progress)
                    : lerp(16.0f, 0.0f, progress);
            }
            return 0.0f;
        }
        if (update < 1200u)
            return 64.0f;
        if (update < 3600u)
            return lerp(64.0f, 8.0f,
                static_cast<float>(update - 1200u) / 2400.0f);
        if (update < 7200u)
            return lerp(8.0f, 0.05f,
                static_cast<float>(update - 3600u) / 3600.0f);
        return 0.0f;
    }

    [[nodiscard]] inline float self_imitation_prior_weight(std::uint64_t age_updates,
        std::size_t sample_count) noexcept
    {
        if (sample_count == 0)
            return 0.0f;
        const float age = static_cast<float>(std::min<std::uint64_t>(age_updates, 2000u));
        return clamp(0.18f / (1.0f + age / 240.0f), 0.040f, 0.18f);
    }

    [[nodiscard]] inline bool policy_regression_guard(float best_score,
        float current_score, bool current_valid) noexcept
    {
        if (!std::isfinite(best_score))
            return false;
        if (!current_valid || !std::isfinite(current_score))
            return true;
        const float allowed_drop = std::max(0.12f, std::abs(best_score) * 0.08f);
        return current_score < best_score - allowed_drop;
    }

    inline constexpr float standing_qualification_seconds = 4.0f;
    inline constexpr float standing_mastery_seconds = 6.0f;
    inline constexpr float casual_walk_teacher_distance = 12.0f;
    inline constexpr float casual_walk_teacher_stride_events = 12.0f;
    inline constexpr float speed_walk_mastery_distance = 18.0f;
    // Keep the public 18 m goal while accepting one 0.5 m terrain/contact
    // measurement interval at the cross-platform qualification boundary.
    // Cadence, speed, survival, collision, and strict-valid gates remain exact.
    inline constexpr float speed_walk_qualification_distance = 17.5f;
    inline constexpr float speed_walk_mastery_stride_events = 14.0f;
    inline constexpr float transition_mastery_distance = 20.0f;
    inline constexpr float transition_mastery_stride_events = 14.0f;
    inline constexpr float run_mastery_distance = 24.0f;
    inline constexpr float run_mastery_stride_events = 16.0f;
    inline constexpr float walk_mastery_distance = 18.0f;
    inline constexpr float walk_mastery_stride_events = 14.0f;

    [[nodiscard]] inline constexpr float gait_task_mastery_distance(
        sim::GaitTask task) noexcept
    {
        switch (task)
        {
        case sim::GaitTask::walk: return casual_walk_teacher_distance;
        case sim::GaitTask::speed_walk: return speed_walk_mastery_distance;
        case sim::GaitTask::walk_run_transition: return transition_mastery_distance;
        case sim::GaitTask::run: return run_mastery_distance;
        }
        return casual_walk_teacher_distance;
    }
    [[nodiscard]] inline constexpr float gait_task_qualification_distance(
        sim::GaitTask task) noexcept
    {
        return task == sim::GaitTask::speed_walk
            ? speed_walk_qualification_distance
            : gait_task_mastery_distance(task);
    }


    [[nodiscard]] inline constexpr float gait_task_mastery_stride_events(
        sim::GaitTask task) noexcept
    {
        switch (task)
        {
        case sim::GaitTask::walk: return casual_walk_teacher_stride_events;
        case sim::GaitTask::speed_walk: return speed_walk_mastery_stride_events;
        case sim::GaitTask::walk_run_transition: return transition_mastery_stride_events;
        case sim::GaitTask::run: return run_mastery_stride_events;
        }
        return casual_walk_teacher_stride_events;
    }

    [[nodiscard]] inline constexpr float gait_task_mastery_speed(
        sim::GaitTask task) noexcept
    {
        switch (task)
        {
        case sim::GaitTask::walk: return 0.32f;
        case sim::GaitTask::speed_walk: return 0.72f;
        case sim::GaitTask::walk_run_transition: return 0.62f;
        case sim::GaitTask::run: return 1.05f;
        }
        return 0.32f;
    }
    inline constexpr float standing_neutral_arm_limit = 38.0f * pi / 180.0f;
    inline constexpr float standing_qualification_spin_limit = 0.16f;
    inline constexpr float standing_mastery_spin_limit = 0.08f;

    enum class MotionEvidenceFailure : std::uint32_t
    {
        none = 0,
        invalid_motion = 1u << 0u,
        no_stable_stance = 1u << 1u,
        missing_recovery = 1u << 2u,
        missing_skill = 1u << 3u,
        missing_progress = 1u << 4u,
        unstable_joints = 1u << 5u,
        body_contact = 1u << 6u,
        non_neutral_posture = 1u << 7u,
        excessive_rotation = 1u << 8u,
        invalid_crouch_posture = 1u << 9u,
        lateral_crab_gait = 1u << 10u,
        lower_leg_scissor = 1u << 11u,
        backward_brace = 1u << 12u
    };

    struct StageMotionQualification
    {
        bool valid{};
        std::uint32_t rejection_mask{};
        std::uint64_t quality_key{};
    };

    [[nodiscard]] inline constexpr std::uint32_t evidence_bit(MotionEvidenceFailure failure) noexcept
    {
        return static_cast<std::uint32_t>(failure);
    }

    [[nodiscard]] inline std::string motion_rejection_summary(std::uint32_t mask)
    {
        std::string result{};
        auto append = [&](MotionEvidenceFailure failure, std::string_view name)
        {
            if ((mask & evidence_bit(failure)) == 0u)
                return;
            if (!result.empty())
                result += " + ";
            result += name;
        };
        append(MotionEvidenceFailure::invalid_motion, "INVALID MOTION");
        append(MotionEvidenceFailure::no_stable_stance, "NO STABLE STANCE");
        append(MotionEvidenceFailure::missing_recovery, "NO RECOVERY");
        append(MotionEvidenceFailure::missing_skill, "MISSING SKILL");
        append(MotionEvidenceFailure::missing_progress, "NO PROGRESS");
        append(MotionEvidenceFailure::unstable_joints, "UNSTABLE JOINTS");
        append(MotionEvidenceFailure::body_contact, "BODY CONTACT");
        append(MotionEvidenceFailure::non_neutral_posture, "NON-NEUTRAL POSTURE");
        append(MotionEvidenceFailure::excessive_rotation, "EXCESSIVE ROTATION");
        append(MotionEvidenceFailure::invalid_crouch_posture, "INVALID CROUCH");
        append(MotionEvidenceFailure::lateral_crab_gait, "LATERAL GAIT");
        append(MotionEvidenceFailure::lower_leg_scissor, "LOWER-LEG SCISSOR");
        append(MotionEvidenceFailure::backward_brace, "BACKWARD BRACE");
        return result.empty() ? "STAGE VALID" : result;
    }

    [[nodiscard]] inline std::string_view primary_motion_rejection_name(
        std::uint32_t mask) noexcept
    {
        if ((mask & evidence_bit(MotionEvidenceFailure::invalid_motion)) != 0u)
            return "INVALID MOTION";
        if ((mask & evidence_bit(MotionEvidenceFailure::body_contact)) != 0u)
            return "BODY CONTACT";
        if ((mask & evidence_bit(MotionEvidenceFailure::non_neutral_posture)) != 0u)
            return "ARMS NOT NEUTRAL";
        if ((mask & evidence_bit(MotionEvidenceFailure::excessive_rotation)) != 0u)
            return "UNCONTROLLED STANDING SPIN";
        if ((mask & evidence_bit(MotionEvidenceFailure::invalid_crouch_posture)) != 0u)
            return "HIP HINGE - NOT A CROUCH";
        if ((mask & evidence_bit(MotionEvidenceFailure::lower_leg_scissor)) != 0u)
            return "LOWER LEGS SCISSOR FOR TOO LONG";
        if ((mask & evidence_bit(MotionEvidenceFailure::backward_brace)) != 0u)
            return "TORSO BRACED AGAINST TRAVEL";
        if ((mask & evidence_bit(MotionEvidenceFailure::lateral_crab_gait)) != 0u)
            return "CRAB WALK - NO SAGITTAL CROSSING";
        if ((mask & evidence_bit(MotionEvidenceFailure::no_stable_stance)) != 0u)
            return "NO SUSTAINED STANCE";
        if ((mask & evidence_bit(MotionEvidenceFailure::missing_recovery)) != 0u)
            return "NO CONTROLLED RECOVERY";
        if ((mask & evidence_bit(MotionEvidenceFailure::missing_skill)) != 0u)
            return "MISSING SKILL EVIDENCE";
        if ((mask & evidence_bit(MotionEvidenceFailure::missing_progress)) != 0u)
            return "NO REAL PROGRESS";
        if ((mask & evidence_bit(MotionEvidenceFailure::unstable_joints)) != 0u)
            return "VIOLENT JOINT MOTION";
        return "STAGE VALID";
    }

    [[nodiscard]] inline std::uint16_t quality_bucket(float value,
        float scale = 10.0f) noexcept
    {
        return static_cast<std::uint16_t>(clamp(value * scale, 0.0f, 65535.0f));
    }

    [[nodiscard]] inline std::uint64_t pack_quality(std::uint16_t primary,
        std::uint16_t secondary, std::uint16_t tertiary, std::uint16_t quaternary) noexcept
    {
        return (static_cast<std::uint64_t>(primary) << 48u)
            | (static_cast<std::uint64_t>(secondary) << 32u)
            | (static_cast<std::uint64_t>(tertiary) << 16u)
            | static_cast<std::uint64_t>(quaternary);
    }

    [[nodiscard]] inline std::uint64_t gait_task_incremental_quality(
        sim::GaitTask task, bool paired_legs, std::uint32_t candidate_runs,
        float candidate_strides, float candidate_distance,
        float candidate_survival, float mean_speed) noexcept
    {
        const auto runs = static_cast<std::uint16_t>(
            std::min<std::uint32_t>(candidate_runs, 65535u));
        const auto strides = static_cast<std::uint16_t>(
            std::min<float>(candidate_strides, 65535.0f));
        if (!paired_legs)
            return pack_quality(quality_bucket(candidate_distance), runs,
                strides, quality_bucket(candidate_survival));
        if (task == sim::GaitTask::walk)
            return pack_quality(strides, runs,
                quality_bucket(candidate_distance),
                quality_bucket(candidate_survival));

        // Speed Walk, transition, and run must select real translation before
        // cadence. Ranking strides first retains rapid in-place shuffling and
        // can leave the mastery graph parked at 80 percent indefinitely.
        return pack_quality(
            quality_bucket(candidate_distance),
            quality_bucket(std::max(0.0f, mean_speed), 100.0f),
            runs,
            strides);
    }

    [[nodiscard]] inline std::uint64_t shuttle_motion_quality(
        std::uint32_t completed_turns, float distance,
        std::uint32_t support_cycles, float elapsed_seconds) noexcept
    {
        return pack_quality(
            static_cast<std::uint16_t>(std::min<std::uint32_t>(
                completed_turns, 65535u)),
            quality_bucket(std::max(0.0f, distance)),
            static_cast<std::uint16_t>(std::min<std::uint32_t>(
                support_cycles, 65535u)),
            quality_bucket(elapsed_seconds));
    }

    [[nodiscard]] inline StageMotionQualification stage_motion_qualification(
        sim::CourseStage stage, const sim::Environment& environment) noexcept
    {
        std::uint32_t rejection = 0u;
        if (!environment.valid_motion())
            rejection |= evidence_bit(MotionEvidenceFailure::invalid_motion);
        if (environment.non_foot_grounded())
            rejection |= evidence_bit(MotionEvidenceFailure::body_contact);
        if (sim::stage_requires_forward_gait(stage)
            && !environment.blueprint().horizontal_body_plan()
            && environment.maximum_backward_brace_seconds()
                > sim::sustained_backward_brace_limit_seconds)
            rejection |= evidence_bit(MotionEvidenceFailure::backward_brace);

        switch (stage)
        {
        case sim::CourseStage::balance:
            if (environment.longest_stable_stance_seconds() < standing_qualification_seconds)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.maximum_upper_body_motor_deviation()
                > standing_neutral_arm_limit)
                rejection |= evidence_bit(MotionEvidenceFailure::non_neutral_posture);
            if (environment.blueprint().paired_leg_chains()
                && (environment.primary_support_span_ratio() < 0.55f
                    || environment.primary_support_span_ratio() > 1.65f))
                rejection |= evidence_bit(MotionEvidenceFailure::non_neutral_posture);
            if (environment.uncontrolled_spin_turns()
                > standing_qualification_spin_limit)
                rejection |= evidence_bit(MotionEvidenceFailure::excessive_rotation);
            if (environment.maximum_joint_speed() > 10.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;
        case sim::CourseStage::duck_press:
            if (!environment.duck_press_completed()
                || environment.duck_recoveries() < 1u
                || environment.duck_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.non_foot_grounded()
                || (!environment.left_supported() && !environment.right_supported()))
                rejection |= evidence_bit(MotionEvidenceFailure::body_contact);
            if (environment.longest_valid_crouch_seconds() < 0.55f)
                rejection |= evidence_bit(MotionEvidenceFailure::invalid_crouch_posture);
            if (environment.blueprint().paired_leg_chains()
                && (environment.primary_support_span_ratio() < 0.42f
                    || environment.primary_support_span_ratio() > 1.82f))
                rejection |= evidence_bit(MotionEvidenceFailure::non_neutral_posture);
            // The physical platen can create a brief solver angular
            // velocity while the rig remains intact, feet-only, held, and
            // recovered. Those stronger stage facts are authoritative here.
            break;
        case sim::CourseStage::shuttle:
        case sim::CourseStage::uneven:
            // A continuous gait transfers support faster than a standing hold.
            // Accept sustained alternating support as the dynamic counterpart
            // to the static stance proof instead of making a 1.2 Hz walk
            // satisfy a contradictory 0.75-second planted-foot requirement.
            if (environment.longest_stable_stance_seconds() < 0.75f
                && (environment.blueprint().paired_leg_chains()
                    ? (environment.alternating_steps() < 4u
                        || environment.limb_crossings() < 2u)
                    : environment.gait_cycles() < 4u))
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.blueprint().paired_leg_chains()
                && sim::crab_walking_motion(environment.alternating_steps(),
                    environment.limb_crossings(), environment.distance_travelled(),
                    environment.elapsed_seconds(),
                    environment.primary_support_span_ratio()))
                rejection |= evidence_bit(MotionEvidenceFailure::lateral_crab_gait);
            if (environment.blueprint().paired_leg_chains()
                && environment.maximum_lower_leg_scissor_seconds()
                    > sim::sustained_scissor_limit_seconds)
                rejection |= evidence_bit(MotionEvidenceFailure::lower_leg_scissor);
            // Qualification is the safe incremental checkpoint gate, not final
            // Walk mastery. Preserve a real two-step sagittal improvement so PPO
            // can build on it instead of discarding every policy below mastery.
            if (environment.blueprint().paired_leg_chains()
                ? (environment.alternating_steps() < 2u
                    || environment.limb_crossings() < 1u)
                : !multi_support_progress_truth(environment.distance_travelled(),
                    environment.gait_cycles(), environment.elapsed_seconds()))
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.0f
                || environment.elapsed_seconds() < 2.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            if (stage == sim::CourseStage::shuttle
                && environment.completed_shuttle_turns() < 1u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            break;
        case sim::CourseStage::crouch_walk:
            if (environment.longest_stable_stance_seconds() < 1.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.longest_valid_crouch_seconds() < 0.30f)
                rejection |= evidence_bit(MotionEvidenceFailure::invalid_crouch_posture);
            if (environment.gait_cycles() < 4u
                || (environment.blueprint().paired_leg_chains()
                    && environment.limb_crossings() < 4u)
                || environment.crouch_walk_seconds() < 2.0f
                || environment.crouch_walk_distance() < 0.75f
                || environment.obstacles_passed() < 3u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
        case sim::CourseStage::ramps:
            if (environment.longest_stable_stance_seconds() < 1.50f
                || environment.stable_stance_seconds() < 0.35f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.powered_jumps() < 1u || environment.landed_jumps() < 1u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            break;
        case sim::CourseStage::hurdles:
            if (environment.longest_stable_stance_seconds() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.alternating_steps() < 3u
                || (environment.blueprint().paired_leg_chains()
                    && environment.limb_crossings() < 3u)
                || environment.obstacles_passed() < 1u
                || (environment.duck_recoveries() < 1u && environment.landed_jumps() < 1u))
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.5f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
        case sim::CourseStage::duck_bars:
            if (environment.longest_stable_stance_seconds() < 1.0f
                || environment.stable_stance_seconds() < 0.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.spin_landings() < 1u
                || environment.maximum_flip_turns() < 0.75f
                || environment.maximum_flip_turns() > 3.05f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            break;
        case sim::CourseStage::moving_hazards:
            if (environment.longest_stable_stance_seconds() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.alternating_steps() < 3u || environment.obstacles_passed() < 1u
                || (environment.duck_recoveries() < 1u && environment.landed_jumps() < 1u
                    && environment.spin_landings() < 1u))
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 2.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
        case sim::CourseStage::climb_descent:
            if (environment.hand_ledge_contacts() < 1u
                || environment.climb_support_transfers() < 2u
                || environment.ledge_climbs() < 1u
                || environment.controlled_descents() < 1u
                || environment.powered_jumps() != 0u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            break;
        case sim::CourseStage::equipment_targets:
            if (environment.longest_stable_stance_seconds() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.target_hits() < 3u
                || environment.shots_fired() < environment.target_hits()
                || environment.equipment_transitions() < 1u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            break;
        case sim::CourseStage::combat_course:
            if (environment.longest_stable_stance_seconds() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.target_hits() < 2u || environment.gait_cycles() < 3u
                || environment.equipment_state() == sim::EquipmentState::dropped
                || environment.equipment_state() == sim::EquipmentState::disarmed)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 2.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
        }

        if (rejection != 0u)
            return { false, rejection, 0u };

        std::uint64_t quality = 0u;
        switch (stage)
        {
        case sim::CourseStage::balance:
            quality = pack_quality(
                quality_bucket(environment.longest_stable_stance_seconds()),
                quality_bucket(environment.stable_stance_seconds()),
                static_cast<std::uint16_t>(65535u
                    - quality_bucket(environment.maximum_upper_body_motor_deviation(), 1000.0f)),
                static_cast<std::uint16_t>(65535u
                    - quality_bucket(environment.uncontrolled_spin_turns(), 1000.0f)));
            break;
        case sim::CourseStage::duck_press:
            quality = pack_quality(
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.duck_recoveries(), 65535u)),
                quality_bucket(environment.duck_seconds()),
                quality_bucket(environment.stable_stance_seconds()),
                quality_bucket(environment.elapsed_seconds()));
            break;
        case sim::CourseStage::crouch_walk:
            quality = pack_quality(
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.gait_cycles(), 65535u)),
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.obstacles_passed(), 65535u)),
                quality_bucket(environment.crouch_walk_distance()),
                quality_bucket(environment.crouch_walk_seconds()));
            break;
        case sim::CourseStage::ramps:
            quality = pack_quality(
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.landed_jumps(), 65535u)),
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.powered_jumps(), 65535u)),
                quality_bucket(environment.stable_stance_seconds()),
                quality_bucket(environment.elapsed_seconds()));
            break;
        case sim::CourseStage::shuttle:
            quality = shuttle_motion_quality(
                environment.completed_shuttle_turns(),
                environment.distance_travelled(),
                environment.blueprint().paired_leg_chains()
                    ? environment.alternating_steps()
                    : environment.gait_cycles(),
                environment.elapsed_seconds());
            break;
        case sim::CourseStage::uneven:
        case sim::CourseStage::hurdles:
        case sim::CourseStage::moving_hazards:
            if (environment.blueprint().paired_leg_chains())
            {
                quality = pack_quality(
                    static_cast<std::uint16_t>(std::min<std::uint32_t>(
                        environment.alternating_steps(), 65535u)),
                    static_cast<std::uint16_t>(std::min<std::uint32_t>(
                        environment.obstacles_passed(), 65535u)),
                    quality_bucket(std::max(0.0f, environment.distance_travelled())),
                    quality_bucket(environment.elapsed_seconds()));
            }
            else
            {
                quality = pack_quality(
                    quality_bucket(std::max(0.0f, environment.distance_travelled())),
                    static_cast<std::uint16_t>(std::min<std::uint32_t>(
                        environment.obstacles_passed(), 65535u)),
                    static_cast<std::uint16_t>(std::min<std::uint32_t>(
                        environment.gait_cycles(), 65535u)),
                    quality_bucket(environment.elapsed_seconds()));
            }
            break;
        case sim::CourseStage::duck_bars:
            quality = pack_quality(
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.spin_landings(), 65535u)),
                quality_bucket(std::min(environment.maximum_flip_turns(), 3.0f), 100.0f),
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.landed_jumps(), 65535u)),
                quality_bucket(environment.elapsed_seconds()));
            break;
        case sim::CourseStage::climb_descent:
            quality = pack_quality(
                static_cast<std::uint16_t>(environment.controlled_descents()),
                static_cast<std::uint16_t>(environment.ledge_climbs()),
                static_cast<std::uint16_t>(environment.climb_support_transfers()),
                static_cast<std::uint16_t>(environment.hand_ledge_contacts()));
            break;
        case sim::CourseStage::equipment_targets:
            quality = pack_quality(
                static_cast<std::uint16_t>(environment.target_hits()),
                static_cast<std::uint16_t>(65535u - std::min<std::uint32_t>(
                    environment.shots_fired() - environment.target_hits(), 65535u)),
                static_cast<std::uint16_t>(environment.equipment_transitions()),
                quality_bucket(environment.longest_stable_stance_seconds()));
            break;
        case sim::CourseStage::combat_course:
            quality = pack_quality(
                static_cast<std::uint16_t>(environment.target_hits()),
                static_cast<std::uint16_t>(environment.gait_cycles()),
                quality_bucket(std::max(0.0f, environment.distance_travelled())),
                quality_bucket(environment.elapsed_seconds()));
            break;
        }
        return { true, 0u, quality };
    }

    [[nodiscard]] inline bool completed_episode_passes_stage_checks(
        sim::CourseStage stage, const sim::Environment& environment) noexcept
    {
        return environment.body_integrity_valid()
            && stage_motion_qualification(stage, environment).valid;
    }


    [[nodiscard]] inline bool stage_display_sample_eligible(sim::CourseStage stage,
        const sim::Environment& environment) noexcept
    {
        const StageMotionQualification qualification =
            stage_motion_qualification(stage, environment);
        if (!qualification.valid || !environment.body_integrity_valid())
            return false;
        if (stage == sim::CourseStage::balance)
        {
            // Qualification proves sustained support. Keep the current sample
            // visible through one solver-frame contact flicker while still
            // rejecting collapsed, arms-up, spinning, or broken frames.
            return environment.uprightness() >= 0.60f
                && environment.maximum_upper_body_motor_deviation()
                    <= standing_neutral_arm_limit
                && environment.uncontrolled_spin_turns()
                    <= standing_qualification_spin_limit
                && (!environment.blueprint().paired_leg_chains()
                    || (environment.primary_support_span_ratio() >= 0.55f
                        && environment.primary_support_span_ratio() <= 1.65f));
        }
        if (stage == sim::CourseStage::duck_press)
        {
            return environment.duck_press_completed()
                && !environment.non_foot_grounded()
                && environment.duck_recoveries() >= 1u
                && environment.duck_seconds() >= 0.75f
                && environment.uprightness() >= 0.60f
                && (environment.left_supported() || environment.right_supported());
        }
        if (stage == sim::CourseStage::crouch_walk)
        {
            return environment.duck_active()
                && !environment.non_foot_grounded()
                && environment.uprightness() >= 0.60f
                && environment.crouch_walk_seconds() >= 0.35f
                && environment.gait_cycles() >= 1u
                && (environment.left_supported() || environment.right_supported());
        }
        return environment.valid_motion() && environment.uprightness() >= 0.45f;
    }

    [[nodiscard]] inline bool training_preview_frame_renderable(
        const sim::Environment& environment) noexcept
    {
        const auto particles = environment.particles();
        if (!environment.blueprint().valid() || particles.empty()
            || particles.size() != environment.blueprint().nodes.size())
            return false;
        for (const sim::Particle& particle : particles)
        {
            if (!std::isfinite(particle.position.x) || !std::isfinite(particle.position.y)
                || !std::isfinite(particle.previous.x) || !std::isfinite(particle.previous.y))
                return false;
        }
        return true;
    }

    [[nodiscard]] inline int training_preview_priority(sim::CourseStage stage,
        const sim::Environment& environment) noexcept
    {
        if (!training_preview_frame_renderable(environment))
            return 0;
        if (stage_display_sample_eligible(stage, environment))
            return 4;
        if (environment.body_integrity_valid()
            && stage_motion_qualification(stage, environment).valid)
            return 3;
        if (environment.body_integrity_valid())
            return 2;
        return 1;
    }

    [[nodiscard]] inline bool incremental_locomotion_candidate(
        sim::CourseStage stage, bool valid_motion, bool body_integrity,
        bool non_foot_grounded, bool paired_legs,
        std::uint32_t alternating_steps, std::uint32_t limb_crossings,
        float distance, float survival_seconds, float support_span_ratio) noexcept
    {
        if (!sim::stage_requires_forward_gait(stage) || !valid_motion
            || !body_integrity || non_foot_grounded || alternating_steps == 0u
            || distance < 0.10f || survival_seconds < 0.75f)
            return false;
        if (!paired_legs)
            return multi_support_progress_truth(distance, alternating_steps,
                survival_seconds);
        if (limb_crossings == 0u)
            return false;
        return !sim::crab_walking_motion(alternating_steps, limb_crossings,
            distance, survival_seconds, support_span_ratio);
    }

    [[nodiscard]] inline bool incremental_locomotion_candidate(
        sim::CourseStage stage, const sim::Environment& environment) noexcept
    {
        return environment.maximum_lower_leg_scissor_seconds()
                <= sim::sustained_scissor_limit_seconds
            && environment.maximum_backward_brace_seconds()
                <= sim::sustained_backward_brace_limit_seconds
            && incremental_locomotion_candidate(stage,
                environment.valid_motion(), environment.body_integrity_valid(),
                environment.non_foot_grounded(),
                environment.blueprint().paired_leg_chains(),
                environment.gait_cycles(), environment.limb_crossings(),
                environment.distance_travelled(), environment.elapsed_seconds(),
                environment.primary_support_span_ratio());
    }

    inline constexpr std::uint64_t strict_evaluation_quality_bit = 1ull << 63u;

    [[nodiscard]] inline bool strict_evaluation_quality(
        std::uint64_t quality) noexcept
    {
        return (quality & strict_evaluation_quality_bit) != 0u;
    }

    [[nodiscard]] inline bool policy_candidate_retainable(
        sim::CourseStage stage, std::uint64_t quality) noexcept
    {
        return !sim::stage_requires_forward_gait(stage)
            || strict_evaluation_quality(quality);
    }

    [[nodiscard]] inline bool policy_candidate_retainable(
        sim::CourseStage stage, std::uint64_t quality,
        std::uint64_t lesson_update,
        const sim::CreatureBlueprint& blueprint) noexcept
    {
        static_cast<void>(lesson_update);
        static_cast<void>(blueprint);
        // Once a forward-gait candidate is eligible for retention it becomes
        // the user-visible and checkpointed proof. Partial distance/cadence
        // evidence is useful for PPO progress, but cannot truthfully replace a
        // controller that passed the complete deterministic evaluation gate.
        return policy_candidate_retainable(stage, quality);
    }

    [[nodiscard]] inline bool policy_candidate_better(std::uint64_t quality,
        float score, std::uint64_t best_quality, float best_score, bool has_best) noexcept
    {
        if (!has_best)
            return quality != 0u && std::isfinite(score);
        if (quality != best_quality)
            return quality > best_quality;
        const float improvement_margin = std::max(0.015f, std::abs(best_score) * 0.004f);
        return std::isfinite(score) && score > best_score + improvement_margin;
    }

    [[nodiscard]] inline bool elite_motion_eligible(sim::CourseStage stage,
        const sim::Environment& environment) noexcept
    {
        return environment.body_integrity_valid()
            && stage_motion_qualification(stage, environment).valid;
    }

    [[nodiscard]] inline bool elite_motion_eligible(sim::CourseStage stage, bool valid_motion,
        std::uint32_t alternating_steps, float distance, float survival_seconds,
        float duck_seconds = 0.0f, std::uint32_t landed_jumps = 0u,
        float maximum_spin_turns = 0.0f, std::uint32_t spin_landings = 0u,
        std::uint32_t obstacles_passed = 0u) noexcept
    {
        if (!valid_motion)
            return false;
        if (stage == sim::CourseStage::balance)
            return survival_seconds >= 3.0f;
        if (!sim::stage_skill_evidence(stage, alternating_steps, duck_seconds,
            landed_jumps, maximum_spin_turns, spin_landings, obstacles_passed))
            return false;
        return sim::stage_requires_forward_gait(stage) ? distance >= 0.60f : true;
    }

    enum class ControllerState : std::uint8_t
    {
        fresh,
        training,
        resumed,
        transferred
    };

    class PolicyNetwork
    {
    public:
        static constexpr std::size_t hidden_size = 64;
        static constexpr std::size_t input_size = sim::observation_count;
        static constexpr std::size_t output_size = sim::action_count;

        struct Evaluation
        {
            std::array<float, output_size> mean{};
            float value{};
        };

        PolicyNetwork();
        explicit PolicyNetwork(std::uint64_t seed);

        [[nodiscard]] Evaluation evaluate(std::span<const float, input_size> observation) const noexcept;
        [[nodiscard]] std::array<float, output_size> deterministic_action(
            std::span<const float, input_size> observation) const noexcept;
        [[nodiscard]] std::array<float, output_size> deterministic_action(
            std::span<const float, input_size> observation,
            const ActionMask& action_mask) const noexcept;
        [[nodiscard]] std::array<float, output_size> sample_action(
            const Evaluation& evaluation, std::uint64_t& random_state,
            float& log_probability) const noexcept;
        [[nodiscard]] std::size_t parameter_count() const noexcept { return parameters_.size(); }
        [[nodiscard]] const std::vector<float>& parameters() const noexcept { return parameters_; }
        [[nodiscard]] std::vector<float>& parameters() noexcept { return parameters_; }
        [[nodiscard]] static std::uint64_t active_actor_parameter_digest(
            std::span<const float> parameters,
            const ActionMask& action_mask) noexcept
        {
            if (parameters.size() != layout_.total)
                return 0u;
            std::uint64_t hash = provenance_hash_u64(1469598103934665603ULL,
                action_mask_bits(action_mask));
            for (std::size_t index = layout_.w1; index < layout_.actor_w; ++index)
                hash = provenance_hash_u64(hash, std::bit_cast<std::uint32_t>(
                    parameters[index]));
            for (std::size_t output = 0; output < output_size; ++output)
            {
                if (!action_mask[output])
                    continue;
                const std::size_t base = layout_.actor_w + output * hidden_size;
                for (std::size_t column = 0; column < hidden_size; ++column)
                    hash = provenance_hash_u64(hash, std::bit_cast<std::uint32_t>(
                        parameters[base + column]));
                hash = provenance_hash_u64(hash, std::bit_cast<std::uint32_t>(
                    parameters[layout_.actor_b + output]));
            }
            return hash;
        }
        [[nodiscard]] std::uint64_t active_actor_parameter_digest() const noexcept
        {
            return active_actor_parameter_digest(parameters_, active_action_mask_);
        }

        void zero_gradients() noexcept;
        void accumulate_gradient(
            std::span<const float, input_size> observation,
            std::span<const float, output_size> action,
            float old_log_probability,
            float advantage,
            float target_value,
            float clip_range,
            float value_coefficient,
            float entropy_coefficient,
            float& policy_loss,
            float& value_loss,
            float& entropy) noexcept;
        void accumulate_imitation_gradient(
            std::span<const float, input_size> observation,
            std::span<const float, output_size> target_action,
            float weight,
            float& imitation_loss) noexcept;

        [[nodiscard]] const std::vector<float>& gradients() const noexcept { return gradients_; }
        [[nodiscard]] std::vector<float>& gradients() noexcept { return gradients_; }
        [[nodiscard]] std::array<float, output_size> standard_deviation() const noexcept;
        void set_exploration(float standard_deviation) noexcept;
        void set_action_mask(const ActionMask& mask) noexcept
        {
            active_action_mask_ = mask;
        }
        [[nodiscard]] const ActionMask& action_mask() const noexcept
        {
            return active_action_mask_;
        }
        [[nodiscard]] bool action_active(std::size_t index) const noexcept
        {
            return index < active_action_mask_.size()
                && active_action_mask_[index];
        }
        [[nodiscard]] std::size_t active_output_count() const noexcept
        {
            return static_cast<std::size_t>(std::count(active_action_mask_.begin(),
                active_action_mask_.end(), true));
        }
        void neutralize_action_slot(std::size_t slot) noexcept;
        void clear_action_slot_state(std::vector<float>& state,
            std::size_t slot) const noexcept;
        [[nodiscard]] float mean_exploration() const noexcept;
        [[nodiscard]] float log_probability(
            std::span<const float, output_size> action,
            const Evaluation& evaluation) const noexcept;

        [[nodiscard]] bool save(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] bool load(const std::filesystem::path& path, std::string& error);

    private:
        struct Layout
        {
            std::size_t w1{};
            std::size_t b1{};
            std::size_t w2{};
            std::size_t b2{};
            std::size_t actor_w{};
            std::size_t actor_b{};
            std::size_t value_w{};
            std::size_t value_b{};
            std::size_t log_std{};
            std::size_t total{};
        };

        [[nodiscard]] static consteval Layout make_layout() noexcept
        {
            Layout result{};
            result.w1 = 0;
            result.b1 = result.w1 + hidden_size * input_size;
            result.w2 = result.b1 + hidden_size;
            result.b2 = result.w2 + hidden_size * hidden_size;
            result.actor_w = result.b2 + hidden_size;
            result.actor_b = result.actor_w + output_size * hidden_size;
            result.value_w = result.actor_b + output_size;
            result.value_b = result.value_w + hidden_size;
            result.log_std = result.value_b + 1;
            result.total = result.log_std + output_size;
            return result;
        }
        [[nodiscard]] float random_normal() noexcept;

        static const Layout layout_;
        std::vector<float> parameters_{};
        std::vector<float> gradients_{};
        std::uint64_t random_state_{ 1 };
        ActionMask active_action_mask_{ default_action_mask() };
    };

    class PpoTrainer
    {
        friend class AutonomousTrainer;
        // Installed once, before AutonomousTrainer begins training.
        std::function<void(const sim::Environment&)> rollout_sample_sink_{};
    public:
        void set_preview_guidance_mode(sim::GuidanceMode mode) noexcept
        {
            preview_guidance_mode_ = mode;
            preview_.set_guidance_mode(mode);
            preview_.set_physical_guidance_authority(
                mode == sim::GuidanceMode::assisted
                ? lesson_teacher_authority(lesson_update_, course_stage_,
                    preview_.blueprint())
                : 0.0f);
            preview_accumulator_seconds_ = 0.0;
        }
        [[nodiscard]] sim::GuidanceMode preview_guidance_mode() const noexcept
        {
            return preview_guidance_mode_;
        }
        [[nodiscard]] sim::GuidanceMode best_policy_guidance_mode() const noexcept
        {
            return best_policy_guidance_mode_;
        }
        [[nodiscard]] GuidanceAuthorityReport preview_authority_report() const noexcept
        {
            return guidance_authority_report(preview_, course_stage_,
                lesson_teacher_authority(lesson_update_, course_stage_, blueprint_));
        }
        class CheckpointData
        {
        public:
            CheckpointData() = default;
            CheckpointData(const CheckpointData&) = default;
            CheckpointData(CheckpointData&&) noexcept = default;
            CheckpointData& operator=(const CheckpointData&) = default;
            CheckpointData& operator=(CheckpointData&&) noexcept = default;
            std::uint32_t training_semantics{ training_semantics_version };
            std::uint64_t rig_signature{};
            std::vector<float> parameters{};
            std::vector<float> first_moment{};
            std::vector<float> second_moment{};
            std::vector<float> best_parameters{};
            sim::GuidanceMode best_guidance_mode{ sim::GuidanceMode::assisted };
            RawPpoReceipt raw_ppo_receipt{};
            std::uint64_t parameters_digest{};
            std::uint64_t best_parameters_digest{};
            std::uint64_t whole_content_digest{};
            std::vector<float> reward_history{};
            std::vector<float> speed_history{};
            std::uint64_t optimizer_step{};
            std::uint64_t random_state{};
            std::uint64_t lesson_update{};
            TrainingMetrics metrics{};
            sim::CourseStage stage{ sim::CourseStage::balance };
            sim::GaitTask gait_task{ sim::GaitTask::walk };
            sim::EquipmentDirective equipment_directive{
                sim::EquipmentDirective::passive };
            std::uint32_t action_mask_bits{};
            float difficulty{ 0.25f };

        private:
            friend class PpoTrainer;
            friend struct RawLearningTestAccess;
            enum class Origin : std::uint8_t
            {
                none,
                live_trainer,
                exact_storage,
                legacy_transfer
            };
            Origin origin_{ Origin::none };
            std::uint64_t artifact_seal_{};
        };

        class VerifiedCheckpoint
        {
        public:
            VerifiedCheckpoint() = default;
            VerifiedCheckpoint(const VerifiedCheckpoint&) = default;
            VerifiedCheckpoint(VerifiedCheckpoint&&) noexcept = default;
            VerifiedCheckpoint& operator=(const VerifiedCheckpoint&) = default;
            VerifiedCheckpoint& operator=(VerifiedCheckpoint&&) noexcept = default;
            [[nodiscard]] bool valid() const noexcept { return data_ != nullptr; }
            [[nodiscard]] const CheckpointData& data() const noexcept
            {
                return *data_;
            }

        private:
            friend class PpoTrainer;
            explicit VerifiedCheckpoint(CheckpointData data)
                : data_(std::make_shared<const CheckpointData>(std::move(data)))
            {
            }
            std::shared_ptr<const CheckpointData> data_{};
        };
        explicit PpoTrainer(const sim::CreatureBlueprint& blueprint,
            std::size_t environment_count = 64,
            bool enable_rollout_workers = true,
            std::size_t maximum_rollout_workers = 0u);
        ~PpoTrainer();

        PpoTrainer(const PpoTrainer&) = delete;
        PpoTrainer& operator=(const PpoTrainer&) = delete;

        void set_blueprint(const sim::CreatureBlueprint& blueprint, bool preserve_policy = false);
        void set_course(sim::CourseStage stage, float difficulty, bool preserve_best = true);
        void set_gait_task(sim::GaitTask task, bool preserve_best = true);
        void set_equipment_directive(sim::EquipmentDirective directive) noexcept;
        void reset_policy(std::uint64_t seed = 0xC0FFEEu,
            bool clear_totals = false);
        void set_exploration(float standard_deviation) noexcept;
        void neutralize_action_slot(std::size_t slot) noexcept
        {
            policy_.neutralize_action_slot(slot);
            preview_policy_.neutralize_action_slot(slot);
            policy_.clear_action_slot_state(adam_.first_moment, slot);
            policy_.clear_action_slot_state(adam_.second_moment, slot);
        }
        void set_cpu_mode(int mode) noexcept;
        [[nodiscard]] int cpu_mode() const noexcept { return cpu_mode_; }
        [[nodiscard]] bool save_checkpoint(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] bool load_checkpoint(const std::filesystem::path& path, std::string& error,
            bool transfer_only = false);
        [[nodiscard]] CheckpointData checkpoint_data() const;
        [[nodiscard]] VerifiedCheckpoint verified_checkpoint() const;
        [[nodiscard]] static bool write_checkpoint_data(const CheckpointData& data,
            const std::filesystem::path& path, std::string& error,
            std::uint64_t* written_content_digest = nullptr);
        [[nodiscard]] static bool read_checkpoint_data(const std::filesystem::path& path,
            CheckpointData& data, std::string& error);
        [[nodiscard]] static bool read_verified_checkpoint(
            const std::filesystem::path& path, VerifiedCheckpoint& checkpoint,
            std::string& error);
        [[nodiscard]] static CheckpointData retarget_checkpoint_for_rig(
            CheckpointData data, std::uint64_t rig_signature) noexcept;
        [[nodiscard]] bool apply_checkpoint_data(CheckpointData data, std::string& error,
            bool transfer_only = false);
        [[nodiscard]] bool apply_verified_checkpoint(
            const VerifiedCheckpoint& checkpoint, std::string& error);
        [[nodiscard]] bool import_lifetime_ledger(const TrainingMetrics& lifetime,
            std::string& error) noexcept;
        [[nodiscard]] bool restore_best_policy() noexcept;
        [[nodiscard]] bool evaluate_retained_policy() noexcept;
        void begin_staged_update();
        void compute_staged_advantages();
        void optimize_staged_update();
        void finish_staged_update();
        [[nodiscard]] bool staged_update_active() const noexcept { return staged_update_active_; }
        void train_one_update();
        void step_preview(float dt = 1.0f / 60.0f);
        void reset_preview(std::uint64_t seed = 0xDEADBEEFu) noexcept;
        void configure_preview_equipment(sim::WeaponClass weapon,
            float target_distance = 8.0f);
        void set_preview_course_motion_enabled(bool enabled) noexcept
        {
            preview_.set_course_motion_enabled(enabled);
        }
        [[nodiscard]] std::uint64_t preview_reset_count() const noexcept
        {
            return preview_reset_sequence_;
        }
        [[nodiscard]] sim::InvalidMotion preview_last_reset_reason() const noexcept
        {
            return preview_last_reset_reason_;
        }
        [[nodiscard]] sim::TrialTerminalCause preview_terminal_cause() const noexcept
        {
            return preview_terminal_cause_;
        }
        [[nodiscard]] bool preview_terminal() const noexcept
        {
            return preview_terminal_;
        }
        [[nodiscard]] std::uint64_t preview_trial_id() const noexcept
        {
            return preview_trial_id_;
        }
        [[nodiscard]] Vec2 preview_terminal_position() const noexcept
        {
            return preview_terminal_position_;
        }
        [[nodiscard]] sim::TerrainRegion preview_terminal_terrain() const noexcept
        {
            return preview_terminal_terrain_;
        }
        [[nodiscard]] float preview_terminal_water_depth() const noexcept
        {
            return preview_terminal_water_depth_;
        }
        [[nodiscard]] bool has_training_terminal_preview() const noexcept
        {
            return retained_training_preview_.has_value();
        }
        [[nodiscard]] const sim::Environment& training_terminal_preview() const noexcept
        {
            return *retained_training_preview_;
        }
        [[nodiscard]] std::uint64_t training_terminal_trial_id() const noexcept
        {
            return retained_training_trial_id_;
        }
        [[nodiscard]] sim::InvalidMotion training_terminal_reason() const noexcept
        {
            return retained_training_reason_;
        }
        [[nodiscard]] sim::TrialTerminalCause training_terminal_cause() const noexcept
        {
            return retained_training_cause_;
        }
        [[nodiscard]] Vec2 training_terminal_position() const noexcept
        {
            return retained_training_position_;
        }
        [[nodiscard]] sim::TerrainRegion training_terminal_terrain() const noexcept
        {
            return retained_training_terrain_;
        }
        [[nodiscard]] float training_terminal_water_depth() const noexcept
        {
            return retained_training_water_depth_;
        }

        [[nodiscard]] const PolicyNetwork& policy() const noexcept { return policy_; }
        [[nodiscard]] PolicyNetwork& policy() noexcept { return policy_; }
        [[nodiscard]] const ActionMask& preview_action_mask() const noexcept
        {
            return preview_policy_.action_mask();
        }
        [[nodiscard]] const sim::Environment& preview() const noexcept { return preview_; }
        [[nodiscard]] const sim::CreatureBlueprint& blueprint() const noexcept { return blueprint_; }
        [[nodiscard]] const TrainingMetrics& metrics() const noexcept { return metrics_; }
        [[nodiscard]] const std::vector<float>& reward_history() const noexcept { return reward_history_; }
        [[nodiscard]] const std::vector<float>& speed_history() const noexcept { return speed_history_; }
        [[nodiscard]] std::size_t environment_count() const noexcept { return environments_.size(); }
        [[nodiscard]] std::span<const sim::Environment> environments() const noexcept { return environments_; }
        [[nodiscard]] ControllerState controller_state() const noexcept { return controller_state_; }
        [[nodiscard]] std::string_view controller_state_name() const noexcept;
        [[nodiscard]] std::uint64_t rig_signature() const noexcept { return blueprint_.signature(); }
        [[nodiscard]] bool has_best_policy() const noexcept { return !best_parameters_.empty(); }
        [[nodiscard]] const std::vector<float>& best_policy_parameters() const noexcept
        {
            return best_parameters_;
        }
        [[nodiscard]] const RawPpoReceipt& raw_ppo_receipt() const noexcept
        {
            return best_raw_ppo_receipt_;
        }
        [[nodiscard]] std::uint64_t optimizer_step() const noexcept { return adam_.step; }
        [[nodiscard]] float exploration() const noexcept { return policy_.mean_exploration(); }
        [[nodiscard]] std::size_t rollout_worker_count() const noexcept { return active_worker_count_; }
        [[nodiscard]] std::size_t maximum_worker_count() const noexcept { return rollout_worker_count_; }
        [[nodiscard]] sim::CourseStage course_stage() const noexcept { return course_stage_; }
        [[nodiscard]] sim::GaitTask gait_task() const noexcept { return gait_task_; }
        [[nodiscard]] sim::EquipmentDirective equipment_directive() const noexcept
        {
            return equipment_directive_;
        }
        [[nodiscard]] float course_difficulty() const noexcept { return course_difficulty_; }
        [[nodiscard]] std::uint64_t lesson_update() const noexcept
        {
            return lesson_update_;
        }
        [[nodiscard]] std::size_t self_imitation_sample_count() const noexcept
        {
            return self_imitation_prior_.size();
        }
        [[nodiscard]] std::size_t foundational_teacher_sample_count() const noexcept
        {
            return foundational_teacher_prior_.size();
        }

    private:
        friend struct RawLearningTestAccess;
        bool autopilot_unaided_stand_{};
        std::uint64_t verified_stand_parameters_{};
        [[nodiscard]] bool trains_unaided_stand() const noexcept
        {
            return autopilot_unaided_stand_
                && course_stage_ == sim::CourseStage::balance
                && blueprint_.human_casual_gait_plan();
        }
        [[nodiscard]] bool verified_unaided_stand() const noexcept
        {
            return trains_unaided_stand() && verified_stand_parameters_ != 0u
                && verified_stand_parameters_ == parameter_digest(policy_.parameters());
        }
        struct Transition
        {
            std::array<float, sim::observation_count> observation{};
            std::array<float, sim::action_count> action{};
            std::array<float, sim::action_count> guided_action{};
            float log_probability{};
            float value{};
            float reward{};
            float advantage{};
            float return_value{};
            float requested_direction{};
            bool terminal{};
            bool stop_requested{};
            bool foundational_private_phase_scrubbed{};
            RolloutOrigin origin{ RolloutOrigin::unspecified };
        };

        struct ImitationSample
        {
            std::array<float, sim::observation_count> observation{};
            std::array<float, sim::action_count> action{};
        };

        struct AdamState
        {
            std::vector<float> first_moment{};
            std::vector<float> second_moment{};
            std::uint64_t step{};
        };

        struct RolloutTotals
        {
            float accumulated_speed{};
            float completed_reward{};
            float completed_distance{};
            std::uint64_t completed_episodes{};
            std::uint64_t valid_episodes{};
            std::uint64_t invalid_episodes{};
            std::uint64_t alternating_steps{};
            std::uint64_t falls{};
            std::uint64_t collisions{};
            std::uint64_t powered_jumps{};
            std::uint64_t landed_jumps{};
            std::uint64_t landed_flips{};
            std::uint64_t obstacles_passed{};
            double total_distance{};
            std::optional<sim::Environment> terminal_preview{};
            sim::InvalidMotion terminal_reason{ sim::InvalidMotion::none };
            sim::TrialTerminalCause terminal_cause{ sim::TrialTerminalCause::none };
            Vec2 terminal_position{};
            sim::TerrainRegion terminal_terrain{ sim::TerrainRegion::firm };
            float terminal_water_depth{};
            float terminal_progress{ -std::numeric_limits<float>::infinity() };
        };

        struct ParallelState;

        [[nodiscard]] float random_uniform() noexcept;
        [[nodiscard]] float random_normal() noexcept;
        [[nodiscard]] std::array<float, sim::action_count> sample_action(
            const PolicyNetwork::Evaluation& evaluation,
            std::uint64_t& random_state,
            float& log_probability) const noexcept;
        void update_policy();
        void evaluate_policy(std::uint64_t evaluation_sequence = 0u);
        void refresh_self_imitation_prior();
        void clear_self_imitation_prior() noexcept;
        void apply_self_imitation_prior();
        void refresh_foundational_teacher_prior();
        void clear_foundational_teacher_prior() noexcept;
        void reset_training_state(bool clear_best = true,
            bool clear_totals = false) noexcept;
        void transition_action_mask(const ActionMask& action_mask) noexcept;
        void apply_adam(float learning_rate, float gradient_scale);
        void append_history(std::vector<float>& history, float value);
        [[nodiscard]] RolloutTotals collect_rollout_partition(std::size_t worker_index,
            std::size_t worker_count, std::uint64_t update_seed);
        void rollout_worker_main(std::size_t worker_index, std::stop_token stop_token);

        void initialize_parallel_workers();
        void shutdown_parallel_workers() noexcept;
        void parallel_accumulate_batch(
            const std::vector<std::size_t>& indices,
            std::size_t begin,
            std::size_t end,
            float clip_range,
            float value_coefficient,
            float entropy_coefficient,
            float& policy_loss,
            float& value_loss,
            float& entropy);
        void parallel_evaluate_policy(std::uint64_t evaluation_sequence);

        static constexpr std::size_t rollout_horizon = 128;

        sim::CreatureBlueprint blueprint_{};
        std::vector<sim::Environment> environments_{};
        sim::Environment preview_{};
        PolicyNetwork policy_{};
        PolicyNetwork preview_policy_{};
        AdamState adam_{};
        std::vector<Transition> rollout_{};
        std::vector<float> episode_rewards_{};
        std::vector<float> episode_distances_{};
        std::vector<std::array<float, sim::action_count>> rollout_previous_actions_{};
        std::vector<float> reward_history_{};
        std::vector<float> speed_history_{};
        std::vector<float> best_parameters_{};
        sim::GuidanceMode best_policy_guidance_mode_{ sim::GuidanceMode::assisted };
        RawPpoReceipt best_raw_ppo_receipt_{};
        std::vector<ImitationSample> self_imitation_prior_{};
        std::vector<ImitationSample> foundational_teacher_prior_{};
        float self_imitation_source_score_{ -std::numeric_limits<float>::infinity() };
        TrainingMetrics metrics_{};
        ControllerState controller_state_{ ControllerState::fresh };
        sim::CourseStage course_stage_{ sim::CourseStage::balance };
        sim::GaitTask gait_task_{ sim::GaitTask::walk };
        sim::EquipmentDirective equipment_directive_{ sim::EquipmentDirective::passive };
        float course_difficulty_{ 0.25f };
        std::uint64_t lesson_update_{};
        int cpu_mode_{ 4 };
        std::size_t active_worker_count_{ 1 };
        std::size_t rollout_worker_count_{ 1 };
        std::size_t rollout_active_worker_count_{ 1 };
        std::vector<RolloutTotals> rollout_worker_totals_{};
        std::mutex rollout_mutex_{};
        std::condition_variable_any rollout_start_cv_{};
        std::condition_variable rollout_done_cv_{};
        std::uint64_t rollout_generation_{};
        std::uint64_t rollout_update_seed_{};
        std::size_t rollout_completed_{};
        std::uint64_t random_state_{ 0x12345678ABCDEFu };
        std::uint64_t preview_reset_sequence_{};
        sim::TrialTerminalCause preview_terminal_cause_{ sim::TrialTerminalCause::none };
        sim::InvalidMotion preview_last_reset_reason_{ sim::InvalidMotion::none };
        std::uint64_t preview_trial_id_{ 1u };
        bool preview_terminal_{};
        Vec2 preview_terminal_position_{};
        sim::TerrainRegion preview_terminal_terrain_{ sim::TerrainRegion::firm };
        float preview_terminal_water_depth_{};
        std::optional<sim::Environment> retained_training_preview_{};
        std::uint64_t retained_training_trial_id_{};
        std::uint64_t retained_training_trial_sequence_{};
        sim::InvalidMotion retained_training_reason_{ sim::InvalidMotion::none };
        sim::TrialTerminalCause retained_training_cause_{ sim::TrialTerminalCause::none };
        Vec2 retained_training_position_{};
        sim::TerrainRegion retained_training_terrain_{ sim::TerrainRegion::firm };
        float retained_training_water_depth_{};
        double preview_accumulator_seconds_{};
        bool preview_equipment_test_enabled_{};
        sim::GuidanceMode preview_guidance_mode_{ sim::GuidanceMode::assisted };
        std::vector<std::jthread> rollout_workers_{};
        std::shared_ptr<ParallelState> parallel_{};
        RolloutTotals staged_totals_{};
        static void seal_checkpoint(CheckpointData& data,
            CheckpointData::Origin origin) noexcept;
        [[nodiscard]] static bool checkpoint_seal_valid(
            const CheckpointData& data) noexcept;
        void reset_raw_handoff_boundary() noexcept;

        detail::RawPpoUpdateEvidence staged_raw_ppo_evidence_{};
        RawPpoReceipt last_raw_ppo_receipt_{};
        std::uint64_t last_ppo_raw_actor_samples_{};
        std::uint64_t last_ppo_assisted_actor_samples_{};
        std::uint64_t last_guided_raw_sources_rejected_{};
        std::uint64_t last_guided_assisted_sources_consumed_{};
        bool raw_handoff_reset_complete_{};
        bool staged_update_active_{};
        bool staged_advantages_ready_{};
        bool staged_optimized_{};
    };
}
