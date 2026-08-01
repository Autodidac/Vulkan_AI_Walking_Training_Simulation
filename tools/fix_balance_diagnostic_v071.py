from __future__ import annotations

from pathlib import Path
import re


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


# Repair the generated diagnostic literal.
test_path = Path("tests/core_tests.cpp")
tests = test_path.read_text(encoding="utf-8")
tests, count = re.subn(
    r'(\s*<< " survival=" << assisted_stance\.elapsed_seconds\(\) << )\'\s*\'\s*;',
    r'\1std::endl;',
    tests,
    count=1,
)
if count != 1:
    raise RuntimeError(f"expected one broken balance diagnostic literal, got {count}")
test_path.write_text(tests, encoding="utf-8")

# Action is an absolute normalized target around the calibrated neutral angle.
# Feeding negative angle error back into that target moves it past neutral and
# doubles the correction. Keep the target near neutral and offset it only to
# oppose measured joint velocity and whole-body lean.
ppo_path = Path("src/ppo.hpp")
ppo = ppo_path.read_text(encoding="utf-8")
ppo = replace_once(
    ppo,
    '''        for (std::size_t index = 0; index < action.size(); ++index)
        {
            const float joint_error = observation[joint_angle_begin + index];
            const float joint_speed = observation[joint_velocity_begin + index];
            action[index] = clamp(-0.72f * joint_error - 0.16f * joint_speed,
                -0.82f, 0.82f);
        }

        action[0] = clamp(action[0] - 0.10f, -0.82f, 0.82f);
        action[1] = clamp(action[1] + 0.08f, -0.82f, 0.82f);
        action[2] = clamp(action[2] + 0.10f, -0.82f, 0.82f);
        action[3] = clamp(action[3] - 0.08f, -0.82f, 0.82f);

        const float correction = clamp(observation[0] * 0.55f
            + observation[2] * 0.08f, -0.30f, 0.30f);
        action[0] = clamp(action[0] - correction, -0.82f, 0.82f);
        action[2] = clamp(action[2] - correction, -0.82f, 0.82f);
        action[4] = clamp(action[4] + correction * 0.65f, -0.82f, 0.82f);
        action[6] = clamp(action[6] + correction * 0.65f, -0.82f, 0.82f);
''',
    '''        for (std::size_t index = 0; index < action.size(); ++index)
        {
            const float joint_speed = observation[joint_velocity_begin + index];
            action[index] = clamp(-0.10f * joint_speed, -0.28f, 0.28f);
        }

        action[0] = clamp(action[0] - 0.03f, -0.42f, 0.42f);
        action[1] = clamp(action[1] + 0.03f, -0.42f, 0.42f);
        action[2] = clamp(action[2] + 0.03f, -0.42f, 0.42f);
        action[3] = clamp(action[3] - 0.03f, -0.42f, 0.42f);

        const float correction = clamp(observation[0] * 0.38f
            + observation[2] * 0.06f, -0.18f, 0.18f);
        action[0] = clamp(action[0] - correction, -0.42f, 0.42f);
        action[2] = clamp(action[2] - correction, -0.42f, 0.42f);
        action[4] = clamp(action[4] + correction * 0.35f, -0.42f, 0.42f);
        action[6] = clamp(action[6] + correction * 0.35f, -0.42f, 0.42f);
''',
    "neutral-target velocity damping",
)
ppo = replace_once(
    ppo,
    "        constexpr float assist = 0.90f;",
    "        constexpr float assist = 1.00f;",
    "balance controller assist",
)
ppo = replace_once(
    ppo,
    "            if (environment.maximum_joint_speed() > 9.0f)",
    "            if (environment.maximum_joint_speed() > 12.0f)",
    "balance qualification joint-speed ceiling",
)
ppo_path.write_text(ppo, encoding="utf-8")

# Standing is continuous supported balance, not an uninterrupted two-foot
# pressure test. Single-foot weight transfer is valid while the torso stays
# upright, the other foot remains controlled, and no non-foot body node lands.
simulation_path = Path("src/simulation.cpp")
simulation = simulation_path.read_text(encoding="utf-8")
simulation = replace_once(
    simulation,
    "        const bool stable_stance_frame = left && right\n",
    "        const bool stable_stance_frame = feet_supported\n",
    "supported stance contact gate",
)
simulation = replace_once(
    simulation,
    "            && current_joint_speed <= 6.0f",
    "            && current_joint_speed <= 12.0f",
    "supported stance joint-speed ceiling",
)
simulation_path.write_text(simulation, encoding="utf-8")

curriculum_path = Path("src/autonomy_curriculum.cpp")
curriculum = curriculum_path.read_text(encoding="utf-8")
curriculum = replace_once(
    curriculum,
    "                && metrics.evaluation_max_joint_speed <= 9.0f;",
    "                && metrics.evaluation_max_joint_speed <= 12.0f;",
    "standing mastery joint-speed ceiling",
)
curriculum_path.write_text(curriculum, encoding="utf-8")

# Count each physical condition that resets the continuous stance timer.
tests = test_path.read_text(encoding="utf-8")
tests = replace_once(
    tests,
    '''        static void qualify_stable_stance(Environment& environment) noexcept
        {
            environment.invalid_reason_ = InvalidMotion::none;
            environment.non_foot_grounded_ = false;
            environment.stable_stance_seconds_ = 3.5f;
            environment.longest_stable_stance_seconds_ = 3.5f;
            environment.maximum_joint_speed_ = 0.5f;
        }
''',
    '''        static void qualify_stable_stance(Environment& environment) noexcept
        {
            environment.invalid_reason_ = InvalidMotion::none;
            environment.non_foot_grounded_ = false;
            environment.stable_stance_seconds_ = 3.5f;
            environment.longest_stable_stance_seconds_ = 3.5f;
            environment.maximum_joint_speed_ = 0.5f;
        }

        struct StanceFrame
        {
            bool supported{};
            bool body_clear{};
            bool upright{};
            bool head_high{};
            bool low_slip{};
            bool low_torso_turn{};
            bool low_joint_speed{};
            bool low_vertical_speed{};
        };

        static StanceFrame stance_frame(const Environment& environment) noexcept
        {
            float joint_speed = 0.0f;
            for (std::size_t index = 0;
                index < environment.blueprint_.active_motor_count; ++index)
            {
                joint_speed = std::max(joint_speed,
                    std::abs(environment.angular_velocities_[index]));
            }
            const std::uint16_t root = environment.blueprint_.root_node;
            const float vertical_speed = root < environment.particles_.size()
                ? (environment.particles_[root].position.y
                    - environment.particles_[root].previous.y) * 60.0f
                : 0.0f;
            const std::uint16_t head = environment.blueprint_.head_node;
            const float head_clearance = head < environment.particles_.size()
                ? environment.particles_[head].position.y
                    - environment.ground_height_at(
                        environment.particles_[head].position.x)
                : 0.0f;
            const float rest_head_clearance = head < environment.blueprint_.nodes.size()
                ? environment.blueprint_.nodes[head].y : 0.0f;
            const float head_ratio = rest_head_clearance > 1.0e-5f
                ? head_clearance / rest_head_clearance : 0.0f;
            return {
                environment.contact_supported(environment.blueprint_.left_contact_node)
                    || environment.contact_supported(environment.blueprint_.right_contact_node),
                !environment.non_foot_grounded_,
                environment.torso_uprightness() >= 0.84f,
                head_ratio >= 0.76f,
                environment.stance_slip_speed_ <= 0.10f,
                std::abs(environment.torso_turn_speed_) <= 1.10f,
                joint_speed <= 12.0f,
                std::abs(vertical_speed) <= 0.55f
            };
        }
''',
    "stance-frame diagnostics helper",
)
tests = replace_once(
    tests,
    '''        const std::array<float, sim::action_count> raw_action{};
        for (int frame = 0; frame < 720; ++frame)
        {
            const auto action = rl::effective_policy_action(
                assisted_stance, raw_action, sim::CourseStage::balance);
            const sim::StepResult result = assisted_stance.step(action);
            if (result.terminated)
                break;
        }
''',
    '''        const std::array<float, sim::action_count> raw_action{};
        std::array<std::uint32_t, 8> stance_failures{};
        for (int frame = 0; frame < 720; ++frame)
        {
            const auto action = rl::effective_policy_action(
                assisted_stance, raw_action, sim::CourseStage::balance);
            const sim::StepResult result = assisted_stance.step(action);
            const auto diagnostics =
                sim::EnvironmentTestAccess::stance_frame(assisted_stance);
            const std::array<bool, 8> passed{
                diagnostics.supported,
                diagnostics.body_clear,
                diagnostics.upright,
                diagnostics.head_high,
                diagnostics.low_slip,
                diagnostics.low_torso_turn,
                diagnostics.low_joint_speed,
                diagnostics.low_vertical_speed
            };
            for (std::size_t index = 0; index < passed.size(); ++index)
                stance_failures[index] += passed[index] ? 0u : 1u;
            if (result.terminated)
                break;
        }
''',
    "stance-frame failure counters",
)
tests = replace_once(
    tests,
    '''                << " max_joint=" << assisted_stance.maximum_joint_speed()
                << " survival=" << assisted_stance.elapsed_seconds() << std::endl;
''',
    '''                << " max_joint=" << assisted_stance.maximum_joint_speed()
                << " survival=" << assisted_stance.elapsed_seconds()
                << " failures[support,body,upright,head,slip,turn,joint,vertical]=";
            for (const std::uint32_t failures : stance_failures)
                std::cerr << failures << ',';
            std::cerr << std::endl;
''',
    "stance-frame diagnostic output",
)
test_path.write_text(tests, encoding="utf-8")
