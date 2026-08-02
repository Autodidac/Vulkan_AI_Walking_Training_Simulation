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
        "            ? 0.28f : (stage == sim::CourseStage::balance\n"
        "                ? 0.14f : (stage == sim::CourseStage::ramps ? 0.22f : 0.18f));",
    ),
    (
        "policy_action[index] = lerp(policy_action[index], teacher[index], 0.90f);",
        "policy_action[index] = lerp(policy_action[index], teacher[index], 0.82f);",
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
