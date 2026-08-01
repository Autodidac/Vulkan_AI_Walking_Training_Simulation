from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

for script in (
    "tools/integrate_skill_core.py",
    "tools/integrate_skill_training.py",
    "tools/integrate_training_stability.py",
    "tools/integrate_training_stability_fixups.py",
):
    subprocess.run(["python", script], cwd=ROOT, check=True)

# The old course test assumed every stage at or beyond the old hurdle enum
# immediately generated obstacles. The reordered curriculum has stationary
# jump and flip lessons, so only the moving-obstacle and mixed-course lessons
# require generated course features.
tests = ROOT / "tests/core_tests.cpp"
text = tests.read_text(encoding="utf-8")
old = '''        if (stage >= sim::CourseStage::hurdles)
            require(!environment.course_features().empty(), "obstacle curriculum stage has no course features");'''
new = '''        if (stage == sim::CourseStage::hurdles
            || stage == sim::CourseStage::moving_hazards)
            require(!environment.course_features().empty(),
                "moving obstacle curriculum stage has no course features");'''
if old not in text:
    raise RuntimeError("expected legacy obstacle-stage assertion was not found")
tests.write_text(text.replace(old, new, 1), encoding="utf-8")

print("Materialized current ordered-skill source and corrected the stale obstacle-stage test.")
