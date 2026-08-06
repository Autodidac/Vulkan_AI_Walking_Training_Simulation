# Runner

Runner 0.7.16 is a combined autonomous physics locomotion trainer, rig editor, deformable-terrain laboratory, and cross-platform C++23 application.

## Build requirements

- CMake 3.28 or newer
- C++23 compiler
- Ninja or Visual Studio
- vcpkg for SDL3, Vulkan, and shaderc on Windows

The project pins and statically links the platform-neutral SandHybrid simulation library. Runner owns the SDL3/Vulkan application, rendering, training, editor, and package lifecycle.

## Windows build

```bat
cmake --preset windows-release --fresh
cmake --build --preset windows-release --parallel
ctest --preset windows-release --output-on-failure
```

Launch the Release application with:

```bat
run.bat
```

`run.bat` resolves the executable relative to itself and works from an unrelated current directory.

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

- Mouse wheel over Live Autopilot: zoom the world view without changing physical scale
- Live panel `ZOOM OUT` / `AUTO VIEW` / `ZOOM IN`: direct camera controls
- `R`: Reset the live preview and restore automatic camera fitting
- `Space`: Pause or resume the live preview
- `Tab`: Switch between Live Autopilot and Rig Lab
- `1`, `2`, `3`: Normal, Faster, and Max CPU modes
- `S`: Save the current rig
- `L`: Load a rig
- `Escape`: Quit

The live view automatically fits the current rig, maintains useful course lookahead, and uses elapsed-time camera smoothing with a screen-space dead zone. Camera magnification never changes simulation scale, terrain coordinates, observations, rewards, or learned state.

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

`--diagnose-acceptance` runs the same deterministic rig and curriculum matrix used by CTest and release-package auditing. `--diagnose-camera` validates adaptive fit, clamps, wheel zoom, lookahead, dead-zone follow, and PIP scale without opening a window.

## Repository records

- [`AGENTS.md`](AGENTS.md) defines cache-first implementation, validation, documentation, and release rules.
- [`CHANGELOG.md`](CHANGELOG.md) is the single release-history document.
- [`missioncache.md`](missioncache.md) is Runner's single authoritative mission ledger with status, acceptance criteria, and immutable release evidence.
- [`docs/SANDHYBRID_INTEGRATION_BRIDGE.md`](docs/SANDHYBRID_INTEGRATION_BRIDGE.md) pins the SandHybrid library and preserves ownership of both canonical ledgers.
- [`docs/RUNNER_V0716_CAMERA_BATCH.md`](docs/RUNNER_V0716_CAMERA_BATCH.md) documents the adaptive live and PIP camera contract.
- [`docs/RUNNER_V0717_EYE_TEST_CORRECTION.md`](docs/RUNNER_V0717_EYE_TEST_CORRECTION.md) documents the released eye-test corrections for crouch recovery, sustained sagittal gait, terrain-conforming support stubs, and supplied optional sprites.

A release is incomplete until Linux and Windows tests, build-tree and installed diagnostics, independent archive extraction, checksum and manifest audits, release-asset re-download, branch cleanup, and open-PR audit all pass.

## v0.7.17 eye-test corrections

- One physical support stub per biped leg; visible forward boots are sprites.
- Sustained sagittal side-view walking is required; crab walking is rejected.
- Quadrupeds must survive, hold, retract, and stably recover from the press.
- Stage advancement requires fresh updates, episodes, and evaluations.
- OPTIONAL ART from all four supplied concepts is packaged with a procedural fallback.
- Rig Lab exposes optional-art and debug-skeleton toggles.

The optional package includes compact P3 reference sheets derived from all four supplied concepts under `assets/optional/runner_armor_concepts/source/`, plus validated P3 foot, helmet, torso, and weapon-preview sprites under `runtime/`. Removing the entire optional directory preserves procedural rendering and all training behavior.
