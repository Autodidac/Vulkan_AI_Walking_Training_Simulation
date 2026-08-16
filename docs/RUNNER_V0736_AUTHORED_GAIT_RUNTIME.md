# Runner v0.7.36 authored gait and bounded runtime

Runner v0.7.36 closes the runtime defects visible in the 2026-08-15 v0.7.35 screenshots and promotes the user's edited `creature.rig` (SHA-256 `C7A6CBAB58E945BCC70ADA3ED59170B522B47CAEFBC2CBAEA149349862A46DAD`) as the canonical humanoid rest shape.

## Authored rest, topology-driven gait

The humanoid head and both complete arm chains now start at the supplied side-view coordinates. Manipulator discovery follows graph topology rather than action-slot assumptions. Arms solve around their authored hand endpoints: zero phase is the exact resting pose beside the torso/pelvis, and the two arms add a small opposed fore/aft swing counter to the established opposed leg cycle. Exploration and post-teacher policy output keep non-support motors bounded, preventing a balancing T-pose from replacing authored rest. Support branches retain their full policy authority so bipeds, quadrupeds, crawlers, hexapods, monopeds, edited rigs, and evolved appendages remain physically trainable.

The leg cycle keeps the proven anatomy-scaled flexed target and single sagittal knee direction. The two feet follow opposed support/swing phases; whole-rig facing mirrors knee/elbow presentation and locomotion together.

## Lesson ownership and stable terrain lifecycle

`3. WALK / RUN` is forward gait only. A new logical `4. BACK / TURN / RETURN` lesson owns the bounded shuttle, backup, stationary turn, whole-rig flip, and return requirements. It uses stable ground so direction changes are learned before hazard traversal. Later deformable lessons continue to own dry sand, waterlogged material, water, holes, falling material, excavation, stacking, and moving-block safety.

An ordinary episode retry resets the rig and transient hazards but preserves the active terrain field, its seed, deposited sand/dirt, holes, and material ownership. An explicit lesson, rig, or fresh-course boundary creates a new deterministic field. Collision, rendering, labels, and diagnostics continue to sample that same field.

## Bounded weapon engagement

Every weapon has a minimum range, maximum range, aim tolerance, cooldown, and lesson hit goal. The simulation itself rejects a trigger outside that window, during an unsafe granular event, after goal completion, or while no target is active. Sidearm, carbine, and launcher targets are authored inside their class range; target completion deactivates the target, so a held trigger cannot fire forever. The policy teacher uses the same gate as runtime physics.

## Persistence and proof

Training semantics are `0x0007'3601`, checkpoint identity is `EPPO36`, and automatic files are `runner-v0736-authored-*` (autonomy state v18 with safe v16/v17 reads). A v0.7.35 automatic checkpoint can contribute validated lifetime totals when no current v0.7.36 save exists; incompatible policy, optimizer, rig champion, and mastery state do not silently resume.

`Runner.V0736AuthoredRuntime` covers the exact supplied rig, authored-rest endpoints, opposed arm/leg motion, every canonical topology, stable shuttle ownership, persistent terrain across repeated seeded retries, explicit terrain replacement, all weapon range boundaries, aim rejection, hit-goal stop, and held-trigger negative behavior. Existing fixed-step terrain and shuttle suites retain repeated-seed and 20/60/240 Hz coverage.