# Runner

Runner 0.7.23 is a combined autonomous physics locomotion trainer, rig editor, deformable-terrain laboratory, and cross-platform C++23 application.

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
- `A`: toggle optional torso/helmet/weapon overlays; foot sprites remain independent
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
```

`--diagnose-acceptance` runs the deterministic rig/curriculum matrix used by package auditing. `--diagnose-camera` validates adaptive fit, clamps, wheel zoom, lookahead, dead-zone follow, and PIP scale. `--diagnose-ui` CPU-composites representative Live and all four Rig Lab pages and fails if any content region is black or visually empty.

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

A release is incomplete until Linux and Windows tests, build-tree and installed diagnostics, independent archive extraction, checksum and manifest audits, release-asset re-download, branch cleanup, and open-PR audit all pass.

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
- Automatic training tunes motor strength, joint range, and stiffness without changing the character's anatomy.
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

The optional package contains derived visual references and runtime sprites under `assets/optional/runner_armor_concepts/`. Removing the optional directory preserves procedural rendering and all training behavior.
