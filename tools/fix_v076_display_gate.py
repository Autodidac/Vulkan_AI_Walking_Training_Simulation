from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / 'src/ppo.hpp'
text = path.read_text(encoding='utf-8')
old = '''        if (!qualification.valid
            || !environment.current_display_posture_valid())
            return false;'''
new = '''        if (!qualification.valid || !environment.body_integrity_valid())
            return false;'''
if new not in text:
    if old not in text:
        raise SystemExit('stage display gate target was not found')
    text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8', newline='\n')

# Qualification already enforces the historical evidence. The stage-specific
# checks below enforce the current frame: current uprightness, live support,
# neutral upper-body joint angles, and low current rotation. This avoids hiding
# a qualified frame merely because a redundant generic posture heuristic was
# tuned for the old lower central-shoulder geometry.
Path(__file__).unlink()
print('aligned PIP display gate with raised central-shoulder geometry')
