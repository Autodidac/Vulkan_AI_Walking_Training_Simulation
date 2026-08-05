# Runner v0.7.16 adaptive camera batch

Runner v0.7.15 corrected the physical world scale and synchronized rendered terrain with collision terrain, but the fixed 22 px/m live camera made otherwise-correct rigs appear too small. v0.7.16 separates physical scale from view magnification.

## Live view

- Automatic zoom fits the current finite rig bounds.
- The corrected default is substantially closer than 22 px/m.
- Automatic and manual scales are clamped.
- Mouse wheel over the world changes only view magnification.
- The side panel provides Zoom Out, Auto View, and Zoom In.
- `R` resets the preview and restores automatic view.
- Lookahead is computed from viewport width and current scale.
- Follow and zoom smoothing are elapsed-time based.
- A screen-space dead zone suppresses root jitter.

No camera operation changes particles, terrain, observations, rewards, course coordinates, training state, or SandHybrid cell scale.

## Training PIP

The PIP receives more usable space, keeps a tighter local course window, uses shared tested scale limits, and continues labeling distant hazards rather than shrinking the body to include them.

## Diagnostics

`Runner.exe --diagnose-camera` validates the compiled camera constants and representative automatic, manual, PIP, lookahead, clamp, and smoothing behavior without opening the application window.

The release gate runs this diagnostic from the build tree, installed package, and independently extracted archive.
