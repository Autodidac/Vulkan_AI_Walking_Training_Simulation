#pragma once

#include "simulation.hpp"
#include "locomotion_strategy.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace runner::rl
{
    inline constexpr std::uint32_t training_semantics_version = 0x0007'2601u;

    [[nodiscard]] inline bool motor_drives_support_branch(
        const sim::CreatureBlueprint& rig,
        const sim::MotorConstraint& motor) noexcept;

    [[nodiscard]] inline std::array<float, sim::action_count> balance_teacher_action(
        const sim::Environment& environment) noexcept
    {
        constexpr std::size_t joint_angle_begin = 4;
        constexpr std::size_t joint_velocity_begin = joint_angle_begin + sim::action_count;
        static_assert(sim::observation_count == 50);
        const auto observation = environment.observation();
        std::array<float, sim::action_count> action{};
        const sim::CreatureBlueprint& rig = environment.blueprint();

        for (std::size_t index = 0; index < rig.active_motor_count; ++index)
        {
            const float joint_offset = observation[joint_angle_begin + index];
            const float joint_speed = observation[joint_velocity_begin + index];
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
            const float correction = clamp(observation[0] * 0.32f
                + observation[2] * 0.05f, -0.15f, 0.15f);
            action[0] = clamp(action[0] - correction, -0.44f, 0.44f);
            action[1] = clamp(action[1] + correction * 0.16f, -0.44f, 0.44f);
            action[2] = clamp(action[2] - correction, -0.44f, 0.44f);
            action[3] = clamp(action[3] - correction * 0.16f, -0.44f, 0.44f);
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
        probe.weight = 0.88f;
        return probe;
    }

    [[nodiscard]] inline float motor_action_for_target_angle(
        const sim::MotorConstraint& motor, float target_angle) noexcept
    {
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

    [[nodiscard]] inline std::uint8_t motor_support_mask(
        const sim::CreatureBlueprint& rig,
        const sim::MotorConstraint& motor) noexcept
    {
        if (!motor.enabled || motor.pivot >= rig.nodes.size()
            || motor.c >= rig.nodes.size() || rig.nodes.size() > 128u)
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
            const std::uint16_t node = stack[--stack_size];
            if (rig.is_left_support_seed(node))
                mask = static_cast<std::uint8_t>(mask | 0x1u);
            if (rig.is_right_support_seed(node))
                mask = static_cast<std::uint8_t>(mask | 0x2u);
            for (const sim::DistanceConstraint& bone : rig.bones)
            {
                if (bone.stiffness < 0.20f)
                    continue;
                std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                if (bone.a == node)
                    next = bone.b;
                else if (bone.b == node)
                    next = bone.a;
                if (next < rig.nodes.size() && !visited[next])
                {
                    visited[next] = true;
                    stack[stack_size++] = next;
                }
            }
        }
        return mask;
    }

    [[nodiscard]] inline bool motor_drives_support_branch(
        const sim::CreatureBlueprint& rig,
        const sim::MotorConstraint& motor) noexcept
    {
        return motor_support_mask(rig, motor) != 0u;
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
            const float shared_hip_flex = 0.5f
                * (std::max(0.0f, -action[0]) + std::max(0.0f, action[2]));
            const float shared_knee_flex = 0.5f
                * (std::max(0.0f, action[1]) + std::max(0.0f, -action[3]));
            constexpr float chain_strength = 0.88f;
            action[0] = lerp(action[0], -shared_hip_flex, chain_strength);
            action[1] = lerp(action[1], shared_knee_flex, chain_strength);
            action[2] = lerp(action[2], shared_hip_flex, chain_strength);
            action[3] = lerp(action[3], -shared_knee_flex, chain_strength);
        }
        else if (stage != sim::CourseStage::balance)
        {
            const float pair_strength = stage == sim::CourseStage::crouch_walk
                ? 0.18f : 0.10f;
            const float hip_mirror = 0.5f * (action[0] - action[2]);
            const float knee_mirror = 0.5f * (action[1] - action[3]);
            action[0] = lerp(action[0], hip_mirror, pair_strength);
            action[2] = lerp(action[2], -hip_mirror, pair_strength);
            action[1] = lerp(action[1], knee_mirror, pair_strength);
            action[3] = lerp(action[3], -knee_mirror, pair_strength);
        }

        if (rig.active_motor_count >= 8u)
        {
            const float arm_pair_strength = sim::stage_allows_controlled_flips(stage)
                ? 0.24f : 0.06f;
            const float shoulder = 0.5f * (action[4] - action[6]);
            const float elbow = 0.5f * (action[5] - action[7]);
            action[4] = lerp(action[4], shoulder, arm_pair_strength);
            action[6] = lerp(action[6], -shoulder, arm_pair_strength);
            action[5] = lerp(action[5], elbow, arm_pair_strength);
            action[7] = lerp(action[7], -elbow, arm_pair_strength);
        }

        if (rig.active_motor_count >= 8u
            && environment.longest_stable_stance_seconds() < 1.0f
            && !sim::stage_allows_controlled_flips(stage))
        {
            for (std::size_t index = 4; index < 8; ++index)
                action[index] *= 0.08f;
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
        auto action = rig.paired_leg_chains()
            ? balance_teacher_action(environment)
            : compact_support_teacher_action(environment, pressure * 0.48f);
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
            action[0] = clamp(action[0] - hip_flex, -0.62f, 0.62f);
            action[1] = clamp(action[1] + knee_flex, -0.82f, 0.82f);
            action[2] = clamp(action[2] + hip_flex, -0.62f, 0.62f);
            action[3] = clamp(action[3] - knee_flex, -0.82f, 0.82f);
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
        signals.uprightness = environment.uprightness();
        signals.root_x = root.x;
        signals.left_support_x = particles[rig.left_contact_node].position.x;
        signals.right_support_x = particles[rig.right_contact_node].position.x;
        signals.left_supported = environment.left_supported();
        signals.right_supported = environment.right_supported();
        signals.near_rise = environment.ground_height_at(root.x + 0.65f) - ground;
        signals.mid_rise = environment.ground_height_at(root.x + 1.50f) - ground;
        signals.far_rise = environment.ground_height_at(root.x + 3.00f) - ground;
        signals.slope = environment.terrain().slope_at(
            sim::terrain_sample_x(root.x, environment.course_progress()));
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

        for (const sim::CourseFeature& feature : environment.course_features())
        {
            if (feature.kind != sim::CourseFeatureKind::moving_hazard
                && feature.kind != sim::CourseFeatureKind::projectile)
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
                signals.incoming_density = feature.kind == sim::CourseFeatureKind::moving_hazard
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

    [[nodiscard]] inline std::array<float, sim::action_count> walking_teacher_action(
        const sim::Environment& environment) noexcept
    {
        auto action = balance_teacher_action(environment);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const locomotion::Plan movement = current_locomotion_plan(environment);
        if (!rig.paired_leg_chains())
        {
            if (rig.support_seed_count() < 4u)
                return action;
            const float phase = environment.elapsed_seconds() * 2.0f * pi
                * movement.cadence_hz + pi * 0.5f;
            const float swing = std::sin(phase) * movement.direction;
            const float amplitude = 0.28f + movement.stride_scale * 0.34f;
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
            {
                const std::uint8_t mask = motor_support_mask(rig, rig.motors[index]);
                if (mask == 0u)
                    continue;
                const float phase_drive = mask == 0x1u ? swing
                    : mask == 0x2u ? -swing
                    : ((index & 1u) == 0u ? swing : -swing);
                action[index] = clamp(action[index] + phase_drive * amplitude,
                    -0.90f, 0.90f);
            }
            return bilateral_joint_synergy_action(environment, action,
                environment.course_stage());
        }

        const float phase = environment.elapsed_seconds() * 2.0f * pi
            * movement.cadence_hz;
        const float swing = std::sin(phase);
        const float directed_swing = swing * movement.direction;
        const float left_lift = std::max(0.0f, swing);
        const float right_lift = std::max(0.0f, -swing);
        const float span_brake = clamp(
            (environment.primary_support_span_ratio() - 1.08f) * 0.52f,
            0.0f, 0.34f);

        if (movement.intent == locomotion::Intent::crawl)
        {
            action[0] = clamp(action[0] - 0.24f + 0.20f * directed_swing,
                -0.82f, 0.82f);
            action[1] = clamp(action[1] + 0.58f + 0.18f * left_lift,
                -0.90f, 0.90f);
            action[2] = clamp(action[2] + 0.24f - 0.20f * directed_swing,
                -0.82f, 0.82f);
            action[3] = clamp(action[3] - 0.58f - 0.18f * right_lift,
                -0.90f, 0.90f);
            if (rig.active_motor_count >= 8u)
            {
                action[4] = clamp(action[4] - 0.34f * directed_swing, -0.70f, 0.70f);
                action[5] = clamp(action[5] + 0.18f * left_lift, -0.55f, 0.55f);
                action[6] = clamp(action[6] + 0.34f * directed_swing, -0.70f, 0.70f);
                action[7] = clamp(action[7] - 0.18f * right_lift, -0.55f, 0.55f);
            }
            return bilateral_joint_synergy_action(environment, action,
                sim::CourseStage::moving_hazards);
        }

        const float hip_amplitude = 0.34f + movement.stride_scale * 0.34f;
        const float knee_amplitude = 0.24f + movement.swing_lift * 0.48f;
        const float stance_extension = movement.stance_extension * 0.18f;
        action[0] = clamp(action[0] + hip_amplitude * directed_swing - span_brake,
            -0.92f, 0.92f);
        action[1] = clamp(action[1] + knee_amplitude * left_lift
            - 0.16f * right_lift, -0.94f, 0.94f);
        action[2] = clamp(action[2] - hip_amplitude * directed_swing + span_brake,
            -0.92f, 0.92f);
        action[3] = clamp(action[3] - knee_amplitude * right_lift
            + 0.16f * left_lift, -0.94f, 0.94f);

        if (environment.left_supported() && !environment.right_supported())
            action[1] = clamp(action[1] - stance_extension, -0.94f, 0.94f);
        if (environment.right_supported() && !environment.left_supported())
            action[3] = clamp(action[3] + stance_extension, -0.94f, 0.94f);

        if (movement.brake)
        {
            action[0] *= 0.72f;
            action[2] *= 0.72f;
        }
        if (rig.active_motor_count >= 8u)
        {
            action[4] = clamp(action[4] - 0.10f * directed_swing, -0.55f, 0.55f);
            action[6] = clamp(action[6] + 0.10f * directed_swing, -0.55f, 0.55f);
        }
        return bilateral_joint_synergy_action(environment, action,
            environment.course_stage());
    }

    [[nodiscard]] inline std::array<float, sim::action_count> crouch_walk_teacher_action(
        const sim::Environment& environment) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const float pressure = std::max(0.72f, environment.duck_obstacle_weight());
        const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.05f;
        const float swing = std::sin(phase);
        auto action = rig.paired_leg_chains()
            ? balance_teacher_action(environment)
            : compact_support_teacher_action(environment, pressure);

        if (rig.paired_leg_chains())
        {
            action[0] = clamp(action[0] - 0.24f * pressure + 0.34f * swing, -0.82f, 0.82f);
            action[1] = clamp(action[1] + 0.50f * pressure
                + 0.34f * std::max(0.0f, swing), -0.90f, 0.90f);
            action[2] = clamp(action[2] + 0.24f * pressure - 0.34f * swing, -0.82f, 0.82f);
            action[3] = clamp(action[3] - 0.50f * pressure
                - 0.34f * std::max(0.0f, -swing), -0.90f, 0.90f);
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

    [[nodiscard]] inline std::array<float, sim::action_count> effective_policy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> policy_action,
        sim::CourseStage stage) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const std::size_t active = rig.active_motor_count;
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
        if (stage == sim::CourseStage::balance)
        {
            const auto teacher = balance_teacher_action(environment);
            const bool established = environment.stable_stance_seconds() >= 0.75f;
            blend_teacher(teacher, established ? 0.46f : 0.72f, established ? 0.52f : 0.78f);
        }
        else if (stage == sim::CourseStage::duck_press)
        {
            const auto teacher = duck_teacher_action(environment);
            blend_teacher(teacher, 0.76f + environment.duck_obstacle_weight() * 0.16f, 0.0f);
            neutralize_non_support(0.995f);
        }
        else if (stage == sim::CourseStage::uneven)
        {
            const auto teacher = walking_teacher_action(environment);
            const locomotion::Plan movement = current_locomotion_plan(environment);
            const float support_assist = movement.intent == locomotion::Intent::recover ? 0.68f : movement.step_up ? 0.56f : 0.34f;
            const float body_assist = movement.intent == locomotion::Intent::recover ? 0.60f : 0.42f;
            blend_teacher(teacher, support_assist, body_assist);
        }
        else if (stage == sim::CourseStage::crouch_walk)
        {
            const auto teacher = crouch_walk_teacher_action(environment);
            blend_teacher(teacher, 0.58f + environment.duck_obstacle_weight() * 0.24f, 0.0f);
            neutralize_non_support(0.98f);
        }
        else if (stage == sim::CourseStage::ramps)
        {
            const auto teacher = balance_teacher_action(environment);
            blend_teacher(teacher, 0.26f, 0.88f);
        }
        else if (stage == sim::CourseStage::hurdles || stage == sim::CourseStage::moving_hazards)
        {
            const auto teacher = walking_teacher_action(environment);
            const locomotion::Plan movement = current_locomotion_plan(environment);
            const float support_assist = movement.intent == locomotion::Intent::crawl ? 0.78f : movement.intent == locomotion::Intent::flee ? 0.52f : movement.intent == locomotion::Intent::recover ? 0.62f : movement.step_up ? 0.48f : 0.24f;
            const float body_assist = movement.intent == locomotion::Intent::crawl ? 0.60f : movement.intent == locomotion::Intent::flee ? 0.34f : 0.20f;
            blend_teacher(teacher, support_assist, body_assist);
        }
        if (environment.longest_stable_stance_seconds() < 1.0f && !sim::stage_allows_controlled_flips(stage))
        {
            for (std::size_t index = 0; index < active; ++index)
            {
                if (!support_motor(index))
                    policy_action[index] *= 0.08f;
            }
        }
        return bilateral_joint_synergy_action(environment, policy_action, stage);
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
        std::uint64_t evaluation_quality_key{};
        std::uint32_t evaluation_rejection_mask{};
        std::uint32_t evaluation_invalid_runs{};
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
        lateral_crab_gait = 1u << 10u
    };

    struct StageMotionQualification
    {
        bool valid{};
        std::uint32_t rejection_mask{};
        std::uint64_t quality_key{};
    };

    [[nodiscard]] inline std::uint32_t evidence_bit(MotionEvidenceFailure failure) noexcept
    {
        return static_cast<std::uint32_t>(failure);
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

    [[nodiscard]] inline StageMotionQualification stage_motion_qualification(
        sim::CourseStage stage, const sim::Environment& environment) noexcept
    {
        std::uint32_t rejection = 0u;
        if (!environment.valid_motion())
            rejection |= evidence_bit(MotionEvidenceFailure::invalid_motion);
        if (environment.non_foot_grounded())
            rejection |= evidence_bit(MotionEvidenceFailure::body_contact);

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
        case sim::CourseStage::uneven:
            if (environment.longest_stable_stance_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.blueprint().paired_leg_chains()
                && sim::crab_walking_motion(environment.alternating_steps(),
                    environment.limb_crossings(), environment.distance_travelled(),
                    environment.elapsed_seconds(),
                    environment.primary_support_span_ratio()))
                rejection |= evidence_bit(MotionEvidenceFailure::lateral_crab_gait);
            // Qualification is the safe incremental checkpoint gate, not final
            // Walk mastery. Preserve a real two-step sagittal improvement so PPO
            // can build on it instead of discarding every policy below mastery.
            if (environment.blueprint().paired_leg_chains()
                ? (environment.alternating_steps() < 2u
                    || environment.limb_crossings() < 1u)
                : environment.gait_cycles() < 2u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.0f
                || environment.elapsed_seconds() < 2.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
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
        case sim::CourseStage::uneven:
        case sim::CourseStage::hurdles:
        case sim::CourseStage::moving_hazards:
            quality = pack_quality(
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.alternating_steps(), 65535u)),
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.obstacles_passed(), 65535u)),
                quality_bucket(std::max(0.0f, environment.distance_travelled())),
                quality_bucket(environment.elapsed_seconds()));
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
        [[nodiscard]] std::size_t parameter_count() const noexcept { return parameters_.size(); }
        [[nodiscard]] const std::vector<float>& parameters() const noexcept { return parameters_; }
        [[nodiscard]] std::vector<float>& parameters() noexcept { return parameters_; }

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

        [[nodiscard]] const std::vector<float>& gradients() const noexcept { return gradients_; }
        [[nodiscard]] std::vector<float>& gradients() noexcept { return gradients_; }
        [[nodiscard]] std::array<float, output_size> standard_deviation() const noexcept;
        void set_exploration(float standard_deviation) noexcept;
        void neutralize_action_slot(std::size_t slot) noexcept;
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
    };

    class PpoTrainer
    {
    public:
        struct CheckpointData
        {
            std::uint32_t training_semantics{ training_semantics_version };
            std::uint64_t rig_signature{};
            std::vector<float> parameters{};
            std::vector<float> first_moment{};
            std::vector<float> second_moment{};
            std::vector<float> best_parameters{};
            std::vector<float> reward_history{};
            std::vector<float> speed_history{};
            std::uint64_t optimizer_step{};
            std::uint64_t random_state{};
            TrainingMetrics metrics{};
            sim::CourseStage stage{ sim::CourseStage::balance };
            float difficulty{ 0.25f };
        };
        explicit PpoTrainer(const sim::CreatureBlueprint& blueprint,
            std::size_t environment_count = 64,
            bool enable_rollout_workers = true);
        ~PpoTrainer();

        PpoTrainer(const PpoTrainer&) = delete;
        PpoTrainer& operator=(const PpoTrainer&) = delete;

        void set_blueprint(const sim::CreatureBlueprint& blueprint, bool preserve_policy = false);
        void set_course(sim::CourseStage stage, float difficulty, bool preserve_best = true);
        void reset_policy(std::uint64_t seed = 0xC0FFEEu,
            bool clear_totals = false);
        void set_exploration(float standard_deviation) noexcept;
        void neutralize_action_slot(std::size_t slot) noexcept
        {
            policy_.neutralize_action_slot(slot);
        }
        void set_cpu_mode(int mode) noexcept;
        [[nodiscard]] int cpu_mode() const noexcept { return cpu_mode_; }
        [[nodiscard]] bool save_checkpoint(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] bool load_checkpoint(const std::filesystem::path& path, std::string& error,
            bool transfer_only = false);
        [[nodiscard]] CheckpointData checkpoint_data() const;
        [[nodiscard]] static bool write_checkpoint_data(const CheckpointData& data,
            const std::filesystem::path& path, std::string& error);
        [[nodiscard]] static bool read_checkpoint_data(const std::filesystem::path& path,
            CheckpointData& data, std::string& error);
        [[nodiscard]] bool apply_checkpoint_data(CheckpointData data, std::string& error,
            bool transfer_only = false);
        [[nodiscard]] bool restore_best_policy() noexcept;
        void begin_staged_update();
        void compute_staged_advantages();
        void optimize_staged_update();
        void finish_staged_update();
        [[nodiscard]] bool staged_update_active() const noexcept { return staged_update_active_; }
        void train_one_update();
        void step_preview(float dt = 1.0f / 60.0f);
        void reset_preview(std::uint64_t seed = 0xDEADBEEFu) noexcept;
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

        [[nodiscard]] const PolicyNetwork& policy() const noexcept { return policy_; }
        [[nodiscard]] PolicyNetwork& policy() noexcept { return policy_; }
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
        [[nodiscard]] std::uint64_t optimizer_step() const noexcept { return adam_.step; }
        [[nodiscard]] float exploration() const noexcept { return policy_.mean_exploration(); }
        [[nodiscard]] std::size_t rollout_worker_count() const noexcept { return active_worker_count_; }
        [[nodiscard]] std::size_t maximum_worker_count() const noexcept { return rollout_worker_count_; }
        [[nodiscard]] sim::CourseStage course_stage() const noexcept { return course_stage_; }
        [[nodiscard]] float course_difficulty() const noexcept { return course_difficulty_; }
        [[nodiscard]] std::size_t self_imitation_sample_count() const noexcept
        {
            return self_imitation_prior_.size();
        }

    private:
        struct Transition
        {
            std::array<float, sim::observation_count> observation{};
            std::array<float, sim::action_count> action{};
            float log_probability{};
            float value{};
            float reward{};
            float advantage{};
            float return_value{};
            bool terminal{};
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
        };

        struct ParallelState;

        [[nodiscard]] float random_uniform() noexcept;
        [[nodiscard]] float random_normal() noexcept;
        [[nodiscard]] std::array<float, sim::action_count> sample_action(
            const PolicyNetwork::Evaluation& evaluation,
            std::uint64_t& random_state,
            float& log_probability) const noexcept;
        void update_policy();
        void evaluate_policy();
        void refresh_self_imitation_prior();
        void clear_self_imitation_prior() noexcept;
        void apply_self_imitation_prior();
        void reset_training_state(bool clear_best = true,
            bool clear_totals = false) noexcept;
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
        void parallel_evaluate_policy();

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
        std::vector<ImitationSample> self_imitation_prior_{};
        float self_imitation_source_score_{ -std::numeric_limits<float>::infinity() };
        TrainingMetrics metrics_{};
        ControllerState controller_state_{ ControllerState::fresh };
        sim::CourseStage course_stage_{ sim::CourseStage::balance };
        float course_difficulty_{ 0.25f };
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
        sim::InvalidMotion preview_last_reset_reason_{ sim::InvalidMotion::none };
        std::vector<std::jthread> rollout_workers_{};
        std::shared_ptr<ParallelState> parallel_{};
        RolloutTotals staged_totals_{};
        bool staged_update_active_{};
        bool staged_advantages_ready_{};
        bool staged_optimized_{};
    };
}
