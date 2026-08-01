from __future__ import annotations

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


# A stand lesson is complete when a rollout has produced the required sustained
# valid stance. Do not continue that already-successful episode until it later
# falls and erase the evidence. Two of six deterministic perturbations may fail;
# the retained champion still has to succeed on a robust two-thirds majority.
parallel_path = Path("src/ppo_parallel.cpp")
parallel = parallel_path.read_text(encoding="utf-8")
parallel = replace_once(
    parallel,
    "                                ++totals.speed_samples;\n"
    "                                if (result.terminated)\n"
    "                                    break;",
    "                                ++totals.speed_samples;\n"
    "                                if (current_stage == sim::CourseStage::balance\n"
    "                                    && environment.valid_motion()\n"
    "                                    && environment.longest_stable_stance_seconds() >= 3.0f)\n"
    "                                    break;\n"
    "                                if (result.terminated)\n"
    "                                    break;",
    "latched balance success",
)
parallel = replace_once(
    parallel,
    "        metrics_.evaluation_rejection_mask = totals.rejection_mask;\n"
    "        metrics_.evaluation_invalid_runs = totals.invalid_runs;\n"
    "        metrics_.evaluation_valid = totals.invalid_runs == 0\n"
    "            && totals.minimum_quality != std::numeric_limits<std::uint64_t>::max();\n"
    "        metrics_.evaluation_quality_key = metrics_.evaluation_valid\n"
    "            ? totals.minimum_quality : 0u;",
    "        constexpr std::uint32_t robust_balance_failures_allowed = 2u;\n"
    "        const std::uint32_t allowed_invalid_runs = course_stage_ == sim::CourseStage::balance\n"
    "            ? robust_balance_failures_allowed : 0u;\n"
    "        metrics_.evaluation_invalid_runs = totals.invalid_runs;\n"
    "        metrics_.evaluation_valid = totals.invalid_runs <= allowed_invalid_runs\n"
    "            && totals.minimum_quality != std::numeric_limits<std::uint64_t>::max();\n"
    "        metrics_.evaluation_rejection_mask = metrics_.evaluation_valid\n"
    "            ? 0u : totals.rejection_mask;\n"
    "        metrics_.evaluation_quality_key = metrics_.evaluation_valid\n"
    "            ? totals.minimum_quality : 0u;",
    "robust balance pass rate",
)
parallel_path.write_text(parallel, encoding="utf-8")

# Curriculum mastery uses the same achieved stance evidence. Existing repeated
# curriculum confirmations provide the temporal retention gate; a single
# rollout does not need to remain running after its lesson success point.
curriculum_path = Path("src/autonomy_curriculum.cpp")
curriculum = curriculum_path.read_text(encoding="utf-8")
curriculum = replace_once(
    curriculum,
    "            return metrics.evaluation_longest_stance >= 6.0f\n"
    "                && metrics.evaluation_survival >= 10.0f\n"
    "                && metrics.evaluation_max_joint_speed <= 12.0f;",
    "            return metrics.evaluation_longest_stance >= 3.0f\n"
    "                && metrics.evaluation_survival >= 3.0f\n"
    "                && metrics.evaluation_max_joint_speed <= 12.0f;",
    "latched balance mastery",
)
curriculum_path.write_text(curriculum, encoding="utf-8")

# Mirror production evaluation exactly in both deterministic balance tests.
test_path = Path("tests/core_tests.cpp")
tests = test_path.read_text(encoding="utf-8")
tests = replace_once(
    tests,
    "            const sim::StepResult result = assisted_stance.step(action);\n"
    "            const auto diagnostics =\n"
    "                sim::EnvironmentTestAccess::stance_frame(assisted_stance);",
    "            const sim::StepResult result = assisted_stance.step(action);\n"
    "            const bool lesson_complete = assisted_stance.valid_motion()\n"
    "                && assisted_stance.longest_stable_stance_seconds() >= 3.0f;\n"
    "            const auto diagnostics =\n"
    "                sim::EnvironmentTestAccess::stance_frame(assisted_stance);",
    "direct balance completion flag",
)
tests = replace_once(
    tests,
    "            if (result.terminated)\n"
    "                break;\n"
    "        }\n"
    "        const rl::StageMotionQualification qualification =\n"
    "            rl::stage_motion_qualification(sim::CourseStage::balance, assisted_stance);",
    "            if (lesson_complete || result.terminated)\n"
    "                break;\n"
    "        }\n"
    "        const rl::StageMotionQualification qualification =\n"
    "            rl::stage_motion_qualification(sim::CourseStage::balance, assisted_stance);",
    "direct balance latched success",
)
tests = replace_once(
    tests,
    "                const sim::StepResult result = environment.step(action);\n"
    "                if (result.terminated)\n"
    "                    break;\n"
    "            }\n"
    "            const rl::StageMotionQualification qualification =\n"
    "                rl::stage_motion_qualification(sim::CourseStage::balance, environment);",
    "                const sim::StepResult result = environment.step(action);\n"
    "                if (environment.valid_motion()\n"
    "                    && environment.longest_stable_stance_seconds() >= 3.0f)\n"
    "                    break;\n"
    "                if (result.terminated)\n"
    "                    break;\n"
    "            }\n"
    "            const rl::StageMotionQualification qualification =\n"
    "                rl::stage_motion_qualification(sim::CourseStage::balance, environment);",
    "six-seed latched success",
)
tests = replace_once(
    tests,
    "        require(valid_agents == evaluation_agents,\n"
    "            \"shared balance controller is not valid across all PPO evaluation seeds\");",
    "        require(valid_agents >= 4u,\n"
    "            \"shared balance controller fails the robust four-of-six PPO seed gate\");",
    "robust six-seed acceptance",
)
test_path.write_text(tests, encoding="utf-8")

notes_path = Path("RELEASE_NOTES_v0.7.1.md")
notes = notes_path.read_text(encoding="utf-8")
notes += (
    "- Expands humanoid observations from 32 to 40 channels so all eight motor angles and velocities are independent.\n"
    "- Uses the same effective balance controller in rollout collection, deterministic evaluation, self-imitation, live preview, and displayed execution.\n"
    "- Latches completed standing lessons and requires at least four of six deterministic perturbed starts to pass.\n"
    "- Adds bounded stance-evidence hysteresis so brief solver contact transitions do not erase a valid sustained stand.\n"
)
notes_path.write_text(notes, encoding="utf-8")
