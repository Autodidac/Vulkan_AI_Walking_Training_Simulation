from pathlib import Path

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')

old = '''### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow
**Status:** IMPLEMENTED — FORCED FOOT PIN REMOVED; FULL VALIDATION REQUIRED
'''
new = '''### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow
**Status:** PARTIAL — HARD PIN REMOVED; GENERAL PLANTED-CONTACT PERSISTENCE FAILURE REOPENED
'''
if old not in text:
    raise SystemExit('WALK-CROUCH-140 refined status not found')
text = text.replace(old, new, 1)

old = '''### WALK-FEET-142 — Proper forward articulated feet and physical traction
**Status:** IMPLEMENTED — CROUCH SUPPORT NOW USES ONLY COLLISION/FRICTION; FULL VALIDATION REQUIRED
'''
new = '''### WALK-FEET-142 — Proper forward articulated feet and physical traction
**Status:** PARTIAL — HARD PIN REMOVED; PHYSICAL CONTACT PERSISTENCE REQUIRED
'''
if old not in text:
    raise SystemExit('WALK-FEET-142 refined status not found')
text = text.replace(old, new, 1)

anchor = '''**Resolution implemented:** the crouch guide now skips every semantic support node. It may shape knees, pelvis, torso, and head, but only `solve_ground` may place heel/ball/toe contacts, alter their retained tangential velocity, or mark them grounded. A dedicated adversarial test perturbs foot position, velocity history, and support state and verifies the guide leaves all three untouched before the real ground solver runs.
'''
addition = anchor + '''
**Validation result:** Linux run `31004249303`, job `92300237503`, proved the guide no longer overwrites feet, but the eight-preset live acceptance matrix fell from 24/24 to 16/24. Every static-crouch case failed because tiny constraint lifts cleared the solver's 0.0025 m contact threshold, leaving semantic feet unsupported while the press descended. The fix must be a stage-independent planted-contact persistence rule with bounded vertical slop and velocity limits. It may retain a previously planted support through tiny numerical separation, but must release on deliberate toe-off, meaningful upward velocity, or clearance beyond the slop. Duck-specific position pinning remains forbidden.
'''
if anchor not in text:
    raise SystemExit('second-audit resolution anchor not found')
text = text.replace(anchor, addition, 1)

traction_anchor = '''- moving limbs are not frozen by stance friction;
'''
traction_addition = traction_anchor + '''- previously planted semantic contacts may use bounded solver contact slop only while near the surface and not lifting deliberately; the rule is global physics, not curriculum assistance;
- toe-off, jump launch, and swing clearance must break persisted contact promptly rather than becoming magnetic feet;
'''
if traction_anchor not in text:
    raise SystemExit('traction acceptance anchor not found')
text = text.replace(traction_anchor, traction_addition, 1)

path.write_text(text, encoding='utf-8')
