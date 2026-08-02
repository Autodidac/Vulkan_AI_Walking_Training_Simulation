from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "src/simulation.cpp"
text = path.read_text(encoding="utf-8")
old = "if (elapsed_seconds_ >= 1.50f && !body_integrity_valid())"
new = "if (elapsed_seconds_ >= 3.50f && !body_integrity_valid())"
if new in text:
    print("integrity grace already tuned")
elif old in text:
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
    print("tuned terminal integrity grace to the post-stance window")
else:
    raise RuntimeError("integrity grace predicate not found")
