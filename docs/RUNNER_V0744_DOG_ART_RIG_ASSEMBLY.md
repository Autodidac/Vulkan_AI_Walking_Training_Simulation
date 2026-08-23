# Runner v0.7.44 Dog art, species rigs, and Human gait

Runner v0.7.44 replaces the Dog presentation with a purpose-built orthographic modular atlas, corrects the authored animal rest geometries used by Live, PIP, and Rig Lab, and finishes the Human walk cycle around the user's latest saved upright rig.

## Dog presentation contract

The Dog atlas contains six disconnected right-facing modules: head and neck collar, torso, tail, upper leg, lower leg, and paw. The deterministic generator removes the source checker field, crops each module independently, preserves transparent-key gutters, and emits fixed-size P3 runtime assets.

The renderer overlaps art across physical joint endpoints instead of exposing bone strings. Per-species profiles control limb thickness, torso coverage, paw size, tail size, and head scale while the same physical node positions and rotations remain authoritative. Debug nodes are still available when explicitly enabled.

## Rig geometry

Dog uses a level canine spine, forward raised head, paired front and rear two-segment legs, four equal-height paw contacts, and equal near/far chain lengths. Chicken keeps a compact avian body, forward head, rear counterweight, and two articulated legs. Hexapod keeps a horizontal body and six two-segment supports. Human authored geometry is not rescaled by this pass.

Factory construction and save/load validation reject cross-species topology. The v0.7.44 regression test also checks repeated factory determinism, invalid contacts, Dog art dimensions, visible content, transparent-key gutters, and paired animal supports.

## Human gait contract

The packaged `human.rig` and factory Human share the user's latest saved neutral coordinates. Paired arm and leg segment lengths remain locked without moving that authored stance.

The code-brain support clock owns exact opposed leg transfer during uneven support and shuttle movement. The policy may add bounded upper-body residuals, but cannot zero the planted-support action. Contralateral arms leave and return to their authored side-rest pose at modest amplitude, knees and elbows retain their authored bend branch, and the final bilateral composition filter runs once after topology-specific inverse kinematics.

Startup safety no longer suppresses Human torso and arm targets while the support reflex is active. Deterministic tests cover adversarial policy cancellation, retained visible body control, neutral geometry, paired lengths, both facings, and the complete 1,200-update replay. The final zero-authority retained proof reports a 25.036 m six-seed mean, 36.50 gait events, and zero invalid seeds; the direct physical shuttle covers 47.2775 m with 67 gait events, 19 sampled crossings, four real turns, and zero backward-brace events.

## Reproduction

The source atlas is tools/art_sources/runner_v0744_dog_modules_source.png. Regenerate the runtime modules with:

    python tools/generate_species_art_assets.py

The generator verifies the source SHA-256 before producing assets. Training state is isolated under runner-v0744 species paths with semantics 0x0007'4407, EPPO44 checkpoints, and RUNAUTONOMY 26 state.
