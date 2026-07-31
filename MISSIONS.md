# EpochRunner Mission Ledger

This file is the release-blocking source of truth for the Vulkan AI walking trainer.

## Rules

- Missions remain `OPEN`, `ACTIVE`, `BLOCKED`, or `VERIFIED` until evidence closes them.
- A feature is not complete because a class, thread, coroutine, or button exists in source.
- Every release must link the implementation, tests, measured behavior, and exact source commit.
- Missed or deferred missions carry forward and may not be silently removed.

## WALK-RIG-001 — Hip/joint editing must never lock the UI

**Status:** VERIFIED

Human, chicken, biped, quadruped, and monoped joint/node edits must be queued and coalesced. The input/render thread must never wait for PPO rollout, optimizer work, deterministic evaluation, curriculum transitions, rig evolution, autosave, or checkpoint serialization.

**Acceptance:**

- Selecting, dragging, and releasing every hip and knee node remains responsive while MAX CPU training is active.
- The public edit call returns within one render frame budget.
- The trainer eventually applies the newest coalesced rig state.
- A stress test repeatedly edits hip nodes while training and detects no deadlock, long stall, data race, or invalid rig publication.

## WALK-CONC-001 — Real persistent CPU parallelism

**Status:** VERIFIED

- Keep persistent rollout workers.
- Parallelize independent environment simulation.
- Parallelize deterministic evaluation where safe.
- Parallelize PPO minibatch gradient generation using worker-local gradients followed by deterministic reduction.
- Reserve capacity for input/render and avoid transient thread creation.
- Report real worker count, utilization, rollout throughput, update throughput, and queue depth.

**Acceptance:** measured training throughput increases across NORMAL, FASTER, and MAX CPU modes on a multicore CPU without freezing the UI.

## WALK-CORO-001 — Useful C++23 coroutine pipeline

**Status:** ACTIVE

Coroutines must divide meaningful trainer stages rather than merely wrapping one serial update:

1. apply queued commands,
2. launch/await rollout collection,
3. compute advantages,
4. launch/await parallel gradient batches,
5. reduce/apply optimizer update,
6. evaluate/curriculum/evolution,
7. publish immutable snapshot,
8. schedule autosave,
9. throttle according to speed mode.

**Acceptance:** coroutine suspension points correspond to independently schedulable work and are visible in diagnostics.

## WALK-UI-001 — Functional speed controls

**Status:** VERIFIED

NORMAL, FASTER, and MAX CPU must change measured trainer duty cycle and throughput. They must not merely batch additional serial updates behind the same mutex.

Show:

- updates per second,
- environment steps per second,
- rollout worker count,
- CPU mode,
- pending commands,
- trainer busy/idle state.

**Acceptance:** automated control tests verify mode latching; runtime benchmark shows distinct throughput and CPU-use behavior for all modes.

## WALK-ARCH-001 — Remove coarse trainer mutex stalls

**Status:** ACTIVE

No mutex may be held across an entire PPO update, multi-agent evaluation, rig evolution, or disk autosave while UI-facing operations can request the same lock.

Use immutable snapshots, command queues, worker-owned mutable state, short publication locks, and asynchronous persistence.

## WALK-OPT-001 — Parallel PPO optimizer

**Status:** VERIFIED

The current rollout simulation is parallel but the policy-gradient minibatch loop is serial. Add worker-local policy/gradient scratch, partition minibatches, reduce gradients deterministically, and apply Adam once per reduced batch.

**Acceptance:** identical seed/config produces deterministic parameters within the documented floating-point tolerance, and profiler data shows optimizer work distributed across workers.

## WALK-IO-001 — Asynchronous checkpoint/autosave

**Status:** OPEN

Serialize immutable policy/optimizer/rig snapshots on an I/O task without holding the live trainer lock. Use temporary-file plus atomic rename semantics.

**Acceptance:** checkpoint saves do not create visible input stalls or pause rollout workers for disk latency.

## WALK-TEST-001 — Concurrency regression suite

**Status:** OPEN

Required fixtures:

- hip drag during NORMAL training,
- hip drag during MAX CPU training,
- repeated preset swaps during training,
- speed-mode switching under load,
- pause/single update/background resume,
- checkpoint save/load under load,
- clean cancellation and shutdown,
- deterministic update comparison,
- ThreadSanitizer-capable CPU test configuration.

## WALK-REL-001 — Verified v0.5 release

**Status:** VERIFIED

Do not publish the next release until:

- Windows Release build passes,
- core and concurrency tests pass,
- Vulkan diagnostic passes,
- hip-edit stress test passes,
- all speed modes show measured differences,
- packaged executable reports the correct version,
- release asset identifies the exact source commit.

## WALK-COURSE-001 — Procedural obstacle and recovery treadmill

**Status:** VERIFIED

The live and training environments must continuously expose movement even when a controller produces no forward translation. A bounded virtual course advances independently of the walker and remains synchronized with physics, observations, rendering, rewards, and diagnostics.

Required terrain classes:

- flat road,
- inclines,
- elevated plateaus,
- declines,
- rolling hills,
- rough and uneven ground.

Required physical obstacle classes:

- rocks and low ground clutter,
- hurdles,
- overhead and duck-under bars,
- moving hazards,
- thrown projectiles with visible trajectories and physical impulse.

Required visual references:

- moving road dashes,
- numbered metre markers,
- mile conversion on markers,
- obstacle labels and approach visibility,
- actual walker distance separated from virtual course distance.

Required recovery behavior:

- collisions or major balance loss open a bounded recovery window,
- improving uprightness earns incremental reward,
- regaining supported upright balance earns a recovery bonus,
- falling or timing out applies a recovery penalty,
- live telemetry shows recovery-active state, attempts, and successes.

**Acceptance:**

- Course progress advances while the creature remains stationary.
- Terrain movement remains continuous across incline, plateau, decline, hill, and uneven sections.
- Every requested obstacle class is generated procedurally and represented in observations.
- Projectile and moving-hazard collisions apply bounded physical disturbance.
- Road markers make virtual movement obvious without faking actual walker distance.
- Recovery reward/state remains finite and deterministic.
- Core tests, full Windows build, Vulkan diagnostic, packaging, and exact release checksum pass before this mission becomes `VERIFIED`.


## WALK-OBS-001 — Complete obstacle sensing and recovery reward integrity

**Status:** VERIFIED

Every physical obstacle class must expose its actual collision size to the policy. Radial rocks, projectiles, and moving hazards use radius; hurdles and overhead bars use rectangular extent. Harmless upright contact must not open a rewardable recovery event.

**Acceptance:**

- Rock, projectile, and moving-hazard radius is present in observations.
- Hurdle and overhead-bar extent remains present in observations.
- Destabilizing impacts and major balance loss start recovery.
- Harmless upright contact cannot farm recovery bonuses.
- Hard falls remain terminal.
- Full Windows/Vulkan build, deterministic tests, diagnostics, package, and checksum pass.

## Current warning

EpochRunner v0.6.1 passed the full Windows/Vulkan build, complete obstacle observation tests, recovery reward-integrity tests, concurrency benchmark, runtime diagnostics, and package gate. Remaining ACTIVE/OPEN missions carry forward unchanged.
