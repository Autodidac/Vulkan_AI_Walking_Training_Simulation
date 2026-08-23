# Runner v0.7.46 exact material cells, natural Human gait, and turn mastery

Runner v0.7.46 closes the post-v0.7.45 eye-test regressions without using preview motion, conveyor authority, or relaxed integrity gates.

## Stable terrain and material cells

- Authored macro terrain is immutable. Rendering and contact queries select the same discrete cell; neither path interpolates neighboring solid heights, firmness, looseness, water depth, or water surface.
- Only explicitly deposited or falling material cells evolve. Static ground does not bubble, migrate, or deform under footsteps.
- Water is read from the same material-cell state used for contact observations so a visual water boundary cannot introduce a hidden support step.
- The contract follows the read-only SimEngine reference model: stable coarse/fine cells, deterministic fixed-step updates, and explicit material deposition.

## Human gait

- The v0.7.45 bent-knee zombie baseline is preserved under `legacy/v0.7.45-zombie-gait/` with exact rig checksums and reproduction identity.
- Human stance geometry solves vertical reach from the authored horizontal stance, permitting a near-straight planted leg at mid-stance while retaining forward-only swing-knee flexion.
- Arms return to the authored side-rest target and use a low-amplitude contralateral swing instead of forward-reaching balance throws.
- Existing heel-to-foot-flat-to-toe support evidence, zero-conveyor authority, strict motion validity, both-facing behavior, and frame-rate independence remain required.

## Turn and return mastery

- Round-trip qualification no longer uses signed net evaluation speed. A correct outbound and return traversal cancels signed speed near zero even when both legs of the shuttle are valid.
- Turn mastery still requires a valid quality key, full distance, stride-transfer count, survival duration, and collision limit.
- Deterministic tests accept qualifying negative, zero, and positive signed net speeds and reject invalid or short evidence.

## Species art fit

- Dog and Chicken head/tail modules use species-owned scale, anchor, trim, and native-axis profiles.
- The Dog tail keeps its authored dorsal orientation instead of inheriting an inverted transverse mirror.
- Head presentation follows the anatomical torso-to-head axis in both facings rather than borrowing the current travel vector.

## Persistence and compatibility

- Training semantics are `0x0007'4601` and species-owned paths use `runner-v0746-*`.
- v0.7.45 contributes lifetime totals only; incompatible v0.7.45 policies, optimizers, retained controllers, and curriculum state are not resumed.

## Verification

Release validation covers exact cell-boundary sampling, stable macro terrain, water/contact parity, Human gait geometry, turn mastery boundaries, species art orientation, Windows SDL3/Vulkan tests, Linux GCC 14 warnings-as-errors, package diagnostics, installed and extracted launchers, and runtime/source archive byte audits.

Packaged eye testing remains authoritative for posture, feet, arm rest, water transitions, art fit, PIP framing, and both facing directions.
