#!/usr/bin/env python3
from pathlib import Path

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')

old_intro = '''The application now exposes enough internal training data to debug the trainer, but the default interface still requires reinforcement-learning knowledge. A normal failed evaluation can show a huge red negative score, `-INF`, hexadecimal quality keys, abbreviated counters, and rejection terminology without explaining whether training is healthy, what improved, what failed, or what must happen next. The default dashboard must answer five ordinary questions: Is it still learning? Is it getting better? What just happened? What is it trying to learn now? What specifically must improve before the next lesson?'''
new_intro = '''The application now exposes enough internal training data to debug the trainer, but the default interface still requires reinforcement-learning knowledge. A normal failed evaluation can show a huge red negative score, `-INF`, hexadecimal quality keys, abbreviated counters, and rejection terminology without explaining whether training is healthy, what improved, what failed, or what must happen next. The default dashboard must answer five ordinary questions: Is it still learning? Is it getting better? What just happened? What is it trying to learn now? What specifically must improve before the next lesson?

The v0.7.20 Rig Lab screenshots also reopen anatomy and locomotion correctness. Automatic training is accepting geometry mutations that alter leg length instead of learning to control a fixed game character. The biped presets are authored as wide frontal splits rather than compact side-view bodies, multi-legged presets use malformed support plates or unarticulated branches, and the two-page Rig Lab places unrelated preset, file, structure, policy, motor, and test controls into one overflowing panel. These are part of the same release because unreadable diagnostics cannot be separated from visibly incorrect rigs and gait evidence.'''
if old_intro not in text:
    raise RuntimeError('v0.7.21 introduction marker missing')
text = text.replace(old_intro, new_intro, 1)

old_compat = '''### WALK-COMPAT-273 — Preserve learned-state compatibility
**Status:** OPEN — RELEASE BLOCKING

This release changes presentation only. Do not change policy dimensions, checkpoint format, training semantics, terrain behavior, curriculum thresholds, retained champion parameters, or v0.7.20 autosave paths. Existing v0.7.20 learned state must resume directly.
'''
new_compat = '''### WALK-STATE-273 — Isolate corrected rig and gait semantics
**Status:** OPEN — RELEASE BLOCKING

The readable dashboard remains presentation-only, but corrected preset geometry, gait evidence, and automatic tuning semantics invalidate silent reuse of v0.7.20 autosaves. Bump training/autonomy semantics and use `runner-v0721-*` autosave paths. Older checkpoints remain explicit transfer inputs only; malformed or structurally evolved v0.7.20 rigs may not silently resume as current presets.
'''
if old_compat not in text:
    raise RuntimeError('WALK-COMPAT-273 block missing')
text = text.replace(old_compat, new_compat, 1)

old_release = '''### WALK-RELEASE-275 — Publish audited Runner v0.7.21
**Status:** OPEN — RELEASE BLOCKING

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, complete deterministic/live/UI suites, readable-dashboard diagnostics, installed/extracted execution, ZIP/checksum/manifest, published-asset re-download byte verification, and cleanup of temporary branches/workflows. The release includes every v0.7.20 locomotion, terrain, preview, DPI, clipping, and icon correction.
'''
new_release = '''### WALK-AUTO-TUNING-275 — Stop automatic anatomy cheating
**Status:** OPEN — RELEASE BLOCKING

Automatic curriculum refinement may tune motor strength, joint range, and structural stiffness only. It may not move nodes, change limb length, widen supports, add/remove/split branches, duplicate feet, or change semantic contacts while learning locomotion. Structural editing remains an explicit Rig Lab operation. Every automatically accepted candidate must preserve the exact node, radius, bone, topology, and support-semantic layout of its source rig.

### WALK-SIDE-GAIT-276 — Require real side-view fore/aft gait
**Status:** OPEN — RELEASE BLOCKING

Biped, humanoid, scaffold, chicken, and monoped presentation must read as side-view anatomy rather than a frontal split. A credited crossing step requires the swing support to begin behind the stance support, clear the terrain, pass ahead of it, and land on the opposite contact phase. A leg that stays permanently ahead, spreads sideways, shuffles both supports, or gains progress only from the treadmill receives no sagittal gait credit.

### WALK-PRESET-ANATOMY-277 — Rebuild every shipped preset from explicit chains
**Status:** OPEN — RELEASE BLOCKING

Audit scaffold, humanoid, biped, chicken, quadruped, four-leg crawler, hexapod, and monoped. Each preset must be connected, finite, centered, correctly scaled, have unique semantic supports, physically meaningful parent-pivot-child motor chains, no support-to-support brace masquerading as a limb, no fused feet, and a recognizable side silhouette. Presets are immutable templates; selecting one always restores its canonical anatomy.

### WALK-MULTILEG-278 — Give multi-legged rigs real support branches
**Status:** OPEN — RELEASE BLOCKING

Quadruped and four-leg crawler use four distinct articulated two-segment legs with eight mapped joints and diagonal gait phases. The hexapod uses six distinct legs and alternating tripod support phases without rigid foot plates joining semantic supports. Multi-support gait bootstrap must drive support branches by semantic phase rather than returning a stationary balance action.

### WALK-RIG-LAB-279 — Replace the overflowing Rig Lab control wall
**Status:** OPEN — RELEASE BLOCKING

Split Rig Lab into focused `PRESETS`, `STRUCTURE`, `MOTORS`, and `TEST` pages. Preset/file/policy/visual controls, node/bone editing, motor setup, and joint/traction testing may not share one unscrollable panel. Use responsive panel/world boxes, deterministic clipping, consistent spacing, full labels, and no overlapping or unreachable controls at every supported window size.

### WALK-RIG-FIT-280 — Center and fit every rig in the editor viewport
**Status:** OPEN — RELEASE BLOCKING

Rig Lab computes bounds from the selected blueprint, centers the actual anatomy, and chooses a safe scale that keeps the full body, support nodes, labels, motor arc, and ground reference visible. Wide quadrupeds and hexapods may not be cropped or shoved to one edge; tall humanoids and monoped rigs may not overlap the joint-test area.

### WALK-RIG-TRUTH-281 — Make editor labels match actual behavior
**Status:** OPEN — RELEASE BLOCKING

Replace `USE EVOLVED`, `RIG GENERATION`, and topology-nursery wording where automatic training now performs controller tuning only. Clearly distinguish canonical preset, manually edited custom rig, retained controller, fresh policy, and automatic parameter tuning. The UI may not imply that changing leg length is a valid walking solution.

### WALK-RIG-TEST-282 — Deterministically lock anatomy, gait, and layout
**Status:** OPEN — RELEASE BLOCKING

Add tests proving automatic tuning preserves anatomy byte-for-byte; every preset is valid, connected, finite, centered, uniquely supported, and appropriately articulated; biped rest poses are compact side-view silhouettes; quadruped/crawler have four distinct articulated legs; hexapod has six independent supports with alternating tripod phases; crossing credit requires behind-to-ahead order reversal; and all four Rig Lab pages fit every supported window size.

### WALK-RELEASE-283 — Publish audited Runner v0.7.21
**Status:** OPEN — RELEASE BLOCKING

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, complete deterministic/live/UI/rig suites, readable-dashboard diagnostics, all-preset acceptance, installed/extracted execution, ZIP/checksum/manifest, published-asset re-download byte verification, and cleanup of temporary branches/workflows. The release includes every v0.7.20 locomotion, terrain, preview, DPI, clipping, and icon correction plus the corrected v0.7.21 rig and gait contract.
'''
if old_release not in text:
    raise RuntimeError('WALK-RELEASE-275 block missing')
text = text.replace(old_release, new_release, 1)

path.write_text(text, encoding='utf-8', newline='\n')
print('Cached Runner v0.7.21 rig, gait, and Rig Lab missions 275-283')
