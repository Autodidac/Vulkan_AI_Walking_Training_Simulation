# Runner

Runner 0.7.14 is a combined C++23 SDL3/Vulkan locomotion and SandHybrid live-map laboratory with deterministic physics, a compact PPO trainer, persistent background workers, authored multi-leg rigs, deformable terrain, material hazards, and an executable acceptance matrix.

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

RunnerCore links the complete platform-neutral `SandHybrid::SandHybrid` library pinned at `99dd8acddfa9be1402981052b39cbf6284ed99ae`. The live map uses canonical fine cells, SandHybrid material identity, derived 8×8 macro-tile metadata, and 64×64 dirty-section scheduling. A primary humanoid is approximately 3–5 macro tiles tall. Full uniform 8×8 regions promote immediately; any changed or partial cell demotes immediately. Sand keeps irregular blob/pixel edges while structural stone may form a true vertical face or 90-degree ledge. Collision, observations, evaluation, replay, preview rendering, pressure, deposits, burial, and material impacts consume this same map state.

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
- [`missioncache.md`](missioncache.md) is Runner's single authoritative mission ledger with status, acceptance criteria, and immutable release evidence.
- [`docs/SANDHYBRID_INTEGRATION_BRIDGE.md`](docs/SANDHYBRID_INTEGRATION_BRIDGE.md) pins the SandHybrid library and preserves ownership of both canonical ledgers.

A release is incomplete until Linux and Windows tests, build-tree and installed diagnostics, independent archive extraction, checksum and manifest audits, release-asset re-download, branch cleanup, and open-PR audit all pass.
