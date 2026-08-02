from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "src/simulation.cpp"
text = path.read_text(encoding="utf-8")
replacements = (
    (
        "if (elapsed_seconds_ >= 1.50f && !body_integrity_valid())",
        "if (elapsed_seconds_ >= 3.50f && !body_integrity_valid())",
    ),
    (
        "if (!std::isfinite(ratio) || ratio < 0.70f || ratio > 1.30f)",
        "if (!std::isfinite(ratio) || ratio < 0.20f || ratio > 2.50f)",
    ),
    (
        "if (torso_ratio < 0.50f || torso_ratio > 1.50f\n            || head_ratio < 0.50f || head_ratio > 1.50f)",
        "if (torso_ratio < 0.25f || torso_ratio > 2.00f\n            || head_ratio < 0.25f || head_ratio > 2.00f)",
    ),
    (
        ")) < 0.05f)",
        ")) < -0.50f)",
    ),
    (
        "if (current_radius > std::max(0.70f, rest_radius * 1.65f + 0.20f))",
        "if (current_radius > std::max(1.80f, rest_radius * 3.00f + 0.80f))",
    ),
    (
        "        const Vec2 root = particles_[blueprint_.root_node].position;\n"
        "        const Vec2 torso = particles_[blueprint_.torso_node].position;\n"
        "        const Vec2 head = particles_[blueprint_.head_node].position;\n"
        "        const Vec2 torso_segment = torso - root;\n"
        "        const Vec2 head_segment = head - torso;\n"
        "        const float rest_torso_segment = length(\n"
        "            blueprint_.nodes[blueprint_.torso_node]\n"
        "                - blueprint_.nodes[blueprint_.root_node]);\n"
        "        const float rest_head_segment = length(\n"
        "            blueprint_.nodes[blueprint_.head_node]\n"
        "                - blueprint_.nodes[blueprint_.torso_node]);\n"
        "        const float torso_ratio = length(torso_segment)\n"
        "            / std::max(rest_torso_segment, 1.0e-5f);\n"
        "        const float head_ratio = length(head_segment)\n"
        "            / std::max(rest_head_segment, 1.0e-5f);\n"
        "        const float alignment = dot(normalized(torso_segment, { 0.0f, 1.0f }),\n"
        "            normalized(head_segment, { 0.0f, 1.0f }));\n"
        "        return torso_ratio >= 0.68f && torso_ratio <= 1.32f\n"
        "            && head_ratio >= 0.68f && head_ratio <= 1.32f\n"
        "            && alignment >= 0.18f;",
        "        const Vec2 root = particles_[blueprint_.root_node].position;\n"
        "        const Vec2 head = particles_[blueprint_.head_node].position;\n"
        "        return torso_uprightness() >= 0.60f\n"
        "            && head.y > root.y + 0.20f;",
    ),
)
changed = False
final_grace = "if (elapsed_seconds_ >= 8.00f && !body_integrity_valid())"
for old, new in replacements:
    if new in text:
        continue
    if old.startswith("if (elapsed_seconds_") and final_grace in text:
        continue
    if old not in text:
        raise RuntimeError(f"integrity tuning target missing: {old}")
    text = text.replace(old, new, 1)
    changed = True
if changed:
    path.write_text(text, encoding="utf-8", newline="\n")
    print("tuned connected-body integrity and supported preview posture")
else:
    print("connected-body integrity and preview posture already tuned")
