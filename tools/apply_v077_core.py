from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def load(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def save(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str) -> None:
    text = load(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"missing target in {path}: {old[:180]!r}")
    save(path, text.replace(old, new, 1))


def sub_once(path: str, pattern: str, replacement: str) -> None:
    text = load(path)
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(f"pattern matched {count} times in {path}: {pattern[:180]!r}")
    save(path, updated)


replace_once("CMakeLists.txt", "project(Runner VERSION 0.7.6 LANGUAGES CXX)",
             "project(Runner VERSION 0.7.7 LANGUAGES CXX)")
replace_once("src/ppo.hpp",
             "inline constexpr std::uint32_t training_semantics_version = 0x0007'0600u;",
             "inline constexpr std::uint32_t training_semantics_version = 0x0007'0700u;")
replace_once("src/autonomy_persistence.cpp", 'output << "RUNAUTONOMY 11\\n";',
             'output << "RUNAUTONOMY 12\\n";')
replace_once("src/autonomy_persistence.cpp", 'magic != "RUNAUTONOMY" || version != 11',
             'magic != "RUNAUTONOMY" || version != 12')

replace_once("src/simulation.hpp",
'''        [[nodiscard]] bool monopedal_gait() const noexcept
        {
            return active_motor_count >= 4u
                && motors[2].enabled && motors[3].enabled
                && motors[2].a == motors[3].a
                && motors[2].pivot == motors[3].pivot;
        }
''',
'''        [[nodiscard]] bool monopedal_gait() const noexcept
        {
            return active_motor_count >= 4u
                && motors[2].enabled && motors[3].enabled
                && motors[2].a == motors[3].a
                && motors[2].pivot == motors[3].pivot;
        }
        [[nodiscard]] bool paired_leg_chains() const noexcept
        {
            return !monopedal_gait() && active_motor_count >= 4u
                && motors[0].enabled && motors[1].enabled
                && motors[2].enabled && motors[3].enabled
                && motors[0].pivot == motors[2].pivot
                && motors[1].a == motors[0].pivot
                && motors[3].a == motors[2].pivot;
        }
''')
replace_once("src/simulation.hpp",
'''        [[nodiscard]] float maximum_upper_body_motor_deviation() const noexcept;
        [[nodiscard]] float posture_failure_seconds() const noexcept
''',
'''        [[nodiscard]] float maximum_upper_body_motor_deviation() const noexcept;
        [[nodiscard]] float primary_support_span_ratio() const noexcept;
        [[nodiscard]] float posture_failure_seconds() const noexcept
''')

sub_once("src/simulation.cpp",
         r'''    void Environment::separate_support_clusters\(\) noexcept\n    \{.*?\n    \}\n\n    bool Environment::body_integrity_valid''',
'''    void Environment::separate_support_clusters() noexcept
    {
        std::array<std::uint16_t, 32> supports{};
        std::size_t support_count = 0;
        auto append = [&](std::uint16_t node)
        {
            if (!valid_node(node) || support_count >= supports.size())
                return;
            if (std::find(supports.begin(), supports.begin() + support_count, node)
                == supports.begin() + support_count)
                supports[support_count++] = node;
        };
        append(blueprint_.left_contact_node);
        append(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            append(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            append(node);

        for (std::size_t first = 0; first < support_count; ++first)
        {
            const std::uint16_t first_index = supports[first];
            Particle& lhs = particles_[first_index];
            for (std::size_t second = first + 1; second < support_count; ++second)
            {
                const std::uint16_t second_index = supports[second];
                if (first_index == second_index
                    || direct_bone(blueprint_, first_index, second_index))
                    continue;
                Particle& rhs = particles_[second_index];
                const float minimum_gap = lhs.radius + rhs.radius + 0.035f;
                const float horizontal = rhs.position.x - lhs.position.x;
                if (std::abs(horizontal) >= minimum_gap)
                    continue;
                float authored_direction = blueprint_.nodes[second_index].x
                    - blueprint_.nodes[first_index].x;
                if (std::abs(authored_direction) < 1.0e-4f)
                    authored_direction = horizontal;
                const float direction = authored_direction < 0.0f ? -1.0f : 1.0f;
                const float correction = (minimum_gap - std::abs(horizontal)) * 0.5f;
                lhs.position.x -= direction * correction;
                lhs.previous.x -= direction * correction * 0.35f;
                rhs.position.x += direction * correction;
                rhs.previous.x += direction * correction * 0.35f;
            }
        }
    }

    bool Environment::body_integrity_valid''')

replace_once("src/simulation.cpp",
'''    bool Environment::current_display_posture_valid() const noexcept
    {
''',
'''    float Environment::primary_support_span_ratio() const noexcept
    {
        if (!valid_node(blueprint_.left_contact_node)
            || !valid_node(blueprint_.right_contact_node)
            || blueprint_.left_contact_node >= blueprint_.nodes.size()
            || blueprint_.right_contact_node >= blueprint_.nodes.size())
            return 1.0f;
        const float rest_span = std::abs(
            blueprint_.nodes[blueprint_.right_contact_node].x
            - blueprint_.nodes[blueprint_.left_contact_node].x);
        if (rest_span < 0.08f)
            return 1.0f;
        const float current_span = std::abs(
            particles_[blueprint_.right_contact_node].position.x
            - particles_[blueprint_.left_contact_node].position.x);
        return current_span / rest_span;
    }

    bool Environment::current_display_posture_valid() const noexcept
    {
''')
replace_once("src/simulation.cpp",
'''        return torso_uprightness() >= 0.60f
            && head.y > root.y + 0.20f;
''',
'''        const bool support_layout_valid = !blueprint_.paired_leg_chains()
            || (primary_support_span_ratio() >= 0.48f
                && primary_support_span_ratio() <= 1.85f);
        return torso_uprightness() >= 0.60f
            && head.y > root.y + 0.20f
            && support_layout_valid;
''')
replace_once("src/simulation.cpp",
'''        const bool catastrophic_stance_failure = non_foot_grounded_
            || current_uprightness < 0.70f
            || head_height_ratio < 0.52f;
        const bool stable_stance_frame = feet_supported
            && current_uprightness >= 0.84f
''',
'''        const float support_span_ratio = primary_support_span_ratio();
        const bool support_layout_valid = !blueprint_.paired_leg_chains()
            || (support_span_ratio >= 0.55f && support_span_ratio <= 1.65f);
        const bool catastrophic_stance_failure = non_foot_grounded_
            || current_uprightness < 0.70f
            || head_height_ratio < 0.52f
            || (blueprint_.paired_leg_chains()
                && (support_span_ratio < 0.35f || support_span_ratio > 2.10f));
        const bool stable_stance_frame = feet_supported
            && support_layout_valid
            && current_uprightness >= 0.84f
''')

sub_once("src/ppo.hpp",
         r'''    \[\[nodiscard\]\] inline std::array<float, sim::action_count> balance_teacher_action\(\n        const sim::Environment& environment\) noexcept\n    \{.*?\n    \}\n\n    \[\[nodiscard\]\] inline std::array<float, sim::action_count> bilateral_joint_synergy_action''',
'''    [[nodiscard]] inline std::array<float, sim::action_count> balance_teacher_action(
        const sim::Environment& environment) noexcept
    {
        constexpr std::size_t joint_angle_begin = 4;
        constexpr std::size_t joint_velocity_begin = joint_angle_begin + sim::action_count;
        static_assert(sim::observation_count == 40);
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

        const bool support_loaded = environment.left_supported()
            || environment.right_supported();
        const float upper_body_authority = support_loaded
            && environment.stable_stance_seconds() >= 0.75f ? 0.35f : 0.10f;
        for (std::size_t index = 4; index < rig.active_motor_count; ++index)
            action[index] *= upper_body_authority;
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
        return std::min<std::size_t>(2u * rig.active_motor_count + 4u, 24u);
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
        else if (lane == active * 2u)
        {
            for (std::size_t index = 0; index < active; ++index)
                probe.action[index] = amplitude;
        }
        else if (lane == active * 2u + 1u)
        {
            for (std::size_t index = 0; index < active; ++index)
                probe.action[index] = -amplitude;
        }
        else if (lane == active * 2u + 2u)
        {
            for (std::size_t index = 0; index < active; ++index)
                probe.action[index] = ((index / 2u) & 1u) == 0u
                    ? amplitude : -amplitude;
        }
        else
        {
            for (std::size_t index = 0; index < active; ++index)
                probe.action[index] = (index & 1u) == 0u
                    ? amplitude : -amplitude;
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
                || motor.pivot >= rig.nodes.size() || motor.c >= rig.nodes.size())
                continue;
            if (!rig.is_support_seed(motor.c))
                continue;
            const sim::Vec2 reference = rig.nodes[motor.a] - rig.nodes[motor.pivot];
            const sim::Vec2 driven = rig.nodes[motor.c] - rig.nodes[motor.pivot];
            if (sim::length(reference) <= 1.0e-5f || sim::length(driven) <= 1.0e-5f)
                continue;
            sim::Vec2 compact = driven;
            compact.x *= 1.0f + pressure * 0.34f;
            compact.y *= 1.0f - pressure * 0.26f;
            const float target = sim::signed_angle(reference, compact);
            const float desired = motor_action_for_target_angle(motor, target);
            action[index] = lerp(action[index], desired, 0.78f);
        }
        return action;
    }

    [[nodiscard]] inline std::array<float, sim::action_count> bilateral_joint_synergy_action''')

replace_once("src/ppo.hpp",
'''        const bool paired_leg_chains = rig.active_motor_count >= 4u
            && rig.motors[0].enabled && rig.motors[1].enabled
            && rig.motors[2].enabled && rig.motors[3].enabled
            && rig.motors[0].pivot == rig.motors[2].pivot
            && rig.motors[1].a == rig.motors[0].pivot
            && rig.motors[3].a == rig.motors[2].pivot;
''',
'''        const bool paired_leg_chains = rig.paired_leg_chains();
''')
replace_once("src/ppo.hpp",
'''        if (!paired_leg_chains)
            return action;

        const float leg_pair_strength = (stage == sim::CourseStage::duck_press
                || stage == sim::CourseStage::crouch_walk)
            ? 0.12f : (stage == sim::CourseStage::balance
                ? 0.18f : (stage == sim::CourseStage::ramps ? 0.22f : 0.18f));
''',
'''        if (!paired_leg_chains)
            return action;
        if (stage == sim::CourseStage::balance)
        {
            for (float& value : action)
                value = clamp(value, -1.0f, 1.0f);
            return action;
        }

        const float leg_pair_strength = stage == sim::CourseStage::duck_press
            ? 0.04f : (stage == sim::CourseStage::crouch_walk
                ? 0.10f : (stage == sim::CourseStage::ramps ? 0.22f : 0.18f));
''')
replace_once("src/ppo.hpp", "        constexpr float chain_strength = 0.04f;\n",
'''        const float chain_strength = stage == sim::CourseStage::duck_press
            ? 0.0f : 0.04f;
''')
replace_once("src/ppo.hpp",
'''        const float hip_pair_mean = 0.5f * (action[0] + action[2]);
        const float knee_pair_mean = 0.5f * (action[1] + action[3]);
        action[0] -= hip_pair_mean * 0.25f;
        action[2] -= hip_pair_mean * 0.25f;
        action[1] -= knee_pair_mean * 0.25f;
        action[3] -= knee_pair_mean * 0.25f;
''',
'''        if (stage != sim::CourseStage::duck_press)
        {
            const float hip_pair_mean = 0.5f * (action[0] + action[2]);
            const float knee_pair_mean = 0.5f * (action[1] + action[3]);
            action[0] -= hip_pair_mean * 0.25f;
            action[2] -= hip_pair_mean * 0.25f;
            action[1] -= knee_pair_mean * 0.25f;
            action[3] -= knee_pair_mean * 0.25f;
        }
''')

sub_once("src/ppo.hpp",
         r'''    \[\[nodiscard\]\] inline std::array<float, sim::action_count> duck_teacher_action\(\n        const sim::Environment& environment\) noexcept\n    \{.*?\n    \}\n\n    \[\[nodiscard\]\] inline std::array<float, sim::action_count> crouch_walk_teacher_action''',
'''    [[nodiscard]] inline std::array<float, sim::action_count> duck_teacher_action(
        const sim::Environment& environment) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const float pressure = environment.duck_press_completed()
            ? 0.0f : environment.duck_obstacle_weight();
        auto action = rig.paired_leg_chains()
            ? balance_teacher_action(environment)
            : compact_support_teacher_action(environment, pressure);
        if (rig.paired_leg_chains() && !environment.duck_press_completed())
        {
            const float span_ratio = environment.primary_support_span_ratio();
            const float outward_help = clamp((0.82f - span_ratio) * 0.10f, 0.0f, 0.08f);
            const float inward_help = clamp((span_ratio - 1.15f) * 0.16f, 0.0f, 0.14f);
            const float hip = 0.065f * pressure + outward_help - inward_help;
            const float knee = 0.52f * pressure;
            action[0] = clamp(action[0] - hip, -0.48f, 0.48f);
            action[1] = clamp(action[1] + knee, -0.78f, 0.78f);
            action[2] = clamp(action[2] + hip, -0.48f, 0.48f);
            action[3] = clamp(action[3] - knee, -0.78f, 0.78f);
        }
        for (std::size_t index = 4; index < rig.active_motor_count; ++index)
            action[index] = 0.0f;
        return bilateral_joint_synergy_action(environment, action,
            sim::CourseStage::duck_press);
    }

    [[nodiscard]] inline std::array<float, sim::action_count> crouch_walk_teacher_action''')

replace_once("src/ppo.hpp",
'''        if (stage == sim::CourseStage::balance)
        {
            const auto teacher = balance_teacher_action(environment);
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.96f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.97f);
        }
''',
'''        if (stage == sim::CourseStage::balance)
        {
            const auto teacher = balance_teacher_action(environment);
            const bool established = environment.stable_stance_seconds() >= 0.75f;
            const float leg_assist = established ? 0.46f : 0.72f;
            const float body_assist = established ? 0.52f : 0.78f;
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], leg_assist);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], body_assist);
        }
''')
replace_once("src/ppo.hpp",
             "            const float leg_assist = 0.72f + pressure * 0.24f;\n",
             "            const float leg_assist = 0.58f + pressure * 0.20f;\n")

sub_once("src/ppo_trainer.cpp",
         r'''        \[\[nodiscard\]\] float skill_bootstrap_weight\(std::uint64_t update,\n            sim::CourseStage stage\) noexcept\n        \{.*?\n        \}\n\n        \[\[nodiscard\]\] std::array<float, sim::action_count> skill_bootstrap_action''',
'''        [[nodiscard]] float skill_bootstrap_weight(std::uint64_t update,
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

        [[nodiscard]] std::array<float, sim::action_count> skill_bootstrap_action''')
replace_once("src/ppo_trainer.cpp",
'''                    const float guided_action = lerp(transition.action[action_index],
                        guided[action_index], bootstrap);
                    transition.action[action_index] = clamp(
                        lerp(previous_action[action_index], guided_action, 0.42f), -1.0f, 1.0f);
''',
'''                    const float guided_action = lerp(transition.action[action_index],
                        guided[action_index], bootstrap);
                    transition.action[action_index] = clamp(
                        lerp(previous_action[action_index], guided_action, 0.60f), -1.0f, 1.0f);
''')
replace_once("src/ppo_trainer.cpp",
'''                transition.action = effective_policy_action(
                    environment, transition.action, course_stage_);
''',
'''                const MotorDiscoveryProbe probe = motor_discovery_probe(
                    environment, environment_index, metrics_.update, step);
                for (std::size_t action_index = 0; action_index < transition.action.size(); ++action_index)
                    transition.action[action_index] = lerp(transition.action[action_index],
                        probe.action[action_index], probe.weight);
                transition.action = effective_policy_action(
                    environment, transition.action, course_stage_);
''')

replace_once("src/ppo.hpp",
'''            if (environment.maximum_upper_body_motor_deviation()
                > standing_neutral_arm_limit)
                rejection |= evidence_bit(MotionEvidenceFailure::non_neutral_posture);
''',
'''            if (environment.maximum_upper_body_motor_deviation()
                > standing_neutral_arm_limit)
                rejection |= evidence_bit(MotionEvidenceFailure::non_neutral_posture);
            if (environment.blueprint().paired_leg_chains()
                && (environment.primary_support_span_ratio() < 0.55f
                    || environment.primary_support_span_ratio() > 1.65f))
                rejection |= evidence_bit(MotionEvidenceFailure::non_neutral_posture);
''')
replace_once("src/ppo.hpp",
'''            if (!environment.duck_press_completed()
                || environment.duck_recoveries() < 1u
                || environment.duck_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
''',
'''            if (!environment.duck_press_completed()
                || environment.duck_recoveries() < 1u
                || environment.duck_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.blueprint().paired_leg_chains()
                && (environment.primary_support_span_ratio() < 0.42f
                    || environment.primary_support_span_ratio() > 1.82f))
                rejection |= evidence_bit(MotionEvidenceFailure::non_neutral_posture);
''')
replace_once("src/ppo.hpp",
'''                && environment.uncontrolled_spin_turns()
                    <= standing_qualification_spin_limit;
''',
'''                && environment.uncontrolled_spin_turns()
                    <= standing_qualification_spin_limit
                && (!environment.blueprint().paired_leg_chains()
                    || (environment.primary_support_span_ratio() >= 0.55f
                        && environment.primary_support_span_ratio() <= 1.65f));
''')

replace_once("src/simulation.cpp",
'''        const float upper_body_posture_penalty = course_stage_ == CourseStage::balance
            ? std::max(0.0f, upper_body_deviation - 0.30f) * 0.035f
            : 0.0f;
''',
'''        const float upper_body_posture_penalty = course_stage_ == CourseStage::balance
            ? std::max(0.0f, upper_body_deviation - 0.30f) * 0.035f
            : 0.0f;
        const float support_span_error = blueprint_.paired_leg_chains()
            ? std::abs(primary_support_span_ratio() - 1.0f) : 0.0f;
        const float support_span_penalty = std::max(0.0f,
            support_span_error - 0.22f) * 0.090f;
''')
replace_once("src/simulation.cpp",
'''                - upper_body_posture_penalty
                - body_contact_penalty;
''',
'''                - upper_body_posture_penalty
                - support_span_penalty
                - body_contact_penalty;
''')
replace_once("src/simulation.cpp",
'''                    - action_energy * 0.0009f - torso_swing_penalty
                    - premature_duck_penalty - body_contact_penalty;
''',
'''                    - action_energy * 0.0009f - torso_swing_penalty
                    - support_span_penalty
                    - premature_duck_penalty - body_contact_penalty;
''')
replace_once("src/simulation.cpp",
'''            else
            {
                const float maintained_crouch = duck_active_ && !non_foot_grounded_
                    ? 0.030f : -0.050f;
                last_reward_ = forward_gait_reward + maintained_crouch
                    + std::max(0.0f, upright) * 0.010f
                    + duck_reward * 1.25f + obstacle_duck_reward
                    + swing_reward + run_reward + real_step_reward
                    + obstacle_lift_reward + pass_reward
                    - backward_penalty - unearned_progress_penalty
                    - double_support_shuffle_penalty - action_energy * 0.0010f
                    - action_change_penalty - collision_penalty - knee_first_penalty
                    - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                    - body_contact_penalty * 2.0f - torso_swing_penalty;
            }
            break;
''',
'''            else
            {
                const float recovered_pose = !duck_active_ && !non_foot_grounded_
                    && stable_stance_seconds_ >= 0.40f ? 0.065f : 0.0f;
                last_reward_ = recovered_pose
                    + std::max(0.0f, upright) * 0.016f
                    + contact * 0.0015f + pass_reward
                    - std::abs(forward_speed_) * 0.0080f
                    - std::abs(distance_travelled_) * 0.0040f
                    - action_energy * 0.0009f - action_change_penalty
                    - support_span_penalty - torso_swing_penalty
                    - body_contact_penalty * 2.0f;
            }
            break;
''')

Path(__file__).unlink()
print("materialized v0.7.7 rig-specific controller and motor discovery")
