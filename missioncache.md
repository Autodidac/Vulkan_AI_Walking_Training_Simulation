# EpochRunner mission cache

This is the authoritative release ledger. Nothing is removed until it is implemented, tested, and linked to exact release evidence. A contradictory packaged runtime screenshot reopens the same mission even when compilation and deterministic tests previously passed.

## Release target

**Target:** EpochRunner v0.7.1

**Release state:** BLOCKED — packaged humanoid training quality regression

## Active runtime regressions

### WALK-MOTOR-012 — Reciprocal parent-side motor reaction
**Status:** REGRESSION

Adam's August 1, 2026 packaged-runtime screenshot shows the humanoid folding around a nearly fixed upper-torso/chest region. Shoulder and elbow actuation must not treat the chest reference side or joint pivot as a world-space pin.

Acceptance:

- chest and shoulder pivot react during direct motor correction;
- the lighter driven limb moves farther than the parent body;
- internal motor action preserves whole-body center of mass within numerical tolerance;
- the packaged humanoid translates and rotates as one connected body;
- the fix does not destabilize biped, quadruped, crawler, hexapod, chicken, or monoped presets.

### WALK-TRAIN-013 — Reject collapsed poses as training success
**Status:** REGRESSION

The screenshot does not show a usable learned result: the humanoid is collapsed, has no clean supported stance, and has no repeatable alternating gait. Reward alone may never promote this pose as a best result, imitation prior, checkpoint anchor, curriculum pass, or displayed representative rollout.

Acceptance:

- standing success requires a sustained upright torso, supported feet, bounded torso/head height, no non-foot ground support, low stance slip, and bounded joint speed;
- walking success additionally requires alternating supported steps and positive net progress without wheel sliding, body rolling, foot-pivot rolling, sustained flight, or zero-progress failure;
- a candidate that fails any stage invariant cannot replace the stage best even when scalar reward is higher;
- the main viewport and PIP never label a failed candidate as best or successful;
- deterministic adversarial tests prove crouched, folded, pinned, skating, rolling, hovering, and motionless reward exploits are rejected.

### WALK-CURR-014 — Evidence-gated ordered curriculum
**Status:** REGRESSION

The curriculum must not leave stand/recovery or preserve later-stage progress until the prerequisite motion is visibly and numerically real.

Acceptance:

1. stand/recover passes only after a sustained valid stance window;
2. duck passes only after valid stand, controlled compression, and return to stand;
3. jump passes only after supported powered takeoff and upright supported landing;
4. walk/run passes only after multiple alternating supported steps with net progress;
5. moving duck/jump requires the corresponding stationary skill plus valid gait;
6. flips require powered takeoff, bounded one-to-three rotations, and upright landing;
7. mixed traversal requires retained evidence for every prerequisite without regression.

A failed evaluation resets the candidate stage evidence and cannot be hidden by average reward.

### WALK-BEST-015 — Stage-valid best-policy and imitation selection
**Status:** OPEN

Best-policy anchoring and self-imitation must use lexicographic stage-valid evidence before scalar reward. Selection order is validity, prerequisite completion, survival duration, real supported-step count, obstacle/skill evidence, net progress, then reward and efficiency. Invalid candidates are excluded rather than merely penalized.

Acceptance:

- current best, historical best, rollback anchor, evolved rig seed, and imitation source share one validation predicate;
- stale best policies created under older physics/reward semantics are rejected by a versioned compatibility signature;
- representative worker rollout is selected from valid candidates only;
- tests prove a high-reward invalid pose loses to a lower-reward valid pose.

### WALK-STATE-016 — Invalidate incompatible learned state
**Status:** OPEN

The chest-reaction, stage-gating, and best-selection semantics change the policy environment. v0.7.0 autosaves, checkpoints, evolved rigs, autonomy state, cached best policies, and imitation samples must not silently resume as v0.7.1-compatible training state.

Acceptance:

- checkpoint metadata includes a training-semantics version/signature;
- incompatible optimizer, curriculum, best-policy, and imitation state is rejected or explicitly imported as weights-only with progress reset;
- default v0.7.1 autosave paths are isolated from v0.7.0;
- UI reports why a state was rejected or reset.

### WALK-RUNTIME-017 — Packaged visual training acceptance
**Status:** OPEN

Compilation and synthetic tests are insufficient for final completion. The packaged Windows build must visibly produce useful training results.

Acceptance:

- humanoid first learns a stable stand without a fixed chest, body contact, skating, or violent joint oscillation;
- later lessons show real alternating steps, powered jumps, controlled ducking, and valid landings in prerequisite order;
- telemetry exposes the exact evidence that qualified the displayed best candidate;
- Adam's packaged-runtime review no longer contradicts the ledger;
- release evidence includes a bounded runtime capture or deterministic replay of the accepted policy.

## Runtime architecture

### WALK-CORO-001 — Meaningful C++23 training pipeline
**Status:** VERIFIED

One update is split into observable coroutine stages: queued commands, rollout collection, advantage computation, parallel gradient generation, deterministic reduction/optimizer application, evaluation/curriculum/evolution, immutable publication, asynchronous persistence scheduling, and throttle/yield. Diagnostics expose the current stage.

### WALK-ARCH-001 — Worker-owned mutable trainer state
**Status:** VERIFIED

The trainer worker exclusively owns mutable PPO, optimizer, curriculum, rig-evolution, and checkpoint-application state. UI operations use coalesced commands and immutable snapshots. UI-facing calls do not contend on a mutex held across a PPO update or deterministic evaluation.

### WALK-IO-001 — Asynchronous checkpoint and autosave
**Status:** VERIFIED

Immutable checkpoint, rig, and state data is copied at a worker publication boundary, pending saves are coalesced, disk work runs on a dedicated `std::jthread`, and publication uses temporary-file plus atomic rename. Save/load requests remain prompt during training.

### WALK-TEST-001 — Cross-platform concurrency regression suite
**Status:** VERIFIED

Coverage includes hip dragging and preset swapping during NORMAL and MAX CPU training, speed-mode switching, pause/single-step/resume, checkpoint save/load under load, clean cancellation, deterministic staged-update comparison, asynchronous persistence coalescing, Linux release tests, and Linux ThreadSanitizer.

## Ordered movement curriculum

### WALK-SKILL-008 — Ordered reusable skills
**Status:** REGRESSION

The intended prerequisite order remains:

1. stand and recover balance,
2. duck and return to standing,
3. joint-powered jump and upright landing,
4. walk and run with alternating supported steps,
5. duck or jump while moving,
6. controlled airborne flips with upright landing,
7. mixed-goal traversal combining the learned skills.

Hazard contact remains legal and physical. Contact creates a bounded event penalty but is not terminal by itself. Passing the hazard earns progress. Joint-powered airtime is bounded by lesson and launch evidence. Up to three airborne spins are allowed in flip-capable lessons; a fourth spin, hovering, unpowered sustained flight, ground rolling, body surfing, planted-foot skating, and wheel sliding remain invalid.

This mission is reopened because runtime behavior does not demonstrate the prerequisite skills despite the prior verified label.

### WALK-ARMS-009 — Humanoid arms for balance and acrobatics
**Status:** REGRESSION

The humanoid has two articulated arms with independently controllable shoulder and elbow motors. Policy and checkpoint dimensions support eight outputs, while four-motor creatures retain an explicit active-motor count. This mission is reopened until arm actuation no longer pins or numerically anchors the parent chest/body.

### WALK-LEARN-010 — Faster learning without regression
**Status:** REGRESSION

Phase-guided gait bootstrap, action smoothing, supported-step evidence, anti-skating gates, per-skill learning-rate reset, best-policy anchoring, regression rollback, and bounded self-imitation exist, but the screenshot proves their acceptance and selection logic still permits unusable results. Completion requires WALK-TRAIN-013 through WALK-RUNTIME-017.

## Existing locomotion and course requirements

### WALK-RIG-001 — Nonblocking hip/joint editing
**Status:** VERIFIED

### WALK-CONC-001 — Persistent CPU parallelism
**Status:** VERIFIED

### WALK-UI-001 — Functional NORMAL/FASTER/MAX CPU controls
**Status:** VERIFIED

### WALK-OPT-001 — Parallel PPO optimizer
**Status:** VERIFIED

### WALK-COURSE-001 — Procedural obstacle and recovery treadmill
**Status:** VERIFIED

### WALK-OBS-001 — Complete obstacle sensing and reward integrity
**Status:** REGRESSION

Runtime acceptance is reopened because reward and observation evidence currently permits collapsed or pinned humanoid poses to appear competitive.

### WALK-PHYS-001 — Semantic support, traction, and world-anchored debris
**Status:** REGRESSION

Runtime acceptance is reopened for the parent-side motor anchoring contradiction.

### WALK-COURSE-002 — Shared mile-marker obstacle schedule
**Status:** VERIFIED

### WALK-GAIT-002 — Alternating stepping instead of wheel sliding
**Status:** REGRESSION

The packaged screenshot does not demonstrate a real alternating gait. This remains open until valid supported steps are required for best-result selection and curriculum progression.

### WALK-SAND-001 — Sand-simulation enemy curriculum
**Status:** VERIFIED

### WALK-ROLL-003 — Head, tail, torso, and foot-node rolling rejection
**Status:** REGRESSION

Invalid-contact and rolling gates must participate in the shared best-policy predicate, not only reward shaping or terminal logic.

### WALK-HAZARD-003 — Hazards are never pickups or contact rewards
**Status:** VERIFIED

### WALK-LOCO-004 — Biped, quadruped, crawler, and hexapod support
**Status:** PARTIAL

Cross-rig compile and deterministic support are retained. Runtime stability must be rechecked after reciprocal motor reaction and strict best-policy gating.

### WALK-IDLE-005 — Zero-progress reset
**Status:** REGRESSION

A motionless or folded candidate must be excluded from best-policy and imitation selection immediately after the bounded zero-progress window.

### WALK-GUIDE-006 — Automatic best-result imitation prior
**Status:** REGRESSION

The imitation prior must never learn from an invalid, pinned, collapsed, skating, rolling, hovering, or prerequisite-incomplete candidate.

## UI and release evidence

### WALK-UI-002 / WALK-UI-003 — Responsive readable telemetry
**Status:** PARTIAL

Layout checks remain verified. Training-quality telemetry is incomplete until the UI shows the stage-valid evidence and rejection reason for the selected candidate.

### WALK-PIP-007 — Actual worker-rollout picture-in-picture
**Status:** REGRESSION

The PIP must use a valid representative rollout, not merely an available worker snapshot. Failed candidates need a visible rejected/invalid state rather than appearing as successful training output.

### WALK-REL-011 — Verified v0.7.0 release
**Status:** VERIFIED

Completed pre-release evidence:

- Linux C++23 release build and deterministic/runtime tests: passed in workflow `30685177669`;
- Linux ThreadSanitizer runtime-pipeline run: passed in workflow `30685177669`;
- Windows 2025 full SDL3/Vulkan/EpochGui Release configure and build: passed in workflow `30685365960`;
- Windows deterministic core, runtime pipeline, concurrency, persistence, arm, and UI tests: passed;
- executable version and Vulkan diagnostic: passed;
- exact Windows-tested source commit recorded in `validation/v0.7.0-windows-premerge.md`.

The release itself remains historical evidence, but its training-quality claims are superseded by the packaged-runtime contradiction recorded above.
