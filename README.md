# EpochRunner

EpochRunner is a standalone C++23 Vulkan 1.3 locomotion laboratory built on
[EpochGui](https://github.com/Autodidac/EpochGui). It recreates the complete
workflow demonstrated in Pezzza's Work's **AI Chicken learns to RUN** video:
an articulated creature editor, motorized 2D simulation, parallel PPO training,
live metrics, policy persistence, and a focused run view.

The implementation and chicken artwork are original. It does not copy the
creator's private Patreon source code or assets.

## Implemented

- Vulkan 1.3 dynamic rendering with SDL3 window/surface integration
- Two persistently mapped vertex buffers, one per in-flight frame
- EpochGui split layouts, input tracking, rounded geometry, bitmap font, and PPM image layout
- Interactive articulated-rig editor
  - drag joints
  - Shift-click to add a node
  - Ctrl-click another node to connect a bone
  - Delete removes user-added nodes
  - edit node radius and four motor limits/strengths
  - save/load `.epochrig`
- Deterministic position-based 2D physics
  - Verlet integration
  - distance constraints
  - angle motors
  - friction, ground contact, fall/reset detection
- Real PPO trainer, not scripted playback
  - shared 64x64 actor-critic MLP
  - Gaussian four-motor action policy
  - generalized advantage estimation
  - clipped PPO objective
  - Adam optimizer
  - 32 parallel environments and 128 rollout steps per update
- Live reward/speed graphs and multi-agent training viewport
- Save/load `.eppo` policies
- Deterministic single-agent run view with speed and distance display
- Core tests for simulation stability, PPO updates, and policy serialization

## Requirements

- CMake 3.28+
- C++23 compiler with CMake module support
  - MSVC 19.38+
  - GCC 14+
  - Clang 18+
- vcpkg checkout with `vcpkg.exe`/`vcpkg` bootstrapped
- Vulkan-capable graphics driver
- Ninja 1.11+

EpochGui is pinned to commit `347ad52e8fc27deb08dea97e56a9b6d8c0db3af2`
when this directory is built outside the EpochGui source tree. When built from
`EpochGui/examples/EpochRunner`, the parent EpochGui target is used directly.

## Windows build

```bat
set VCPKG_ROOT=C:\path\to\vcpkg
build_windows.bat
```

`build_windows.bat` automatically uses `%USERPROFILE%\source\repos\vcpkg`
when `VCPKG_ROOT` is not already set. CMake manifest mode installs SDL3,
Vulkan Loader/Headers, and shaderc into the project-local `vcpkg_installed`
directory. No Vulkan SDK or external `glslc.exe` is required.

Or manually:

```bat
cmake --preset windows-release
cmake --build --preset windows-release
ctest --test-dir build/windows-release --output-on-failure
```

## Linux build

```bash
export VCPKG_ROOT="$HOME/vcpkg"
./build_linux.sh
```

## Dependency-free core validation

The physics and PPO core can be configured without SDL3, Vulkan, or EpochGui:

```bash
cmake --preset core-tests
cmake --build --preset core-tests
ctest --preset core-tests
```

## Controls

| Input | Action |
|---|---|
| `1`, `2`, `3` | Editor, training, run mode |
| Left drag | Move an editor joint |
| Shift + left click | Add a node |
| Ctrl + left click | Connect selected node to clicked node |
| Delete | Remove a user-added node |
| `S`, `L` | Save/load rig in editor mode |
| Space | Start/pause training or pause run preview |
| `R` | Reset run preview |
| Escape | Exit |

## Architecture

```text
src/math.hpp             allocation-free math primitives
src/simulation.*         deterministic articulated PBD environment
src/ppo_network.cpp      actor-critic network
src/ppo_trainer.cpp      parallel PPO trainer
src/canvas.cpp           triangle canvas
src/renderer.*           SDL3/Vulkan 1.3 renderer
src/app.*                EpochGui-driven editor/training/run application
tools/shader_compiler.cpp vcpkg shaderc GLSL-to-SPIR-V build tool
shaders/                 minimal vertex-color Vulkan shaders
assets/chicken.ppm       original bounded PPM UI asset
tests/core_tests.cpp     deterministic headless validation
```

EpochGui remains renderer-neutral. EpochRunner consumes its reusable layout,
input, font, image, and rounded-geometry modules while owning Vulkan, SDL3,
shaders, application state, physics, and reinforcement learning.
