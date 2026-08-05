from pathlib import Path

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')

old = '''### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow
**Status:** IMPLEMENTED — DETERMINISTIC, CROSS-PLATFORM, AND SCREENSHOT VALIDATION REQUIRED
'''
new = '''### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow
**Status:** PARTIAL — POSTURE EVIDENCE PASSES; FORCED FOOT-PIN REMOVAL REOPENED BEFORE RELEASE
'''
if old not in text:
    raise SystemExit('WALK-CROUCH-140 status anchor not found')
text = text.replace(old, new, 1)

old = '''### WALK-FEET-142 — Proper forward articulated feet and physical traction
**Status:** IMPLEMENTED — DETERMINISTIC, CROSS-PLATFORM, AND SCREENSHOT VALIDATION REQUIRED
'''
new = '''### WALK-FEET-142 — Proper forward articulated feet and physical traction
**Status:** PARTIAL — GROUND FRICTION PASSES; STATIC CROUCH HARD-PIN REOPENED BEFORE RELEASE
'''
if old not in text:
    raise SystemExit('WALK-FEET-142 status anchor not found')
text = text.replace(old, new, 1)

anchor = '''Traction must be physical and state-aware rather than an unconditional position pin or cosmetic anti-slide gate:
'''
addition = anchor + '''- no curriculum stabilizer may assign semantic heel/ball/toe x/y coordinates, erase their velocity, or mark them grounded; only collision and the ground/friction solver may establish support;
'''
if anchor not in text:
    raise SystemExit('physical traction anchor not found')
text = text.replace(anchor, addition, 1)

anchor = '''**Acceptance:** an adversarial hip-hinge pose that clears the press is rejected; a bilateral squat passes repeated seeded hold/recovery tests; all seven presets retain valid Stand behavior; released screenshot visibly shows pelvis-down/knees-bent posture.
'''
replacement = '''**Acceptance:** an adversarial hip-hinge pose that clears the press is rejected; a bilateral squat passes repeated seeded hold/recovery tests without forced foot coordinates; all eight presets retain valid Stand behavior; released screenshot visibly shows pelvis-down/knees-bent posture.
'''
if anchor not in text:
    raise SystemExit('crouch acceptance anchor not found')
text = text.replace(anchor, replacement, 1)
text = text.replace('- all seven preset Stand and static-crouch behavior;',
                    '- all eight preset Stand and static-crouch behavior;', 1)

finding_anchor = '''### WALK-REGRESSION-146 — Exhaustive interaction audit for v0.7.15
**Status:** OPEN — RELEASE BLOCKING
'''
finding = finding_anchor + '''
**Second-audit finding:** the static crouch stabilizer still wrote authored x positions, floor y positions, previous positions, and `grounded=true` into every semantic foot contact several times per solver iteration. That invalidated the claimed friction-only support model and could manufacture the very crouch being evaluated. The release remains blocked until those assignments are removed and adversarial tests prove support, squat shape, recovery, and bounded slip still pass through the real ground solver.
'''
if finding_anchor not in text:
    raise SystemExit('regression audit anchor not found')
text = text.replace(finding_anchor, finding, 1)

path.write_text(text, encoding='utf-8')
