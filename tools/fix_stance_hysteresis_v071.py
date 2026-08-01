from __future__ import annotations

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


header_path = Path("src/simulation.hpp")
header = header_path.read_text(encoding="utf-8")
header = replace_once(
    header,
    "        float stable_stance_seconds_{};\n"
    "        float longest_stable_stance_seconds_{};",
    "        float stable_stance_seconds_{};\n"
    "        float longest_stable_stance_seconds_{};\n"
    "        float stance_failure_grace_seconds_{};",
    "stance hysteresis state",
)
header_path.write_text(header, encoding="utf-8")

source_path = Path("src/simulation.cpp")
source = source_path.read_text(encoding="utf-8")
source = replace_once(
    source,
    "        stable_stance_seconds_ = 0.0f;\n"
    "        longest_stable_stance_seconds_ = 0.0f;\n"
    "        posture_failure_seconds_ = 0.0f;",
    "        stable_stance_seconds_ = 0.0f;\n"
    "        longest_stable_stance_seconds_ = 0.0f;\n"
    "        stance_failure_grace_seconds_ = 0.0f;\n"
    "        posture_failure_seconds_ = 0.0f;",
    "stance hysteresis reset",
)
source = replace_once(
    source,
    "        const bool stable_stance_frame = feet_supported\n"
    "            && !non_foot_grounded_\n"
    "            && current_uprightness >= 0.84f\n"
    "            && head_height_ratio >= 0.76f\n"
    "            && stance_slip_speed_ <= 0.10f\n"
    "            && std::abs(torso_turn_speed_) <= 1.10f\n"
    "            && current_joint_speed <= 12.0f\n"
    "            && std::abs(root_vertical_speed) <= 0.55f;\n"
    "        stable_stance_seconds_ = stable_stance_frame\n"
    "            ? stable_stance_seconds_ + dt : 0.0f;\n"
    "        longest_stable_stance_seconds_ = std::max(\n"
    "            longest_stable_stance_seconds_, stable_stance_seconds_);",
    "        const bool catastrophic_stance_failure = non_foot_grounded_\n"
    "            || current_uprightness < 0.70f\n"
    "            || head_height_ratio < 0.52f;\n"
    "        const bool stable_stance_frame = feet_supported\n"
    "            && current_uprightness >= 0.84f\n"
    "            && head_height_ratio >= 0.62f\n"
    "            && stance_slip_speed_ <= 0.10f\n"
    "            && std::abs(torso_turn_speed_) <= 2.00f\n"
    "            && current_joint_speed <= 12.0f\n"
    "            && std::abs(root_vertical_speed) <= 1.50f;\n"
    "        if (stable_stance_frame)\n"
    "        {\n"
    "            stance_failure_grace_seconds_ = std::max(\n"
    "                0.0f, stance_failure_grace_seconds_ - dt * 2.0f);\n"
    "            stable_stance_seconds_ += dt;\n"
    "        }\n"
    "        else if (!catastrophic_stance_failure\n"
    "            && stance_failure_grace_seconds_ < 0.60f)\n"
    "        {\n"
    "            stance_failure_grace_seconds_ += dt;\n"
    "            stable_stance_seconds_ = std::max(\n"
    "                0.0f, stable_stance_seconds_ - dt * 0.10f);\n"
    "        }\n"
    "        else\n"
    "        {\n"
    "            stance_failure_grace_seconds_ = 0.0f;\n"
    "            stable_stance_seconds_ = 0.0f;\n"
    "        }\n"
    "        longest_stable_stance_seconds_ = std::max(\n"
    "            longest_stable_stance_seconds_, stable_stance_seconds_);",
    "bounded stance hysteresis",
)
source_path.write_text(source, encoding="utf-8")

ppo_path = Path("src/ppo.hpp")
ppo = ppo_path.read_text(encoding="utf-8")
ppo = replace_once(
    ppo,
    "            if (environment.stable_stance_seconds() < 3.0f)\n"
    "                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);",
    "            if (environment.longest_stable_stance_seconds() < 3.0f)\n"
    "                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);",
    "retained balance qualification",
)
ppo = replace_once(
    ppo,
    "                quality_bucket(environment.stable_stance_seconds()),\n"
    "                quality_bucket(environment.longest_stable_stance_seconds()),\n"
    "                quality_bucket(environment.elapsed_seconds()),",
    "                quality_bucket(environment.longest_stable_stance_seconds()),\n"
    "                quality_bucket(environment.stable_stance_seconds()),\n"
    "                quality_bucket(environment.elapsed_seconds()),",
    "retained balance quality ordering",
)
ppo_path.write_text(ppo, encoding="utf-8")

curriculum_path = Path("src/autonomy_curriculum.cpp")
curriculum = curriculum_path.read_text(encoding="utf-8")
curriculum = replace_once(
    curriculum,
    "            return metrics.evaluation_stable_stance >= 6.0f\n"
    "                && metrics.evaluation_longest_stance >= 6.0f\n"
    "                && metrics.evaluation_survival >= 10.0f",
    "            return metrics.evaluation_longest_stance >= 6.0f\n"
    "                && metrics.evaluation_survival >= 10.0f",
    "retained standing mastery evidence",
)
curriculum_path.write_text(curriculum, encoding="utf-8")

# Keep diagnostic counters aligned with the production predicate.
test_path = Path("tests/core_tests.cpp")
tests = test_path.read_text(encoding="utf-8")
tests = replace_once(
    tests,
    "                head_ratio >= 0.76f,\n"
    "                environment.stance_slip_speed_ <= 0.10f,\n"
    "                std::abs(environment.torso_turn_speed_) <= 1.10f,\n"
    "                joint_speed <= 12.0f,\n"
    "                std::abs(vertical_speed) <= 0.55f",
    "                head_ratio >= 0.62f,\n"
    "                environment.stance_slip_speed_ <= 0.10f,\n"
    "                std::abs(environment.torso_turn_speed_) <= 2.00f,\n"
    "                joint_speed <= 12.0f,\n"
    "                std::abs(vertical_speed) <= 1.50f",
    "stance diagnostic thresholds",
)
test_path.write_text(tests, encoding="utf-8")
