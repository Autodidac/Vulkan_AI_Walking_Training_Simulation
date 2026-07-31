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

## Current warning

EpochRunner v0.5.0 passed its Windows build, concurrency tests, speed-mode benchmark, Vulkan diagnostic, and packaging gate. Remaining ACTIVE/OPEN missions carry forward unchanged.
