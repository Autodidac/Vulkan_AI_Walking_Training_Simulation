# Runner v0.7.22 black-frame hotfix

Runner v0.7.22 repairs the packaged rendering regression that left Live Autopilot, the training dashboard, the training PIP, Rig Lab, and each Rig Lab page as black interiors with only their borders visible.

## Root cause

Runner draws clipped view content first and then redraws the rounded card border. The border call previously passed a default-constructed `Color{}` as the inner fill. `Color` intentionally defaults to alpha `1.0`, so that value is opaque black rather than transparent. The final border pass therefore covered the already-rendered scene and controls.

The hotfix uses the named `ui_render::transparent_fill` contract for every border-only pass. The Vulkan blend state treats its zero alpha as a no-op for the card interior while preserving the requested outer border.

## Coverage

The fix applies to:

- the large Live world;
- the right-side training dashboard;
- the live training PIP;
- progress and status card outlines;
- the Rig Lab viewport;
- the `PRESETS`, `STRUCTURE`, `MOTORS`, and `TEST` Rig Lab pages.

## Regression protection

The clipping suite now requires clipped geometry to emit real triangles and validates nested clip intersections. A CPU visible-frame test composites the final triangle stream at sample points inside every Live and Rig Lab region. This catches the exact failure mode where correct content exists earlier in the vertex stream but a later opaque rectangle hides it.

## Compatibility

This is a rendering-only hotfix. Runner v0.7.22 preserves the v0.7.21 controller dimensions, locomotion curriculum, canonical rigs, gait evidence, terrain behavior, checkpoint semantics, and `runner-v0721-*` autosave paths.
