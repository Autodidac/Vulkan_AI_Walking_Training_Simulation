# EpochRunner mission cache

This is the authoritative release ledger. Nothing is removed until it is implemented, tested, and linked to exact release evidence.

## Release target

**Target:** EpochRunner v0.7.0

**Release state:** ACTIVE — completion pass in progress

## Runtime architecture

### WALK-CORO-001 — Meaningful C++23 training pipeline
**Status:** ACTIVE

Split one update into observable coroutine stages: queued commands, rollout collection, advantage computation, parallel gradient generation, deterministic reduction/optimizer application, evaluation/curriculum/evolution, immutable publication, asynchronous persistence scheduling, and throttle/yield. Diagnostics must expose the current stage.

### WALK-ARCH-001 — Worker-owned mutable trainer state
**Status:** ACTIVE

The trainer worker exclusively owns mutable PPO, optimizer, curriculum, rig-evolution, and checkpoint-application state. UI operations use coalesced commands and immutable snapshots. No UI-facing call may contend on a mutex held across a PPO update or deterministic evaluation.

### WALK-IO-001 — Asynchronous checkpoint and autosave
**Status:** ACTIVE

Copy immutable checkpoint/rig/state data at a worker publication boundary, coalesce pending saves, write on a dedicated `std::jthread`, and publish through temporary-file plus atomic rename. Save/load requests made during training must return promptly and may not perform disk I/O on the input/render thread.

### WALK-TEST-001 — Cross-platform concurrency regression suite
**Status:** ACTIVE

Cover hip dragging and preset swapping during NORMAL and MAX CPU training, speed-mode switching, pause/single-step/resume, checkpoint save/load under load, clean cancellation, deterministic staged-update comparison, asynchronous persistence coalescing, and a Linux ThreadSanitizer build/run.

## Ordered movement curriculum

### WALK-SKILL-008 — Ordered reusable skills
**Status:** ACTIVE

Teach and validate in prerequisite order:

1. stand and recover balance,
2. duck and return to standing,
3. joint-powered jump and upright landing,
4. walk and run with alternating supported steps,
5. duck or jump while moving,
6. controlled airborne flips with upright landing,
7. mixed-goal traversal combining the learned skills.

Hazard contact is legal and physical. Contact creates a bounded event penalty but is not terminal by itself. Passing the hazard is the goal and earns progress. Joint-powered airtime is bounded by lesson and launch evidence. Up to three airborne spins are allowed in flip-capable lessons; a fourth spin, hovering, unpowered sustained flight, ground rolling, body surfing, planted-foot skating, and wheel sliding remain invalid.

### WALK-ARMS-009 — Humanoid arms for balance and acrobatics
**Status:** ACTIVE

Give the humanoid two articulated arms with independently controllable shoulder and elbow motors. Expand policy/checkpoint dimensions safely, preserve four-motor creatures through an active-motor count, render and simulate the arms as ordinary body segments, and test that arm actions affect the humanoid without corrupting biped, quadruped, crawler, hexapod, chicken, or monoped rigs.

### WALK-LEARN-010 — Faster learning without regression
**Status:** ACTIVE

Retain phase-guided gait bootstrap, action smoothing, explicit supported-step evidence, anti-skating gates, per-skill learning-rate reset, best-policy anchoring, regression rollback, and bounded best-result self-imitation. Deterministic evaluation and curriculum advancement must use duck, jump, landing, spin, obstacle-pass, and supported-gait evidence.

## Existing locomotion and course requirements

### WALK-RIG-001 — Nonblocking hip/joint editing
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-CONC-001 — Persistent CPU parallelism
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-UI-001 — Functional NORMAL/FASTER/MAX CPU controls
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-OPT-001 — Parallel PPO optimizer
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-COURSE-001 — Procedural obstacle and recovery treadmill
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-OBS-001 — Complete obstacle sensing and reward integrity
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-PHYS-001 — Semantic support, traction, and world-anchored debris
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-COURSE-002 — Shared mile-marker obstacle schedule
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-GAIT-002 — Alternating stepping instead of wheel sliding
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-SAND-001 — Sand-simulation enemy curriculum
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-ROLL-003 — Head, tail, torso, and foot-node rolling rejection
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-HAZARD-003 — Hazards are never pickups or contact rewards
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-LOCO-004 — Biped, quadruped, crawler, and hexapod support
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-IDLE-005 — Zero-progress reset
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

### WALK-GUIDE-006 — Automatic best-result imitation prior
**Status:** VERIFIED IN PRIOR RELEASE — REVALIDATE

## UI and release evidence

### WALK-UI-002 / WALK-UI-003 — Responsive readable telemetry
**Status:** ACTIVE

Replace subjective closure with deterministic layout checks: minimum font scale, title/tab separation, side-panel width, wrapped status bounds, and no PIP/control overlap at supported minimum window dimensions. Packaged visual review remains useful but is no longer the only evidence.

### WALK-PIP-007 — Actual worker-rollout picture-in-picture
**Status:** ACTIVE

Keep the immutable representative worker rollout. Add deterministic layout bounds proving it remains inside the world viewport and does not cover the control panel or primary telemetry.

### WALK-REL-011 — Verified v0.7.0 release
**Status:** BLOCKED BY ACTIVE MISSIONS

Required closure evidence:

- Windows 2025 full SDL3/Vulkan/EpochGui Release configure and build;
- Windows deterministic core, runtime pipeline, concurrency, and persistence tests;
- executable version and Vulkan diagnostic;
- Linux C++23 core/runtime tests;
- Linux ThreadSanitizer runtime-pipeline run;
- exact tested source commit recorded;
- packaged Windows x64 artifact and SHA-256 recorded;
- tag and GitHub release published from the exact tested source;
- all cache entries changed to `VERIFIED` or explicitly documented as user-only visual preference;
- work, diagnostic, release, and cleanup branches removed;
- zero open pull requests.
