from __future__ import annotations

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


# Balance uses the same motors as later locomotion, but it should not receive
# full acrobatic impulse while learning a static support manifold.
simulation_path = Path("src/simulation.cpp")
simulation = simulation_path.read_text(encoding="utf-8")
simulation = replace_once(
    simulation,
    "        const float correction = clamp(error, -0.24f, 0.24f) * motor.strength;",
    "        const float stage_motor_scale = course_stage_ == CourseStage::balance\n"
    "            ? 0.70f : 1.0f;\n"
    "        const float correction = clamp(error, -0.24f, 0.24f)\n"
    "            * motor.strength * stage_motor_scale;",
    "balance motor impulse scale",
)
simulation_path.write_text(simulation, encoding="utf-8")

# Arms have roughly twice the angular travel of legs. Applying identical
# normalized velocity feedback to them produces much larger target excursions
# and can kick the torso. Use separate damping and clamps.
ppo_path = Path("src/ppo.hpp")
ppo = ppo_path.read_text(encoding="utf-8")
ppo = replace_once(
    ppo,
    "            const float joint_speed = observation[joint_velocity_begin + index];\n"
    "            action[index] = clamp(-0.10f * joint_speed, -0.28f, 0.28f);",
    "            const float joint_speed = observation[joint_velocity_begin + index];\n"
    "            const bool arm_motor = index >= 4u;\n"
    "            const float damping = arm_motor ? 0.025f : 0.080f;\n"
    "            const float limit = arm_motor ? 0.12f : 0.24f;\n"
    "            action[index] = clamp(-damping * joint_speed, -limit, limit);",
    "travel-aware joint damping",
)
ppo = replace_once(
    ppo,
    "        const float correction = clamp(observation[0] * 0.38f\n"
    "            + observation[2] * 0.06f, -0.18f, 0.18f);\n"
    "        action[0] = clamp(action[0] - correction, -0.42f, 0.42f);\n"
    "        action[2] = clamp(action[2] - correction, -0.42f, 0.42f);\n"
    "        action[4] = clamp(action[4] + correction * 0.35f, -0.42f, 0.42f);\n"
    "        action[6] = clamp(action[6] + correction * 0.35f, -0.42f, 0.42f);",
    "        const float correction = clamp(observation[0] * 0.32f\n"
    "            + observation[2] * 0.05f, -0.14f, 0.14f);\n"
    "        action[0] = clamp(action[0] - correction, -0.32f, 0.32f);\n"
    "        action[2] = clamp(action[2] - correction, -0.32f, 0.32f);\n"
    "        action[4] = clamp(action[4] + correction * 0.10f, -0.14f, 0.14f);\n"
    "        action[6] = clamp(action[6] + correction * 0.10f, -0.14f, 0.14f);",
    "bounded torso counterbalance",
)
ppo_path.write_text(ppo, encoding="utf-8")

# Match PPO's deterministic evaluation seeds exactly.
test_path = Path("tests/core_tests.cpp")
tests = test_path.read_text(encoding="utf-8")
anchor = '''    require(rl::policy_candidate_better(2u, 1.0f, 1u, 1000.0f, true),
        "higher stage-valid evidence loses to scalar reward");
'''
addition = '''    {
        constexpr std::size_t evaluation_agents = 6;
        std::uint32_t valid_agents = 0;
        for (std::size_t agent = 0; agent < evaluation_agents; ++agent)
        {
            const std::uint64_t seed = 0xE000u
                + static_cast<std::uint64_t>(agent) * 4099u;
            sim::Environment environment{ humanoid, seed };
            environment.set_course(sim::CourseStage::balance, 0.25f);
            const std::array<float, sim::action_count> raw_action{};
            for (int frame = 0; frame < 1200; ++frame)
            {
                const auto action = rl::effective_policy_action(
                    environment, raw_action, sim::CourseStage::balance);
                const sim::StepResult result = environment.step(action);
                if (result.terminated)
                    break;
            }
            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(sim::CourseStage::balance, environment);
            valid_agents += qualification.valid ? 1u : 0u;
            if (!qualification.valid)
            {
                std::cerr << "evaluation seed " << seed
                    << " rejection=" << qualification.rejection_mask
                    << " invalid=" << static_cast<int>(environment.invalid_reason())
                    << " stance=" << environment.stable_stance_seconds()
                    << " longest=" << environment.longest_stable_stance_seconds()
                    << " max_joint=" << environment.maximum_joint_speed()
                    << " survival=" << environment.elapsed_seconds() << std::endl;
            }
        }
        require(valid_agents == evaluation_agents,
            "shared balance controller is not valid across all PPO evaluation seeds");
    }

'''
tests = replace_once(tests, anchor, addition + anchor, "evaluation seed robustness test")
test_path.write_text(tests, encoding="utf-8")
