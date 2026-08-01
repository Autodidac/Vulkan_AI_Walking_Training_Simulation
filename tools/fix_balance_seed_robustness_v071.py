from __future__ import annotations

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


# Match PPO's deterministic evaluation seeds exactly without changing the
# controller that already passed the twelve-second single-seed physics test.
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
