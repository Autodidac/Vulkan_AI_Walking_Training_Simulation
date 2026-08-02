from pathlib import Path

root = Path(__file__).resolve().parents[1]
mission_path = root / 'missioncache.md'
mission = mission_path.read_text(encoding='utf-8')
entry = '''

### WALK-HAZARD-079 — Falling material, impact, burial, and escape training
**Status:** CARRIED FORWARD — NOT IN v0.7.5

Add dynamic overhead hazards driven by the same terrain/material simulation: falling sand, collapsing loose slopes, rocks, debris, and thrown objects. Observations must include incoming direction, velocity, estimated impact time, material density, local burial depth, free-space direction, and whether the head, torso, or support limbs are obstructed. The rig must learn to evade when possible, brace when avoidance is impossible, remain oriented after impact, dig or push toward free space, recover from forward-prone or partially buried states, regain foot support, and continue the assigned stage.

Acceptance requires seeded scenarios covering glancing hits, direct hits, accumulating sand, partial burial, full-body obstruction with an escape path, and repeated impacts. Success cannot be credited for tunnelling, teleporting, deleting material, remaining motionless under debris, or exploiting detached limbs. Suffocation or complete burial without an escape route terminates the attempt honestly. This mission is paired with WALK-SAND-078 and is intentionally carried to the next release rather than delaying the v0.7.5 PIP/curriculum correction.
'''
if '### WALK-HAZARD-079' not in mission:
    mission += entry
mission_path.write_text(mission, encoding='utf-8', newline='\n')

notes_path = root / 'RELEASE_NOTES_v0.7.5.md'
notes = notes_path.read_text(encoding='utf-8')
line = '- Carries falling sand/debris avoidance, impact recovery, burial escape, and continuation training into the next release mission ledger.\n'
if line not in notes:
    notes = notes.replace('# Runner v0.7.5\n\n', '# Runner v0.7.5\n\n' + line, 1)
notes_path.write_text(notes, encoding='utf-8', newline='\n')

Path(__file__).unlink()
print('carried falling-material avoidance and burial recovery into missioncache.md')
