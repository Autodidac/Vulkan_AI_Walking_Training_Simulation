# Runner v0.7.34 curriculum-safe rig optimization

Runner v0.7.34 corrects a packaged-runtime failure found in the user's real v0.7.33 autosave. The run was still on Static Crouch after 113 candidate attempts and 48 accepted control changes, while its binary checkpoint retained only 15 Crouch lesson updates. The issue was not slow learning: accepted rig candidates repeatedly replaced the main trainer with a four-update nursery checkpoint.

## Safe curriculum boundary

Automatic control tuning and morphology evolution are deferred until the current lesson has completed its measured update, episode, and evaluation requirements. Static Crouch must reach its exact update-200 raw-policy handoff. Foundational Walk / Run must reach the topology-specific zero-authority handoff. Stand never mutates the rig. Later lessons use their complete fresh-work boundary. Once a valid mastery confirmation is recorded, candidate work pauses until the consecutive confirmation streak either completes or is broken by new evidence. Rejected candidates do not change the main trainer.

## State-preserving candidate adaptation

A candidate now receives a retargeted copy of the current checkpoint: policy parameters, Adam moments, lesson-local age, stage, difficulty, random state, and every cumulative lifetime field remain intact. Only evaluation and retained-champion evidence measured on the previous physical rig is invalidated. The bounded four-update candidate adaptation starts from that preserved state, so accepted work adds monotonically instead of publishing an isolated four-update ledger. Newly activated topology slots remain explicitly neutralized before adaptation. Acceptance still requires six deterministic raw-policy held-out seeds, finite qualification, complexity cost, and a real improvement margin; failure restores the exact prior rig and checkpoint.

## Persistence and compatibility

Training semantics are `0x0007'3401`, checkpoint magic is `EPPO34`, and automatic files use `runner-v0734-curriculum-*`. When no v0.7.34 state exists, a readable v0.7.33 checkpoint contributes only its validated lifetime ledger; its policy, optimizer, mastery, candidate schedule, and corrupted short lesson age do not resume as current state. State format 17 remains unchanged because its fields did not change.

Deterministic coverage locks the negative Stand case, incomplete Crouch work, exact Crouch handoff, topology-scoped Walk handoff, insufficient episodes/tests, in-progress mastery-streak exclusion, render-cadence independence, control candidate retarget, old-rig evidence invalidation, monotonic high lifetime totals, bounded adaptation, rig-signature compatibility, and checkpoint round-trip.