# Epoch2DWalkEngine v0.7.50 persistent trials, Director, and art authoring

## Physical traversal and trial truth

The course and creature remain in one fixed world frame. Every authored terrain interval has a stable challenge identity, and terminal previews retain the exact trial ID, terminal cause, world position, terrain material, water depth, and physical pose until an explicit retry or declared next-trial transition. A time limit is reported as `TIME LIMIT`; it is not mislabeled `VALID` and it does not silently reset the visible preview.

Support friction is finite. Each contact receives one material-, saturation-, normal-load-, and foot-geometry-aware Coulomb budget for the complete fixed tick, shared across all solver passes. A planted foot may slip and releases for swing; neither the root nor a support particle is locked to a rail. The repeated-seed lake gate requires Human, Chicken, Dog, and Hexapod to enter, traverse, contact the far shore, and exit the same authored basin with `used traction <= available traction` and valid motion.

Walk, Speed Walk, Walk/Run Transition, Run, Crouch Walk, Wade, Swim, and Shore Exit are separate task requests with separate physical envelopes. Ordinary wet ground remains a surface-traction problem; only measured immersion selects a water traversal mode.

## Physical equipment tasks and AI Director

Speed Walk retains its public 18 m goal. Qualification allows one 0.5 m terrain/contact integration interval so the same valid, 32-transfer physical replay is not reported as permanently 80% complete on one compiler. Its speed, cadence, survival, collision, and strict-valid checks are unchanged, and the controller still has no root translation, conveyor, infinite foot lock, or completion authority.

The Human's default carbine is real equipment with mass, a mount, joint-held orientation, ammunition, cooldown, projectile integration, and impact/miss feedback. The selectable equipment directives are Safe Carry Walk, Low Ready Walk, Stop and Plant, Gun Stance, Acquire and Aim, Fire and Correct, and Break Contact. Fire is rejected until the body is stopped, support is planted, the arm chain has settled the muzzle inside angular and angular-rate tolerances, and cooldown/ammunition permit a shot. A miss changes only the next aim request; an existing projectile is never steered.

`director::TaskGraph` is a stable-ID prerequisite graph used by curriculum, autonomous-NPC, and player-guided profiles. Selection reports requested-goal, unresolved-stall, repeated-failure, prerequisite-ready, graph-complete, or no-eligible-task reasoning. Construction and tool-traversal nodes are present but disabled until their real physical implementations exist. The Director chooses tasks only: it cannot translate a root, create a support, award a hit, or declare completion.

The installed library exports the same graph, task command, canonical pose/contact/material/equipment snapshot, control request, fixed-step result, and diagnostic event stream used by Runner. A headless consumer can drive a caller-owned `sim::Environment` without SDL, Vulkan, Runner UI state, or hidden filesystem authority.

## Shared art authoring

Rig Lab's ART page renders the production rig, pose, terrain, equipment, and modular pixels through the same renderer used by Live and PIP. It supports species/module selection; anchor-along and anchor-normal offsets; pivot, length, thickness, rotation, flips, and depth cue; skeleton/contact overlay; live or frozen physical pose; zoom and high-contrast background; undo/redo; reset; near-side selection; and validated package-relative species-owned save/load.

The accepted Human, Chicken, Dog, and Hexapod silhouettes remain the source art. Rendering fits opaque longitudinal bounds to the real physical attachment span, retains authored transverse thickness, seats terminal feet at physical contact, and keeps near/far branches readable. Human hands and the safe-carried weapon derive from the actual arm/manipulator chain rather than a cosmetic render-only pose.

## Release gates

`Runner.V0750PersistentDirectorArt` covers terminal retention, task selection, unsupported-task rejection, no-rail Director behavior, external-host stepping, stable challenge lookup, all-species lake traversal, traction budgets, equipment directives, physical fire sequencing, art transform bounds, history, and species-owned layout persistence. All prior contact, terrain, course, gait, weapon, species-art, UI, camera, package, and acceptance suites remain required.

Release acceptance additionally requires Linux GCC 14 warnings-as-errors and every CTest, the complete Windows SDL3/Vulkan build and every test, all build-tree and installed diagnostics, independently extracted launchers from unrelated working directories, five archive checksum and per-file-manifest audits, exact-commit source comparison, authentic packaged bitmap review, Site upload through the commit-bound gate, live re-download/hash verification, and scoped temporary-work cleanup.
