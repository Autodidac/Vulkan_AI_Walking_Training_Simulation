# Optional Runner modular armor

This directory contains the v0.7.29 purpose-built exact lateral-orthographic runtime sprites. The old concept-sheet references and attribution bundle were removed; only the remade runtime set is packaged.

Runtime files under `runtime/`:

- `helmet_side.ppm` - 32x32 - SHA-256 `b8ca4b3512b7dcae509f0ba3091ef5b4d710e9c92db74ba8c688645a354c9181`
- `torso_side.ppm` - 32x40 - SHA-256 `347397b654af39e25ebfa8601a87c4f0d1f69c02ab8861d363b158b2cf83e3a7`
- `upper_arm_side.ppm` - 36x20 - SHA-256 `d103b1a11a30d42eaff18cc0ad463f02d945f0e374b8ecf929eb449070e9e05b`
- `forearm_side.ppm` - 40x20 - SHA-256 `cfe130f82baa9a0201c4aaef2147a0b711c270abbef1962f817cf5e57f3eda8e`
- `thigh_side.ppm` - 36x20 - SHA-256 `469cd809e9b422dab7dab7a154428e6108d3824c4b4b3420a7f5346d2f16a1e4`
- `shin_side.ppm` - 38x20 - SHA-256 `1ce0eb5712e21ff6cba0f3a77a4ca85020535d53c288b6e9f0a2a4b45fd4ced8`
- `foot_side.ppm` - 32x28 - SHA-256 `25f2450bd1cc38ade93d7c047d909c7c6909ddfab806f98c961eb8de3c7daea5`
- `weapon_side.ppm` - 44x24 - SHA-256 `c9da04e58aa44295366fa348ab5c6bd3d030280077f7c0a201511e6e15347e40`

The compact runtime density is deliberate: the high-resolution transparent source atlas remains under `tools/art_sources/`, while these lateral-orthographic sprites keep the complete production frame below 75% of the 8 MiB Vulkan vertex budget.

`#ff00ff` is an explicit transparency key, so dark armor outlines remain visible. Removing this optional directory preserves procedural rendering and all simulation/training behavior. Press `A` to toggle the art at runtime.
