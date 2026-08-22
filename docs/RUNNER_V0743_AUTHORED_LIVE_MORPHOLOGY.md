# Runner v0.7.43 authored pose and live morphology

## Authority

The saved species rig is the single neutral-pose authority. Reset, balance targets, torso measurements, support geometry, observations, teacher actions, rendering, PIP framing, and Rig Lab all derive from the same authored graph. Human paired upper/lower arm and leg lengths remain linked so editing one side cannot create accidental asymmetry.

## Live morphology transaction

A valid edit is previewed immediately in the active environment. The worker continues publishing its prior compatible snapshot until it has accepted the new rig signature; only then can the preview be committed. Invalid intermediate graphs never replace the active physical rig or persisted species file. Cancelling restores the last published rig and course without resetting the camera or editor selection.

Geometry and motor-contract changes use a new signature and isolated v0.7.43 policy namespace. Presentation-only changes do not alter the physical signature.

## Diagnostics

Rig Lab reports:

- live preview versus committed state and the active morphology signature;
- selected node authored position, radius, link count, and support role;
- selected bone endpoints, authored length, and stiffness;
- selected motor role, action slot, current/authored/target angle, limits, velocity, command, and support-chain state.

The skeleton, motor arcs, rest/current pose, support/contact, and art-anchor overlays remain independently selectable in the existing Rig Lab pages.

## Compatibility

v0.7.43 writes EPPO43 checkpoints and RUNAUTONOMY 25 state under species-owned runner-v0743 paths. v0.7.42 checkpoints may contribute validated lifetime totals only; controller parameters are not resumed across the authored-pose semantic boundary.

## Validation

Deterministic tests cover the exact saved Human pose, paired anatomy, opposed gait startup, finite motor diagnostics, valid/repeated/invalid/cancelled morphology previews, immutable authored terrain, bounded equipment engagement, all production species, and package document presence.

Multi-support retention counts completed topology-aware support transfers: Dog requires 10 m / 18 transfers and Hexapod 14 m / 22 transfers, with full replay survival and the existing strict-validity gates still required.
