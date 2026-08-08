#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "src/simulation.cpp"
text = path.read_text(encoding="utf-8")
old = '''        if (!upright_walking_stage
            || !blueprint_.paired_leg_chains()
            || blueprint_.horizontal_multi_support_plan()
            || recovery_active_ || non_foot_grounded_)
            return;
'''
new = '''        if (!upright_walking_stage
            || !blueprint_.paired_leg_chains()
            || blueprint_.horizontal_multi_support_plan())
            return;
'''
if old not in text:
    raise RuntimeError("strict walking-geometry guard pattern not found")
text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8", newline="\n")
print("Runner v0.7.25 strict walking geometry enabled for all contact/recovery frames")
