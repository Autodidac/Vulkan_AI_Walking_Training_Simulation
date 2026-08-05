#!/usr/bin/env python3
from pathlib import Path

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')
marker = '# Runner cache-first engineering policy and active release plan\n'
if marker not in text:
    raise SystemExit('missioncache title missing')
if '# Runner v0.7.17 released-eye-test correction and 25-mission expansion' in text:
    raise SystemExit('v0.7.17 eye-test batch already cached')

section = r'''

# Runner v0.7.17 released-eye-test correction and 25-mission expansion

**Release state:** CACHED BEFORE IMPLEMENTATION — five screenshot-reopened failures plus 25 additional compatible missions. The equipment/carry/target curriculum remains intact and is renumbered to v0.7.18 rather than discarded.

The released v0.7.16 package visibly contradicts prior automated closure in five areas: the quadruped remains trapped under the crouch press and is crushed; other rigs advance into Walk almost immediately and produce only a couple of steps; the articulated heel/ball/toe physics geometry performs poorly on granular terrain; the supplied optional armor/character artwork is not present as usable packaged/runtime art; and the supposed side-view gait remains lateral crab-walking rather than a sagittal one-foot-past-the-other gait. These exact missions are reopened and visual evidence outranks earlier deterministic claims.

### WALK-QUAD-CROUCH-181 — Quadruped must complete crouch, hold, retract, and recover
**Status:** OPEN — RELEASED EYE TEST REOPENED

The quadruped must compress through all four support chains, remain below the press without body clipping, survive the hold, then restore its authored standing body height after retraction. Remaining permanently compressed, being driven into terrain, or timing out under the platen fails.

### WALK-PRESS-SAFETY-182 — Rig-aware non-crushing press envelope
**Status:** OPEN — RELEASED EYE TEST REOPENED

The press target, descent speed, contact impulse, hold duration, and emergency retraction must derive from current rig bounds and support topology. The press may test crouching but may not continue descending through a finite supported rig or manufacture a terminal crush state.

### WALK-QUAD-RECOVERY-183 — Four-chain extension teacher and recovery evidence
**Status:** OPEN — RELEASED EYE TEST REOPENED

Quadruped recovery must command all four hip/knee chains toward the authored neutral stance after the platen retracts. Acceptance requires recovered root/torso height, bounded joint speed, four semantic supports, no non-foot contact, and a stable hold rather than only a momentary clearance frame.

### WALK-CURRICULUM-DWELL-184 — Do not instantly skip mastered-looking stages
**Status:** OPEN — RELEASED EYE TEST REOPENED

No rig may advance from Stand or Static Crouch after one short or inherited-looking sample. Each stage requires a minimum number of fresh updates, multiple complete episodes, repeated strict evaluations, and visible retained evidence produced under the current rig and current training semantics.

### WALK-WALK-DISTANCE-185 — Require sustained walking rather than two steps
**Status:** OPEN — RELEASED EYE TEST REOPENED

Walk mastery requires sustained forward distance, repeated alternating cycles, minimum elapsed gait time, useful swing clearance, and continued upright support. Two steps, one lucky crossing, course motion without body translation, or immediate stage transition cannot qualify.

### WALK-SAGITTAL-GAIT-186 — True side-view sagittal walking
**Status:** OPEN — RELEASED EYE TEST REOPENED

The near and far legs must swing primarily in screen/world X, pass one another, land ahead of the stance foot, and drive forward root progress. The controller and renderer must present a normal side-view gait rather than lateral abduction/adduction.

### WALK-CRAB-REJECT-187 — Reject lateral crab-walking
**Status:** OPEN — RELEASED EYE TEST REOPENED

Persistent support-span widening, hip abduction, sideways stepping, symmetric outward foot motion, or forward translation produced without sagittal crossing receives no gait credit and cannot seed champion, imitation, evolution, or PIP state.

### WALK-STEP-LENGTH-188 — Measured stride length and foot-passing evidence
**Status:** OPEN

Record signed swing-foot displacement relative to the stance foot, minimum stride length, crossing count, heel-ahead landing, and cadence. Gait qualification requires both left-over-right and right-over-left passes for multi-leg bipeds and topology-appropriate fore/hind cycling for quadrupeds.

### WALK-STUB-FEET-189 — Replace terrain-hostile multi-node feet with joint stubs
**Status:** OPEN — RELEASED EYE TEST REOPENED

Bipedal and humanoid physics feet become short single-contact ankle stubs with one semantic support point per leg. Remove the rigid heel/ball/toe triangle from collision and constraints while preserving honest support, friction, swing release, and terrain pressure.

### WALK-SPRITE-FEET-190 — Render visible sprite feet on non-colliding stubs
**Status:** OPEN — RELEASED EYE TEST REOPENED

A forward-facing side-view foot sprite is anchored to each stub, follows lower-leg orientation with bounded visual rotation, mirrors near/far shading without reversing forward direction, and never contributes collision geometry or training observations.

### WALK-TERRAIN-FOOT-191 — Stub contacts conform to granular terrain
**Status:** OPEN

Single support stubs sample the same SandHybrid surface used by pressure and collision, tolerate bounded cell-scale height variation, release cleanly for swing, and do not bridge, snag, or lever against multiple distant terrain cells.

### WALK-FOOT-ORIENTATION-192 — Feet always point along travel
**Status:** OPEN

Every side-view sprite foot points toward +X by default and follows deliberate backward travel only when that behavior is explicitly selected. Left/right identity changes depth and shading, not forward direction.

### WALK-SUPPORT-SIMPLIFY-193 — One semantic contact per support chain
**Status:** OPEN

Contact bookkeeping, static friction, gait strikes, pressure, support intervals, editor labels, persistence, and tests must use one stub contact per leg. Compatibility import must reject or remap obsolete heel/ball/toe semantic arrays explicitly.

### WALK-OPTIONAL-ART-194 — Add all four supplied concept sheets
**Status:** OPEN — RELEASED EYE TEST REOPENED

Package the four supplied armor/character/weapon concept sheets under `assets/optional/runner_armor_concepts/source/` with stable filenames, a provenance/readme file, dimensions, SHA-256 values, and an explicit statement that they are optional user-supplied references.

### WALK-ART-RUNTIME-195 — Load optional foot/armor art safely
**Status:** OPEN

Create a compact runtime P3 atlas derived from the supplied art for side-view feet and simple armor overlays. The application loads it when valid, reports its presence in package diagnostics, and falls back to deterministic procedural sprites without affecting physics or startup.

### WALK-ARMOR-MAP-196 — Map optional armor pieces to rig segments
**Status:** OPEN

Define non-physical sprite anchors for head, torso, upper/lower arms, upper/lower legs, and feet. Missing pieces or unsupported anatomy fall back independently; armor cannot hide support telemetry in Rig Lab debug mode.

### WALK-SIDE-LAYERS-197 — Stable near/far side-view layer order
**Status:** OPEN

Far limbs render first with dimmer shading, torso next, near limbs afterward, and feet at the correct chain depth. Layering must make leg crossing legible without changing collision or semantic identity.

### WALK-WEAPON-SPRITE-198 — Optional weapon sprite anchor and safe carry preview
**Status:** OPEN

Use the supplied fictional weapon art as a non-functional visual preview anchored to valid hand nodes in Rig Lab. This does not activate the deferred firing curriculum and cannot alter policy dimensions, mass, recoil, or runtime behavior.

### WALK-ART-TOGGLE-199 — Explicit optional-art and debug-skeleton controls
**Status:** OPEN

Expose a clear Rig Lab control to toggle optional armor/foot sprites and a separate debug-skeleton overlay. Live training defaults to sprites when available while deterministic diagnostics can force the procedural fallback.

### WALK-QUAD-TEST-200 — Repeated quadruped press/recovery acceptance
**Status:** OPEN

Run repeated seeded quadruped crouch cycles through settle, descent, hold, retraction, full extension, and stable recovery. Reject body clipping, crush termination, unrecovered body height, lost support chains, and infinite crouch residence.

### WALK-DWELL-TEST-201 — Fresh-stage minimum-work regression tests
**Status:** OPEN

Tests prove inherited counters, one evaluation, two steps, or a short lucky episode cannot advance Stand, Crouch, or Walk. Fresh current-stage episodes and minimum dwell requirements must be satisfied after every stage or rig change.

### WALK-GAIT-TEST-202 — Side-view gait and crab adversarial fixtures
**Status:** OPEN

Positive fixtures prove alternating sagittal crossing and sustained distance. Negative fixtures reject split stance, lateral abduction, symmetric outward stepping, friction shuffle, course-only motion, and two-step early exit.

### WALK-STUB-TEST-203 — Stub-foot terrain and release fixtures
**Status:** OPEN

Test single-contact support on flat, stepped, loose, sloped, and deforming terrain; bounded planted slip; prompt swing/toe-off release; no multi-cell bridge; and finite pressure/observation values for every affected preset.

### WALK-ART-TEST-204 — Optional-art parser, atlas, fallback, and package tests
**Status:** OPEN

Validate the derived P3 atlas with the production loader, verify every supplied source sheet and hash in the package, run with optional assets removed, and prove fallback sprites produce the same physics and acceptance results.

### WALK-PIP-PARITY-205 — PIP uses the same side-view sprites and gait truth
**Status:** OPEN

Training PIP and live view share layer order, stub-foot sprites, optional-art fallback, gait direction, and rejection telemetry. PIP may not make crab walking look like forward gait or hide a crushed quadruped.

### WALK-EDITOR-206 — Rig Lab controls for stubs, layers, and gait diagnostics
**Status:** OPEN

Rig Lab displays one support stub per chain, side-view layer assignment, signed stride/crossing telemetry, lateral/crab rejection, current stage dwell, press safety/recovery state, optional-art status, and sprite/debug toggles.

### WALK-STATE-207 — Isolate corrected foot, gait, and curriculum semantics
**Status:** OPEN

Bump training, checkpoint, evolved-rig, and autonomy-state semantics and use `runner-v0717-*` paths. v0.7.16 policies with multi-node feet, short stage dwell, or crab gait cannot silently resume; explicit transfer is weights-only when dimensions and support remapping are valid.

### WALK-DOC-208 — Consolidate v0.7.17 documentation
**Status:** OPEN

Update README, CHANGELOG, AGENTS.md-referenced workflow expectations, focused side-view/stub-foot documentation, optional-art provenance, controls, diagnostics, state migration, and user eye-test reopening rules without creating a second changelog or mission ledger.

### WALK-PACKAGE-209 — Audit source and optional assets in every package form
**Status:** OPEN

Build-tree, installed, extracted, and re-downloaded packages must contain AGENTS.md, missioncache, changelog, focused v0.7.17 documentation, all four supplied optional sheets, provenance/hashes, and the runtime atlas; diagnostics must pass both with and without optional assets.

### WALK-RELEASE-210 — Publish audited Runner v0.7.17
**Status:** OPEN

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, every deterministic suite, repeated quadruped press/recovery, fresh-stage dwell, sustained sagittal gait, crab rejection, stub-foot terrain, optional-art/fallback, 24+ locomotion acceptance, build/installed/extracted diagnostics, executable-relative `run.bat`, ZIP/checksum/manifest, release re-download and byte comparison, exact ledger evidence, zero open cleanup PRs, and only `main`. Released eye-test evidence remains authoritative and reopens the exact mission.
'''

text = text.replace(marker, marker + section, 1)
text = text.replace('# Runner v0.7.17 equipment, carry, and target curriculum',
                    '# Runner v0.7.18 equipment, carry, and target curriculum', 1)
text = text.replace('after the v0.7.16 viewport release.',
                    'after the v0.7.17 eye-test correction release.', 1)
text = text.replace('### WALK-RELEASE-155 — Publish audited Runner v0.7.17',
                    '### WALK-RELEASE-155 — Publish audited Runner v0.7.18', 1)
path.write_text(text, encoding='utf-8', newline='\n')
