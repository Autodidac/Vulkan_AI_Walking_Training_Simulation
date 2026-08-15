# Runner v0.7.33 granular hazards, complete facing, and Rig Lab readability

Runner v0.7.33 is the corrective local release for the v0.7.32 packaged eye-test failures. The rendered character, physics rig, controller observations, active terrain, equipment, and editor now share explicit contracts instead of relying on humanoid slot numbers or screenshot-specific offsets.

## Whole-rig presentation

- Facing is one signed property applied to helmet, torso, upper/lower limbs, boots, supplied-sheet hands, skeleton layering, and mounted equipment.
- Limb sprites retain proximal/distal joint ownership while their transverse art axis mirrors with facing and branch side. Boots use the signed facing even when a support segment is vertical or fallen.
- Hands remain the hash-locked derivative of the user-supplied Side View row. Their opaque bounds are fitted from forearm thickness, preserve wrist overlap, and are bounded for malformed, compact, edited, and evolved rigs.
- Humanoid proportions use a shared graph-derived profile: shorter support chains and body height, unchanged accepted arm reach, same-sided knee and elbow bends, and phase-opposed arm swing.

## Fixed-step granular terrain

The labeled material owns its behavior. Dry sand and mud are mutable; firm dirt remains structural. A seeded fixed-step event system can excavate sand, drop bounded sand/mud particles, and drop moving blocks. Particles fall, collide with the same terrain queried by rig contacts, settle, deposit, stack, and update the rendered surface. Blocks remain hazards until their speed stays below the safe threshold for the full dwell window. The controller receives incoming direction, velocity, density, and hazard-safe state and holds or evades while motion is unsafe, then traverses the settled result.

Events are delayed until the rig has useful gait/stability evidence, bounded to the active shuttle traversal, and cleared on retry, turn, lesson, rig, completion, and checkpoint boundaries. Deterministic tests compare state fingerprints after identical 20, 60, and 240 Hz render schedules.

## Equipment

Rig Lab equipment authoring configures the actual preview environment. Weapons mount to a topology-discovered manipulator terminal, mirror and aim with facing, remain visible when carried or dropped, and fire fixed-step projectiles with bounded lifetime, collision, hit telemetry, and cleanup. `NONE` produces no projectiles.

## Rig Lab

The Test card uses dedicated measured rows for selection, range actions, motion pattern, status, and manual input. The world viewport fits only the active graph, includes ground in its bounds, and uses the identical transform for rendering, dragging, adding, and hit testing. Reference ghosts no longer control framing; they are thin and low-alpha. Node radii are screen-bounded, labels are compact, and unsupported separator glyphs were removed. The Test world also renders selected equipment, aim target, and shot/hit telemetry.

## State and package boundary

Training semantics are `0x0007'3302`, checkpoint magic is `EPPO33`, and automatic files use `runner-v0733-granular-*`. A compatible v0.7.32 file may contribute its lifetime ledger, but policy, optimizer, mastery, rig, and preview state start fresh under the corrected semantics. The package requires this document and the supplied-sheet `hand_side.ppm` runtime asset.

The final Windows SDL3/Vulkan matrix passes 30/30 tests and the Linux GCC 14 warnings-as-errors matrix passes 25/25 tests, including fresh five-rig Walk and raw Crouch learners on both platforms. Installed and independently extracted launchers pass every production diagnostic from an unrelated working directory and match the 49-file manifest. The packaged walk-eye proof retained a zero-authority humanoid champion at update 1,115 and replayed `24.586 m / 55.67` mean steps over six valid seeds with `0.250 s` maximum scissor duration. Direct packaged Vulkan art and course frames plus the five-layout 150% DPI compositor gate close the visual audit. The release remains local only.
