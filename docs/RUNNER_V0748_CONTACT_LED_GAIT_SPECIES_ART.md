# Runner v0.7.48 contact-led gait and species art

## Observable corrections

The v0.7.47 packaged screenshots reopened two completed claims. The Human translated as if its pelvis were constrained to a horizontal rail while the feet slid through an approximate cycle. Chicken, Dog, and Hexapod still exposed detached or incorrectly scaled modules because presentation metadata used the wrong physical endpoints and retained transparent source margins.

v0.7.48 treats those screenshots as release-gating evidence.

## Human contact-led locomotion

A Human stance contact now acquires a world-space X anchor only on the contact transition. While support remains latched, the physical foot is kept at that anchor and the contact height is sampled from the authoritative current terrain/material surface. The anchor is released with the support transition, allowing the other leg to swing and acquire the next plant. This makes pelvis translation an outcome of alternating support constraints instead of an independent visual scroll.

Grounded boot presentation uses the same support truth. Its sole is horizontal, mirrored by facing, and seated on the physical contact point. An airborne boot still follows the shin so toe clearance and swing pitch remain visible.

The Human support clock retains the authored near-straight stance leg and moderate stride. The two arm chains are discovered from motor topology in stable semantic order. Each arm uses the phase of its same-side leg with a negative cosine offset, so the arm moves aft when that leg reaches forward. The amplitude is four percent of arm-chain length around the saved relaxed side-rest target. Uneven walking and shuttle retain exact Human body-clock authority after lesson handoff; a learned residual cannot push both arms forward together.

## Raw-learning authority boundary

The Human owns a mandatory local articulation cluster that keeps the authored torso/head branch, passive neck, and manipulator endpoints within physical range and velocity envelopes. It rotates branches only around the physical pelvis and cannot translate the root, choose support, select gait phase, add swing impulse, or move the course. This is part of the articulated plant in both Assisted and Raw modes.

Raw policy audit separately forces lesson teacher, topology reflex, safety reflex, posture guide, swing clearance, and course motion to zero. Advanced Diagnostics displays `LOCAL JOINT CLUSTER` independently from those optional authorities. A zero policy therefore remains stationary, while the action-space gait demonstration and the learned controller can remain structurally viable.

The release proof evaluates one immutable retained Human Walk controller across six Assisted seeds and six Raw seeds. Both paths must average at least 12 metres and 12 physical contact cycles with zero invalid runs; Raw must also report no rejection mask or invalid reason. The accepted deterministic result is 15.259 m / 26.33 cycles Assisted and 12.895 m / 20.83 cycles Raw, with 0/6 invalid runs in both modes.


## Species art assembly

The START placard remains tethered to the exact zero-metre course coordinate, but its label box is offset left of the spawn silhouette with a visible leader. Other distance markers retain their world-aligned label positions.

Human presentation keeps both physical side-view limb chains. The near chain is fully readable while the far chain uses a deliberately lower shared opacity; compact arm and hand envelopes prevent overlapping paired anatomy from reading as duplicated or detached art. The torso attitude correction rotates only torso/head nodes around the physical pelvis and cannot translate the root or manufacture progress.

Runtime asset generation now:

- keys magenta source mattes before computing visible bounds;
- crops from opaque subject pixels rather than atlas rectangles;
- fits the subject into a bounded runtime module with a one-pixel safety gutter;
- uses smaller runtime densities that remain below the renderer vertex-memory headroom;
- preserves separate Chicken, Dog, and Hexapod module families.

Presentation uses explicit physical topology:

- Chicken torso follows its authored body axis, its head begins at the physical neck attachment, and its tail follows the actual tail node rather than a support/body substitute.
- Dog torso, neck/head, tail, four articulated legs, and paws use the Dog graph.
- Hexapod torso/head and all six articulated support branches use the Hexapod graph, including the coupled middle knees.

Human remains the world-scale reference. Species are not normalized to Human screen height, and Auto View cannot conceal a bad module scale.

## Persistence contract

The controller meaning changes because planted-contact physics and Human body actions changed. Training semantics advance to `0x0007'4801`, with species-owned automatic paths:

- `runner-v0748-{species}-autosave.eppo`
- `runner-v0748-{species}-evolved.rig`
- `runner-v0748-{species}-autonomy.state`

A v0.7.47 checkpoint may contribute validated lifetime totals only. Authored `human.rig`, `chicken.rig`, `dog.rig`, and `hexapod.rig` files keep their existing species-owned format and remain loadable.

## Deterministic coverage

`Runner.V0748ContactArt` covers:

- visible subject bounds for all 18 species runtime modules;
- species-specific physical body, head, and tail attachment topology;
- exact opposed Human arm offsets at positive and negative phases;
- facing mirroring and non-finite arm inputs;
- grounded, mirrored Human boot transforms;
- a deliberately displaced planted Human foot held to its exact world anchor at 20, 60, and 240 Hz.

Existing Core, authored-runtime, locomotion/terrain, species-rig, Dog-art, shuttle, course, UI, package, camera, and cold-learning gates remain mandatory. Direct Live, PIP, Rig Lab, and fixed-zoom packaged captures remain authoritative over numeric completion.

## Release boundary

v0.7.47 remains the published release until this successor passes the complete Windows SDL3/Vulkan and Linux GCC 14 warnings-as-errors matrices, all feature diagnostics, installed and independently extracted launchers, archive/manifest audits, and visible packaged acceptance. Publication requires a separate explicit instruction and must not overwrite v0.7.47 artifacts.
