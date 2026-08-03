from pathlib import Path

root = Path(__file__).resolve().parents[1]

def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{label}: expected one match, found {count}')
    return text.replace(old, new, 1)

mission = root / 'missioncache.md'
text = mission.read_text(encoding='utf-8')
text = replace_once(text, '''### WALK-CHICKEN-048 — Rebuild the chicken preset as a bird
**Status:** PACKAGE VERIFIED
''', '''### WALK-CHICKEN-048 — Rebuild the chicken preset as a bird
**Status:** REOPENED BY v0.7.7 LIVE SCREENSHOT — corrected by WALK-CHICKEN-096
''', 'chicken status')
text = replace_once(text, '''### WALK-LIVE-066 — Screenshot-level packaged-runtime acceptance
**Status:** PACKAGE VERIFIED — AWAITING LIVE CONFIRMATION
''', '''### WALK-LIVE-066 — Screenshot-level packaged-runtime acceptance
**Status:** IN PROGRESS — v0.7.7 chicken screenshot showed rearward collapse, 0.3 s best stance, 1.11 turns, and 0/6 valid seeds
''', 'live status')
text = replace_once(text, '''### WALK-REG-029 — Reopen live simulation quality after v0.7.2 screenshots
**Status:** PACKAGE VERIFIED — AWAITING LIVE CONFIRMATION
''', '''### WALK-REG-029 — Reopen live simulation quality after v0.7.2 screenshots
**Status:** IN PROGRESS — carried into v0.7.8 chicken and material correction
''', 'regression status')
text = replace_once(text, '''### WALK-OBS-018 — Non-overlapping eight-motor observation layout
**Status:** VERIFIED

The humanoid retains forty non-overlapping observation channels covering eight angles, eight velocities, contacts, foot placement, terrain, obstacle, stage, and phase state.
''', '''### WALK-OBS-018 — Non-overlapping eight-motor observation layout
**Status:** VERIFIED — expanded without overlap in v0.7.8

The humanoid retains fifty non-overlapping observation channels: the original eight angles, eight velocities, contacts, foot placement, obstacle, stage, and phase state plus terrain firmness, looseness, slope, burial, obstruction, incoming material, and escape direction.
''', 'observation ledger')
if '### WALK-CHICKEN-096' in text:
    raise RuntimeError('v0.7.8 completion missions already exist')
text = text.rstrip() + '''

### WALK-CHICKEN-096 — Correct live chicken balance regression
**Status:** IN PROGRESS

Use a real vertical semantic torso above the horizontal bird body, keep the raised neck, head, beak, tail, two articulated legs, and separate feet, and preserve leg-only motors. Six deterministic balance seeds must all sustain strict standing mastery without body collapse, integrity loss, or more than 0.55 uncontrolled turns.

### WALK-VISUAL-097 — Biomechanical rig animation treatment
**Status:** IN PROGRESS

Decorate live rigs, training PIP, and rig-lab previews with procedural anatomy rings, neural-link pulses, semantic-node halos, faint motion-study ghosts, and a small neural-chip motif. The effect must be generated from current rig state, require no external image asset, preserve telemetry readability, and never alter physics or input hit testing.

### WALK-ACCEPT-098 — Complete all v0.7.8 mission acceptance
**Status:** IN PROGRESS

Reconcile every open or screenshot-reopened ledger item, run strict chicken six-seed balance acceptance, seeded deformable-terrain conservation and collapse tests, deterministic repeated material events, partial burial with an escape side, full burial with honest termination, direct and glancing impacts, Linux warnings-as-errors, the complete Windows Vulkan package, executable-relative launch, ZIP manifest, SHA-256, and release re-download audit.
'''
mission.write_text(text.rstrip() + '\n', encoding='utf-8')

notes = root / 'RELEASE_NOTES_v0.7.8.md'
text = notes.read_text(encoding='utf-8').rstrip()
text += '''
- Rebuilds the chicken around a vertical semantic torso and central load-bearing brace while retaining its horizontal bird body, raised head, beak, tail, leg-only motors, and separate feet; six seeded strict-balance runs guard the live 0/6 regression.
- Adds procedural biomechanical animation overlays to live rigs, the training PIP, and rig lab: semantic anatomy rings, neural-link pulses, motion-study ghosts, node halos, and a compact neural-chip motif with no new asset dependency.
- Expands material acceptance with deterministic repeated events, partial burial and escape-side detection, full no-escape burial termination, and direct/glancing impact anti-tunneling checks.
'''
notes.write_text(text.rstrip() + '\n', encoding='utf-8')
Path(__file__).unlink()
