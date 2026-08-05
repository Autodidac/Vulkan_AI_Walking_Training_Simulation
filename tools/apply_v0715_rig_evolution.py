from pathlib import Path

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')
old = '''### WALK-EVOLUTION-143 — Structural rig evolution with nursery adaptation
**Status:** IMPLEMENTED — DETERMINISTIC AND PACKAGE VALIDATION REQUIRED
'''
new = '''### WALK-EVOLUTION-143 — Structural rig evolution with nursery adaptation
**Status:** PARTIAL — TOPOLOGY OPERATORS PASS; NEW ACTIVE JOINT SLOT GROWTH REOPENED BEFORE RELEASE
'''
if old not in text:
    raise SystemExit('WALK-EVOLUTION-143 status anchor not found')
text = text.replace(old, new, 1)
anchor = '- preserve existing motor slots where possible and initialize new slots neutrally;\n'
addition = anchor + '''- when a split creates a usable articulated joint and an action slot is free, activate that joint, zero the transferred actor row/bias for the new slot, and let the bounded nursery discover control without injecting stale unused-output motion;
- prove explicit transfer-only import accepts dimension-compatible older semantics while normal resume still rejects them and resets optimizer/champion state;
'''
if anchor not in text:
    raise SystemExit('new motor slot criterion anchor not found')
text = text.replace(anchor, addition, 1)
path.write_text(text, encoding='utf-8')
