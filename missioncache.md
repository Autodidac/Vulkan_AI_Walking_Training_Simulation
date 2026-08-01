# EpochRunner mission cache

This is the authoritative release ledger. Nothing is removed until it is implemented, tested, and linked to exact release evidence.

## Release target

**Target:** EpochRunner v0.7.0

**Release state:** VERIFIED — EpochRunner v0.7.0 published

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
**Status:** VERIFIED

The curriculum teaches and validates in prerequisite order:

1. stand and recover balance,
2. duck and return to standing,
3. joint-powered jump and upright landing,
4. walk and run with alternating supported steps,
5. duck or jump while moving,
6. controlled airborne flips with upright landing,
7. mixed-goal traversal combining the learned skills.

Hazard contact is legal and physical. Contact creates a bounded event penalty but is not terminal by itself. Passing the hazard earns progress. Joint-powered airtime is bounded by lesson and launch evidence. Up to three airborne spins are allowed in flip-capable lessons; a fourth spin, hovering, unpowered sustained flight, ground rolling, body surfing, planted-foot skating, and wheel sliding remain invalid.

### WALK-ARMS-009 — Humanoid arms for balance and acrobatics
**Status:** VERIFIED

The humanoid has two articulated arms with independently controllable shoulder and elbow motors. Policy and checkpoint dimensions safely support eight outputs, while four-motor creatures retain an explicit active-motor count. Arms render and simulate as ordinary body segments, and tests verify arm actions without corrupting biped, quadruped, crawler, hexapod, chicken, or monoped rigs.

### WALK-LEARN-010 — Faster learning without regression
**Status:** VERIFIED

Phase-guided gait bootstrap, action smoothing, explicit supported-step evidence, anti-skating gates, per-skill learning-rate reset, best-policy anchoring, regression rollback, and bounded best-result self-imitation are retained. Deterministic evaluation and curriculum advancement use duck, jump, landing, spin, obstacle-pass, and supported-gait evidence.

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
**Status:** VERIFIED

### WALK-PHYS-001 — Semantic support, traction, and world-anchored debris
**Status:** VERIFIED

### WALK-COURSE-002 — Shared mile-marker obstacle schedule
**Status:** VERIFIED

### WALK-GAIT-002 — Alternating stepping instead of wheel sliding
**Status:** VERIFIED

### WALK-SAND-001 — Sand-simulation enemy curriculum
**Status:** VERIFIED

### WALK-ROLL-003 — Head, tail, torso, and foot-node rolling rejection
**Status:** VERIFIED

### WALK-HAZARD-003 — Hazards are never pickups or contact rewards
**Status:** VERIFIED

### WALK-LOCO-004 — Biped, quadruped, crawler, and hexapod support
**Status:** VERIFIED

### WALK-IDLE-005 — Zero-progress reset
**Status:** VERIFIED

### WALK-GUIDE-006 — Automatic best-result imitation prior
**Status:** VERIFIED

## UI and release evidence

### WALK-UI-002 / WALK-UI-003 — Responsive readable telemetry
**Status:** VERIFIED

Deterministic layout checks enforce supported minimum dimensions, title/tab separation, side-panel width, status bounds, and PIP/control non-overlap. Packaged visual preference review remains optional rather than release-blocking.

### WALK-PIP-007 — Actual worker-rollout picture-in-picture
**Status:** VERIFIED

The PIP uses an immutable representative worker rollout. Deterministic bounds prove it remains inside the world viewport and does not cover the control panel or primary telemetry.

### WALK-REL-011 — Verified v0.7.0 release
**Status:** VERIFIED

Completed pre-release evidence:

- Linux C++23 release build and deterministic/runtime tests: passed in workflow `30685177669`;
- Linux ThreadSanitizer runtime-pipeline run: passed in workflow `30685177669`;
- Windows 2025 full SDL3/Vulkan/EpochGui Release configure and build: passed in workflow `30685365960`;
- Windows deterministic core, runtime pipeline, concurrency, persistence, arm, and UI tests: passed;
- executable version and Vulkan diagnostic: passed;
- exact Windows-tested source commit recorded in `validation/v0.7.0-windows-premerge.md`.

Final release evidence is recorded in `validation/v0.7.0.md`. Repository branch and pull-request cleanup is performed immediately after publication.
