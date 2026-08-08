# Runner v0.7.29 modular armor art remake

Runner v0.7.29 replaces the legacy concept-sheet derivatives with a purpose-built side-view runtime set. The supplied alien-tech armor sheet was used only as visual direction. It is not copied into the repository or package, and the old source-sheet/provenance bundle is removed.

## Remade source and deterministic extraction

The selected transparent 1536x1024 atlas is stored at `tools/art_sources/runner_v0729_modular_atlas.png` with SHA-256 `b42b09d1c628da640dd9a39ab2a9730438bffdf490da25f458701d0d2ed6c28e`. `tools/generate_runner_armor_assets.py` crops its fixed 4x2 cells, rotates longitudinal limb pieces into joint-to-distal orientation, downsamples them into bounded canvases, and writes dependency-free P3 runtime sprites.

The runtime set is:

- `helmet_side.ppm` — 32x32
- `torso_side.ppm` — 32x40
- `upper_arm_side.ppm` — 36x20
- `forearm_side.ppm` — 40x20
- `thigh_side.ppm` — 36x20
- `shin_side.ppm` — 38x20
- `foot_side.ppm` — 32x28
- `weapon_side.ppm` — 44x24

## Renderer contract

The P3 loader detects a four-corner `#ff00ff` key. Keyed assets hide only that explicit color, preserving black outlines and dark graphite interior panels. Legacy non-keyed P3 art retains its old near-black fallback behavior.

Arm and leg sprites are mapped onto the corresponding authored motor pivot/driven-node segment. Their local horizontal axis follows the moving bone, their thickness is bounded from that segment's screen span, left/right pieces mirror across the segment, and far-side opacity follows the existing near-leg selector. The mapping is enabled only for paired biped chains; quadruped, crawler, hexapod, monoped, and unrelated custom topology never receive humanoid limb assumptions.

Torso and helmet sprites remain bounded to the physical root/torso and head nodes. The weapon sprite follows the simulation equipment mount and aim angle. Missing or malformed parts fall back independently to procedural geometry. Debug nodes remain visible over the art. The generated runtime density is intentionally lower than the retained source atlas: `Runner.exe --diagnose-art` renders the production course frame offscreen and requires it to stay below 75% of the shared 8 MiB Vulkan vertex-buffer limit.

## Isolation and validation

The art toggle is presentation-only. No asset name, sprite state, chroma key, or placement value enters simulation, contacts, observations, rewards, policy dimensions, checkpoints, curriculum, or persistence. Training semantics, checkpoint dimensions, and autosave compatibility remain v0.7.28 because this pass changes presentation only. `Runner.V0729ModularArt` and `Runner.ArtDiagnostic` cover all eight files, dimensions, padding, shared material colors, explicit key detection, dark-pixel preservation, malformed input, repeated loads, generator membership, obsolete-source removal, topology guards, and deterministic simulation identity.
