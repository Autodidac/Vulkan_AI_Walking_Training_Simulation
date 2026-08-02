#pragma once

#include "simulation.hpp"

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
    inline constexpr std::uint32_t training_semantics_version = 0x0007'0502u;

    [[nodiscard]] inline std::array<float, sim::action_count> balance_teacher_action(
        const sim::Environment& environment) noexcept
    {
        constexpr std::size_t joint_angle_begin = 4;
        constexpr std::size_t joint_velocity_begin = joint_angle_begin + sim::action_count;
        static_assert(sim::observation_count == 40);
        const auto observation = environment.observation();
        std::array<float, sim::action_count> action{};

        const std::size_t leg_count = std::min<std::size_t>(4u,
            environment.blueprint().active_motor_count);
        for (std::size_t index = 0; index < leg_count; ++index)
        {
            const float joint_speed = observation[joint_velocity_begin + index];
            action[index] = clamp(-0.12f * joint_speed, -0.30f, 0.30f);
        }
        for (std::size_t index = 4; index < environment.blueprint().active_motor_count; ++index)
        {
            const float joint_speed = observation[joint_velocity_begin + index];
            action[index] = clamp(-0.035f * joint_speed, -0.08f, 0.08f);
        }

        // Plant and load the feet through the leg chains before granting the
        // arms meaningful authority. The support correction is intentionally
        // asymmetric so a missing foot is recovered instead of mirrored.
        // Rest geometry is the standing target. Do not preload a crouch
        // before both rigid feet have established support.
        action[0] = clamp(action[0], -0.34f, 0.34f);
        action[1] = clamp(action[1], -0.34f, 0.34f);
        action[2] = clamp(action[2], -0.34f, 0.34f);
        action[3] = clamp(action[3], -0.34f, 0.34f);
        if (!environment.left_supported())
        {
            action[0] = clamp(action[0] - 0.012f, -0.38f, 0.38f);
            action[1] = clamp(action[1] + 0.020f, -0.40f, 0.40f);
        }
        if (!environment.right_supported())
        {
            action[2] = clamp(action[2] + 0.012f, -0.38f, 0.38f);
            action[3] = clamp(action[3] - 0.020f, -0.40f, 0.40f);
        }

        const float correction = clamp(observation[0] * 0.40f
            + observation[2] * 0.07f, -0.20f, 0.20f);
        action[0] = clamp(action[0] - correction, -0.52f, 0.52f);
        action[1] = clamp(action[1] + correction * 0.20f, -0.52f, 0.52f);
        action[2] = clamp(action[2] - correction, -0.52f, 0.52f);
        action[3] = clamp(action[3] - correction * 0.20f, -0.52f, 0.52f);

        const bool feet_loaded = environment.left_supported() && environment.right_supported();
        const float arm_authority = feet_loaded
            && environment.stable_stance_seconds() >= 1.0f ? 0.20f : 0.03f;
        for (std::size_t index = 4; index < environment.blueprint().active_motor_count; ++index)
            action[index] *= arm_authority;
        return action;
    }

    [[nodiscard]] inline std::array<float, sim::action_count> bilateral_joint_synergy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> action,
        sim::CourseStage stage) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const bool paired_leg_chains = rig.active_motor_count >= 4u
            && rig.motors[0].enabled && rig.motors[1].enabled
            && rig.motors[2].enabled && rig.motors[3].enabled
            && rig.motors[0].pivot == rig.motors[2].pivot
            && rig.motors[1].a == rig.motors[0].pivot
            && rig.motors[3].a == rig.motors[2].pivot;
        if (!paired_leg_chains)
            return action;

        const float leg_pair_strength = (stage == sim::CourseStage::duck_press
                || stage == sim::CourseStage::crouch_walk)
            ? 0.12f : (stage == sim::CourseStage::balance
                ? 0.18f : (stage == sim::CourseStage::ramps ? 0.22f : 0.18f));
        auto mirror_pair = [&](std::size_t left, std::size_t right, float strength)
        {
            const float mirrored = 0.5f * (action[left] - action[right]);
            action[left] = lerp(action[left], mirrored, strength);
            action[right] = lerp(action[right], -mirrored, strength);
        };
        mirror_pair(0, 2, leg_pair_strength);
        mirror_pair(1, 3, leg_pair_strength);

        if (rig.active_motor_count >= 8u)
        {
            const float arm_pair_strength = sim::stage_allows_controlled_flips(stage)
                ? 0.24f : 0.06f;
            mirror_pair(4, 6, arm_pair_strength);
            mirror_pair(5, 7, arm_pair_strength);
        }

        // Keep a light hip/knee chain prior without forcing both legs into the
        // same folded pose. PPO retains most of the residual leg authority.
        constexpr float chain_strength = 0.04f;
        const float left_chain = 0.5f * (-action[0] + action[1]);
        const float right_chain = 0.5f * (action[2] - action[3]);
        action[0] = lerp(action[0], -left_chain, chain_strength);
        action[1] = lerp(action[1], left_chain, chain_strength);
        action[2] = lerp(action[2], right_chain, chain_strength);
        action[3] = lerp(action[3], -right_chain, chain_strength);

        // The chain prior must not reintroduce same-direction pair motion.
        const float hip_pair_mean = 0.5f * (action[0] + action[2]);
        const float knee_pair_mean = 0.5f * (action[1] + action[3]);
        action[0] -= hip_pair_mean * 0.25f;
        action[2] -= hip_pair_mean * 0.25f;
        action[1] -= knee_pair_mean * 0.25f;
        action[3] -= knee_pair_mean * 0.25f;

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
        auto action = balance_teacher_action(environment);
        const float pressure = environment.duck_press_completed()
            ? std::max(0.72f, environment.duck_obstacle_weight())
            : environment.duck_obstacle_weight();
        if (!environment.duck_press_completed())
        {
            action[0] = clamp(action[0] - 0.30f * pressure, -0.70f, 0.70f);
            action[1] = clamp(action[1] + 0.62f * pressure, -0.82f, 0.82f);
            action[2] = clamp(action[2] + 0.30f * pressure, -0.70f, 0.70f);
            action[3] = clamp(action[3] - 0.62f * pressure, -0.82f, 0.82f);
        }
        else
        {
            const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.05f;
            const float swing = std::sin(phase);
            action[0] = clamp(action[0] - 0.24f * pressure + 0.34f * swing, -0.82f, 0.82f);
            action[1] = clamp(action[1] + 0.50f * pressure + 0.34f * std::max(0.0f, swing), -0.90f, 0.90f);
            action[2] = clamp(action[2] + 0.24f * pressure - 0.34f * swing, -0.82f, 0.82f);
            action[3] = clamp(action[3] - 0.50f * pressure - 0.34f * std::max(0.0f, -swing), -0.90f, 0.90f);
        }
        for (std::size_t index = 4; index < environment.blueprint().active_motor_count; ++index)
            action[index] = 0.0f;
        return bilateral_joint_synergy_action(environment, action, sim::CourseStage::duck_press);
    }

    [[nodiscard]] inline std::array<float, sim::action_count> crouch_walk_teacher_action(
        const sim::Environment& environment) noexcept
    {
        auto action = balance_teacher_action(environment);
        const float pressure = std::max(0.72f, environment.duck_obstacle_weight());
        const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.05f;
        const float swing = std::sin(phase);
        action[0] = clamp(action[0] - 0.24f * pressure + 0.34f * swing, -0.82f, 0.82f);
        action[1] = clamp(action[1] + 0.50f * pressure
            + 0.34f * std::max(0.0f, swing), -0.90f, 0.90f);
        action[2] = clamp(action[2] + 0.24f * pressure - 0.34f * swing, -0.82f, 0.82f);
        action[3] = clamp(action[3] - 0.50f * pressure
            - 0.34f * std::max(0.0f, -swing), -0.90f, 0.90f);
        for (std::size_t index = 4; index < environment.blueprint().active_motor_count; ++index)
            action[index] = 0.0f;
        return bilateral_joint_synergy_action(environment, action,
            sim::CourseStage::crouch_walk);
    }

    [[nodiscard]] inline std::array<float, sim::action_count> effective_policy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> policy_action,
        sim::CourseStage stage) noexcept
    {
        const std::size_t active = environment.blueprint().active_motor_count;
        if (stage == sim::CourseStage::balance)
        {
            const auto teacher = balance_teacher_action(environment);
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.96f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.97f);
        }
        else if (stage == sim::CourseStage::duck_press)
        {
            const auto teacher = duck_teacher_action(environment);
            const float pressure = environment.duck_obstacle_weight();
            const float leg_assist = 0.72f + pressure * 0.24f;
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], leg_assist);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], 0.0f, 0.995f);
        }
        else if (stage == sim::CourseStage::crouch_walk)
        {
            const auto teacher = crouch_walk_teacher_action(environment);
            const float leg_assist = 0.58f + environment.duck_obstacle_weight() * 0.24f;
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], leg_assist);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], 0.0f, 0.98f);
        }
        else if (stage == sim::CourseStage::ramps)
        {
            const auto teacher = balance_teacher_action(environment);
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.26f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.88f);
        }

        if (active >= 8u
            && environment.longest_stable_stance_seconds() < 1.0f
            && !sim::stage_allows_controlled_flips(stage))
        {
            for (std::size_t index = 4; index < 8; ++index)
                policy_action[index] *= 0.08f;
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

    enum class MotionEvidenceFailure : std::uint32_t
    {
        none = 0,
        invalid_motion = 1u << 0u,
        no_stable_stance = 1u << 1u,
        missing_recovery = 1u << 2u,
        missing_skill = 1u << 3u,
        missing_progress = 1u << 4u,
        unstable_joints = 1u << 5u,
        body_contact = 1u << 6u
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
            if (environment.longest_stable_stance_seconds() < 3.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.maximum_joint_speed() > 12.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;
        case sim::CourseStage::duck_press:
            if (environment.longest_stable_stance_seconds() < 2.0f
                || environment.stable_stance_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (!environment.duck_press_completed()
                || environment.duck_recoveries() < 1u
                || environment.duck_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.maximum_joint_speed() > 12.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;
        case sim::CourseStage::uneven:
            if (environment.longest_stable_stance_seconds() < 1.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.gait_cycles() < 4u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
        case sim::CourseStage::crouch_walk:
            if (environment.longest_stable_stance_seconds() < 1.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.gait_cycles() < 4u
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
                quality_bucket(environment.elapsed_seconds()),
                static_cast<std::uint16_t>(65535u
                    - quality_bucket(environment.maximum_joint_speed(), 100.0f)));
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


    [[nodiscard]] inline bool stage_display_sample_eligible(sim::CourseStage stage,
        const sim::Environment& environment) noexcept
    {
        const StageMotionQualification qualification =
            stage_motion_qualification(stage, environment);
        if (!qualification.valid
            || !environment.current_display_posture_valid())
            return false;
        if (stage == sim::CourseStage::balance)
        {
            return environment.stable_stance_seconds() >= 1.0f
                && environment.uprightness() >= 0.82f
                && (environment.left_supported() || environment.right_supported());
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
        void reset_policy(std::uint64_t seed = 0xC0FFEEu);
        void set_exploration(float standard_deviation) noexcept;
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
        void reset_training_state(bool clear_best = true) noexcept;
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
        std::vector<std::jthread> rollout_workers_{};
        std::shared_ptr<ParallelState> parallel_{};
        RolloutTotals staged_totals_{};
        bool staged_update_active_{};
        bool staged_advantages_ready_{};
        bool staged_optimized_{};
    };
}
