# Optional Runner modular armor

This directory contains the v0.7.29 purpose-built side-view runtime sprites. The old concept-sheet references and attribution bundle were removed; only the remade runtime set is packaged.

Runtime files under `runtime/`:

- `helmet_side.ppm` - 32x32 - SHA-256 `4413d828605b854753a99f1158b49f96c16c71cdaff5722fbd57213bc0873e62`
- `torso_side.ppm` - 32x40 - SHA-256 `12ea0617a3dfb8b5f7d3ef7dd236ab58d2222d95c84785405e4ffe532b5ddc88`
- `upper_arm_side.ppm` - 36x20 - SHA-256 `6b736acd83db85cc977e3c7a885401a3cfe5c2bb1a8c8b4979eb4a4b963329bb`
- `forearm_side.ppm` - 40x20 - SHA-256 `b909a1001f27ab2f04844366789ae27ea59b07c3383780d28932f43733f13dd2`
- `thigh_side.ppm` - 36x20 - SHA-256 `a16787358e160ae5a825ff4e9982149b8e581fe9935b5e5261ee53e490b85cb9`
- `shin_side.ppm` - 38x20 - SHA-256 `2ad29c9daa350ee49aefc66ef4afce4fc76201a6f0b8db441b2a87f2e500f4cf`
- `foot_side.ppm` - 32x28 - SHA-256 `df038344654743e6eb5e8628bb2c8cec3ea7e4a2c2bf8c2fc2d6a9dab026903e`
- `weapon_side.ppm` - 44x24 - SHA-256 `7b57cf779b3dfef4bd50be0a9d7f602b15605d3d01e225e9dd319c71daf9586c`

The compact runtime density is deliberate: the high-resolution transparent source atlas remains under `tools/art_sources/`, while these side-view sprites keep the complete production frame below 75% of the 8 MiB Vulkan vertex budget.

`#ff00ff` is an explicit transparency key, so dark armor outlines remain visible. Removing this optional directory preserves procedural rendering and all simulation/training behavior. Press `A` to toggle the art at runtime.
