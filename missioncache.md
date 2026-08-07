# Runner cache-first engineering policy and active release plan

`missioncache.md` is the single authoritative active mission ledger. Closed historical mission definitions and their exact release evidence remain preserved in immutable Git history and tags; duplicate imported copies were consolidated out here so active work is not hidden beneath stale ledgers. No open mission was discarded.

## Mandatory refinement loop

1. Cache requested behavior and observable acceptance criteria before product-source edits.
2. Inventory interactions across anatomy, physics, gait, curriculum, policy state, persistence, UI, rendering, terrain, tests, packaging, branches, and releases.
3. Record compatibility and regression risks before implementation.
4. Implement the smallest coherent system change rather than a screenshot-only patch.
5. Add deterministic positive, negative, adversarial, and repeated-seed tests.
6. Run Linux warnings-as-errors, the complete Windows SDL3/Vulkan build and tests, build-tree/installed/extracted diagnostics, checksum/manifest audits, and visual review where appearance or motion matters.
7. Re-read source and this ledger after validation. New consequences stay explicit and OPEN until resolved.
8. Merge, tag, publish, re-download, byte-verify, and clean branches only after exact evidence is recorded.
9. Released-package eye testing outranks automated closure and reopens only the matching mission.

# Runner v0.7.18 runtime recovery, controls, and observability

**Release state:** PUBLISHED — LINUX/WINDOWS/PACKAGE/RE-DOWNLOAD/CLEANUP VERIFIED.

The v0.7.17 packaged terrain is explicitly retained. Direct runtime observations reopen only these facts: useful course reference markers are missing from the starting view; the visible update count reaches about 10 and appears to reset; cumulative total updates are not continuously visible; the trainer remains effectively at the beginning and ordinary walking has regressed; controls and telemetry are difficult to discover or interpret; and the optional torso/helmet skin is visually unacceptable.

Source audit found the exact update-loop contradiction: policy evaluation occurs at update 1 and every fifth update, while v0.7.17 resets a no-champion policy on every third invalid evaluation. That can reset the local policy counter at update 10 although Stand requires 120 fresh updates before its dwell gate can complete. `reset_training_state()` preserves cumulative totals, so training history survives but the primary UI hides it behind a resettable counter.

### WALK-RUNTIME-RESET-211 — Remove the update-10 nursery reset contradiction
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

A no-champion controller may not be automatically discarded before the current stage has received a meaningful training budget. Stand must be able to accumulate its full 120-update fresh-work requirement. Any later automatic nursery restart requires a substantially larger fresh-update/evaluation budget and preserves cumulative totals.

### WALK-TOTAL-UPDATES-212 — Make cumulative training progress continuously visible
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The live world and Training Results show all-time `total_updates` continuously alongside the resettable policy/stage update, evaluation count, reset count, and updates/second. A policy restart must never look like all training progress disappeared.

### WALK-STAGE-PROGRESS-213 — Explain current stage work
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Publish fresh updates, episodes, and evaluations since stage entry plus each required threshold. The UI states whether the trainer is waiting on work, strict evidence, or mastery confirmations instead of only saying it is starting.

### WALK-MARKERS-214 — Restore useful markers without touching terrain
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Keep the current terrain, collision, pressure, treadmill transform, and course physics unchanged. Add a visible START reference and recurring near-course distance markers inside the initial viewport. Marker positions remain world/course-progress correct.

### WALK-MARKER-LABELS-215 — Use practical near-distance labels
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Metric reference markers use metres near the start and kilometres only at kilometre scale. Imperial markers use feet near the start and miles only at mile scale. Do not show nearby signs as `0.00 KM` or `0.00 MI`.

### WALK-CONTROLS-216 — Make runtime controls match their documentation
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

`Tab` switches Live/Rig Lab; `Space` runs/pauses background training; `1/2/3` select Normal/Faster/Max CPU; `T` toggles Results/Lifetime Totals; `U` toggles Metric/Imperial; `A` toggles optional body armor; `R` resets only live preview/camera state. Runtime mappings and README must agree.

### WALK-CONTROL-UI-217 — Put control help and trainer state in the application
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The top bar/live panel advertise controls and continuously expose training state, speed mode, pause state, pipeline stage, stage-work progress, and throughput without requiring source knowledge.

### WALK-SKIN-218 — Disable the unacceptable fake body skin by default
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Optional torso/helmet/weapon overlays default OFF and remain explicitly toggleable. Forward sprite feet remain independently available. Optional art never affects physics, observations, policy state, terrain, package startup, or deterministic acceptance.

### WALK-WALK-BOOTSTRAP-219 — Restore useful early walking guidance
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Once Walk begins, paired-leg policies receive sufficient sagittal fore/aft teacher/bootstrap authority to demonstrate alternating foot passing and forward progress long enough for PPO to learn it. Existing crab-walk rejection, support integrity, and sustained-distance mastery remain strict.

### WALK-STATE-220 — Isolate corrected v0.7.18 learned/runtime state
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Bump training and autonomy-state semantics and use `runner-v0718-*` autosave paths so v0.7.17 reset-loop state cannot silently resume. Manual compatible weight transfer remains explicit.

### WALK-SOURCE-AUDIT-221 — Reconcile stale runtime assumptions across the source tree
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Audit application input/rendering, autonomy, PPO, curriculum, persistence, UI layout, CMake, tests, repository audit, docs, package contents, and release workflow for stale versions, stale controls, contradictory counters, and dead temporary infrastructure.

### WALK-REGRESSION-222 — Deterministically test reset, marker, state, and gait recovery
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Tests prove update 10 cannot trigger nursery reset; the complete Stand dwell can accumulate; a later bounded nursery restart remains possible; starting marker spacing is visible; v0.7.18 semantics are isolated; and paired-leg walking assistance produces meaningful opposite-phase sagittal drive without changing terrain coordinates.

### WALK-DOC-223 — Consolidate v0.7.18 documentation
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Update README, CHANGELOG, focused v0.7.18 documentation, this ledger, CMake install contents, and repository/package audits. Do not create another changelog or mission ledger.

### WALK-PACKAGE-224 — Audit the complete v0.7.18 package
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, every deterministic suite, 24+ live locomotion acceptance, camera/package diagnostics, installed and extracted execution, optional-art fallback, executable-relative `run.bat`, ZIP/checksum/manifest, and workflow artifact upload.

### WALK-RELEASE-225 — Publish and verify Runner v0.7.18
**Status:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED

Merge only validated source, tag `v0.7.18`, publish audited assets, re-download and byte-verify them, record exact evidence, delete temporary workflows/branches, close cleanup PRs, and leave only `main`.

# Runner v0.7.18 treadmill-coordinate walking correction

**Release state:** PUBLISHED — LINUX/WINDOWS/PACKAGE/RE-DOWNLOAD/CLEANUP VERIFIED.

The overnight v0.7.17 eye test reaches Walk but reports only zero-to-two credited steps while the course itself moves at walking speed. Source audit found a coordinate-frame contradiction: moving lessons scroll terrain with `course_progress()`, but gait strike displacement, `distance_travelled_`, `forward_speed_`, and forward reward are measured only in fixed screen/world X. A correct treadmill gait can therefore walk in place relative to the camera yet receive zero travelled distance, fail the 5.5 cm step-displacement gate, fail the 6 m qualification gate, and never create a valid Walk champion. The existing qualification gate also conflates a safe incremental candidate with final stage mastery, so a two-step improvement is discarded instead of checkpointed.

### WALK-COURSE-FRAME-226 — Use terrain-relative locomotion coordinates
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Moving-course locomotion distance and per-frame forward progress use the same transform as the scrolling terrain: world X plus `course_progress()`. Static Stand/Crouch/Jump lessons remain unchanged because their course speed is zero.

### WALK-STEP-FRAME-227 — Credit real alternating strikes on the treadmill
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Alternating step displacement is measured in terrain-relative locomotion X while foot crossing, swing-air time, swing clearance, and contact transitions remain physical world-space evidence. A walker may stay camera-centered without losing legitimate step credit.

### WALK-SPEED-FRAME-228 — Report and train terrain-relative forward speed
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Logical forward speed on moving lessons includes course speed plus physical root speed. PPO evaluation, speed mastery, reward shaping, telemetry, and overspeed use the resulting ground-relative speed; static lessons are numerically unchanged.

### WALK-INCREMENTAL-CHAMPION-229 — Separate safe candidate qualification from mastery
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Walk may checkpoint a physically valid incremental sagittal candidate after two alternating steps, at least one genuine limb crossing, one metre of terrain-relative progress, and two seconds of survival. Final Walk mastery remains strict at the existing 18 m / 16 stride / speed / survival requirements, and crab walking, body contact, invalid motion, and structural failures remain rejected.

### WALK-IDLE-GATE-230 — Preserve anti-idle and anti-vibration behavior
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The one-second zero-progress anti-idle window stays in camera/world space and still requires useful swing lift or a credited step. Merely standing still while terrain scrolls must not count as active gait.

### WALK-BOOTSTRAP-231 — Keep useful guidance long enough to establish gait
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Early Walk bootstrap remains strongly sagittal through the first meaningful training window, then decays gradually so PPO takes control after a valid incremental walker exists.

### WALK-COORDINATE-TEST-232 — Deterministically lock the coordinate contract
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Regression tests prove terrain-relative distance/frame progress, nonzero moving-course distance with a camera-centered rig, opposite-phase teacher drive, and the existing v0.7.18 reset/marker contracts. Full Linux and Windows release gates remain mandatory.

# Carried open work

### WALK-CLIMB-134 — Reachable ledge climb and controlled backward descent
**Status:** OPEN — CARRIED TO A LATER RELEASE

Add a hard-wall curriculum where a rig climbs without jumping when hands can reach a ledge and turns backward to lower itself when the remaining fall is no greater than standing height. Completion requires hand/ledge contact, support transfer, no powered takeoff, and controlled feet-first recovery.

# Runner v0.7.19 equipment, carry, and target curriculum

**Release state:** CACHED AND OPEN — intentionally separated from the v0.7.18 runtime-recovery hotfix because it changes policy dimensions and checkpoint compatibility.

### WALK-EQUIPMENT-148 — Unarmed, safe carry, ready, disarmed, and dropped states
**Status:** OPEN

### WALK-WEAPONS-149 — Multiple abstract gameplay weapon classes
**Status:** OPEN

### WALK-TARGET-150 — Aim and fire at deterministic targets across distances
**Status:** OPEN

### WALK-COMBAT-CURRICULUM-151 — Preserve locomotion while carrying and firing
**Status:** OPEN

### WALK-EQUIPMENT-EDITOR-152 — Equipment and target editor controls
**Status:** OPEN

### WALK-POLICY-153 — Separate locomotion motors from equipment actions
**Status:** OPEN — ARCHITECTURE DECISION REQUIRED BEFORE IMPLEMENTATION

The existing anatomy motor slots remain anatomy controls. Equipment state, aim, and trigger require a separately versioned policy-action extension with explicit observation/checkpoint migration tests.

### WALK-EQUIPMENT-REGRESSION-154 — Optional-subsystem nonregression audit
**Status:** OPEN

### WALK-RELEASE-155 — Publish audited equipment release
**Status:** OPEN

# Recent immutable release evidence

## Runner v0.7.18

**Status:** PUBLISHED.

- v0.7.18 validation source: `157b1754a40193e58b457b49e17c55b2cb7ee6e7`.
- Main release workflow run: `31169049948`.

## Runner v0.7.17

**Status:** PUBLISHED — RELEASE ASSETS RE-DOWNLOADED AND VERIFIED; LATER USER EYE TESTING REOPENED ONLY MISSIONS 211–225.

- PR #56 merged to `main` at `673aade7d02523df96687479289a1a3f81729326`.
- Published tag: `v0.7.17`.
- Authoritative PR validation run: `31097579829`.
- Linux GCC 14 warnings-as-errors and deterministic suite: passed.
- Full Windows SDL3/Vulkan build and complete test matrix: passed.
- All eight six-seed Stand cases: passed.
- All eight four-seed crouch/hold/recover cases: passed.
- Live acceptance matrix: 24/24 passed.
- Build-tree, installed, optional-art-removed fallback, archive, independent extraction, checksum, manifest, and artifact gates: passed.
- Published assets were re-downloaded and byte-verified; completed and accidental v0.7.17 branches were removed.

## Runner v0.7.16

**Status:** PUBLISHED — RELEASE ASSETS RE-DOWNLOADED AND VERIFIED.

- PR #55; merge `1577706cade4a47cfde9c2834af22279e2cd793f`.
- Validation run `31030378702`.
- Adaptive camera, PIP/layout, Linux, Windows, package, installed/extracted diagnostics, ZIP/checksum/manifest, publication, and cleanup passed.

## Runner v0.7.15

**Status:** PUBLISHED — RELEASE ASSETS AND PACKAGE AUDIT VERIFIED.

- Terrain/render synchronization, real crouch qualification, side-view gait crossing, physical traction, structural rig evolution, editor diagnostics, Linux/Windows/package gates, publication, and cleanup passed at release time.
- Later contradictory runtime behavior is tracked by the current matching missions rather than rewriting historical evidence.

## Historical ledger preservation

All earlier closed mission definitions, imported legacy copies, validation findings, and exact release evidence remain available in Git history and release tags. This consolidation removes duplicate/stale copies from the active file; it does not erase or reclassify historical evidence. Any historical requirement that becomes relevant again is reopened here with a new current mission and explicit acceptance criteria.
