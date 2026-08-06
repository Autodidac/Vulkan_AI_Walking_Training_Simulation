# Runner cache-first engineering policy and active release plan

# Runner v0.7.18 runtime recovery, controls, and observability release

**Release state:** CACHED BEFORE IMPLEMENTATION — released v0.7.17 eye-test regressions reopened. Terrain is explicitly preserved. The deferred equipment/carry/target curriculum moves intact to v0.7.19.

Direct packaged-runtime observations on v0.7.17 are authoritative: the terrain itself appears correct; reference/mile markers are absent from the initial live view; the visible update counter reaches approximately 10 and then appears to reset; all-time updates are hidden behind a secondary totals page; the trainer remains effectively stuck at the beginning and never reaches useful walking; the live controls and status information are difficult to discover or interpret; and the optional torso/helmet skin is visually unacceptable. Source audit confirms the update-loop cause: policy evaluation occurs at updates 1, 5, and 10, while `manage_curriculum_locked()` resets a no-champion policy on every third invalid evaluation. That reset therefore occurs at update 10 even though Stand requires 120 fresh updates before its dwell gate can complete. `reset_training_state()` preserves `total_updates`, so the cumulative work exists but the primary UI displays the resettable stage/update counter instead.

### WALK-RUNTIME-RESET-211 — Remove the update-10 nursery-reset contradiction
**Status:** OPEN — RELEASE BLOCKING

A no-champion controller may not be automatically discarded before the current stage has received a meaningful training budget. Stand must be able to accumulate at least its full 120-update dwell requirement without policy reset. Any later nursery reset requires an explicit large fresh-update/evaluation budget and must preserve cumulative totals.

### WALK-TOTAL-UPDATES-212 — Make cumulative updates impossible to miss
**Status:** OPEN — RELEASE BLOCKING

Live world and Training Results must show `total_updates` continuously, alongside the resettable stage/policy update count, evaluation count, reset count, and updates/second. The user must be able to distinguish training progress from a policy reset without opening another page.

### WALK-STAGE-PROGRESS-213 — Expose actual stage-work progress
**Status:** OPEN — RELEASE BLOCKING

Publish fresh updates, episodes, and evaluations since stage entry plus their required thresholds. The live panel must explain whether the trainer is waiting on work, strict evidence, or mastery confirmations instead of presenting an opaque starting state.

### WALK-MARKERS-214 — Restore visible reference markers near the start
**Status:** OPEN — RELEASE BLOCKING

Keep the current terrain/collision coordinate system unchanged. Restore a START marker and useful recurring distance markers inside the initial live viewport. Marker world positions must remain treadmill/course-progress correct.

### WALK-MARKER-LABELS-215 — Use readable meter/foot marker labels
**Status:** OPEN — RELEASE BLOCKING

Metric reference markers use practical meter labels near the start; imperial markers use practical foot labels before mile-scale distances. Do not display tiny `0.00 KM`/`0.00 MI` labels for nearby markers.

### WALK-CONTROLS-216 — Restore documented keyboard controls
**Status:** OPEN — RELEASE BLOCKING

`Tab` switches Live/Rig Lab; `Space` runs/pauses background training; `1/2/3` select Normal/Faster/Max CPU; `T` toggles results/totals; `U` toggles metric/imperial; `A` toggles optional armor; `R` resets only the live camera/preview. Runtime mappings must match README and visible help.

### WALK-CONTROL-UI-217 — Put control help in the application
**Status:** OPEN — RELEASE BLOCKING

The top bar and live panel must advertise the controls rather than requiring source/README knowledge. Training state, speed mode, pause state, and pipeline stage must remain visible.

### WALK-SKIN-218 — Disable the unacceptable body skin by default
**Status:** OPEN — RELEASE BLOCKING

Keep forward sprite feet available, but optional torso/helmet/weapon overlays default OFF and remain explicitly toggleable. Optional visuals never affect physics, training, observations, package startup, or the terrain.

### WALK-WALK-BOOTSTRAP-219 — Restore useful early walking guidance
**Status:** OPEN — RELEASE BLOCKING

Once the trainer reaches Walk, early paired-leg policies receive strong enough sagittal teacher guidance to produce visible fore/aft alternating leg motion and forward progress while still allowing PPO authority to grow. Do not loosen crab-walk rejection or stage mastery evidence.

### WALK-STATE-220 — Isolate corrected runtime state
**Status:** OPEN — RELEASE BLOCKING

Bump training/autonomy semantics and use `runner-v0718-*` autosave paths so the broken v0.7.17 reset loop or stale controller state cannot silently resume. Manual checkpoint transfer remains explicit.

### WALK-SOURCE-AUDIT-221 — Remove stale version/control assumptions
**Status:** OPEN — RELEASE BLOCKING

Audit application, autonomy, PPO, UI layout, persistence, tests, docs, CMake, package audit, and release automation for stale v0.7.6/v0.7.16/v0.7.17 runtime strings and contradictory control/update assumptions.

### WALK-REGRESSION-222 — Deterministically test the reset, marker, and gait recovery
**Status:** OPEN — RELEASE BLOCKING

Tests prove: update 10 cannot trigger nursery reset; the full Stand dwell can accumulate; a later bounded reset remains possible; initial reference-marker spacing is visible; v0.7.18 semantics are isolated; and the paired-leg walking teacher provides strong opposite-phase sagittal drive without changing terrain coordinates.

### WALK-DOC-223 — Document runtime recovery and controls
**Status:** OPEN — RELEASE BLOCKING

Update README, CHANGELOG, focused v0.7.18 documentation, missioncache, CMake install contents, and repository audit. Keep a single changelog and a single mission ledger.

### WALK-PACKAGE-224 — Audit the complete v0.7.18 package
**Status:** OPEN — RELEASE BLOCKING

Linux warnings-as-errors, full Windows SDL3/Vulkan build, all tests, acceptance/camera/package diagnostics, installed/extracted runs, optional-art fallback, ZIP/checksum/manifest, and unrelated-directory `run.bat` must all pass.

### WALK-RELEASE-225 — Publish and verify Runner v0.7.18
**Status:** OPEN — RELEASE BLOCKING

Merge only validated source, tag `v0.7.18`, publish audited assets, re-download and byte-verify them, record exact evidence, remove the release branch, and leave only `main`. User eye testing may reopen any matching mission.

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

**Release state:** CACHED BEFORE IMPLEMENTATION — RELEASE BLOCKING.

The v0.7.17 packaged terrain is explicitly retained. Direct runtime observations reopen only these facts: useful course reference markers are missing from the starting view; the visible update count reaches about 10 and appears to reset; cumulative total updates are not continuously visible; the trainer remains effectively at the beginning and ordinary walking has regressed; controls and telemetry are difficult to discover or interpret; and the optional torso/helmet skin is visually unacceptable.

Source audit found the exact update-loop contradiction: policy evaluation occurs at update 1 and every fifth update, while v0.7.17 resets a no-champion policy on every third invalid evaluation. That can reset the local policy counter at update 10 although Stand requires 120 fresh updates before its dwell gate can complete. `reset_training_state()` preserves cumulative totals, so training history survives but the primary UI hides it behind a resettable counter.

### WALK-RUNTIME-RESET-211 — Remove the update-10 nursery reset contradiction
**Status:** OPEN — RELEASE BLOCKING

A no-champion controller may not be automatically discarded before the current stage has received a meaningful training budget. Stand must be able to accumulate its full 120-update fresh-work requirement. Any later automatic nursery restart requires a substantially larger fresh-update/evaluation budget and preserves cumulative totals.

### WALK-TOTAL-UPDATES-212 — Make cumulative training progress continuously visible
**Status:** OPEN — RELEASE BLOCKING

The live world and Training Results show all-time `total_updates` continuously alongside the resettable policy/stage update, evaluation count, reset count, and updates/second. A policy restart must never look like all training progress disappeared.

### WALK-STAGE-PROGRESS-213 — Explain current stage work
**Status:** OPEN — RELEASE BLOCKING

Publish fresh updates, episodes, and evaluations since stage entry plus each required threshold. The UI states whether the trainer is waiting on work, strict evidence, or mastery confirmations instead of only saying it is starting.

### WALK-MARKERS-214 — Restore useful markers without touching terrain
**Status:** OPEN — RELEASE BLOCKING

Keep the current terrain, collision, pressure, treadmill transform, and course physics unchanged. Add a visible START reference and recurring near-course distance markers inside the initial viewport. Marker positions remain world/course-progress correct.

### WALK-MARKER-LABELS-215 — Use practical near-distance labels
**Status:** OPEN — RELEASE BLOCKING

Metric reference markers use metres near the start and kilometres only at kilometre scale. Imperial markers use feet near the start and miles only at mile scale. Do not show nearby signs as `0.00 KM` or `0.00 MI`.

### WALK-CONTROLS-216 — Make runtime controls match their documentation
**Status:** OPEN — RELEASE BLOCKING

`Tab` switches Live/Rig Lab; `Space` runs/pauses background training; `1/2/3` select Normal/Faster/Max CPU; `T` toggles Results/Lifetime Totals; `U` toggles Metric/Imperial; `A` toggles optional body armor; `R` resets only live preview/camera state. Runtime mappings and README must agree.

### WALK-CONTROL-UI-217 — Put control help and trainer state in the application
**Status:** OPEN — RELEASE BLOCKING

The top bar/live panel advertise controls and continuously expose training state, speed mode, pause state, pipeline stage, stage-work progress, and throughput without requiring source knowledge.

### WALK-SKIN-218 — Disable the unacceptable fake body skin by default
**Status:** OPEN — RELEASE BLOCKING

Optional torso/helmet/weapon overlays default OFF and remain explicitly toggleable. Forward sprite feet remain independently available. Optional art never affects physics, observations, policy state, terrain, package startup, or deterministic acceptance.

### WALK-WALK-BOOTSTRAP-219 — Restore useful early walking guidance
**Status:** OPEN — RELEASE BLOCKING

Once Walk begins, paired-leg policies receive sufficient sagittal fore/aft teacher/bootstrap authority to demonstrate alternating foot passing and forward progress long enough for PPO to learn it. Existing crab-walk rejection, support integrity, and sustained-distance mastery remain strict.

### WALK-STATE-220 — Isolate corrected v0.7.18 learned/runtime state
**Status:** OPEN — RELEASE BLOCKING

Bump training and autonomy-state semantics and use `runner-v0718-*` autosave paths so v0.7.17 reset-loop state cannot silently resume. Manual compatible weight transfer remains explicit.

### WALK-SOURCE-AUDIT-221 — Reconcile stale runtime assumptions across the source tree
**Status:** OPEN — RELEASE BLOCKING

Audit application input/rendering, autonomy, PPO, curriculum, persistence, UI layout, CMake, tests, repository audit, docs, package contents, and release workflow for stale versions, stale controls, contradictory counters, and dead temporary infrastructure.

### WALK-REGRESSION-222 — Deterministically test reset, marker, state, and gait recovery
**Status:** OPEN — RELEASE BLOCKING

Tests prove update 10 cannot trigger nursery reset; the complete Stand dwell can accumulate; a later bounded nursery restart remains possible; starting marker spacing is visible; v0.7.18 semantics are isolated; and paired-leg walking assistance produces meaningful opposite-phase sagittal drive without changing terrain coordinates.

### WALK-DOC-223 — Consolidate v0.7.18 documentation
**Status:** OPEN — RELEASE BLOCKING

Update README, CHANGELOG, focused v0.7.18 documentation, this ledger, CMake install contents, and repository/package audits. Do not create another changelog or mission ledger.

### WALK-PACKAGE-224 — Audit the complete v0.7.18 package
**Status:** OPEN — RELEASE BLOCKING

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, every deterministic suite, 24+ live locomotion acceptance, camera/package diagnostics, installed and extracted execution, optional-art fallback, executable-relative `run.bat`, ZIP/checksum/manifest, and workflow artifact upload.

### WALK-RELEASE-225 — Publish and verify Runner v0.7.18
**Status:** OPEN — RELEASE BLOCKING

Merge only validated source, tag `v0.7.18`, publish audited assets, re-download and byte-verify them, record exact evidence, delete temporary workflows/branches, close cleanup PRs, and leave only `main`.

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
