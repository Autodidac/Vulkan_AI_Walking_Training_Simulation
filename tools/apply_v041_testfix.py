from pathlib import Path

path = Path("tests/core_tests.cpp")
text = path.read_text(encoding="utf-8")
old = '    require(humanoid.nodes.size() == 7, "human-calibrated rig should have pelvis, torso, head, knees, and feet");\n'
new = '    require(humanoid.nodes.size() == 11, "human-calibrated rig should include passive heel/toe feet");\n'
if old not in text:
    raise SystemExit("stale seven-node humanoid assertion was not found")
text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8", newline="\n")
print("Updated humanoid feet node-count assertion")
