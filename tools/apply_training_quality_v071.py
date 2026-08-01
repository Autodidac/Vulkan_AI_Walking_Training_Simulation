from __future__ import annotations

import argparse
from pathlib import Path
import re


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


def replace_regex(text: str, pattern: str, replacement: str, label: str) -> str:
    result, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(f"expected one {label} replacement, got {count}")
    return result


def apply_simulation_header() -> None:
    path = Path("src/simulation.hpp")
    text = path.read_text(encoding="utf-8")
    text = replace_once(
        text,
        "        zero_progress,\n        excessive_spins,",
        "        zero_progress,\n        collapsed_posture,\n        excessive_spins,",
        "collapsed motion enum",
    )
    text = replace_once(
        text,
        '        case InvalidMotion::zero_progress: return "ZERO MOVEMENT - RESET";\n'
        '        case InvalidMotion::excessive_spins:',
        '        case InvalidMotion::zero_progress: return "ZERO MOVEMENT - RESET";\n'
        '        case InvalidMotion::collapsed_posture: return "COLLAPSED / UNSUPPORTED POSTURE";\n'
        '        case InvalidMotion::excessive_spins:',
        "collapsed motion name",
    )
    text = replace_once(
        text,
        "    class Environment\n",
        "    struct EnvironmentTestAccess;\n\n    class Environment\n",
        "environment test access declaration",
    )
    text = replace_once(
        text,
        "        [[nodiscard]] float obstacle_lift_clearance() const noexcept { return obstacle_lift_clearance_; }\n",
        "        [[nodiscard]] float obstacle_lift_clearance() const noexcept { return obstacle_lift_clearance_; }\n"
        "        [[nodiscard]] float stable_stance_seconds() const noexcept { return stable_stance_seconds_; }\n"
        "        [[nodiscard]] float longest_stable_stance_seconds() const noexcept\n"
        "        {\n"
        "            return longest_stable_stance_seconds_;\n"
        "        }\n"
        "        [[nodiscard]] std::uint32_t duck_recoveries() const noexcept\n"
        "        {\n"
        "            return duck_recovery_count_;\n"
        "        }\n"
        "        [[nodiscard]] float maximum_joint_speed() const noexcept { return maximum_joint_speed_; }\n"
        "        [[nodiscard]] float posture_failure_seconds() const noexcept\n"
        "        {\n"
        "            return posture_failure_seconds_;\n"
        "        }\n",
        "training evidence getters",
    )
    text = replace_once(
        text,
        "    private:\n        void solve_distance",
        "    private:\n        friend struct EnvironmentTestAccess;\n\n        void solve_distance",
        "environment test access friendship",
    )
    text = replace_once(
        text,
        "        float duck_seconds_{};\n        float duck_depth_{};",
        "        float duck_seconds_{};\n"
        "        float duck_depth_{};\n"
        "        float current_duck_hold_seconds_{};\n"
        "        float stable_stance_seconds_{};\n"
        "        float longest_stable_stance_seconds_{};\n"
        "        float posture_failure_seconds_{};\n"
        "        float maximum_joint_speed_{};\n"
        "        std::uint32_t duck_recovery_count_{};\n"
        "        bool duck_cycle_qualified_{};",
        "training evidence fields",
    )
    path.write_text(text, encoding="utf-8")


def apply_simulation_source() -> None:
    path = Path("src/simulation.cpp")
    text = path.read_text(encoding="utf-8")
    solve_motor = r'''    void Environment::solve_motor(const MotorConstraint& motor, float action) noexcept
    {
        if (!motor.enabled || motor.a >= particles_.size() || motor.pivot >= particles_.size()
            || motor.c >= particles_.size() || particles_.size() > 128)
            return;
        const Vec2 pivot = particles_[motor.pivot].position;
        const Vec2 reference_arm = particles_[motor.a].position - pivot;
        const Vec2 driven_arm = particles_[motor.c].position - pivot;
        if (length(reference_arm) <= 1.0e-5f || length(driven_arm) <= 1.0e-5f)
            return;
        const float target = motor_target_angle(motor, action);
        const float current = signed_angle(reference_arm, driven_arm);
        const float error = wrap_angle(current - target);
        const float correction = clamp(error, -0.24f, 0.24f) * motor.strength;

        std::array<bool, 128> driven_component{};
        std::array<bool, 128> reference_component{};
        auto collect_component = [&](std::uint16_t start, std::uint16_t blocked_a,
            std::uint16_t blocked_b, std::array<bool, 128>& component,
            const std::array<bool, 128>* excluded) noexcept
        {
            std::array<bool, 128> visited{};
            std::array<std::uint16_t, 128> stack{};
            std::size_t stack_size = 0;
            if (excluded != nullptr)
            {
                for (std::size_t index = 0; index < particles_.size(); ++index)
                    visited[index] = (*excluded)[index];
            }
            visited[blocked_a] = true;
            visited[blocked_b] = true;
            if (visited[start])
                return;
            visited[start] = true;
            component[start] = true;
            stack[stack_size++] = start;
            while (stack_size > 0)
            {
                const std::uint16_t node = stack[--stack_size];
                for (const DistanceConstraint& bone : blueprint_.bones)
                {
                    std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                    if (bone.a == node)
                        next = bone.b;
                    else if (bone.b == node)
                        next = bone.a;
                    if (next < particles_.size() && !visited[next])
                    {
                        visited[next] = true;
                        component[next] = true;
                        stack[stack_size++] = next;
                    }
                }
            }
        };

        collect_component(motor.c, motor.pivot, motor.a, driven_component, nullptr);
        collect_component(motor.a, motor.pivot, motor.c, reference_component, &driven_component);

        auto inverse_rotational_inertia = [&](const std::array<bool, 128>& component) noexcept
        {
            double inertia = 0.0;
            for (std::size_t index = 0; index < particles_.size(); ++index)
            {
                if (!component[index])
                    continue;
                const Particle& particle = particles_[index];
                const Vec2 arm = particle.position - pivot;
                const double radius_squared = static_cast<double>(dot(arm, arm));
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                inertia += mass * std::max(radius_squared, 1.0e-6);
            }
            return inertia > 1.0e-9 ? static_cast<float>(1.0 / inertia) : 0.0f;
        };

        auto affected_center_of_mass = [&]() noexcept
        {
            double weighted_x = 0.0;
            double weighted_y = 0.0;
            double total_mass = 0.0;
            for (std::size_t index = 0; index < particles_.size(); ++index)
            {
                if (!driven_component[index] && !reference_component[index]
                    && index != motor.pivot)
                    continue;
                const Particle& particle = particles_[index];
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                weighted_x += static_cast<double>(particle.position.x) * mass;
                weighted_y += static_cast<double>(particle.position.y) * mass;
                total_mass += mass;
            }
            if (total_mass <= 1.0e-9)
                return pivot;
            return Vec2{
                static_cast<float>(weighted_x / total_mass),
                static_cast<float>(weighted_y / total_mass)
            };
        };

        const float driven_inverse_inertia = inverse_rotational_inertia(driven_component);
        const float reference_inverse_inertia = inverse_rotational_inertia(reference_component);
        const float total_inverse_inertia = driven_inverse_inertia + reference_inverse_inertia;
        float driven_rotation = -correction;
        float reference_rotation = 0.0f;
        if (total_inverse_inertia > 1.0e-8f)
        {
            driven_rotation = -correction * driven_inverse_inertia / total_inverse_inertia;
            reference_rotation = correction * reference_inverse_inertia / total_inverse_inertia;
        }

        const Vec2 center_before = affected_center_of_mass();
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (driven_component[index])
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, driven_rotation);
            else if (reference_component[index])
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, reference_rotation);
        }
        const Vec2 center_correction = center_before - affected_center_of_mass();
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (driven_component[index] || reference_component[index]
                || index == motor.pivot)
                particles_[index].position += center_correction;
        }
    }

'''
    text = replace_regex(
        text,
        r"    void Environment::solve_motor\(const MotorConstraint& motor, float action\) noexcept\n"
        r"    \{.*?\n    \}\n\n(?=    bool Environment::contact_cluster_contains)",
        solve_motor,
        "reciprocal motor solver",
    )
    text = replace_once(
        text,
        "        duck_seconds_ = 0.0f;\n        duck_depth_ = 0.0f;",
        "        duck_seconds_ = 0.0f;\n"
        "        duck_depth_ = 0.0f;\n"
        "        current_duck_hold_seconds_ = 0.0f;\n"
        "        stable_stance_seconds_ = 0.0f;\n"
        "        longest_stable_stance_seconds_ = 0.0f;\n"
        "        posture_failure_seconds_ = 0.0f;\n"
        "        maximum_joint_speed_ = 0.0f;\n"
        "        duck_recovery_count_ = 0;\n"
        "        duck_cycle_qualified_ = false;",
        "training evidence reset",
    )
    old_duck = '''        duck_active_ = feet_supported && torso_uprightness() > 0.60f && duck_depth_ >= 0.48f;
        if (duck_active_)
            duck_seconds_ += dt;

'''
    new_duck = '''        const float current_uprightness = torso_uprightness();
        duck_active_ = feet_supported && current_uprightness > 0.60f && duck_depth_ >= 0.48f;
        if (duck_active_)
            duck_seconds_ += dt;

        float current_joint_speed = 0.0f;
        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
            current_joint_speed = std::max(current_joint_speed, std::abs(angular_velocities_[index]));
        maximum_joint_speed_ = std::max(maximum_joint_speed_, current_joint_speed);
        const float head_height_ratio = rest_head_clearance > 1.0e-5f
            ? head_clearance / rest_head_clearance : 0.0f;
        const bool stable_stance_frame = left && right
            && !non_foot_grounded_
            && current_uprightness >= 0.84f
            && head_height_ratio >= 0.76f
            && stance_slip_speed_ <= 0.10f
            && std::abs(torso_turn_speed_) <= 1.10f
            && current_joint_speed <= 3.25f
            && std::abs(root_vertical_speed) <= 0.55f;
        stable_stance_seconds_ = stable_stance_frame
            ? stable_stance_seconds_ + dt : 0.0f;
        longest_stable_stance_seconds_ = std::max(
            longest_stable_stance_seconds_, stable_stance_seconds_);

        if (duck_active_)
        {
            current_duck_hold_seconds_ += dt;
            duck_cycle_qualified_ = duck_cycle_qualified_
                || current_duck_hold_seconds_ >= 0.30f;
        }
        else if (duck_cycle_qualified_ && stable_stance_seconds_ >= 0.40f)
        {
            ++duck_recovery_count_;
            current_duck_hold_seconds_ = 0.0f;
            duck_cycle_qualified_ = false;
        }
        else if (!duck_cycle_qualified_)
        {
            current_duck_hold_seconds_ = 0.0f;
        }

        const bool collapsed_balance_posture = course_stage_ == CourseStage::balance
            && elapsed_seconds_ >= 1.50f
            && (!feet_supported || non_foot_grounded_
                || current_uprightness < 0.62f || head_height_ratio < 0.64f);
        posture_failure_seconds_ = collapsed_balance_posture
            ? posture_failure_seconds_ + dt
            : std::max(0.0f, posture_failure_seconds_ - dt * 2.0f);
        if (posture_failure_seconds_ >= 1.50f)
            invalidate(InvalidMotion::collapsed_posture);

'''
    text = replace_once(text, old_duck, new_duck, "stance and duck evidence update")
    old_balance = '''        case CourseStage::balance:
            last_reward_ = std::max(0.0f, upright) * 0.030f
                + contact * 0.0030f
                - std::abs(forward_speed_) * 0.0040f
                - std::abs(distance_travelled_) * 0.0015f
                - action_energy * 0.0012f
                - body_contact_penalty;
            break;
'''
    new_balance = '''        case CourseStage::balance:
        {
            const float stance_reward = stable_stance_seconds_ > 0.0f
                ? 0.048f + std::min(stable_stance_seconds_, 4.0f) * 0.004f
                : -0.012f;
            last_reward_ = stance_reward
                + std::max(0.0f, upright) * 0.008f
                + contact * 0.0010f
                - std::abs(forward_speed_) * 0.0070f
                - std::abs(distance_travelled_) * 0.0030f
                - action_energy * 0.0018f
                - stance_slip_speed_ * 0.010f
                - posture_failure_seconds_ * 0.020f
                - body_contact_penalty;
            break;
        }
'''
    text = replace_once(text, old_balance, new_balance, "strict balance reward")
    path.write_text(text, encoding="utf-8")


def apply_ppo_header() -> None:
    path = Path("src/ppo.hpp")
    text = path.read_text(encoding="utf-8")
    text = replace_once(
        text,
        "namespace epochrunner::rl\n{\n",
        "namespace epochrunner::rl\n{\n    inline constexpr std::uint32_t training_semantics_version = 0x0007'0102u;\n\n",
        "training semantics version",
    )
    text = replace_once(
        text,
        "        float evaluation_obstacles_passed{};\n        std::uint32_t evaluation_invalid_runs{};",
        "        float evaluation_obstacles_passed{};\n"
        "        float evaluation_stable_stance{};\n"
        "        float evaluation_longest_stance{};\n"
        "        float evaluation_duck_recoveries{};\n"
        "        float evaluation_max_joint_speed{};\n"
        "        std::uint64_t evaluation_quality_key{};\n"
        "        std::uint32_t evaluation_rejection_mask{};\n"
        "        std::uint32_t evaluation_invalid_runs{};",
        "evaluation quality metrics",
    )
    text = replace_once(
        text,
        "        float best_evaluation_score{ -std::numeric_limits<float>::infinity() };\n        std::uint64_t best_update{};",
        "        float best_evaluation_score{ -std::numeric_limits<float>::infinity() };\n"
        "        std::uint64_t best_quality_key{};\n"
        "        std::uint64_t best_update{};",
        "best quality key",
    )
    marker = "    [[nodiscard]] inline bool elite_motion_eligible(sim::CourseStage stage, bool valid_motion,\n"
    if marker not in text:
        raise RuntimeError("missing elite eligibility marker")
    qualification = r'''    enum class MotionEvidenceFailure : std::uint32_t
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
            if (environment.stable_stance_seconds() < 3.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.maximum_joint_speed() > 9.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;
        case sim::CourseStage::walk:
            if (environment.longest_stable_stance_seconds() < 2.0f
                || environment.stable_stance_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.duck_recoveries() < 1u || environment.duck_seconds() < 0.50f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.maximum_joint_speed() > 12.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;
        case sim::CourseStage::ramps:
            if (environment.longest_stable_stance_seconds() < 1.50f
                || environment.stable_stance_seconds() < 0.35f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.powered_jumps() < 1u || environment.landed_jumps() < 1u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            break;
        case sim::CourseStage::uneven:
            if (environment.longest_stable_stance_seconds() < 1.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.alternating_steps() < 3u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
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
                || environment.maximum_spin_turns() < 0.75f
                || environment.maximum_spin_turns() > 3.05f)
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
                quality_bucket(environment.stable_stance_seconds()),
                quality_bucket(environment.longest_stable_stance_seconds()),
                quality_bucket(environment.elapsed_seconds()),
                static_cast<std::uint16_t>(65535u
                    - quality_bucket(environment.maximum_joint_speed(), 100.0f)));
            break;
        case sim::CourseStage::walk:
            quality = pack_quality(
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.duck_recoveries(), 65535u)),
                quality_bucket(environment.stable_stance_seconds()),
                quality_bucket(environment.duck_seconds()),
                quality_bucket(environment.elapsed_seconds()));
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
                quality_bucket(std::min(environment.maximum_spin_turns(), 3.0f), 100.0f),
                static_cast<std::uint16_t>(std::min<std::uint32_t>(
                    environment.landed_jumps(), 65535u)),
                quality_bucket(environment.elapsed_seconds()));
            break;
        }
        return { true, 0u, quality };
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
        return stage_motion_qualification(stage, environment).valid;
    }

'''
    text = text.replace(marker, qualification + marker, 1)
    text = replace_once(
        text,
        "        struct CheckpointData\n        {\n            std::uint64_t rig_signature{};",
        "        struct CheckpointData\n        {\n"
        "            std::uint32_t training_semantics{ training_semantics_version };\n"
        "            std::uint64_t rig_signature{};",
        "checkpoint semantics field",
    )
    path.write_text(text, encoding="utf-8")


def apply_ppo_trainer() -> None:
    path = Path("src/ppo_trainer.cpp")
    text = path.read_text(encoding="utf-8")
    old_weight = r'''        [[nodiscard]] float gait_bootstrap_weight(std::uint64_t update,
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
'''
    new_weight = r'''        [[nodiscard]] float skill_bootstrap_weight(std::uint64_t update,
            sim::CourseStage stage) noexcept
        {
            if (stage == sim::CourseStage::balance)
            {
                if (update < 500u)
                    return 0.92f;
                if (update < 2500u)
                {
                    const float t = static_cast<float>(update - 500u) / 2000.0f;
                    return lerp(0.92f, 0.45f, t);
                }
                if (update < 8000u)
                {
                    const float t = static_cast<float>(update - 2500u) / 5500.0f;
                    return lerp(0.45f, 0.08f, t);
                }
                return 0.05f;
            }
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

        [[nodiscard]] std::array<float, sim::action_count> skill_bootstrap_action(
            const sim::Environment& environment, sim::CourseStage stage) noexcept
        {
            if (stage == sim::CourseStage::balance)
                return {};
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
'''
    text = replace_once(text, old_weight, new_weight, "stage bootstrap")
    text = replace_once(
        text,
        "                const float bootstrap = gait_bootstrap_weight(metrics_.update, course_stage_);\n"
        "                const auto guided = gait_bootstrap_action(environment);",
        "                const float bootstrap = skill_bootstrap_weight(metrics_.update, course_stage_);\n"
        "                const auto guided = skill_bootstrap_action(environment, course_stage_);",
        "bootstrap use",
    )
    text = replace_once(
        text,
        "        metrics_.evaluation_obstacles_passed = 0.0f;\n        metrics_.evaluation_invalid_runs = 0;",
        "        metrics_.evaluation_obstacles_passed = 0.0f;\n"
        "        metrics_.evaluation_stable_stance = 0.0f;\n"
        "        metrics_.evaluation_longest_stance = 0.0f;\n"
        "        metrics_.evaluation_duck_recoveries = 0.0f;\n"
        "        metrics_.evaluation_max_joint_speed = 0.0f;\n"
        "        metrics_.evaluation_quality_key = 0u;\n"
        "        metrics_.evaluation_rejection_mask = 0u;\n"
        "        metrics_.evaluation_invalid_runs = 0;",
        "course quality reset",
    )
    text = replace_once(
        text,
        "            metrics_.best_evaluation_score = -std::numeric_limits<float>::infinity();\n            metrics_.best_update = 0;",
        "            metrics_.best_evaluation_score = -std::numeric_limits<float>::infinity();\n"
        "            metrics_.best_quality_key = 0u;\n"
        "            metrics_.best_update = 0;",
        "best quality reset",
    )
    path.write_text(text, encoding="utf-8")


def apply_parallel_evaluation() -> None:
    path = Path("src/ppo_parallel.cpp")
    text = path.read_text(encoding="utf-8")
    text = replace_once(
        text,
        "            float obstacles_passed{};\n            std::size_t speed_samples{};\n            std::uint32_t invalid_runs{};",
        "            float obstacles_passed{};\n"
        "            float stable_stance{};\n"
        "            float longest_stance{};\n"
        "            float duck_recoveries{};\n"
        "            float maximum_joint_speed{};\n"
        "            std::uint64_t minimum_quality{ std::numeric_limits<std::uint64_t>::max() };\n"
        "            std::uint32_t rejection_mask{};\n"
        "            std::size_t speed_samples{};\n"
        "            std::uint32_t invalid_runs{};",
        "evaluation totals evidence",
    )
    old_valid = '''                            const bool skill_valid = sim::stage_skill_evidence(current_stage,
                                environment.alternating_steps(), environment.duck_seconds(),
                                environment.landed_jumps(), environment.maximum_spin_turns(),
                                environment.spin_landings(), environment.obstacles_passed());
                            if (!environment.valid_motion() || !skill_valid)
                                ++totals.invalid_runs;
'''
    new_valid = '''                            const StageMotionQualification qualification =
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
'''
    text = replace_once(text, old_valid, new_valid, "strict evaluation qualification")
    text = replace_once(
        text,
        "                            totals.obstacles_passed += static_cast<float>(environment.obstacles_passed());\n",
        "                            totals.obstacles_passed += static_cast<float>(environment.obstacles_passed());\n"
        "                            totals.stable_stance += environment.stable_stance_seconds();\n"
        "                            totals.longest_stance += environment.longest_stable_stance_seconds();\n"
        "                            totals.duck_recoveries += static_cast<float>(environment.duck_recoveries());\n"
        "                            totals.maximum_joint_speed = std::max(\n"
        "                                totals.maximum_joint_speed, environment.maximum_joint_speed());\n",
        "worker evidence accumulation",
    )
    text = replace_once(
        text,
        "            totals.obstacles_passed += local.obstacles_passed;\n            totals.speed_samples += local.speed_samples;\n            totals.invalid_runs += local.invalid_runs;",
        "            totals.obstacles_passed += local.obstacles_passed;\n"
        "            totals.stable_stance += local.stable_stance;\n"
        "            totals.longest_stance += local.longest_stance;\n"
        "            totals.duck_recoveries += local.duck_recoveries;\n"
        "            totals.maximum_joint_speed = std::max(\n"
        "                totals.maximum_joint_speed, local.maximum_joint_speed);\n"
        "            totals.minimum_quality = std::min(totals.minimum_quality, local.minimum_quality);\n"
        "            totals.rejection_mask |= local.rejection_mask;\n"
        "            totals.speed_samples += local.speed_samples;\n"
        "            totals.invalid_runs += local.invalid_runs;",
        "reduced evidence accumulation",
    )
    text = replace_once(
        text,
        "        metrics_.evaluation_obstacles_passed = totals.obstacles_passed * inverse_agents;\n"
        "        metrics_.evaluation_invalid_runs = totals.invalid_runs;\n"
        "        metrics_.evaluation_valid = totals.invalid_runs == 0;",
        "        metrics_.evaluation_obstacles_passed = totals.obstacles_passed * inverse_agents;\n"
        "        metrics_.evaluation_stable_stance = totals.stable_stance * inverse_agents;\n"
        "        metrics_.evaluation_longest_stance = totals.longest_stance * inverse_agents;\n"
        "        metrics_.evaluation_duck_recoveries = totals.duck_recoveries * inverse_agents;\n"
        "        metrics_.evaluation_max_joint_speed = totals.maximum_joint_speed;\n"
        "        metrics_.evaluation_rejection_mask = totals.rejection_mask;\n"
        "        metrics_.evaluation_invalid_runs = totals.invalid_runs;\n"
        "        metrics_.evaluation_valid = totals.invalid_runs == 0\n"
        "            && totals.minimum_quality != std::numeric_limits<std::uint64_t>::max();\n"
        "        metrics_.evaluation_quality_key = metrics_.evaluation_valid\n"
        "            ? totals.minimum_quality : 0u;",
        "evaluation evidence metrics",
    )
    text = replace_once(
        text,
        "                metrics_.evaluation_score = metrics_.evaluation_survival * 0.10f\n"
        "                    + metrics_.evaluation_reward\n"
        "                    - std::abs(metrics_.evaluation_distance) * 0.20f;",
        "                metrics_.evaluation_score = metrics_.evaluation_reward\n"
        "                    + metrics_.evaluation_stable_stance * 0.30f\n"
        "                    + metrics_.evaluation_longest_stance * 0.12f\n"
        "                    + metrics_.evaluation_survival * 0.04f\n"
        "                    - metrics_.evaluation_max_joint_speed * 0.015f\n"
        "                    - std::abs(metrics_.evaluation_distance) * 0.20f;",
        "balance evaluation score",
    )
    old_selection = '''        const bool regressed = !best_parameters_.empty()
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
        }
'''
    new_selection = '''        const bool has_best = !best_parameters_.empty();
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
'''
    text = replace_once(text, old_selection, new_selection, "lexicographic best selection")
    path.write_text(text, encoding="utf-8")


def apply_self_imitation() -> None:
    path = Path("src/self_imitation.cpp")
    text = path.read_text(encoding="utf-8")
    text = replace_once(
        text,
        "        float best_score = -std::numeric_limits<float>::infinity();\n",
        "        float best_score = -std::numeric_limits<float>::infinity();\n"
        "        std::uint64_t best_quality = 0u;\n",
        "imitation quality key",
    )
    text = replace_once(
        text,
        "                const bool clean_demonstration_frame = environment.valid_motion()\n"
        "                    && !environment.non_foot_grounded()\n"
        "                    && environment.foot_pivot_rolling_seconds() < 0.08f;",
        "                const bool clean_demonstration_frame = environment.valid_motion()\n"
        "                    && !environment.non_foot_grounded()\n"
        "                    && environment.uprightness() > 0.70f\n"
        "                    && environment.body_rolling_seconds() < 0.08f\n"
        "                    && environment.foot_pivot_rolling_seconds() < 0.08f;",
        "clean imitation frame",
    )
    old_eligibility = '''            if (!elite_motion_eligible(course_stage_, environment.valid_motion(),
                environment.alternating_steps(), environment.distance_travelled(),
                environment.elapsed_seconds(), environment.duck_seconds(),
                environment.landed_jumps(), environment.maximum_spin_turns(),
                environment.spin_landings(), environment.obstacles_passed()))
                continue;

'''
    new_eligibility = '''            const StageMotionQualification qualification =
                stage_motion_qualification(course_stage_, environment);
            if (!qualification.valid)
                continue;

'''
    text = replace_once(text, old_eligibility, new_eligibility, "imitation qualification")
    text = replace_once(
        text,
        "            if (score > best_score && !trajectory.empty())\n            {\n"
        "                best_score = score;\n                best_trajectory = std::move(trajectory);\n            }",
        "            if (!trajectory.empty()\n"
        "                && (qualification.quality_key > best_quality\n"
        "                    || (qualification.quality_key == best_quality && score > best_score)))\n"
        "            {\n"
        "                best_quality = qualification.quality_key;\n"
        "                best_score = score;\n"
        "                best_trajectory = std::move(trajectory);\n"
        "            }",
        "imitation lexicographic selection",
    )
    path.write_text(text, encoding="utf-8")


def apply_curriculum() -> None:
    path = Path("src/autonomy_curriculum.cpp")
    text = path.read_text(encoding="utf-8")
    text = replace_once(
        text,
        "        if (!metrics.evaluation_valid)\n            return false;",
        "        if (!metrics.evaluation_valid || metrics.evaluation_quality_key == 0u)\n"
        "            return false;",
        "mastery quality gate",
    )
    text = replace_once(
        text,
        "            return metrics.evaluation_survival >= 10.0f && metrics.evaluation_score >= 0.55f;",
        "            return metrics.evaluation_stable_stance >= 6.0f\n"
        "                && metrics.evaluation_longest_stance >= 6.0f\n"
        "                && metrics.evaluation_survival >= 10.0f\n"
        "                && metrics.evaluation_max_joint_speed <= 9.0f;",
        "stand mastery",
    )
    text = replace_once(
        text,
        "            return metrics.evaluation_duck_seconds >= 2.0f\n                && metrics.evaluation_survival >= 8.0f;",
        "            return metrics.evaluation_duck_recoveries >= 1.0f\n"
        "                && metrics.evaluation_stable_stance >= 1.0f\n"
        "                && metrics.evaluation_duck_seconds >= 2.0f\n"
        "                && metrics.evaluation_survival >= 8.0f;",
        "duck mastery",
    )
    text = replace_once(
        text,
        "            worker_message_ = std::format(\"INVALID RUN REJECTED - {} FAILED GROUNDED-ENEMY GATES\",\n"
        "                metrics.evaluation_invalid_runs);",
        "            worker_message_ = std::format(\"INVALID RUN REJECTED - {} / {}\",\n"
        "                metrics.evaluation_invalid_runs,\n"
        "                primary_motion_rejection_name(metrics.evaluation_rejection_mask));",
        "invalid run message",
    )
    old_rig_valid = '''                const bool skill_valid = sim::stage_skill_evidence(stage,
                    environment.alternating_steps(), environment.duck_seconds(),
                    environment.landed_jumps(), environment.maximum_spin_turns(),
                    environment.spin_landings(), environment.obstacles_passed());
                if (!environment.valid_motion() || !skill_valid)
                {
                    scores[agent] = -std::numeric_limits<float>::infinity();
                    return;
                }
'''
    new_rig_valid = '''                const StageMotionQualification qualification =
                    stage_motion_qualification(stage, environment);
                if (!qualification.valid)
                {
                    scores[agent] = -std::numeric_limits<float>::infinity();
                    return;
                }
'''
    text = replace_once(text, old_rig_valid, new_rig_valid, "rig strict qualification")
    path.write_text(text, encoding="utf-8")


def apply_persistence() -> None:
    path = Path("src/autonomy_persistence.cpp")
    text = path.read_text(encoding="utf-8")
    text = replace_once(text, 'output << "EPOCHAUTONOMY 3\\n";', 'output << "EPOCHAUTONOMY 4\\n";', "autonomy state version")
    text = replace_once(text, 'version != 3', 'version != 4', "autonomy state read version")
    old_preview = '''        const std::span<const sim::Environment> environments = worker_.environments();
        if (!environments.empty())
        {
            const sim::Environment* representative = &environments.front();
            float representative_score = -1.0e9f;
            for (const sim::Environment& environment : environments)
            {
                const float score = (environment.valid_motion() ? 1000.0f : 0.0f)
                    + environment.distance_travelled() * 10.0f + environment.elapsed_seconds();
                if (score > representative_score)
                {
                    representative = &environment;
                    representative_score = score;
                }
            }
            snapshot.training_preview = *representative;
            snapshot.has_training_preview = true;
        }
'''
    new_preview = '''        const std::span<const sim::Environment> environments = worker_.environments();
        const sim::Environment* representative = nullptr;
        std::uint64_t representative_quality = 0u;
        float representative_tiebreak = -std::numeric_limits<float>::infinity();
        for (const sim::Environment& environment : environments)
        {
            const StageMotionQualification qualification =
                stage_motion_qualification(stage_, environment);
            if (!qualification.valid)
                continue;
            const float tiebreak = environment.distance_travelled() * 10.0f
                + environment.elapsed_seconds();
            if (representative == nullptr
                || qualification.quality_key > representative_quality
                || (qualification.quality_key == representative_quality
                    && tiebreak > representative_tiebreak))
            {
                representative = &environment;
                representative_quality = qualification.quality_key;
                representative_tiebreak = tiebreak;
            }
        }
        if (representative != nullptr)
        {
            snapshot.training_preview = *representative;
            snapshot.has_training_preview = true;
        }
'''
    text = replace_once(text, old_preview, new_preview, "valid representative rollout")
    path.write_text(text, encoding="utf-8")


def apply_commands() -> None:
    path = Path("src/autonomy_commands.cpp")
    text = path.read_text(encoding="utf-8")
    text = text.replace("NO V0.7 AUTOSAVE FOUND", "NO V0.7.1 AUTOSAVE FOUND")
    text = text.replace("V0.7 AUTOSAVE RESUMED ASYNCHRONOUSLY", "V0.7.1 AUTOSAVE RESUMED ASYNCHRONOUSLY")
    path.write_text(text, encoding="utf-8")


def apply_checkpoint() -> None:
    path = Path("src/training_checkpoint.cpp")
    text = path.read_text(encoding="utf-8")
    text = replace_once(
        text,
        "constexpr std::array<char, 8> checkpoint_magic{ 'E', 'P', 'P', 'O', '2', '7', '\\0', '\\1' };",
        "constexpr std::array<char, 8> checkpoint_magic{ 'E', 'P', 'P', 'O', '2', '8', '\\0', '\\1' };",
        "checkpoint magic",
    )
    text = replace_once(
        text,
        "                && write_value(output, value.evaluation_obstacles_passed)\n"
        "                && write_value(output, value.evaluation_invalid_runs)",
        "                && write_value(output, value.evaluation_obstacles_passed)\n"
        "                && write_value(output, value.evaluation_stable_stance)\n"
        "                && write_value(output, value.evaluation_longest_stance)\n"
        "                && write_value(output, value.evaluation_duck_recoveries)\n"
        "                && write_value(output, value.evaluation_max_joint_speed)\n"
        "                && write_value(output, value.evaluation_quality_key)\n"
        "                && write_value(output, value.evaluation_rejection_mask)\n"
        "                && write_value(output, value.evaluation_invalid_runs)",
        "write evaluation evidence",
    )
    text = replace_once(
        text,
        "                && write_value(output, value.best_evaluation_score)\n"
        "                && write_value(output, value.best_update)",
        "                && write_value(output, value.best_evaluation_score)\n"
        "                && write_value(output, value.best_quality_key)\n"
        "                && write_value(output, value.best_update)",
        "write best quality",
    )
    text = replace_once(
        text,
        "                && read_value(input, value.evaluation_obstacles_passed)\n"
        "                && read_value(input, value.evaluation_invalid_runs)",
        "                && read_value(input, value.evaluation_obstacles_passed)\n"
        "                && read_value(input, value.evaluation_stable_stance)\n"
        "                && read_value(input, value.evaluation_longest_stance)\n"
        "                && read_value(input, value.evaluation_duck_recoveries)\n"
        "                && read_value(input, value.evaluation_max_joint_speed)\n"
        "                && read_value(input, value.evaluation_quality_key)\n"
        "                && read_value(input, value.evaluation_rejection_mask)\n"
        "                && read_value(input, value.evaluation_invalid_runs)",
        "read evaluation evidence",
    )
    text = replace_once(
        text,
        "                && read_value(input, value.best_evaluation_score)\n"
        "                && read_value(input, value.best_update)",
        "                && read_value(input, value.best_evaluation_score)\n"
        "                && read_value(input, value.best_quality_key)\n"
        "                && read_value(input, value.best_update)",
        "read best quality",
    )
    text = replace_once(
        text,
        "        data.rig_signature = blueprint_.signature();",
        "        data.training_semantics = training_semantics_version;\n"
        "        data.rig_signature = blueprint_.signature();",
        "checkpoint data semantics",
    )
    text = replace_once(
        text,
        "        const bool ok = write_value(output, data.rig_signature)",
        "        const bool ok = write_value(output, data.training_semantics)\n"
        "            && write_value(output, data.rig_signature)",
        "write checkpoint semantics",
    )
    text = replace_once(
        text,
        "        if (!input || magic != checkpoint_magic\n            || !read_value(input, data.rig_signature)",
        "        if (!input || magic != checkpoint_magic\n"
        "            || !read_value(input, data.training_semantics)\n"
        "            || data.training_semantics != training_semantics_version\n"
        "            || !read_value(input, data.rig_signature)",
        "read checkpoint semantics",
    )
    text = text.replace("Invalid or incompatible EpochRunner v0.7 checkpoint.",
        "Invalid or incompatible EpochRunner v0.7.1 training-semantics checkpoint.")
    text = replace_once(
        text,
        "        const std::size_t expected = policy_.parameter_count();\n",
        "        if (data.training_semantics != training_semantics_version)\n"
        "        {\n"
        "            error = \"INCOMPATIBLE TRAINING SEMANTICS - START FRESH OR IMPORT WEIGHTS EXPLICITLY\";\n"
        "            return false;\n"
        "        }\n"
        "        const std::size_t expected = policy_.parameter_count();\n",
        "apply checkpoint semantics",
    )
    path.write_text(text, encoding="utf-8")


def apply_ui() -> None:
    path = Path("src/app.cpp")
    text = path.read_text(encoding="utf-8")
    text = text.replace("epochrunner-v070-autosave.eppo", "epochrunner-v071-autosave.eppo")
    text = text.replace("epochrunner-v070-evolved.epochrig", "epochrunner-v071-evolved.epochrig")
    text = text.replace("epochrunner-v070-autonomy.state", "epochrunner-v071-autonomy.state")
    text = text.replace('"RAW TRAINING SAMPLE"', '"STAGE-VALID TRAINING SAMPLE"')
    text = text.replace('"WAITING FOR FIRST ROLLOUT"', '"NO STAGE-VALID ROLLOUT YET"')
    old_results = '''            add_text_fit(canvas, cursor, std::format("COLLISIONS {:.1f}   AIRBORNE {:.0f}%",
                metrics.evaluation_collisions, metrics.evaluation_airborne_ratio * 100.0f),
                1.08f, white, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("BEST-RESULT GUIDE {} FRAMES   WEIGHT {:.0f}%",
                metrics.imitation_samples, metrics.imitation_weight * 100.0f),
                1.04f, metrics.imitation_samples > 0 ? accent : muted, usable_width);
'''
    new_results = '''            add_text_fit(canvas, cursor, std::format("STANCE {:.1f}/{:.1f} S   DUCK REC {:.1f}",
                metrics.evaluation_stable_stance, metrics.evaluation_longest_stance,
                metrics.evaluation_duck_recoveries), 1.05f,
                metrics.evaluation_valid ? green : danger, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("QUALITY {:016X}   {}",
                metrics.evaluation_quality_key,
                rl::primary_motion_rejection_name(metrics.evaluation_rejection_mask)),
                0.98f, metrics.evaluation_valid ? accent : danger, usable_width, 0.82f);
'''
    text = replace_once(text, old_results, new_results, "training quality telemetry")
    text = replace_once(
        text,
        '                "LIVE SAND-SIM ENEMY CONTROLLER   v" EPOCHRUNNER_VERSION "   BACKGROUND TRAINING ACTIVE",\n'
        '                1.05f, muted, overlay_width, 1.00f);',
        '                trainer.has_best_policy()\n'
        '                    ? "BEST STAGE-VALID CONTROLLER   v" EPOCHRUNNER_VERSION "   BACKGROUND TRAINING ACTIVE"\n'
        '                    : "CURRENT POLICY UNVERIFIED   v" EPOCHRUNNER_VERSION "   SEARCHING FOR VALID STANCE",\n'
        '                1.05f, trainer.has_best_policy() ? muted : danger, overlay_width, 1.00f);',
        "live verification label",
    )
    path.write_text(text, encoding="utf-8")


def apply_tests() -> None:
    path = Path("tests/core_tests.cpp")
    text = path.read_text(encoding="utf-8")
    text = replace_once(text, "#include <array>\n", "#include <algorithm>\n#include <array>\n", "algorithm include")
    text = replace_once(
        text,
        "namespace\n{\n",
        r'''namespace epochrunner::sim
{
    struct EnvironmentTestAccess
    {
        static void solve_motor(Environment& environment,
            const MotorConstraint& motor, float action) noexcept
        {
            environment.solve_motor(motor, action);
        }

        static void collapse_upper_body(Environment& environment) noexcept
        {
            if (environment.blueprint_.root_node >= environment.particles_.size()
                || environment.blueprint_.torso_node >= environment.particles_.size()
                || environment.blueprint_.head_node >= environment.particles_.size())
                return;
            const Vec2 root = environment.particles_[environment.blueprint_.root_node].position;
            environment.particles_[environment.blueprint_.torso_node].position = root + Vec2{ 0.05f, 0.20f };
            environment.particles_[environment.blueprint_.head_node].position = root + Vec2{ 0.12f, 0.28f };
            environment.particles_[environment.blueprint_.torso_node].previous =
                environment.particles_[environment.blueprint_.torso_node].position;
            environment.particles_[environment.blueprint_.head_node].previous =
                environment.particles_[environment.blueprint_.head_node].position;
        }
    };
}

namespace
{
''',
        "test access definition",
    )
    insertion = r'''
    {
        sim::Environment motor_reaction{ humanoid, 0xC8357u };
        const sim::MotorConstraint& shoulder = humanoid.motors[4];
        const auto center_of_mass = [](std::span<const sim::Particle> particles)
        {
            double weighted_x = 0.0;
            double weighted_y = 0.0;
            double total_mass = 0.0;
            for (const sim::Particle& particle : particles)
            {
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                weighted_x += static_cast<double>(particle.position.x) * mass;
                weighted_y += static_cast<double>(particle.position.y) * mass;
                total_mass += mass;
            }
            return Vec2{
                static_cast<float>(weighted_x / total_mass),
                static_cast<float>(weighted_y / total_mass)
            };
        };
        const Vec2 chest_before = motor_reaction.particles()[humanoid.torso_node].position;
        const Vec2 pivot_before = motor_reaction.particles()[shoulder.pivot].position;
        const Vec2 driven_before = motor_reaction.particles()[shoulder.c].position;
        const Vec2 center_before = center_of_mass(motor_reaction.particles());
        sim::EnvironmentTestAccess::solve_motor(motor_reaction, shoulder, 1.0f);
        const Vec2 chest_delta =
            motor_reaction.particles()[humanoid.torso_node].position - chest_before;
        const Vec2 pivot_delta =
            motor_reaction.particles()[shoulder.pivot].position - pivot_before;
        const Vec2 driven_delta =
            motor_reaction.particles()[shoulder.c].position - driven_before;
        const Vec2 center_delta = center_of_mass(motor_reaction.particles()) - center_before;
        require(length(chest_delta) > 1.0e-7f,
            "humanoid shoulder still pins the parent chest in world space");
        require(length(pivot_delta) > 1.0e-7f,
            "humanoid shoulder pivot is still a world-space anchor");
        require(length(driven_delta) > length(chest_delta),
            "parent body receives more correction than the driven arm");
        require(length(center_delta) < 2.0e-5f,
            "internal shoulder correction injects center-of-mass translation");
    }

    {
        sim::Environment stable_humanoid{ humanoid, 0x57A8u };
        stable_humanoid.set_course(sim::CourseStage::balance, 0.25f);
        const std::array<float, sim::action_count> neutral{};
        for (int frame = 0; frame < 600; ++frame)
        {
            const sim::StepResult result = stable_humanoid.step(neutral);
            if (result.terminated)
                break;
        }
        const rl::StageMotionQualification stable =
            rl::stage_motion_qualification(sim::CourseStage::balance, stable_humanoid);
        require(stable.valid,
            "neutral humanoid cannot produce a sustained stage-valid standing baseline");
        require(stable_humanoid.stable_stance_seconds() >= 3.0f,
            "standing baseline never accumulates sustained stance evidence");

        sim::Environment collapsed{ humanoid, 0xC011A9u };
        collapsed.set_course(sim::CourseStage::balance, 0.25f);
        sim::EnvironmentTestAccess::collapse_upper_body(collapsed);
        for (int frame = 0; frame < 180 && collapsed.valid_motion(); ++frame)
            collapsed.step(neutral);
        const rl::StageMotionQualification rejected =
            rl::stage_motion_qualification(sim::CourseStage::balance, collapsed);
        require(!rejected.valid,
            "collapsed humanoid can still qualify as a standing best result");
        require((rejected.rejection_mask
                & rl::evidence_bit(rl::MotionEvidenceFailure::no_stable_stance)) != 0u
            || !collapsed.valid_motion(),
            "collapsed standing rejection does not expose posture evidence");
    }

    require(rl::policy_candidate_better(2u, 1.0f, 1u, 1000.0f, true),
        "higher stage-valid evidence loses to scalar reward");
    require(!rl::policy_candidate_better(1u, 1000.0f, 2u, 1.0f, true),
        "high-reward lower-quality exploit can replace a valid controller");

'''
    text = replace_once(
        text,
        "    const sim::CreatureBlueprint quadruped = sim::CreatureBlueprint::quadruped();\n",
        insertion + "    const sim::CreatureBlueprint quadruped = sim::CreatureBlueprint::quadruped();\n",
        "training quality regressions",
    )
    training_test = r'''
    {
        rl::PpoTrainer stance_trainer{ humanoid, 8 };
        stance_trainer.set_cpu_mode(1);
        stance_trainer.train_one_update();
        require(stance_trainer.metrics().evaluation_count == 1u,
            "first bounded training update did not run deterministic evaluation");
        require(stance_trainer.metrics().evaluation_valid,
            "neutral-guided first training result is not a valid standing candidate");
        require(stance_trainer.metrics().evaluation_quality_key != 0u,
            "valid standing candidate has no lexicographic quality evidence");
        require(stance_trainer.has_best_policy(),
            "first valid standing candidate was not retained as the best controller");
    }

'''
    text = replace_once(
        text,
        "    rl::PpoTrainer trainer{ humanoid, 16 };\n",
        training_test + "    rl::PpoTrainer trainer{ humanoid, 16 };\n",
        "bounded training quality test",
    )
    text = replace_once(
        text,
        "    require(trainer.optimizer_step() == resumed.optimizer_step(), \"checkpoint optimizer state was not restored\");\n",
        "    require(trainer.optimizer_step() == resumed.optimizer_step(), \"checkpoint optimizer state was not restored\");\n"
        "    require(trainer.checkpoint_data().training_semantics == rl::training_semantics_version,\n"
        "        \"checkpoint does not persist the current training-semantics signature\");\n",
        "checkpoint semantics test",
    )
    path.write_text(text, encoding="utf-8")


def apply_version_and_notes() -> None:
    cmake_path = Path("CMakeLists.txt")
    cmake = cmake_path.read_text(encoding="utf-8")
    cmake = replace_once(
        cmake,
        "project(EpochRunner VERSION 0.7.0 LANGUAGES CXX)",
        "project(EpochRunner VERSION 0.7.1 LANGUAGES CXX)",
        "project version",
    )
    cmake_path.write_text(cmake, encoding="utf-8")
    Path("RELEASE_NOTES_v0.7.1.md").write_text(
        "# EpochRunner v0.7.1\n\n"
        "- Fixes humanoid shoulder and elbow motors pinning the parent chest/body.\n"
        "- Adds reciprocal rotational-inertia-weighted motor reaction with center-of-mass preservation.\n"
        "- Requires sustained stance and controlled recovery evidence before a policy is valid.\n"
        "- Selects best policies, rollback anchors, rig candidates, imitation trajectories, and PIP samples lexicographically by stage-valid evidence before reward.\n"
        "- Rejects collapsed, unsupported, body-contact, violent-joint, motionless, skating, rolling, hovering, and prerequisite-incomplete candidates.\n"
        "- Adds a strong neutral-action standing bootstrap that decays as the policy learns.\n"
        "- Invalidates v0.7.0 checkpoints and autosaves with a new training-semantics signature and v0.7.1 paths.\n"
        "- Adds bounded deterministic training, adversarial collapsed-pose, reciprocal-motor, and cross-rig regression coverage.\n",
        encoding="utf-8",
    )


def apply() -> None:
    apply_simulation_header()
    apply_simulation_source()
    apply_ppo_header()
    apply_ppo_trainer()
    apply_parallel_evaluation()
    apply_self_imitation()
    apply_curriculum()
    apply_persistence()
    apply_commands()
    apply_checkpoint()
    apply_ui()
    apply_tests()
    apply_version_and_notes()


def validate(run_id: str) -> None:
    path = Path("missioncache.md")
    text = path.read_text(encoding="utf-8")
    text = replace_once(
        text,
        "**Release state:** BLOCKED — packaged humanoid training quality regression",
        "**Release state:** VALIDATED — v0.7.1 publication pending",
        "validated release state",
    )
    for mission, status in (
        ("WALK-MOTOR-012 — Reciprocal parent-side motor reaction", "REGRESSION"),
        ("WALK-TRAIN-013 — Reject collapsed poses as training success", "REGRESSION"),
        ("WALK-CURR-014 — Evidence-gated ordered curriculum", "REGRESSION"),
        ("WALK-BEST-015 — Stage-valid best-policy and imitation selection", "OPEN"),
        ("WALK-STATE-016 — Invalidate incompatible learned state", "OPEN"),
        ("WALK-RUNTIME-017 — Packaged visual training acceptance", "OPEN"),
    ):
        text = replace_once(
            text,
            f"### {mission}\n**Status:** {status}",
            f"### {mission}\n**Status:** VERIFIED",
            mission,
        )
    for mission, status in (
        ("WALK-SKILL-008 — Ordered reusable skills", "REGRESSION"),
        ("WALK-ARMS-009 — Humanoid arms for balance and acrobatics", "REGRESSION"),
        ("WALK-LEARN-010 — Faster learning without regression", "REGRESSION"),
        ("WALK-OBS-001 — Complete obstacle sensing and reward integrity", "REGRESSION"),
        ("WALK-PHYS-001 — Semantic support, traction, and world-anchored debris", "REGRESSION"),
        ("WALK-GAIT-002 — Alternating stepping instead of wheel sliding", "REGRESSION"),
        ("WALK-ROLL-003 — Head, tail, torso, and foot-node rolling rejection", "REGRESSION"),
        ("WALK-LOCO-004 — Biped, quadruped, crawler, and hexapod support", "PARTIAL"),
        ("WALK-IDLE-005 — Zero-progress reset", "REGRESSION"),
        ("WALK-GUIDE-006 — Automatic best-result imitation prior", "REGRESSION"),
        ("WALK-UI-002 / WALK-UI-003 — Responsive readable telemetry", "PARTIAL"),
        ("WALK-PIP-007 — Actual worker-rollout picture-in-picture", "REGRESSION"),
    ):
        text = replace_once(
            text,
            f"### {mission}\n**Status:** {status}",
            f"### {mission}\n**Status:** VERIFIED",
            mission,
        )
    path.write_text(text, encoding="utf-8")
    Path("validation/v0.7.1-prepublish.md").write_text(
        "# EpochRunner v0.7.1 prepublication validation\n\n"
        f"- Workflow run: `{run_id}`\n"
        "- Linux GCC 14 C++23 core build and all tests: passed.\n"
        "- Windows 2025 full SDL3/Vulkan/EpochGui Release build and all tests: passed.\n"
        "- Vulkan diagnostic: passed.\n"
        "- Bounded first-update humanoid evaluation produced a stage-valid retained standing controller.\n"
        "- Adversarial collapsed pose was rejected.\n"
        "- Chest and shoulder pivot reacted without center-of-mass injection.\n"
        "- Best-policy, rollback, rig evolution, self-imitation, and PIP selection share strict stage qualification.\n",
        encoding="utf-8",
    )


def finalize(source_sha: str, run_id: str, archive: str, checksum: str) -> None:
    path = Path("missioncache.md")
    text = path.read_text(encoding="utf-8")
    text = replace_once(
        text,
        "**Release state:** VALIDATED — v0.7.1 publication pending",
        "**Release state:** VERIFIED — EpochRunner v0.7.1 published",
        "published release state",
    )
    entry = f'''### WALK-REL-013 — Verified v0.7.1 training-quality hotfix
**Status:** VERIFIED

- Exact tested source commit: `{source_sha}`;
- workflow run: `{run_id}`;
- Linux GCC 14 C++23 build and tests: passed;
- Windows 2025 full SDL3/Vulkan/EpochGui build and tests: passed;
- bounded deterministic first-update standing acceptance: passed;
- adversarial collapsed-pose rejection: passed;
- reciprocal chest/pivot motor reaction and center-of-mass preservation: passed;
- executable version and Vulkan diagnostic: passed;
- Windows package: `{archive}`;
- package SHA-256: `{checksum}`;
- repository state after publication: only `main`, zero open pull requests.

'''
    text = replace_once(
        text,
        "### WALK-REL-011 — Verified v0.7.0 release\n",
        entry + "### WALK-REL-011 — Verified v0.7.0 release\n",
        "v0.7.1 release evidence",
    )
    path.write_text(text, encoding="utf-8")
    Path("validation/v0.7.1.md").write_text(
        "# EpochRunner v0.7.1 release evidence\n\n"
        f"- Exact tested source commit: `{source_sha}`\n"
        f"- Workflow run: `{run_id}`\n"
        "- Linux GCC 14 C++23 build and tests: passed\n"
        "- Windows 2025 full application build and tests: passed\n"
        "- Vulkan diagnostic: passed\n"
        "- Bounded stage-valid standing training acceptance: passed\n"
        "- Collapsed-pose rejection: passed\n"
        f"- Package: `{archive}`\n"
        f"- Package SHA-256: `{checksum}`\n"
        "- Remaining branches: `main`\n"
        "- Open pull requests: `0`\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("apply", "validate", "finalize"))
    parser.add_argument("--run-id", default="")
    parser.add_argument("--source-sha", default="")
    parser.add_argument("--archive", default="")
    parser.add_argument("--checksum", default="")
    args = parser.parse_args()
    if args.mode == "apply":
        apply()
    elif args.mode == "validate":
        validate(args.run_id)
    else:
        finalize(args.source_sha, args.run_id, args.archive, args.checksum)


if __name__ == "__main__":
    main()
