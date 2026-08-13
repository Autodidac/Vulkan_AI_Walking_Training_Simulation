# Runner v0.7.32 shuttle, facing, gait, and hand assembly

Runner v0.7.32 completes the compact bidirectional preview requested from the v0.7.31 eye tests. Physics remains fixed-step and deformable sand remains active; the release changes locomotion intent and presentation without translating the root or adding course motion.

## Finite shuttle contract

Walk-capable preview lessons use a compact 12 metre arena from -2 m to 10 m. At each end the rig brakes to a controlled speed, keeps its current facing while backing 0.30 m, holds locomotion during a 0.32 second turn, mirrors its complete presentation, and resumes forward travel toward the opposite end. Distance accumulates only from finite, speed-bounded displacement in the requested direction. Camera auto-fit frames the arena and the status bar reports FORWARD, BRAKING, BACKING, or TURNING plus facing.

Lesson-appropriate obstacles remain absent until two clean turns, fourteen physical gait cycles, and two seconds of stable stance. At readiness two bounded features are generated ahead of the active traversal. They are removed during backing and turning, regenerated ahead after the turn, and cleared by lesson changes and resets. Collision, passage, knee-first checks, approach observations, terrain lookahead, reward speed, and equipment aim share the active direction sign.

## Rig and armor contract

Both support knees bend toward the same sagittal side for a given facing instead of forming the crab/X pose. The two complete manipulator chains use phase-opposed fore/aft targets, with one arm leading and the other trailing. Orthographic torso and helmet art mirror with facing, near/far limb ordering swaps, and the open graphite/cyan glove extracted from the strict `Side View` row of the hash-locked user-supplied sheet follows every terminal manipulator graph edge. Thin authoritative bones remain visible beneath assembled armor; no generated body fallback was restored.

## Edited-rig optimization and morphology evolution

Rig Lab exposes two explicit rig-scoped modes. CONTROL OPTIMIZE is the fresh-rig default and changes only motor strength and joint range; nodes, supports, radii, bones, stiffness, semantics, and topology remain byte-stable. MORPHOLOGY EVOLVE routes through bounded parameter, resize, split, append, duplicate-support, and removable-leaf graph mutations. Every candidate must remain finite, connected, structurally valid, editable, and graph-fit for art.

A topology-changing candidate receives policy transfer, neutral activation of any new motor output, and a short nursery adaptation. Publication then uses six deterministic held-out seeds with raw policy output, no teacher blend, no course conveyor, and an explicit node/bone/motor/support complexity cost. A candidate that does not clear the strict stage qualification and improvement margin is rejected; failed application restores the exact champion rig and controller. State version 17 persists the selected mode with the rig, while version 16 loads safely as CONTROL OPTIMIZE.

## Crouch and compatibility

Static Crouch now drives both support knees toward the same sagittal side throughout its proven fade and exact update-200 zero-authority boundary, preventing the hip-hinge target error without weakening raw-policy mastery. Training semantics advance to 0x0007'3201 and state uses runner-v0732-shuttle-* names. Incompatible v0.7.31 state imports only its validated lifetime ledger before a fresh policy, optimizer, and mastery run.

Deterministic coverage includes both boundaries and facings, reverse/turn/forward sequencing, invalid inputs, repeated runs, 20/60/240 Hz turn equivalence, all canonical topologies, delayed feature lifecycle and cleanup, mirrored equipment aim, bilateral knee direction, phase-opposed arms, exact crouch handoff, packaged hand decoding, anatomy-immutable control routing, deterministic morphology growth, invalid-mode fallback, and real v17/v16 autosave round trips.
