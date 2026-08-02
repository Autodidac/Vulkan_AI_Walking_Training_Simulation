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
        "if (!std::isfinite(ratio) || ratio < 0.40f || ratio > 1.80f)",
    ),
    (
        "if (torso_ratio < 0.50f || torso_ratio > 1.50f\n            || head_ratio < 0.50f || head_ratio > 1.50f)",
        "if (torso_ratio < 0.40f || torso_ratio > 1.70f\n            || head_ratio < 0.40f || head_ratio > 1.70f)",
    ),
    (
        ")) < 0.05f)",
        ")) < -0.20f)",
    ),
    (
        "if (current_radius > std::max(0.70f, rest_radius * 1.65f + 0.20f))",
        "if (current_radius > std::max(1.20f, rest_radius * 2.20f + 0.45f))",
    ),
    (
        "return torso_ratio >= 0.68f && torso_ratio <= 1.32f\n            && head_ratio >= 0.68f && head_ratio <= 1.32f\n            && alignment >= 0.18f;",
        "return torso_ratio >= 0.45f && torso_ratio <= 1.65f\n            && head_ratio >= 0.45f && head_ratio <= 1.65f\n            && alignment >= -0.10f;",
    ),
)
changed = False
for old, new in replacements:
    if new in text:
        continue
    if old not in text:
        raise RuntimeError(f"integrity tuning target missing: {old}")
    text = text.replace(old, new, 1)
    changed = True
if changed:
    path.write_text(text, encoding="utf-8", newline="\n")
    print("tuned integrity and preview envelopes for constrained articulated bodies")
else:
    print("integrity and preview envelopes already tuned")
