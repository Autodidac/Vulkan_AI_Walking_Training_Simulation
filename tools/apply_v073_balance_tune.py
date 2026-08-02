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
        "        action[0] = clamp(action[0] - 0.035f, -0.46f, 0.46f);\n"
        "        action[1] = clamp(action[1] + 0.045f, -0.46f, 0.46f);\n"
        "        action[2] = clamp(action[2] + 0.035f, -0.46f, 0.46f);\n"
        "        action[3] = clamp(action[3] - 0.045f, -0.46f, 0.46f);",
        "        // Rest geometry is the standing target. Do not preload a crouch\n"
        "        // before both rigid feet have established support.\n"
        "        action[0] = clamp(action[0], -0.34f, 0.34f);\n"
        "        action[1] = clamp(action[1], -0.34f, 0.34f);\n"
        "        action[2] = clamp(action[2], -0.34f, 0.34f);\n"
        "        action[3] = clamp(action[3], -0.34f, 0.34f);",
    ),
    (
        "action[0] = clamp(action[0] - 0.09f, -0.52f, 0.52f);\n"
        "            action[1] = clamp(action[1] + 0.14f, -0.56f, 0.56f);",
        "action[0] = clamp(action[0] - 0.012f, -0.38f, 0.38f);\n"
        "            action[1] = clamp(action[1] + 0.020f, -0.40f, 0.40f);",
    ),
    (
        "action[2] = clamp(action[2] + 0.09f, -0.52f, 0.52f);\n"
        "            action[3] = clamp(action[3] - 0.14f, -0.56f, 0.56f);",
        "action[2] = clamp(action[2] + 0.012f, -0.38f, 0.38f);\n"
        "            action[3] = clamp(action[3] - 0.020f, -0.40f, 0.40f);",
    ),
    (
        "const float leg_pair_strength = stage == sim::CourseStage::walk\n"
        "            ? 0.34f : (stage == sim::CourseStage::balance\n"
        "                || stage == sim::CourseStage::ramps ? 0.28f : 0.20f);",
        "const float leg_pair_strength = stage == sim::CourseStage::walk\n"
        "            ? 0.28f : (stage == sim::CourseStage::balance\n"
        "                ? 0.18f : (stage == sim::CourseStage::ramps ? 0.22f : 0.18f));",
    ),
    (
        "constexpr float chain_strength = 0.08f;",
        "constexpr float chain_strength = 0.04f;",
    ),
    (
        "        action[2] = lerp(action[2], right_chain, chain_strength);\n"
        "        action[3] = lerp(action[3], -right_chain, chain_strength);\n\n"
        "        if (rig.active_motor_count >= 8u",
        "        action[2] = lerp(action[2], right_chain, chain_strength);\n"
        "        action[3] = lerp(action[3], -right_chain, chain_strength);\n\n"
        "        // The chain prior must not reintroduce same-direction pair motion.\n"
        "        const float hip_pair_mean = 0.5f * (action[0] + action[2]);\n"
        "        const float knee_pair_mean = 0.5f * (action[1] + action[3]);\n"
        "        action[0] -= hip_pair_mean * 0.25f;\n"
        "        action[2] -= hip_pair_mean * 0.25f;\n"
        "        action[1] -= knee_pair_mean * 0.25f;\n"
        "        action[3] -= knee_pair_mean * 0.25f;\n\n"
        "        if (rig.active_motor_count >= 8u",
    ),
    (
        "policy_action[index] = lerp(policy_action[index], teacher[index], 0.90f);",
        "policy_action[index] = lerp(policy_action[index], teacher[index], 0.96f);",
    ),
    (
        "    {\n        return stage_motion_qualification(stage, environment).valid;\n    }\n\n    [[nodiscard]] inline bool elite_motion_eligible(sim::CourseStage stage, bool valid_motion,",
        "    {\n        return environment.body_integrity_valid()\n            && stage_motion_qualification(stage, environment).valid;\n    }\n\n    [[nodiscard]] inline bool elite_motion_eligible(sim::CourseStage stage, bool valid_motion,",
    ),
))

patch("src/ppo_parallel.cpp", ((
    "                            if (!qualification.valid)\n"
    "                            {\n"
    "                                ++totals.invalid_runs;\n"
    "                                totals.rejection_mask |= qualification.rejection_mask;\n"
    "                            }",
    "                            if (!qualification.valid || !environment.body_integrity_valid())\n"
    "                            {\n"
    "                                ++totals.invalid_runs;\n"
    "                                totals.rejection_mask |= qualification.rejection_mask;\n"
    "                                if (!environment.body_integrity_valid())\n"
    "                                    totals.rejection_mask |= evidence_bit(\n"
    "                                        MotionEvidenceFailure::invalid_motion);\n"
    "                            }",
),))

print("tuned neutral-first standing and publication integrity gates")
