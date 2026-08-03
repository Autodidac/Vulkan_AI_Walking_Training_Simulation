# Runner

Runner 0.7.10 is a C++23 SDL3/Vulkan locomotion laboratory with deterministic physics, a compact PPO trainer, persistent background workers, authored multi-leg rigs, deformable terrain, material hazards, and an executable acceptance matrix.

## Current curriculum

Training uses eight evidence-gated stages:

1. stand and balance;
2. static crouch, hold, and recover;
3. walk and run with real gait cycles;
4. crouch-walk over deformable uneven terrain and avoid low obstacles;
5. powered jump and controlled landing;
6. moving low-bar or hurdle traversal;
7. controlled flips with no more than three turns and a valid landing;
8. mixed traversal that preserves earlier skills.

Scalar reward cannot skip prerequisites. Wheel-sliding, body rolling, detached or fused supports, uncontrolled flight, excessive flips, motionless exploits, and invalid body contact cannot seed champion, imitation, evolution, or training-preview state.

## Authored rigs

The built-in presets are chicken, biped, humanoid, quadruped, four-leg crawler, hexapod, and monoped. Every preset has explicit support semantics and a rig-specific control path. The monoped uses a single-leg gait cycle rather than fake alternating biped steps.

## Runtime model

- The training worker owns mutable PPO, optimizer, curriculum, rig-evolution, and persistence state.
- The UI renders immutable publications and does not block on training updates.
- NORMAL, FASTER, and MAX select persistent CPU worker budgets.
- Vulkan is used for presentation; the compact policy and optimizer remain on CPU.
- Checkpoints and autosaves are versioned and written asynchronously through temporary-file and atomic-rename replacement.

## Terrain and hazards

A deterministic deformable sand heightfield drives collision, observations, evaluation, replay, live rendering, and the training PIP. Foot pressure compacts and displaces material. Falling sand deposits into terrain; rocks and debris bounce, roll, settle, and transfer impact velocity. Burial, obstruction, incoming material, and escape direction are observable, and no-escape burial terminates honestly.

## Controls

- `1`: Live Autopilot
- `2` or `3`: Rig Lab
- `Space`: Pause or resume background training
- `R`: Reset the live preview
- `S`: Save the current rig
- `L`: Load a rig
- `Delete`: Remove the selected non-required node
- `Shift + click`: Add a node
- `Ctrl + click`: Connect the selected node to another node

## Build and test

Requirements: CMake 3.28+, a C++23 compiler, Ninja, Vulkan 1.3+, SDL3, shaderc, and vcpkg manifest mode for the complete application.

Windows:

```powershell
cmake --preset windows-release --fresh
cmake --build --preset windows-release --parallel
ctest --preset windows-release --output-on-failure
```

Linux deterministic core suite:

```bash
cmake -S . -B build/linux -G Ninja \
  -DRUNNER_BUILD_APP=OFF \
  -DRUNNER_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --parallel
ctest --test-dir build/linux --output-on-failure
```

## Run and diagnose

Double-click `run.bat` on Windows. An extracted release launches the adjacent executable; a source checkout uses or builds the configured Release target. Arguments are forwarded.

Useful diagnostics:

```text
Runner.exe --version
Runner.exe --diagnose-vulkan
Runner.exe --diagnose-package
Runner.exe --diagnose-acceptance
```

`--diagnose-acceptance` runs the same deterministic rig and curriculum matrix used by CTest and release-package auditing.

## Repository records

- [`CHANGELOG.md`](CHANGELOG.md) is the single release-history document.
- [`missioncache.md`](missioncache.md) is the single authoritative mission ledger with status, acceptance criteria, and immutable release evidence.

A release is incomplete until Linux and Windows tests, build-tree and installed diagnostics, independent archive extraction, checksum and manifest audits, release-asset re-download, branch cleanup, and open-PR audit all pass.
