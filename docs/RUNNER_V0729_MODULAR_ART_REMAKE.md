# Runner v0.7.29 modular armor art remake

Runner v0.7.29 replaces the legacy concept-sheet derivatives with a purpose-built exact lateral-orthographic runtime set. Only the remade build atlas and compact runtime sprites are retained; the old source-sheet/provenance bundle is absent from the repository and package.

## Remade source and deterministic extraction

The selected transparent 1403x1121 atlas is stored at `tools/art_sources/runner_v0729_modular_atlas.png` with SHA-256 `b7e8d5a8cc7cc57af473161bd2e8a9feb0608301e7a4d8e629e8a289c8e71428`. `tools/generate_runner_armor_assets.py` crops eight explicit non-overlapping subject boxes, rotates longitudinal limb pieces into joint-to-distal orientation, downsamples them into bounded canvases, and writes dependency-free P3 runtime sprites.

The runtime set is:

- `helmet_side.ppm` — 32x32
- `torso_side.ppm` — 32x40
- `upper_arm_side.ppm` — 36x20
- `forearm_side.ppm` — 40x20
- `thigh_side.ppm` — 36x20
- `shin_side.ppm` — 38x20
- `foot_side.ppm` — 32x28
- `weapon_side.ppm` — 44x24

## Projection contract

Every retained source part is a true side elevation: a single lateral silhouette with no visible front/chest plane, three-quarter turn, converging edge, perspective depth, or foreshortened counterpart. Helmet, torso, arm, leg, boot, and weapon proportions therefore remain readable when mapped directly into Runner's 2D world plane. The explicit crop boxes isolate all eight subjects and prevent a neighboring part from leaking across a nominal grid boundary.

## Renderer contract

The P3 loader detects a four-corner `#ff00ff` key. Keyed assets hide only that explicit color, preserving black outlines and dark graphite interior panels. Legacy non-keyed P3 art retains its old near-black fallback behavior.

Arm and leg sprites are mapped onto the corresponding authored motor pivot/driven-node segment. Their local horizontal axis follows the moving bone, their thickness is bounded from that segment's screen span, left/right pieces mirror across the segment, and far-side opacity follows the existing near-leg selector. The mapping is enabled only for paired biped chains; quadruped, crawler, hexapod, monoped, and unrelated custom topology never receive humanoid limb assumptions.

Torso and helmet sprites remain bounded to the physical root/torso and head nodes. Plate thickness and torso extent scale within those authored segments, while topology-derived shoulder-frame bones render behind the torso plate so the exact side profile remains legible. The weapon sprite follows the simulation equipment mount and aim angle. Missing or malformed parts fall back independently to procedural geometry. Debug nodes remain visible over the art. The generated runtime density is intentionally lower than the retained source atlas: `Runner.exe --diagnose-art` renders both the production course and orthographic close-up frames offscreen and requires each to stay below 75% of the shared 8 MiB Vulkan vertex-buffer limit. `Runner.exe --art-eye-test` opens the frozen close-up in the real Vulkan UI and labels the strict side-elevation contract directly on the production frame.

## Isolation and validation

The art toggle is presentation-only. No asset name, sprite state, chroma key, or placement value enters simulation, contacts, observations, rewards, policy dimensions, checkpoints, curriculum, or persistence. Training semantics, checkpoint dimensions, and autosave compatibility remain v0.7.28 because this pass changes presentation only. `Runner.V0729ModularArt` and `Runner.ArtDiagnostic` cover all eight files, dimensions, explicit source boxes, padding, shared material colors, exact side-profile labels, explicit key detection, dark-pixel preservation, malformed input, repeated loads, generator membership, obsolete-source removal, topology guards, production/close-up vertex budgets, and deterministic simulation identity. The final 2575x1407 Vulkan `--art-eye-test` frame is retained at `validation/v0729_modular_art_eye_test.png` with SHA-256 `c00844efd75e85e736ab740a46fe567a0ca2829890de6c67f246b55cb4f87738`.
