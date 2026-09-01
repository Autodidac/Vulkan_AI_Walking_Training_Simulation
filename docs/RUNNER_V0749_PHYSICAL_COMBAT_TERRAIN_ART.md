# Runner v0.7.49 physical combat, terrain, and extremity art

## Physical world authority

The training world is stationary. `set_course_motion_enabled(true)` cannot enable a conveyor, `course_speed()` and `course_progress()` remain zero, and distance/step evidence comes from articulated world-space displacement and contact. Terrain, obstacles, projectiles, particles, markers, collision, and rendering all use the same world coordinate.

The material course owns one exact 640x360 authored footprint of canonical 16-byte SandHybrid `SceneCell` state. It is mapped to district 0 of the compact 5120x1440 world at the authoritative global surface Y. Global cell X/Y are shown in Live and Advanced diagnostics. This is an integration/training footprint, not a fabricated claim that Runner contains SandHybrid's complete Vulkan `reset.comp` world.

Fine material cells are rendered directly over the sampled structural substrate. Ground pressure and deposited material mutate the same storage used by collision. Reference plaques attach to their world positions at the ground line; material names belong in telemetry rather than floating over the simulation.

## Physical equipment state graph

A firearm follows this state sequence:

1. `SAFE CARRY`: locomotion is allowed and no trigger can fire.
2. `GUN STANCE`: the controller brakes through ordinary body actions until measured speed is below the stop gate.
3. `AIMING`: the arm chains move the authored mount/barrel; a scalar request is not aim authority.
4. `READY`: actual barrel error, angular rate, settle time, support, target, hazard, range, and cooldown gates all pass.
5. `FIRE`: the shot inherits the physical muzzle position and barrel direction.

Projectiles follow their launch velocity and gravity. Collision is swept across the complete per-tick segment so a fast shot cannot tunnel through a target. A miss is recorded when it crosses the target plane or reaches ground/range; its signed vertical error applies a bounded correction to the next commanded arm pose only. No in-flight steering, hit snapping, target teleport, or root translation exists.

The same teacher/action path is used by autonomous NPC training and player-guided control. Player input can request movement, stance, aim, and trigger intent, but the physical gates remain authoritative.

## Human extremity art

Upper-arm and forearm thickness are derived from their own segment spans. Joint overlap is bounded by adjacent physical radii. Hand art receives the real forearm span and cannot inflate to the torso assembly envelope. Boot art is fitted from the heel/toe plate, lower-leg span, joint radius, contact state, and physical sole direction, keeping the visible sole on the contact plane without hiding the ankle articulation.

## Diagnostics

Speed Walk shows its exact mastery evidence rather than a generic 80-percent bar: current/required distance, steps, speed, survival, collisions, and confirmation count. Advanced diagnostics show stance slip, static-world global cell, gait cycles, equipment state, stopped status, commanded versus actual aim, settle time, and misses.

After the Human handoff, a deterministic cohort ramps from one to all eight Raw rollout environments over 300 updates. Fresh policy evaluations inherit the selected gait and use six Raw/static-world seeds. Walk retention continues to prioritize physical step rhythm against sliding; Speed Walk, transition, and run rank worst-seed translation and speed ahead of cadence so rapid in-place shuffling cannot own the retained controller. `--diagnose-speed-walk` executes the exact retained Walk to Speed Walk transfer and refuses assisted evaluation, course motion, or partial mastery.
Final mastery confirmations restore and independently replay the exact retained Speed Walk champion. Exploratory weights that drift after retention cannot own or erase the final 20 percent. Headless Vulkan diagnostics recognize XCB/Xlib presentation-surface absence while still rejecting a missing loader, broken shader, or package fault.


## Deterministic coverage

`Runner.V0749PhysicalCombatTerrain` covers:

- exact SandHybrid footprint/world geometry and canonical cell ABI;
- a negative attempt to enable course motion;
- positive, negative, and adversarial swept collisions;
- deterministic signed miss correction;
- stopped/unsupported aim gates and scalar-only aim cheating;
- repeated-seed `gun stance -> aiming -> ready -> physical hit` behavior;
- finite, bone-bounded arm, hand, and boot dimensions.

The existing contact-led gait, deformable terrain, species art, authored runtime, Rig Lab, camera, package, and acceptance suites remain required. A release is not accepted until the packaged Windows/Linux examples and installed SDK consumers pass independently and direct runtime captures pass visual review.
