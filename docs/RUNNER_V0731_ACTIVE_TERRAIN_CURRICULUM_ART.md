# Runner v0.7.31 active terrain, lesson ownership, and rig-art truth

Runner v0.7.31 closes the failures visible in the packaged v0.7.29 eye tests without turning the course into rigid ground. The starting stance, active material simulation, learning clocks, controller handoff, and optional art now share explicit contracts that can be tested independently.

## Launch contact and active terrain

Every authored support seed is rigidly shifted onto the exact interpolated launch surface after deterministic terrain construction. The 0.70 m launch core is structural, flat, and protected from pressure, deposit, and relaxation; a 0.55 m transition blends into active sand. No support begins below collision, and at least one support starts in contact for every canonical rig and repeated seed.

Beyond that core, terrain remains live. Seeded dry sand begins roughly 1.4-2.05 m from launch, material order varies, waterlogged and shallow-water bands have physical depth, and the later hole is real collision geometry. Pressure removes and redistributes loose volume, relaxation moves unstable material, and deformation scales from measurable 30% curriculum motion to the full 100% hazard. Large course objects remain delayed until the existing gait-evidence gate.

The production renderer fills a continuous substrate to the viewport bottom, samples its visible collision line from `Environment::ground_height_at`, and then layers material and water detail. A changing cosmetic lower edge can no longer look like collision, while every real surface change is visible.

## Lesson-local learning and finite ownership

`PpoTrainer` persists a lesson update counter distinct from rig lifetime updates. A real stage change resets lesson age while preserving compatible policy parameters, Adam moments, and rig totals; same-stage difficulty changes do not erase useful work. Checkpoints store the new clock, and older checkpoints migrate it explicitly to zero.

Crouch assistance fades from update 180 to an exact update-420 handoff. Walk assistance is topology scoped. Multi-support rigs now fade from update 600 to 900, and action blending, rollout bootstrap, guided imitation, assisted-best clearing, evaluation, preview authority, and telemetry all use the same boundary. The 1,200-update cold gate therefore contains 300 complete zero-authority updates instead of ending at the first unassisted instant.

The multi-support reference controller also honors the shared recovery plan: four-support gait drive is reduced while balance reserve is being recovered, while the six-support authored gait remains on its stable full-body clock. This prevents a quadruped from continuing full stroke while already pitching on deforming sand.

## Rig topology and optional art

Support, manipulator, torso, head, and terminal-boot roles come from graph reachability through the authored rig instead of motor slot numbers. The shared current armor language is applied to every visible preset. Boots translate, rotate, pivot, and mirror from terminal support segments; limb plates follow motor segments; torso and helmet follow physical body axes. These transforms are presentation-only.

The near-duplicate Scaffold remains an internal calibration fixture. Seven distinct presets remain user visible: Humanoid, Biped, Chicken, Monoped, Quadruped, Low Four-Leg Crawler, and Hexapod. Their rig signatures are pairwise distinct, and the crawler silhouette is structurally lower than the quadruped.

`--diagnose-art` now renders the production course, all seven visible rigs sequentially, and a deterministic horizontal humanoid pose. Every frame must remain below 75% of the shared 8 MiB Vulkan vertex budget.

## Determinism and acceptance

Tests cover protected launch mutation, support placement, pressure and volume conservation, 30%-versus-100% deformation, smooth foundation boundaries, real holes and water, repeated-seed material order, all-topology graph roles, arbitrary boot transforms, preset identity, crouch learning, multi-support handoff, and exact full terrain state at 20, 60, and 240 render Hz. Partial, negative, and NaN render deltas cannot advance simulation, terrain, curriculum, or lesson clocks.

The bounded cold learner retains strict-valid zero-authority controllers over six seeds with course motion disabled. Local v0.7.31 evidence includes quadruped 8.70 m / 54.5 support cycles, crawler 15.50 m / 54.7 cycles, hexapod 26.48 m / 51.7 cycles, and humanoid 33.26 m / 33.2 real steps. Tagged Linux, Windows, package, installed/extracted, Vulkan eye-test, checksum, manifest, and public re-download evidence remains mandatory before publication.
