# Runner v0.7.15 viewport and recovery contracts

The live viewport uses 22 pixels per meter, frames the ground at 72% of the viewport height, and follows the rig with additional course space ahead. The rig is intentionally a small course subject rather than the dominant screen object.

The live terrain renderer expands exposed, active, and near-surface macro regions into 0.140625-meter fine cells. Only deep, inactive, fully uniform 8x8 macro tiles render as aggregate quads. The former interpolated surface polyline and moving dashed pseudo-ground are removed, and the zero-distance marker is suppressed.

Training autosaves use v0.7.15-specific names. Catastrophic invalid or backward evaluations restore the last verified champion and restart the current lesson. When no champion exists, three catastrophic evaluations reset the failed policy nursery instead of spending unlimited updates on a zero-quality controller.

Acceptance requires the Linux deterministic suite and the complete Windows SDL3/Vulkan build, diagnostics, installation, extracted-package audit, checksum, and manifest to pass.
The renderer and physics now use the same canonical treadmill coordinate conversion. Deformable terrain source coordinates are shifted into world space by the exact inverse of the collision sampler, so pressure marks, deposited material, hills, feet, and obstacles remain locked together. Balance, static crouch, jump, and flip lessons use a flat y=0 collision plane and therefore render a flat compacted surface instead of exposing the unrelated deformable map.
