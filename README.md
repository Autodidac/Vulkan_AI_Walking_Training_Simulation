# Runner

## v0.7.31 active terrain, lesson ownership, and rig-art transforms

- Keeps the hazardous sand simulation live: pressure deformation is positive at early difficulty and grows to full strength, while waterlogged ground, water, and seeded holes remain physical hazards.
- Protects only the short launch/calibration pad, rigidly places every rig on its exact sampled surface, and smoothly transitions into active terrain without an invisible starting curb.
- Draws the continuous substrate and collision-surface line from the same evolving terrain samples used by contact physics.
- Gives every curriculum lesson its own persisted update clock, so Crouch and Walk receive finite measured assistance even after long Stand training without erasing compatible policy, optimizer, or rig totals.
- Requires a cold humanoid to learn and retain raw zero-authority Crouch, recover upright, and then enter Walk without switching rigs.
- Extends the topology-derived zero-authority handoff for quadruped, crawler, and hexapod learning while retaining strict post-handoff controllers across seeded replays.
- Rotates, translates, mirrors, and pivots boots and body art from authored rig geometry; the same orthographic armor family now fits all seven distinct user-visible rigs.
- Makes the near-duplicate Scaffold blueprint an internal calibration fixture and expands art diagnostics across every exposed rig plus a horizontal fallen pose.
- Preserves exact fixed-step state across 20, 60, and 240 Hz rendering, including terrain evolution, lesson clocks, policy handoff, contacts, and presentation transforms.
- Isolates corrected checkpoints and automatic state under `runner-v0731-active-*` paths.
## v0.7.30 sustained-walk learning recovery

- Stops forward-gait nursery randomization from repeatedly erasing partial walkers while cumulative rig updates keep rising.
- Gives every authored topology an exactly observable foundational gait clock and motor-only physical reference with no conveyor or root translation.
- Separates supervised gait imitation from PPO minibatch gradients, then fades teacher authority to exactly zero at topology-scoped handoff boundaries.
- Clears only assisted-era champion state at handoff; learned network weights and Adam moments continue, and retained best policies must republish unassisted.
- Reserves strict-valid champion quality above partial-invalid candidates while preserving genuine incremental gait before final mastery.
- Reconciles continuous-gait support transfer with Walk qualification and prevents real support cycles from accumulating as micro-motion, without weakening crab, body-contact, skating, rolling, vibration, or no-progress rejection.
- Adds the real `Runner.V0730ColdStart` worker/optimizer/publication/evaluation/preview gate, ten-seed physical gait probes, exact 20/60/240 render-cadence state equivalence, and a packaged `--walk-eye-test` that cold-trains and renders a measured zero-authority humanoid stride.
- Isolates corrected training semantics and automatic state under `runner-v0730-walk-*` paths.

## v0.7.29 modular armor art remake

- Replaces the old concept-sheet derivatives with eight purpose-built exact lateral-orthographic sprites: helmet, torso, upper arm, forearm, thigh, shin, boot, and compact energy weapon.
- Rotates and scales arm/leg plates with their authored biped motor segments while preserving existing near/far limb layers and debug contacts.
- Uses an explicit magenta key so dark graphite outlines remain visible; missing or malformed pieces fall back independently to procedural rendering.
- Keeps quadruped, crawler, hexapod, monoped, and unrelated custom topology free from humanoid art assumptions.
- Keeps the art toggle presentation-only: no physics, observations, rewards, policy, curriculum, checkpoint, or persistence changes.
- Adds `Runner.exe --diagnose-art`, which renders both the fixed production course and strict side-profile close-up frames and enforces 25% headroom inside the shared 8 MiB Vulkan vertex budget.
- Adds `Runner.exe --art-eye-test`, a deterministic frozen close-up that makes perspective, foreshortening, or a visible front/chest plane immediately obvious.

## v0.7.28 physical material course completion

- Replaces Walk / Run launch hazards with a protected physical runway and seeded contiguous firm ground, dry sand, waterlogged ground, shallow water, and recoverable holes.
- Keeps falling sand/debris and authored obstacles out of basic gait; advanced material pressure requires both runway distance and real gait cycles.
- Renders the actual near-surface cells, water column, hole profile, target, weapon, and projectiles instead of deep decorative geology or mislabeled sand grains.
- Adds full rejection-mask decoding, invalid-motion detail, live terrain/water/equipment truth, and nearest-feature distance.
- Completes reachable hand-ledged climbing with support transfer and controlled backward descent, plus optional sidearm/carbine/launcher target and combat lessons.
- Preserves v0.7.27 anatomy behavior when equipment is disabled and supports explicit EPPO28 checkpoint transfer into neutral new channels.
- Keeps preview, water, material, projectile, climb, and equipment timing fixed-step equivalent at 20, 60, and 240 render Hz.
- Adds `Runner.exe --diagnose-course` and repeated-seed positive, negative, adversarial, compatibility, and cadence gates.
- Adds `Runner.exe --course-eye-test`, a fixed production start frame that fits the protected runway and all material regions into one packaged visual audit.

## v0.7.27 authored-contact gait evidence

- Tracks strikes, swing time, and lift at each authored support seed while retaining topology-derived controller phase groups.
- Distinguishes a real multi-legged support transfer from fully planted skating without weakening the planted anti-skating rejection.
- Removes conveyor-derived locomotion credit from rollouts, champion evaluation, and the large preview; progress must come from simulated rig displacement.
- Adds `Runner.exe --diagnose-rig-training`, a deterministic 100-update biped/quadruped/crawler/hexapod comparison with distance, stride, invalid-seed, and preview-reset evidence.
- Advances every runtime simulation at fixed 60 Hz regardless of render cadence; 20, 60, and 240 Hz produce the same preview physics state.
- Fixes MSVC Debug constexpr compilation and makes the background pipeline test prove a staged publication instead of depending on optimized wall-clock throughput.
- Autosave names, checkpoint semantics, package docs, and release validation are isolated to v0.7.27.

Runner 0.7.31 is a combined autonomous physics locomotion trainer, rig editor, deformable-terrain laboratory, and cross-platform C++23 application.

## Build requirements

- CMake 3.28 or newer
- C++23 compiler
- Ninja or Visual Studio
- Python 3 for deterministic icon generation
- vcpkg for SDL3, Vulkan, and shaderc on Windows

The project pins and statically links the platform-neutral SandHybrid simulation library. Runner owns the SDL3/Vulkan application, rendering, training, editor, and package lifecycle.

## Windows build

```bat
cmake --preset windows-release --fresh
cmake --build --preset windows-release --parallel
ctest --preset windows-release --output-on-failure
```

Launch the Release application with `run.bat`. The launcher resolves the executable relative to itself and works from an unrelated current directory.

## Linux deterministic build

```bash
cmake -S . -B build/linux -G Ninja \
  -DRUNNER_BUILD_APP=OFF \
  -DRUNNER_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --parallel
ctest --test-dir build/linux --output-on-failure
```

## Live controls

- Mouse wheel: zoom the Live Autopilot world view without changing physical scale
- `R`: reset the live preview and restore automatic camera fitting
- `Space`: pause or resume background training
- `Tab`: switch between Live Autopilot and Rig Lab
- `1`, `2`, `3`: Normal, Faster, and Max CPU modes
- `T`: cycle Summary / Totals / Advanced Diagnostics
- `U`: toggle Metric / Imperial reference labels
- `A`: toggle the complete modular helmet, torso, arm, leg, boot, and weapon presentation
- `S`: save the current rig
- `L`: load a rig
- `Escape`: quit

The default Summary page explains learning health, lesson progress, the latest test, current useful evidence, the retained best controller, and the exact next goal. Raw scores, losses, quality keys, pipeline state, and throughput remain available on Advanced Diagnostics.

The live view automatically fits the current rig, maintains useful course lookahead, and uses elapsed-time camera smoothing with a screen-space dead zone. Camera magnification never changes simulation scale, terrain coordinates, observations, rewards, or learned state.

## Reference markers

Runner v0.7.18 leaves the terrain/collision treadmill unchanged and restores visible reference signs near launch: START plus recurring 10 m metric or 50 ft imperial markers. Near markers use metres/feet rather than misleading `0.00 KM`/`0.00 MI` labels.

## Rig Lab

Rig Lab provides preset selection, node and bone editing, motor ranges and strength, individual and grouped joint testing, gait and crouch test patterns, firm/loose-ground traction tests, near/far side selection, optional-art visibility, debug-skeleton visibility, save/load, champion restore, and fresh-policy controls.

Invalid structural edits are rejected without blocking the background trainer.

## Diagnostics

```bat
Runner.exe --version
Runner.exe --diagnose-vulkan
Runner.exe --diagnose-package
Runner.exe --diagnose-acceptance
Runner.exe --diagnose-camera
Runner.exe --diagnose-ui
Runner.exe --diagnose-art
Runner.exe --diagnose-rig-training
Runner.exe --diagnose-course
Runner.exe --course-eye-test
Runner.exe --walk-eye-test
Runner.exe --art-eye-test
```

`--diagnose-rig-training` runs the v0.7.31 cold-start learner: the four-motor biped, eight-motor humanoid, quadruped, crawler, and hexapod must each retain and directly replay a strict-valid post-handoff controller with zero teacher authority; both paired-leg subjects must exceed 18 m / a 14-step six-seed average, and multi-support rigs must sustain repeated physical contact cycles after their lesson-local handoff. `--walk-eye-test` independently cold-trains the default eight-motor humanoid, refuses any missing, invalid, assisted-era, or pre-handoff champion, replays the retained controller at zero authority over six seeds, and freezes a real lifted-step production frame after the 18 m / 16-step gate. The diagnostic also runs repeated-seed physical gait references and exact 20/60/240 render-cadence state checks. `--diagnose-acceptance` runs the deterministic rig/curriculum matrix used by package auditing. `--diagnose-camera` validates adaptive fit, clamps, wheel zoom, lookahead, dead-zone follow, and PIP scale. `--diagnose-ui` CPU-composites representative Live and all four Rig Lab pages and fails if any content region is black or visually empty. `--diagnose-art` renders the production course, all seven exposed orthographic rig presentations, and a horizontal fallen humanoid; every frame must remain below 75% of the shared 8 MiB Vulkan vertex budget. `--diagnose-course` verifies launch-contact alignment, live difficulty-scaled deformation, seeded material diversity, water/hole geometry, observation truth, delayed objects, physical climb/descent, optional-subsystem identity, and 20/60/240 Hz equivalence. `--course-eye-test` opens the real Vulkan UI at a deterministic frozen Walk / Run start frame and labels the launch pad and active material regions for direct packaged inspection. `--art-eye-test` opens a frozen close-up humanoid in strict side elevation.

## Repository records

- [`AGENTS.md`](AGENTS.md) defines cache-first implementation, validation, documentation, and release rules.
- [`CHANGELOG.md`](CHANGELOG.md) is the single release-history document.
- [`missioncache.md`](missioncache.md) is Runner's single authoritative active mission ledger; closed historical ledgers remain in Git history and release tags.
- [`docs/SANDHYBRID_INTEGRATION_BRIDGE.md`](docs/SANDHYBRID_INTEGRATION_BRIDGE.md) pins the SandHybrid library.
- [`docs/RUNNER_V0716_CAMERA_BATCH.md`](docs/RUNNER_V0716_CAMERA_BATCH.md) documents the adaptive camera contract.
- [`docs/RUNNER_V0717_EYE_TEST_CORRECTION.md`](docs/RUNNER_V0717_EYE_TEST_CORRECTION.md) documents the crouch/gait/stub-foot correction.
- [`docs/RUNNER_V0718_RUNTIME_RECOVERY.md`](docs/RUNNER_V0718_RUNTIME_RECOVERY.md) documents the update-loop, marker, control, telemetry, skin, and walking recovery.
- [`docs/RUNNER_V0719_GENERAL_LOCOMOTION.md`](docs/RUNNER_V0719_GENERAL_LOCOMOTION.md) documents balance reserve, terrain adaptation, running, reversal, flee behavior, and emergency recovery.
- [`docs/RUNNER_V0720_UI_PREVIEW_ICON.md`](docs/RUNNER_V0720_UI_PREVIEW_ICON.md) documents logical DPI, clipping, preview continuity, and application icon integration.
- [`docs/RUNNER_V0721_READABLE_TELEMETRY.md`](docs/RUNNER_V0721_READABLE_TELEMETRY.md) defines every plain-language training status, counter, goal, and color rule.
- [`docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md`](docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md) documents the opaque border-fill regression and visible-frame tests.
- [`docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md`](docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md) documents the true rounded-outline and center-preservation contract.
- [`docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md`](docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md) documents rigid bones, stage-qualified totals, mastery-aware completion, and the exact screenshot icon source.
- [`docs/RUNNER_V0725_ART_LEG_HOTFIX.md`](docs/RUNNER_V0725_ART_LEG_HOTFIX.md) documents compact node-attached armor and supported stance-leg extension.
- [`docs/RUNNER_V0726_TRAINING_TRUTH.md`](docs/RUNNER_V0726_TRAINING_TRUTH.md) documents rig-scoped training truth, static preview motion, and reset telemetry.
- [`docs/RUNNER_V0727_RIG_TRAINING_EVIDENCE.md`](docs/RUNNER_V0727_RIG_TRAINING_EVIDENCE.md) documents authored-contact gait evidence and the fixed four-rig comparison.
- [`docs/RUNNER_V0728_COURSE_COMPLETION.md`](docs/RUNNER_V0728_COURSE_COMPLETION.md) documents the physical material course, climb/equipment completion, checkpoint migration, and diagnostic contract.
- [`docs/RUNNER_V0729_MODULAR_ART_REMAKE.md`](docs/RUNNER_V0729_MODULAR_ART_REMAKE.md) documents the remade atlas, keyed runtime sprites, node-bound rendering, fallbacks, and isolation gates.
- [`docs/RUNNER_V0730_SUSTAINED_WALK_RECOVERY.md`](docs/RUNNER_V0730_SUSTAINED_WALK_RECOVERY.md) documents the cold-start failure, observable gait clocks, optimizer separation, authority handoff, strict retention, and zero-authority acceptance gates.
- [docs/RUNNER_V0731_ACTIVE_TERRAIN_CURRICULUM_ART.md](docs/RUNNER_V0731_ACTIVE_TERRAIN_CURRICULUM_ART.md) documents active terrain/contact truth, lesson-local ownership, raw Crouch learning, topology-derived assistance, all-rig art transforms, and frame-independence evidence.

A release is incomplete until Linux and Windows tests, build-tree and installed diagnostics, independent archive extraction, checksum and manifest audits, release-asset re-download, branch cleanup, and open-PR audit all pass.

## v0.7.25 compact armor and stance-leg hotfix

- Keeps the approved helmet and foot artwork while removing the oversized translucent torso bitmap.
- Builds a compact chest plate, shoulder caps, forearm guards, and cyan indicator from the real body joints.
- Prevents supported walking legs from folding until the knee appears to telescope into the pelvis.
- Reconstructs each paired leg with exact two-link geometry after restoring supported stance extension.
- Applies walking-chain integrity from startup instead of waiting through the visible first interval.
- Preserves swing-leg bend, static crouch, crouch-walk, monoped, quadruped, crawler, and hexapod motion paths.
- Isolates corrected v0.7.25 controller state and autosaves.
- Synchronizes the EpochGui logical-pixel font sizing contract at commit `130f33fe31d73564a35a622f3bb5ddcc2b5105d5`.
- Renders `%` correctly and replaces overflowing work fractions with `UPDATES/RUNS/TESTS READY` labels once each sample budget is met.

## v0.7.24 structural integrity and truthful telemetry

- Uses the exact selected gameplay screenshot crop as the canonical application icon source.
- Treats every load-bearing distance constraint as a rigid fixed-length bone.
- Performs a final post-contact structural projection and rejects excessive residual bone-length error.
- Restricts automatic refinement to motor strength and joint range; anatomy and stiffness remain fixed.
- Counts completed rollouts as passed only when they satisfy the current stage checks.
- Separates training work, repeat tests, and mastery passes so zero mastery cannot show 100% completion.
- Renames high-volume totals to simulated runs, passed/failed stage checks, and features cleared.
- Uses smaller procedural joints and limbs by default; optional overlay art remains explicitly opt-in.
- Isolates v0.7.24 autosaves and training semantics from older compressible-rig state.

## v0.7.23 true rounded-outline rendering hotfix

- Replaces the fake outer-fill/transparent-inset border with actual bounded rounded perimeter geometry.
- Keeps the center of every border-only card untouched instead of covering it with the linear-space border color.
- Restores final-composite color-diversity checks so hidden source geometry cannot certify a flat black or gray frame.
- Tests the center and edge pixels directly, then checks Live, dashboard, PIP, Rig Lab viewport, and all four Rig Lab pages.
- Preserves v0.7.21 training, rig, gait, terrain, checkpoint, and autosave semantics.

## v0.7.22 black-frame hotfix

- Restores the Live world, dashboard, training PIP, Rig Lab viewport, and all four Rig Lab pages after an opaque post-content border fill hid their interiors.
- Replaces ambiguous default-constructed UI fill colors with one explicit zero-alpha border-only fill contract.
- Makes Canvas clipping tests require real emitted triangles and validates nested clip intersections.
- Adds CPU final-frame compositing tests that fail when a later opaque rectangle hides otherwise-correct content.
- Preserves all v0.7.21 controller, rig, gait, terrain, checkpoint, and autosave semantics.

## v0.7.21 readable training dashboard

- Replaces the default raw negative-score display with a plain-language learning-health headline.
- Shows conservative lesson progress from required updates, attempts, and repeat tests.
- Translates the latest rejected test into one actionable reason without implying that saved training was lost.
- Reports stage-specific useful evidence and the exact current mastery goal.
- Explains total updates, attempts, valid attempts, resets, rollbacks, and retained champions on-screen.
- Keeps raw score, quality key, losses, optimizer state, throughput, and pipeline data on an explicit Advanced page.
- Automatic training tunes motor strength and joint range without changing anatomy, rest lengths, or structural stiffness.
- Shipped bipeds use compact side-view rest poses and gait credit requires a real behind-to-ahead support crossing.
- Quadruped and crawler presets use four articulated two-segment legs; the hexapod uses six independent tripod-phase supports.
- Rig Lab is split into Presets, Structure, Motors, and Test pages and auto-fits every preset in the editor viewport.
- v0.7.21 uses isolated autosave/training semantics; older checkpoints remain explicit transfer inputs.

## v0.7.20 UI and preview continuity

- Uses logical SDL coordinates end-to-end for readable Windows high-DPI rendering.
- Clips world and PIP geometry to their cards instead of allowing terrain or markers behind the GUI.
- Keeps the large live preview running across normal training publications and retained champion updates.
- Builds canonical C++23 source directly; the configure-time source patcher is gone.
- Embeds and packages a complete high-contrast Runner icon set.

## v0.7.19 general locomotion

- Uses a reusable material-independent locomotion strategy shared by PPO bootstrap and reward targeting.
- Values balance reserve and controlled support transfer before raw speed.
- Slows, lifts, loads, and levers over reachable ledges and plateaus instead of repeatedly striking them at fixed cadence.
- Gates running behind established walking, clear terrain, and adequate balance reserve; braking is rewarded before difficult terrain.
- Trains signed-direction reversal and flee behavior for imminent moving or thrown threats.
- Allows crawling only as an obstructed/buried emergency escape and never counts it as upright Walk/Run mastery.
- Adds low-rate falling sand to general deformable-terrain lessons; deposited sand changes the terrain through the same live SandHybrid bridge.
- The large preview follows the best validated champion when available and varies deterministic restart seeds instead of replaying one failing two-step episode forever.
- Restricts motor-discovery probes to the Balance nursery so they no longer overwrite early Walk actions for hundreds of updates.

## v0.7.18 runtime recovery

- Removes the update-10 no-champion nursery-reset contradiction; automatic restart is delayed beyond the full stage work budget while cumulative totals survive.
- Makes total updates, local updates, evaluations, resets, stage thresholds, pipeline state, and throughput understandable in the live UI.
- Restores START and useful near-course reference markers without altering the terrain simulation.
- Corrects keyboard mappings so runtime controls match this README.
- Defaults the optional fake body armor overlays off while retaining visual-only sprite feet.
- Strengthens early sagittal walking bootstrap so ordinary fore/aft alternating gait is demonstrated long enough to learn without weakening crab-walk rejection.
- Uses isolated v0.7.18 autosave paths and a bumped autonomy-state format.

## v0.7.17 retained corrections

- One physical support stub per biped leg; visible forward boots are sprites.
- Sustained sagittal side-view walking remains required; crab walking remains rejected.
- Quadrupeds must survive, hold, retract, and stably recover from the press.
- Stage advancement requires fresh updates, episodes, and evaluations.

The optional package contains only the eight remade runtime sprites under `assets/optional/runner_armor_concepts/runtime/`. The transparent remake atlas and deterministic generator remain developer-side under `tools/`; removing the optional runtime directory preserves procedural rendering and all training behavior.
