# EpochRunner v0.7 Runtime Master Plan

This plan closes the remaining runtime missions without regressing the verified v0.6 obstacle course or v0.6.1 observation hardening.

## Release-blocking missions

### WALK-CORO-001 — Useful C++23 coroutine pipeline

Replace the current three-yield wrapper around `train_one_update()` with explicit stages:

1. apply/coalesce commands,
2. dispatch rollout workers,
3. suspend until rollout completion,
4. compute advantages,
5. dispatch gradient batch,
6. suspend until gradient completion,
7. deterministic reduction and Adam update,
8. repeat bounded minibatches/epochs,
9. dispatch deterministic evaluation when due,
10. suspend until evaluation completion,
11. curriculum/evolution,
12. publish immutable snapshot,
13. enqueue persistence snapshot,
14. throttle/cancel.

Every suspended stage must be visible in diagnostics. The training owner thread must sleep while persistent workers run instead of polling or spinning.

### WALK-ARCH-001 — Worker-owned mutable state

`PpoTrainer worker_` becomes single-owner state used only by the training thread. UI operations communicate through coalescing commands and immutable snapshots. No UI-facing call may wait for a mutex held across rollout collection, optimizer work, evaluation, rig evolution, or disk I/O.

### WALK-IO-001 — Asynchronous checkpoint and autosave

Export an immutable checkpoint payload after publication. A persistent I/O `std::jthread` writes checkpoint, rig, and autonomy state using temporary files and atomic replacement. Newer autosave payloads coalesce older pending payloads. Training never waits for disk latency.

### WALK-TEST-001 — Concurrency regression suite

Required automated coverage:

- repeated hip edits during NORMAL and MAX CPU,
- preset swaps while rollout and optimizer workers are active,
- rapid speed-mode switching,
- pause, single update, and background resume,
- checkpoint save during training,
- autosave coalescing,
- clean stop-token cancellation from every suspended pipeline stage,
- deterministic update comparison,
- pipeline-stage telemetry progression,
- ThreadSanitizer-capable Linux CPU configuration.

## Architecture

```text
UI/render thread
  command queue -> training owner
  immutable published snapshot <- training owner

training owner jthread + C++23 coroutine
  commands
  rollout dispatch / suspend
  advantage calculation
  optimizer dispatch / suspend / deterministic reduction
  evaluation dispatch / suspend
  curriculum and snapshot publication
  persistence enqueue

persistent rollout/gradient/evaluation workers
  no transient thread creation
  worker-local scratch
  completion generation + condition variable

persistent I/O jthread
  immutable payload only
  latest-payload coalescing
  temp file + atomic rename
```

## Acceptance gates

- Visual Studio 2026 Release build with warnings-as-errors.
- Linux GCC/Clang CPU-only test build.
- All deterministic, obstacle, recovery, hip-edit, pipeline, and throughput tests pass.
- No UI-facing trainer call blocks beyond one frame budget during active training.
- NORMAL, FASTER, and MAX CPU retain measured throughput differences.
- Shutdown succeeds from every coroutine suspension point.
- Autosave latency does not reduce rollout throughput or pause input.
- Exact source commit, package checksum, and release evidence are recorded.
