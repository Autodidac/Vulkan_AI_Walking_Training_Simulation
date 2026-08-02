# EpochRunner mission cache

This is the authoritative release ledger. A mission is VERIFIED only when implementation, deterministic acceptance, cross-platform validation, packaged-runtime behavior, and release evidence agree. Contradictory runtime evidence reopens the mission.

## Release target

**Target:** EpochRunner v0.7.2

**Release state:** PACKAGE VERIFIED — release publication and published-asset audit pending

There are no open implementation missions. Publication, final release-asset re-download, and repository cleanup remain before the release state can become published.

## v0.7.2 packaged-runtime regression correction

Adam's August 1, 2026 screenshots contradicted the v0.7.1 runtime conclusion. They reopened the affected missions instead of being treated as cosmetic feedback.

### WALK-REG-022 — Whole-simulation regression correction
**Status:** VERIFIED

The carried-forward rig, controller, curriculum, persistence, PIP, launch, concurrency, and package requirements were revalidated after the correction. Linux and Windows builds, all deterministic tests, runtime diagnostics, package extraction, and independent checksum verification passed.

### WALK-SYNERGY-023 — Coordinated joint groups with learned residuals
**Status:** VERIFIED

Bilateral hip, knee, shoulder, and elbow groups provide stage-aware movement structure while PPO retains residual per-joint control. The same effective controller is used by rollout collection, deterministic evaluation, rig evaluation, preview, and live execution.

### WALK-FOOT-024 — Dedicated semantic feet below articulated ankles
**Status:** VERIFIED

Lower-leg motor endpoints are ankles rather than foot contacts. Each biped leg uses a braced passive ankle adapter connected to a separate foot plate, heel, and toe. Only those explicit semantic foot nodes receive support and traction classification.

### WALK-DUCK-025 — Obstacle-conditioned duck, clear, and recover lesson
**Status:** VERIFIED

The second lesson presents a moving low bar. Qualification requires lowering the head with semantic foot support, clearing the actual obstacle, and returning to stable stance. Empty-space crouching and permanent collapse cannot complete or seed the lesson.

### WALK-SAMPLE-026 — Current-frame training-sample integrity
**Status:** VERIFIED

Historical stance evidence cannot authorize a currently collapsed PIP frame. Display eligibility requires current semantic foot support, no current non-foot support, and intact direct pelvis-to-torso and torso-to-head body segments. A valid crouch remains displayable while the screenshot collapse is rejected.

### WALK-LAUNCH-027 — Reject stale source-tree executables and v0.7.1 state
**Status:** VERIFIED

Source-tree `run.bat` selects the current Windows Release build instead of a stale root executable. Installed packages select the adjacent executable. v0.7.2 uses new checkpoint semantics, autonomy-state format, and isolated autosave paths.

## Training quality

### WALK-MOTOR-012 — Reciprocal parent-side motor reaction
**Status:** VERIFIED

Every motor divides correction between the driven subtree and complete parent side using rotational inertia, without world-space joint anchors or center-of-mass injection.

### WALK-OBS-018 — Non-overlapping eight-motor observation layout
**Status:** VERIFIED

The humanoid retains forty non-overlapping observation channels covering eight angles, eight velocities, contacts, foot placement, terrain, obstacle, stage, and phase state.

### WALK-TRAIN-013 — Reject collapsed poses as training success
**Status:** VERIFIED

Collapsed, body-supported, violent-joint, rolling, skating, hovering, motionless, and prerequisite-incomplete candidates cannot become champions, rollback anchors, evolved-rig seeds, imitation sources, or displayed training samples.

### WALK-CURR-014 — Evidence-gated ordered curriculum
**Status:** VERIFIED

Advancement requires retained evidence for stand, low-bar duck and recovery, powered jump and landing, alternating gait, moving avoidance, controlled flips, and mixed traversal. Scalar reward cannot bypass missing evidence.

### WALK-BEST-015 — Stage-valid best-policy and imitation selection
**Status:** VERIFIED

Stage validity and evidence quality outrank scalar reward throughout champion selection, rollback, rig evolution, imitation, and PIP representative selection.

### WALK-EVAL-019 — Latched robust balance evaluation
**Status:** VERIFIED

A completed three-second stage-valid stand is latched. Six perturbed starts are evaluated and at least four must succeed; invalid later-stage evaluation remains strict.

### WALK-CTRL-020 — Shared effective controller path
**Status:** VERIFIED

Training, deterministic evaluation, imitation replay, rig evaluation, preview, and live execution share the coordinated effective controller.

### WALK-STATE-016 — Invalidate incompatible learned state
**Status:** VERIFIED

Training checkpoints use v0.7.2 semantics `0x0007'0200`, autonomy state format 5, and v0.7.2 autosave paths. Earlier optimizer, curriculum, champion, rig-evolution, and imitation state cannot silently resume.

### WALK-RUNTIME-017 — Bounded packaged-runtime acceptance
**Status:** VERIFIED

The full Windows application passed bounded standing acquisition, robust perturbed evaluation, adversarial collapse rejection, current-frame PIP rejection, concurrency tests, runtime pipeline tests, package layout, and unrelated-working-directory diagnostics.

### WALK-LAUNCH-021 — Executable-relative clean-folder launch
**Status:** VERIFIED

The build tree, installed executable, installed `run.bat`, independently extracted executable, and independently extracted `run.bat` all passed version and package diagnostics from unrelated working directories.

## Runtime architecture

### WALK-CORO-001 — Meaningful C++23 training pipeline
**Status:** VERIFIED

Queued commands, rollout collection, advantage computation, parallel gradient generation, deterministic reduction, optimizer application, evaluation, immutable publication, persistence, and throttle stages remain observable.

### WALK-ARCH-001 — Worker-owned mutable trainer state
**Status:** VERIFIED

The worker exclusively owns mutable PPO, optimizer, curriculum, rig-evolution, and checkpoint state; the UI consumes immutable publications and coalesced commands.

### WALK-IO-001 — Asynchronous checkpoint and autosave
**Status:** VERIFIED

Checkpoint, rig, and autonomy-state snapshots remain coalesced and published by a dedicated `std::jthread` through temporary-file and atomic-rename writes.

### WALK-TEST-001 — Cross-platform concurrency regression suite
**Status:** VERIFIED

Hip edits, preset changes, CPU modes, pause/single-step, checkpoint load/save, staged updates, cancellation, coalescing, Linux GCC 14, and Windows full-application coverage pass.

## Ordered movement curriculum

### WALK-SKILL-008 — Ordered reusable skills
**Status:** VERIFIED

Stand, low-bar duck and recover, jump and land, walk and run, moving avoidance, controlled flips, and mixed traversal remain ordered by prerequisite evidence.

### WALK-ARMS-009 — Humanoid arms for balance and acrobatics
**Status:** VERIFIED

Independent shoulder and elbow motors retain dedicated observations and participate in coordinated balance and acrobatic control.

### WALK-LEARN-010 — Faster learning without regression
**Status:** VERIFIED

Physical balance and obstacle primitives guide early learning while PPO retains residual control. Smoothing, anti-skating gates, skill resets, rollback, and bounded imitation remain active.

## Existing locomotion and course requirements

### WALK-RIG-001 — Nonblocking hip and joint editing
**Status:** VERIFIED

### WALK-CONC-001 — Persistent CPU parallelism
**Status:** VERIFIED

### WALK-UI-001 — Functional NORMAL, FASTER, and MAX CPU controls
**Status:** VERIFIED

### WALK-OPT-001 — Parallel PPO optimizer
**Status:** VERIFIED

### WALK-COURSE-001 — Procedural obstacle and recovery treadmill
**Status:** VERIFIED

### WALK-OBS-001 — Complete obstacle sensing and reward integrity
**Status:** VERIFIED

Obstacle type, distance, geometry, and motion are observable. Low-bar ducking is conditioned on the observed bar, and invalid posture cannot be promoted by reward.

### WALK-PHYS-001 — Semantic support, traction, and world-anchored debris
**Status:** VERIFIED

Only explicit foot nodes receive traction or support semantics. Non-foot ground contact remains body contact, and course features remain world-scheduled rather than attached to the actor.

### WALK-COURSE-002 — Shared mile-marker obstacle schedule
**Status:** VERIFIED

### WALK-GAIT-002 — Alternating stepping instead of wheel sliding
**Status:** VERIFIED

Walking qualification requires real alternating supported steps and positive progress; wheel sliding cannot qualify.

### WALK-SAND-001 — Sand-simulation enemy curriculum
**Status:** VERIFIED

### WALK-ROLL-003 — Head, tail, torso, and foot-node rolling rejection
**Status:** VERIFIED

Rolling, skating, and non-foot support participate in the shared qualification path and cannot seed elite state.

### WALK-HAZARD-003 — Hazards are never pickups or contact rewards
**Status:** VERIFIED

### WALK-LOCO-004 — Biped, quadruped, crawler, and hexapod support
**Status:** VERIFIED

Deterministic preset coverage remains for humanoid, biped, chicken, quadruped, crawler, hexapod, and monoped rigs.

### WALK-IDLE-005 — Zero-progress reset
**Status:** VERIFIED

Motionless candidates reset and cannot enter elite selection.

### WALK-GUIDE-006 — Automatic best-result imitation prior
**Status:** VERIFIED

Only stage-valid, clean trajectories can enter the imitation prior; duck imitation additionally requires clearing the low bar.

## UI

### WALK-UI-002 / WALK-UI-003 — Responsive readable telemetry
**Status:** VERIFIED

Layout checks pass and telemetry exposes stance, retained stance, recovery, evidence quality, and primary rejection reason.

### WALK-PIP-007 — Actual valid worker-rollout picture-in-picture
**Status:** VERIFIED

The PIP publishes only a currently displayable stage-qualified worker rollout and otherwise reports that no valid rollout exists.

## Release evidence

### WALK-REL-011 — Historical v0.7.0 release evidence
**Status:** VERIFIED — HISTORICAL

The v0.7.0 evidence remains historical and was superseded by the later runtime screenshots.

### WALK-REL-013 — Historical v0.7.1 training-quality hotfix
**Status:** VERIFIED — HISTORICAL

The v0.7.1 package and build evidence remain valid for that artifact, but its simulation-quality conclusion was superseded by Adam's August 1 screenshots and corrected in v0.7.2.

### WALK-REL-028 — v0.7.2 simulation-quality correction
**Status:** PACKAGE VERIFIED — RELEASE PUBLICATION PENDING

- Exact Windows-tested and packaged source: `3c4b815678fda0b2651136bba90b4d64a6cc9a27`;
- clean implementation source before evidence-only commits: `38134978a08ef59d793fbc82c45bbe4090bebacb`;
- Linux GCC 14 successful run: `30732783454`;
- later clean-source Linux validation: `30732908362`;
- Windows full application and package run: `30733668446`;
- Windows job: `91458283228`;
- four Windows tests: passed;
- build-tree executable version, Vulkan, and package diagnostics from unrelated CWD: passed;
- installed executable and installed `run.bat` diagnostics from unrelated CWD: passed;
- independently extracted executable and `run.bat` diagnostics from unrelated CWD: passed;
- package: `EpochRunner-v0.7.2-windows-x64.zip`;
- package file count: `11`;
- package SHA-256: `E74F5BFFEAE2ADFCD53D45C84D63364772BEE33066452D43433D1BF803873AD7`;
- Actions artifact ID: `8828973915`;
- Actions artifact digest: `632316E2ECD7419E7E471F7AF8D2BFD7C0490F7E24B7459D0CE1D0559C50DBE0`;
- assistant-side re-download and per-file manifest verification: passed.
