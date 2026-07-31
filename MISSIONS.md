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

## WALK-PHYS-001 — Biped support, traction, and world-anchored debris

**Status:** ACTIVE

Passive heel/toe geometry must participate in semantic left/right support. Designated feet require usable traction, while incidental head, tail, and body contacts must not pin the creature. Procedural rocks, hazards, and debris must remain in course/world coordinates and may not inherit actor translation.

**Acceptance:**

- Passive biped heel/toe contacts drive support observations, gait validation, airborne checks, rewards, and recovery.
- Foot contact retains substantially less horizontal velocity than incidental body contact.
- Head, tail, and torso ground contact slide rather than becoming unintended brakes.
- A course feature's world position depends on its stable sequence and course progress, never root position.
- Incompatible v0.6.1 autosaves are not resumed automatically.
- Full Windows/Vulkan build, deterministic tests, Vulkan diagnostic, package, checksum, and exact-source evidence pass.

## WALK-UI-002 — Readable responsive telemetry

**Status:** ACTIVE

The live and rig interfaces must remain readable at ordinary desktop sizes. Text may not overlap adjacent labels, buttons, cards, or the title bar. Long status messages must wrap or fit within their panel instead of being clipped into neighboring content.

**Acceptance:**

- Increase the default bitmap-font scale and minimum fitted scale.
- Use a taller responsive title bar with non-overlapping tabs and subtitle.
- Give live and rig side panels enough width for their controls.
- Group live metrics into readable cards with larger vertical spacing.
- Wrap long trainer/status lines and fit world telemetry to viewport width.
- Full Windows/Vulkan build and executable diagnostics pass with the responsive layout.

## WALK-COURSE-002 — Mile-marker obstacle schedule

**Status:** VERIFIED

All generated course elements must be anchored to shared course/mile-marker coordinates rather than actor position. Rocks, hurdles, overhead bars, moving hazards, and thrown projectiles must each appear in the advanced lesson. Every lesson receives a visible safe runway before its first obstacle, while bounded virtual course speed keeps the next marker from taking excessive real time to arrive.

**Acceptance:**

- Marker rendering and obstacle generation use the same spacing constant.
- The first three markers form a 24-metre safe runway.
- Advanced lessons cycle rock, hurdle, overhead bar, moving hazard, and projectile on consecutive markers.
- Moving hazards oscillate around their marker; projectiles originate and arc around their marker.
- No obstacle position includes actor/root translation.
- Virtual course speed brings the first obstacle into view promptly without removing the safe runway.
- Full Windows/Vulkan build, deterministic schedule tests, diagnostics, package, and exact-source evidence pass.

## WALK-GAIT-002 — Real stepping instead of wheel sliding

**Status:** ACTIVE

Forward reward must represent foot-led alternating walking, not a body sliding across planted contacts. A knee may not clear a rock or hurdle before its corresponding foot. Sustained double-supported sliding is an invalid gait exploit.

**Acceptance:**

- Zero-step sliding receives no positive forward-progress multiplier.
- Alternating contact plus visible swing-foot clearance earns the strongest gait multiplier.
- Grounded foot-cluster slip is measured and penalized.
- Sustained double-supported root motion with slipping feet terminates as wheel sliding.
- Knee-before-foot traversal over rocks or hurdles receives a strong per-step penalty and increments telemetry.
- Foot-first traversal is not penalized.
- Full Windows/Vulkan build, deterministic gait tests, diagnostics, package, and exact-source evidence pass.

## WALK-SAND-001 — Sand-sim enemy locomotion curriculum

**Status:** ACTIVE

Retarget the curriculum from a generic treadmill demonstration to a grounded enemy controller suitable for later integration into a cellular sand simulation.

**Acceptance:**

- Spawn stance and flat patrol precede terrain and hazards.
- Long flat sections separate sand mounds and loose/deformed patches.
- Early debris and low-clearance hazards appear only on flat ground.
- Terrain-plus-hazard combinations unlock only in later combat traversal at higher difficulty.
- Deterministic evaluation actually runs long enough to encounter the first hazard.

## WALK-ROLL-003 — Head, tail, and body rolling are invalid locomotion

**Status:** ACTIVE

Non-foot body contact may slide during a fall but may not become a movement strategy.

**Acceptance:**

- Head contact cannot remain grounded long enough to propel the rig.
- Tail, torso, knee, and other non-foot ground contacts are detected semantically.
- Sustained body-ground rotation terminates as `HEAD / TAIL / BODY ROLLING`.
- Body-ground motion receives no gait progress multiplier and receives a strong penalty.
- A new autosave namespace prevents the v0.6.2 rolling policy from resuming.

## WALK-HAZARD-003 — Obstacles are hazards, never pickups or rewards

**Status:** ACTIVE

- Obstacles remain present through contact and are culled only after passing behind the actor.
- Ordinary obstacle contact cannot open a positive recovery-reward loop.
- Collision penalties exceed any incidental contact benefit.
- Hazard labels communicate danger rather than collectible/reward semantics.

## WALK-UI-003 — User-verified readable typography

**Status:** ACTIVE

The previous UI mission was incorrectly closed from compilation evidence without a visual acceptance pass. Increase all bitmap text substantially, enlarge minimum fitted text, marker signs, hazard labels, panels, and the default window. This mission remains active until the packaged application is visually confirmed readable by the user.

## Current warning

The user visually rejected the v0.6.2 typography and confirmed that rolling and pickup-like obstacle behavior remained. Those missions are reopened. v0.6.3 may record build and deterministic-test evidence, but visual readability remains `ACTIVE` until user confirmation. All other ACTIVE and OPEN missions carry forward unchanged.
