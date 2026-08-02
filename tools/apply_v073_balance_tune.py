from pathlib import Path

root = Path(__file__).resolve().parents[1]


def patch(relative: str, replacements: tuple[tuple[str, str], ...]) -> None:
    path = root / relative
    text = path.read_text(encoding="utf-8")
    changed = False
    for old, new in replacements:
        if new in text:
            continue
        if old not in text:
            raise RuntimeError(f"balance tune target missing in {relative}: {old}")
        text = text.replace(old, new, 1)
        changed = True
    if changed:
        path.write_text(text, encoding="utf-8", newline="\n")


patch("src/simulation.cpp", ((
    "if (elapsed_seconds_ >= 3.50f && !body_integrity_valid())",
    "if (elapsed_seconds_ >= 8.00f && !body_integrity_valid())",
),))

patch("src/ppo.hpp", (
    (
        "action[0] = clamp(action[0] - 0.09f, -0.52f, 0.52f);\n"
        "            action[1] = clamp(action[1] + 0.14f, -0.56f, 0.56f);",
        "action[0] = clamp(action[0] - 0.025f, -0.48f, 0.48f);\n"
        "            action[1] = clamp(action[1] + 0.040f, -0.50f, 0.50f);",
    ),
    (
        "action[2] = clamp(action[2] + 0.09f, -0.52f, 0.52f);\n"
        "            action[3] = clamp(action[3] - 0.14f, -0.56f, 0.56f);",
        "action[2] = clamp(action[2] + 0.025f, -0.48f, 0.48f);\n"
        "            action[3] = clamp(action[3] - 0.040f, -0.50f, 0.50f);",
    ),
    (
        "const float leg_pair_strength = stage == sim::CourseStage::walk\n"
        "            ? 0.34f : (stage == sim::CourseStage::balance\n"
        "                || stage == sim::CourseStage::ramps ? 0.28f : 0.20f);",
        "const float leg_pair_strength = stage == sim::CourseStage::walk\n"
        "            ? 0.30f : (stage == sim::CourseStage::balance\n"
        "                ? 0.28f : (stage == sim::CourseStage::ramps ? 0.24f : 0.20f));",
    ),
    (
        "        action[2] = lerp(action[2], right_chain, chain_strength);\n"
        "        action[3] = lerp(action[3], -right_chain, chain_strength);\n\n"
        "        if (rig.active_motor_count >= 8u",
        "        action[2] = lerp(action[2], right_chain, chain_strength);\n"
        "        action[3] = lerp(action[3], -right_chain, chain_strength);\n\n"
        "        // The chain prior must not reintroduce same-direction pair motion.\n"
        "        // Remove only a quarter of each pair mean so the policy keeps\n"
        "        // useful residual freedom while the final commands stay coordinated.\n"
        "        const float hip_pair_mean = 0.5f * (action[0] + action[2]);\n"
        "        const float knee_pair_mean = 0.5f * (action[1] + action[3]);\n"
        "        action[0] -= hip_pair_mean * 0.25f;\n"
        "        action[2] -= hip_pair_mean * 0.25f;\n"
        "        action[1] -= knee_pair_mean * 0.25f;\n"
        "        action[3] -= knee_pair_mean * 0.25f;\n\n"
        "        if (rig.active_motor_count >= 8u",
    ),
))

path = root / "src/ppo.hpp"
text = path.read_text(encoding="utf-8")
needle = "        if (!environment.valid_motion())\n            rejection |= evidence_bit(MotionEvidenceFailure::invalid_motion);"
replacement = "        if (!environment.valid_motion() || !environment.body_integrity_valid())\n            rejection |= evidence_bit(MotionEvidenceFailure::invalid_motion);"
if replacement not in text:
    if needle not in text:
        raise RuntimeError("stage qualification integrity target missing")
    path.write_text(text.replace(needle, replacement, 1), encoding="utf-8", newline="\n")

print("tuned feet-first balance recovery and qualification integrity")
