# EpochRunner mission cache

This is the authoritative release ledger. A mission is VERIFIED only when implementation, deterministic acceptance, cross-platform validation, packaged-runtime behavior, and release evidence agree. Contradictory runtime evidence reopens the mission.

## Release target

**Target:** EpochRunner v0.7.2

**Release state:** IMPLEMENTED — cross-platform and packaged-runtime verification pending


## v0.7.2 packaged-runtime regression correction

Adam's August 1, 2026 screenshots contradict the v0.7.1 runtime conclusion: the displayed training sample can be collapsed while labeled valid, foot semantics are entangled with lower-leg joints, independent motor outputs obscure useful movement structure, and ducking is not learned as obstacle avoidance. Contradictory evidence reopens the affected mission IDs; no v0.7.1 success claim is used as substitute evidence.

### WALK-REG-022 — Whole-simulation regression correction
**Status:** IMPLEMENTED — REVALIDATION PENDING

Revalidate every carried-forward mission after the rig, controller, curriculum, persistence, UI sample, launch, and package corrections. Build success alone is insufficient.

### WALK-SYNERGY-023 — Coordinated joint groups with learned residuals
**Status:** IMPLEMENTED — REVALIDATION PENDING

Bilateral hips, knees, shoulders, and elbows share stage-aware movement synergies. PPO retains residual control, but rollout, evaluation, rig evaluation, preview, and live execution no longer treat eight motors as unrelated gates.

### WALK-FOOT-024 — Dedicated semantic feet below the articulated ankles
**Status:** IMPLEMENTED — REVALIDATION PENDING

Lower-leg motor endpoints are ankles, not feet. Each biped foot has a separate passive plate, heel, and toe, and only explicit semantic foot nodes receive traction or foot-support classification.

### WALK-DUCK-025 — Obstacle-conditioned duck, clear, and recover lesson
**Status:** IMPLEMENTED — REVALIDATION PENDING

The second lesson presents a moving low bar. Qualification requires lowering the head with planted semantic feet, clearing the bar, and returning to a stable stance; permanent crouching and unrelated joint motion do not qualify.

### WALK-SAMPLE-026 — Current-frame training-sample integrity
**Status:** IMPLEMENTED — REVALIDATION PENDING

A rollout that was valid earlier cannot be displayed while its current frame is collapsed, body-supported, or otherwise incompatible with the active lesson. The PIP is empty until a currently displayable stage-qualified frame exists.

### WALK-LAUNCH-027 — Reject stale source-tree executables and v0.7.1 state
**Status:** IMPLEMENTED — REVALIDATION PENDING

Source-tree `run.bat` prefers the current Release build and cannot silently launch an old root executable. v0.7.2 uses new checkpoint, autosave, rig, and autonomy-state semantics and paths.

## Training-quality correction

### WALK-MOTOR-012 — Reciprocal parent-side motor reaction
**Status:** VERIFIED

Every motor divides angular correction between the driven subtree and the complete remaining body using rotational inertia. The joint pivot and parent body are not world anchors. Internal correction preserves whole-body center of mass. Direct tests require chest motion, pivot motion, driven-side dominance, and bounded center-of-mass drift.

### WALK-OBS-018 — Non-overlapping eight-motor observation layout
**Status:** VERIFIED

The humanoid uses forty observation channels: eight motor angles, eight motor velocities, contacts, foot placement, terrain, obstacle, stage, and phase state. Shoulder and elbow channels no longer overwrite leg velocity, contact, or foot-position inputs. Compile-time layout checks and deterministic tests protect the channel boundaries.

### WALK-TRAIN-013 — Reject collapsed poses as training success
**Status:** IMPLEMENTED — REVALIDATION PENDING

Standing qualification requires a retained sustained upright stance, continuous foot support with bounded contact hysteresis, valid head and torso height, no non-foot support, bounded slip, bounded vertical motion, and bounded joint speed. Collapsed, body-contact, violent-joint, rolling, skating, hovering, motionless, and prerequisite-incomplete candidates cannot become the best policy, rollback anchor, evolved-rig seed, imitation source, or displayed training sample.

### WALK-CURR-014 — Evidence-gated ordered curriculum
**Status:** IMPLEMENTED — REVALIDATION PENDING

The curriculum advances only through retained prerequisite evidence: stand and recover; duck and return to stand; powered takeoff and upright landing; alternating supported walking and running; moving duck or jump; one-to-three controlled spins and upright landing; then mixed traversal. Scalar reward cannot bypass missing evidence.

### WALK-BEST-015 — Stage-valid best-policy and imitation selection
**Status:** IMPLEMENTED — REVALIDATION PENDING

Best-policy selection is lexicographic: stage validity and evidence quality first, scalar reward only as a tie-break. Evaluation, champion rollback, rig evolution, self-imitation, and PIP representative selection use the same qualification predicate. A high-reward invalid pose loses to a lower-reward valid controller.

### WALK-EVAL-019 — Latched robust balance evaluation
**Status:** VERIFIED

A balance rollout latches success when it completes a three-second stage-valid stand; it is not forced to continue until a later fall erases completed lesson evidence. The deterministic evaluation set contains six perturbed starts and requires at least four valid successes. Early collapse, body contact, flipping, or failure to establish a valid stand still fails the seed. Later stages remain strict with zero invalid evaluation runs.

### WALK-CTRL-020 — Shared effective controller path
**Status:** IMPLEMENTED — REVALIDATION PENDING

PPO rollout collection, deterministic evaluation, self-imitation replay, live preview, and displayed best-policy execution use the same effective balance controller. The standing stabilizer is not hidden training-only shaping. Motor targets use neutral-target velocity damping rather than incorrect torque-style angle feedback.

### WALK-STATE-016 — Invalidate incompatible learned state
**Status:** IMPLEMENTED — REVALIDATION PENDING

Training checkpoints carry the v0.7.1 semantics signature and new checkpoint format. Incompatible v0.7.0 optimizer, curriculum, best-policy, rig-evolution, and imitation state is rejected. Autosave paths and autonomy-state format are isolated for v0.7.1, and the UI reports incompatibility instead of silently resuming stale behavior.

### WALK-RUNTIME-017 — Bounded packaged-runtime acceptance
**Status:** IMPLEMENTED — REVALIDATION PENDING

The release gate builds the packaged Windows application and runs deterministic acceptance proving that standing acquisition retains a stage-valid champion within forty PPO updates, at least four of six perturbed evaluation starts complete the standing lesson, an adversarial collapsed pose is rejected, and the shared balance controller survives real physics rather than injected evidence. UI telemetry exposes stance duration, retained longest stance, recovery evidence, quality key, and rejection reason. The PIP remains empty until a stage-valid representative rollout exists.

### WALK-LAUNCH-021 — Executable-relative clean-folder launch
**Status:** IMPLEMENTED — REVALIDATION PENDING

EpochRunner resolves shaders and assets from `SDL_GetBasePath()` rather than the caller's working directory. Visual Studio launches from the executable directory. The build tree and installed release folder both pass `--diagnose-package` when invoked from an unrelated directory, and the package includes the required shaders, assets, and runtime DLLs.

## Runtime architecture

### WALK-CORO-001 — Meaningful C++23 training pipeline
**Status:** VERIFIED

Queued commands, rollout collection, advantage computation, parallel gradient generation, deterministic reduction and optimizer application, evaluation and curriculum, immutable publication, asynchronous persistence, and throttle/yield remain observable coroutine stages.

### WALK-ARCH-001 — Worker-owned mutable trainer state
**Status:** VERIFIED

The trainer worker exclusively owns mutable PPO, optimizer, curriculum, rig-evolution, and checkpoint state. UI operations use coalesced commands and immutable snapshots.

### WALK-IO-001 — Asynchronous checkpoint and autosave
**Status:** VERIFIED

Immutable checkpoint, rig, and state snapshots are coalesced and written by a dedicated `std::jthread` using temporary-file plus atomic-rename publication.

### WALK-TEST-001 — Cross-platform concurrency regression suite
**Status:** VERIFIED

Coverage retains hip editing, preset swapping under NORMAL and MAX CPU load, speed-mode switching, pause and single-step, checkpoint operations under load, deterministic staged updates, cancellation, persistence coalescing, Linux release validation, and Windows full-application validation.

## Ordered movement curriculum

### WALK-SKILL-008 — Ordered reusable skills
**Status:** IMPLEMENTED — REVALIDATION PENDING

Stand, duck and recover, jump and land, walk and run, moving duck and jump, controlled flips, and mixed traversal are taught and validated in prerequisite order. Hazard contact remains physical and legal; passing the hazard is the goal. Hovering, unpowered sustained flight, more than three spins, ground rolling, body surfing, planted-foot skating, and wheel sliding remain invalid.

### WALK-ARMS-009 — Humanoid arms for balance and acrobatics
**Status:** VERIFIED

The humanoid retains independent shoulder and elbow motors across eight policy outputs. Reciprocal motor reaction prevents the shared chest or shoulder pivot from acting as a fixed anchor, and all arm state has dedicated observation channels.

### WALK-LEARN-010 — Faster learning without regression
**Status:** IMPLEMENTED — REVALIDATION PENDING

The standing stabilizer supplies a shared, physically executed balance prior while PPO learns the residual policy. Gait bootstrap, action smoothing, anti-skating gates, skill-specific learning-rate reset, evidence-first champion anchoring, rollback, and bounded self-imitation remain active without allowing reward exploits.

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
**Status:** IMPLEMENTED — REVALIDATION PENDING

Reward is subordinate to stage-valid evidence. Obstacle geometry and motion remain observable, and invalid posture cannot be promoted by scalar reward.

### WALK-PHYS-001 — Semantic support, traction, and world-anchored debris
**Status:** IMPLEMENTED — REVALIDATION PENDING

### WALK-COURSE-002 — Shared mile-marker obstacle schedule
**Status:** VERIFIED

### WALK-GAIT-002 — Alternating stepping instead of wheel sliding
**Status:** VERIFIED

Walking qualification requires real alternating supported steps and positive progress; wheel sliding cannot qualify.

### WALK-SAND-001 — Sand-simulation enemy curriculum
**Status:** VERIFIED

### WALK-ROLL-003 — Head, tail, torso, and foot-node rolling rejection
**Status:** IMPLEMENTED — REVALIDATION PENDING

Rolling and non-foot support participate in the shared qualification predicate and cannot seed best-policy or imitation state.

### WALK-HAZARD-003 — Hazards are never pickups or contact rewards
**Status:** VERIFIED

### WALK-LOCO-004 — Biped, quadruped, crawler, and hexapod support
**Status:** VERIFIED

Cross-rig deterministic tests protect biped, humanoid, quadruped, crawler, hexapod, chicken, and monoped presets after the complete-body reciprocal motor partition and observation-layout expansion.

### WALK-IDLE-005 — Zero-progress reset
**Status:** VERIFIED

Motionless candidates are reset and excluded from elite selection.

### WALK-GUIDE-006 — Automatic best-result imitation prior
**Status:** IMPLEMENTED — REVALIDATION PENDING

Only stage-valid trajectories with clean frames can enter the imitation prior; quality evidence outranks reward.

## UI and release evidence

### WALK-UI-002 / WALK-UI-003 — Responsive readable telemetry
**Status:** VERIFIED

Existing layout checks remain. Training telemetry exposes current and longest sustained stance, duck recovery, evidence quality, and the primary rejection reason.

### WALK-PIP-007 — Actual valid worker-rollout picture-in-picture
**Status:** IMPLEMENTED — REVALIDATION PENDING

The PIP displays only a stage-qualified representative worker rollout. It shows `NO STAGE-VALID ROLLOUT YET` instead of presenting a failed sample as success.

### WALK-REL-011 — Historical v0.7.0 release evidence
**Status:** VERIFIED

The v0.7.0 build and test evidence remains historical, but its training-quality conclusion was superseded by Adam's August 1, 2026 packaged-runtime screenshot and corrected in v0.7.1.

### WALK-REL-013 — Verified v0.7.1 training-quality hotfix
**Status:** VERIFIED

- Exact tested source commit: `843d387bc0cf5660af9414cf408f5054eb88654f`;
- workflow run: `30724619700`;
- Linux GCC 14 C++23 build and tests: passed;
- Windows 2025 full SDL3, Vulkan, and EpochGui build and tests: passed;
- forty-channel eight-motor observation layout: passed;
- robust four-of-six perturbed standing evaluation: passed;
- bounded forty-update stage-valid champion acquisition: passed;
- adversarial collapsed-pose rejection: passed;
- complete-body reciprocal motor reaction and center-of-mass preservation: passed;
- executable version, Vulkan diagnostic, and unrelated-working-directory package launch: passed;
- Windows package, including `run.bat`: `EpochRunner-v0.7.1-windows-x64.zip`;
- package SHA-256: `00B093F0794777F8BE8624A69DBB8F257986E6522BD2819AE5745257FF974562`;
- remaining branches: `main`;
- open pull requests: `0`.
