from pathlib import Path

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')

old = '''### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow
**Status:** PARTIAL — POSTURE EVIDENCE PASSES; FORCED FOOT-PIN REMOVAL REOPENED BEFORE RELEASE
'''
new = '''### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow
**Status:** PARTIAL — HARD PIN REMOVAL EXPOSED GENERAL PLANTED-CONTACT PERSISTENCE FAILURE
'''
if old not in text:
    raise SystemExit('WALK-CROUCH-140 current status not found')
text = text.replace(old, new, 1)

old = '''### WALK-FEET-142 — Proper forward articulated feet and physical traction
**Status:** PARTIAL — GROUND FRICTION PASSES; STATIC CROUCH HARD-PIN REOPENED BEFORE RELEASE
'''
new = '''### WALK-FEET-142 — Proper forward articulated feet and physical traction
**Status:** PARTIAL — HARD PIN REMOVAL EXPOSED PHYSICAL CONTACT-PERSISTENCE GAP
'''
if old not in text:
    raise SystemExit('WALK-FEET-142 current status not found')
text = text.replace(old, new, 1)

finding = '''**Second-audit finding:** the static crouch stabilizer still wrote authored x positions, floor y positions, previous positions, and `grounded=true` into every semantic foot contact several times per solver iteration. That invalidated the claimed friction-only support model and could manufacture the very crouch being evaluated. The release remains blocked until those assignments are removed and adversarial tests prove support, squat shape, recovery, and bounded slip still pass through the real ground solver.
'''
addition = finding + '''
**Removal validation:** Linux run `31004249303`, job `92300237503`, proved the guide no longer overwrote semantic support nodes, but the live acceptance matrix fell from 24/24 to 16/24. Every static-crouch case failed because tiny constraint lifts cleared the ground solver's 0.0025 m threshold, leaving feet unsupported while the press descended. The next correction must be a general planted-contact persistence/slop rule in `solve_ground`, not a duck-specific pin. It may retain a previously planted support only through bounded numerical separation and bounded upward speed; deliberate toe-off, jump launch, and real swing clearance must release promptly.
'''
if finding not in text:
    raise SystemExit('second-audit finding not found')
text = text.replace(finding, addition, 1)

traction_anchor = '''- moving limbs are not frozen by stance friction;
'''
traction_addition = traction_anchor + '''- previously planted semantic contacts may use bounded solver contact slop only while near the surface and not lifting deliberately; the rule is global physics, not curriculum assistance;
- toe-off, jump launch, and swing clearance must break persisted contact promptly rather than becoming magnetic feet;
'''
if traction_anchor not in text:
    raise SystemExit('traction acceptance anchor not found')
text = text.replace(traction_anchor, traction_addition, 1)

path.write_text(text, encoding='utf-8')
