# Runner v0.7.28 physical material course completion

## Screenshot-driven correction

The v0.7.27 Walk / Run screenshot is the acceptance reference for this pass. It showed thrown-object hazards at launch, three overlapping labels, decorative layered ground with no legible sand/water/hole behavior, zero features cleared, and only a raw score plus hexadecimal rejection mask. v0.7.28 treats that visible result as a contract failure.

The shared root cause was broader than presentation: every simulated falling sand grain had been copied into the authored obstacle list, the first event timer began near launch, terrain-relative conveyor progress could unlock pressure without a gait, and the renderer drew deep material strata rather than the physical contact surface.

## Physical course contract

Walk / Run now begins with a protected 10–12 m firm runway. A deterministic seed then varies contiguous regions of firm ground, dry deformable sand, waterlogged ground, shallow water, and recoverable holes. Fine cells own the contact height and material; water depth owns buoyancy and drag; holes are real negative geometry. Idle terrain preserves volume, while deposits and support pressure alter only physical cells.

Basic Walk / Run has no authored rocks, bars, projectiles, or falling material. Advanced moving-hazard and combat lessons may apply material pressure only after both 8–12 m of course travel and at least two real gait cycles. Ambient grains remain ambient particles, are rendered without hazard labels, and only rocks/debris collide as hazards.

The renderer draws near-surface cells, the actual water column/line, and bounded unique callouts. Advanced diagnostics name every rejection-mask reason, retain the concrete invalid-motion reason, and report the current material region, water depth/submersion, nearest feature, and distance.

`Runner.exe --course-eye-test` is the reproducible packaged visual gate. It freezes a seeded production Walk / Run environment at the start line, disables training mutation, holds a release-size wide view, and labels the protected firm runway followed by dry deformable sand, waterlogged sand, shallow water, and the physical hole. The start PIP also states the 8-12 m plus two-real-gait-cycle pressure gate, so an early object or duplicate callout is directly visible.

## Carried mission completion

The climb lesson authors a reachable fixed ledge after the approach runway. Powered arm endpoints are explicit manipulators: a hand must contact/grasp the edge, a support must transfer to the top without powered takeoff, a later regrasp begins backward descent, and a controlled feet-first lower-ground contact completes it. Ascent and descent cannot be credited in the same step.

Equipment is a separate optional controller extension with unarmed, safe-carry, ready, disarmed, and dropped states; sidearm, carbine, and launcher profiles; deterministic varied-distance targets; aim, cooldown, recoil, ballistic projectiles, hits, pickup/drop/disarm, and separate target/combat lessons. Anatomy remains eight topology-derived motor actions. Equipment adds three masked outputs and ten observations; when disabled, those outputs consume no exploration RNG, loss, entropy, or gradients.

## Compatibility and time contract

The network expands from 50 observations/8 actions to 60/11 without rerolling the v0.7.27 anatomy stream. Authentic EPPO28 checkpoints are resume-blocked but can be explicitly transferred: every old first-layer, hidden, anatomy actor, value, and exploration parameter is copied exactly; new observation weights and equipment actor outputs start neutral; optimizer, best, and mastery state reset.

Simulation velocities use the actual fixed-step delta, damping is exponential in elapsed time, and material impulses, water response, projectiles, climb transitions, equipment cooldowns, and preview accumulation are elapsed-time correct. The course diagnostic proves exact full particles, terrain cells, features, material particles, water/burial state, climb evidence, equipment/targets/projectiles, and observations at 20, 60, and 240 render Hz across material, mixed-hazard, climb, equipment-target, and combat stages.

## Permanent evidence

`Runner.exe --diagnose-course` and `Runner.V0728CourseCompletion` cover:

- protected runway, unique authored marker identities, and no idle material pressure;
- all five material regions across eight deterministic seeds plus seed variation, water depth, and hole geometry;
- finite observation truth and optional-equipment identity;
- a positive post-runway material event and a negative idle control;
- physical grasp, climb, separate regrasp, support transfers, controlled descent, and no powered jump;
- equipment state transition, shot, target hit, repeated-run determinism, and 20/60/240 Hz equivalence;
- authentic EPPO28 transfer, exact anatomy mapping, neutral new channels, reset state, and truncated-input rejection.

Release completion additionally requires all Linux GCC 14 warnings-as-errors tests, the complete Windows SDL3/Vulkan build and tests, every build-tree/installed/extracted diagnostic, unrelated-directory `run.bat`, manifest/checksum audits, and published-asset byte comparison.