#!/usr/bin/env python3
from pathlib import Path

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')
marker = '# Runner v0.7.17 equipment, carry, and target curriculum'
if '# Runner v0.7.17 eye-test locomotion and sprite completion' in text:
    raise SystemExit(0)
section = r'''
# Runner v0.7.17 eye-test locomotion and sprite completion

**Release state:** CACHED — USER EYE TEST REOPENED RUNTIME QUALITY; 37 MISSIONS SELECTED.

The released v0.7.16 package corrected camera scale but visibly failed locomotion acceptance: the quadruped remains trapped under the crouch press and is crushed, other rigs advance after only a couple of steps, feet are poorly matched to deformable terrain, gait reads as frontal/crab motion instead of side-view sagittal walking, and the supplied modular armor/weapon art is not used by the packaged application. Automated evidence is subordinate to this eye test and the exact missions below are reopened before implementation.

### WALK-QUAD-181 — Quadruped survives the crouch press
**Status:** OPEN — EYE-TEST REGRESSION

The quadruped press profile must respect its horizontal body plan and may not descend below a recoverable topology-relative compression envelope.

### WALK-QUAD-182 — Quadruped completes crouch hold and recovery
**Status:** OPEN — EYE-TEST REGRESSION

Quadruped static crouch requires a controlled supported compression, hold, press retraction, and return to authored stance without body crushing or permanent collapse.

### WALK-PRESS-183 — Body-plan-relative press clearance
**Status:** OPEN

Press depth and target clearance derive from support-to-head/body bounds for paired-leg and multi-contact rigs rather than one biped constant.

### WALK-PRESS-184 — Crush termination is not training progress
**Status:** OPEN

Penetration, sustained body contact, or unrecovered compression invalidates the attempt and can never count as crouch mastery.

### WALK-STAGE-185 — No immediate Stand-to-Walk skip
**Status:** OPEN — EYE-TEST REGRESSION

Every preset must retain repeated strict Stand and crouch evidence before ordinary gait training begins.

### WALK-WALK-186 — Minimum 24 alternating stride events
**Status:** OPEN — EYE-TEST REGRESSION

Walk mastery requires at least 24 genuine alternating stride events rather than two or three visible steps.

### WALK-WALK-187 — Minimum 20 metres sustained travel
**Status:** OPEN

Walk mastery requires at least 20 metres of forward progress without treadmill-only or planted-foot translation credit.

### WALK-WALK-188 — Minimum 20 seconds gait survival
**Status:** OPEN

Walk mastery requires a sustained 20-second valid gait evaluation with bounded collisions and no collapse.

### WALK-MASTERY-189 — Six confirmations for early skills
**Status:** OPEN

Stand, crouch, and ordinary walk each require six independent strict confirmations before stage advancement.

### WALK-MASTERY-190 — Evidence must span multiple evaluations
**Status:** OPEN

One unusually short or lucky evaluation cannot immediately advance a preset into a later lesson.

### WALK-SIDE-191 — True sagittal side-view locomotion
**Status:** OPEN — EYE-TEST REGRESSION

Biped and humanoid hips, knees, feet, and rendering must read as forward/back movement in one side-view plane rather than left/right crab walking.

### WALK-SIDE-192 — Narrow authored depth stance
**Status:** OPEN

Near/far legs begin with a small depth/readability offset, not a wide horizontal split that turns the gait into a jumping jack.

### WALK-SIDE-193 — Visible limb crossing
**Status:** OPEN

The swing leg must visibly pass the stance leg in x while stable near/far draw order preserves semantic identity.

### WALK-SIDE-194 — Forward footfall direction
**Status:** OPEN

Heel/stub contacts land ahead of the pelvis during forward gait and behind it during recovery; lateral-only foot cycling is rejected.

### WALK-SIDE-195 — Reject crab shuffle qualification
**Status:** OPEN

Wide support-span shuffling, mirrored outward stepping, and double-support sideways translation cannot qualify as walk mastery.

### WALK-FOOT-196 — Replace multi-node passive feet with joint stubs
**Status:** OPEN — EYE-TEST REGRESSION

Biped-style rigs use one compact physical contact stub below each ankle instead of rigid heel/ball/toe triangles that snag and lever against deformable terrain.

### WALK-FOOT-197 — Sprite-rendered boots over physics stubs
**Status:** OPEN

The renderer draws a forward boot sprite/silhouette at each semantic contact while physics remains a compact joint stub.

### WALK-FOOT-198 — Stable deformable-terrain stub contact
**Status:** OPEN

Contact stubs use bounded radius, traction, release, and terrain pressure so they neither skate like wheels nor hook into granular cells.

### WALK-FOOT-199 — Remove obsolete passive-foot assumptions
**Status:** OPEN

Toe-specific control, tests, editor labels, and support logic must tolerate or remove absent heel/ball/toe clusters without hidden fallback pinning.

### WALK-ART-200 — Load optional modular armor sprite assets
**Status:** OPEN — EYE-TEST REGRESSION

The four supplied transparent sprite sheets are preserved in the repository and package and are discoverable by the runtime asset catalog.

### WALK-ART-201 — Package male and female armor references
**Status:** OPEN

Male/female heads, torso, limbs, boots, helmets, and weapons remain available as optional source art without becoming startup dependencies.

### WALK-ART-202 — Runtime armor style selection
**Status:** OPEN

The live and rig-lab renderer exposes a procedural fallback and optional armored humanoid style; absence of optional art remains nonfatal.

### WALK-ART-203 — Side-view armor assembly contract
**Status:** OPEN

Optional parts align to current root, torso, head, joint, and contact positions in side view rather than replacing physics with a fixed full-body picture.

### WALK-ART-204 — Near/far limb sprite depth
**Status:** OPEN

Far limbs render first with reduced intensity; torso and near limbs render afterward so crossing remains readable.

### WALK-ART-205 — Helmet and uncovered-head variants
**Status:** OPEN

The optional catalog retains helmeted and uncovered male/female variants for later deterministic selection.

### WALK-ART-206 — Weapon sprite references
**Status:** OPEN

Supplied compact and long weapon sprites are packaged for the future equipment curriculum without stealing locomotion motor slots.

### WALK-ART-207 — Optional-art manifest and provenance
**Status:** OPEN

Document source filenames, dimensions, SHA-256 hashes, intended runtime use, and fallback behavior.

### WALK-ART-208 — Package diagnostic verifies optional catalog
**Status:** OPEN

Package diagnostics verify optional art when present and still pass when the complete optional directory is removed.

### WALK-STATE-209 — Isolate corrected v0.7.17 semantics
**Status:** OPEN

Bump training semantics so v0.7.16 crouch, short-walk, crab-gait, and passive-foot policies cannot silently resume.

### WALK-STATE-210 — Isolate v0.7.17 autosave paths
**Status:** OPEN

Policy, evolved-rig, and autonomy-state files use a new `runner-v0717-*` namespace.

### WALK-TEST-211 — Quadruped press/recovery matrix
**Status:** OPEN

Deterministic repeated-seed tests prove quadruped compression, hold, retraction, recovery, bounded penetration, and no crushing.

### WALK-TEST-212 — Early-stage endurance matrix
**Status:** OPEN

Tests reject two-step walk advancement and require the configured confirmations, duration, distance, and stride evidence.

### WALK-TEST-213 — Contact-stub terrain tests
**Status:** OPEN

Tests cover stub topology, finite terrain contact, bounded slip, prompt release, pressure, and absence of passive-foot triangles.

### WALK-TEST-214 — Side-view gait tests
**Status:** OPEN

Tests prove narrow authored stance, alternating forward footfalls, visible crossing, and rejection of crab shuffle fixtures.

### WALK-TEST-215 — Optional-art package tests
**Status:** OPEN

Repository and Windows package tests verify the four supplied assets, manifest, hashes, package presence, and optional-directory fallback.

### WALK-DOC-216 — Document runtime eye-test corrections
**Status:** OPEN

README, camera/locomotion documentation, controls, asset usage, fallback, and acceptance expectations describe the actual v0.7.17 behavior.

### WALK-CHANGELOG-217 — Consolidated v0.7.17 changelog
**Status:** OPEN

CHANGELOG records one release entry covering quadruped recovery, endurance gates, side-view gait, stub feet, optional art, tests, state isolation, and packaging.

### WALK-RELEASE-218 — Publish audited Runner v0.7.17
**Status:** OPEN

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, every deterministic and live acceptance suite, quadruped recovery, sustained walk evidence, contact-stub tests, side-view tests, optional-art package/fallback checks, installed/extracted diagnostics, checksum/manifest, release re-download, branch cleanup, and zero open PRs. Contradictory released-package eye-test evidence reopens the exact mission.

'''
text = text.replace(marker, section + '# Runner v0.7.18 equipment, carry, and target curriculum', 1)
text = text.replace('**Release state:** CACHED AND OPEN — carried intact after the v0.7.16 viewport release.', '**Release state:** CACHED AND OPEN — carried intact after the v0.7.17 eye-test correction.', 1)
text = text.replace('### WALK-RELEASE-155 — Publish audited Runner v0.7.17', '### WALK-RELEASE-155 — Publish audited Runner v0.7.18', 1)
path.write_text(text, encoding='utf-8', newline='\n')
