# Epoch2DWalkEngine library and Runner example

## Product layout

`Epoch2DWalkEngine` is the reusable cross-platform C++23 locomotion library. `Runner` is the complete SDL3/Vulkan example application and trainer built on that library. The split is architectural, not a feature reduction: Runner retains Live Autopilot, Rig Lab, background PPO training, persistence, diagnostics, authored rigs, modular art, and the SandHybrid course.

The library owns:

- species-aware rig topology, validation, serialization, and morphology;
- articulated particle/constraint physics, support contacts, water and terrain interaction;
- observations, actions, locomotion planning, authored gait reflexes, PPO, retention, and curriculum;
- checkpoint and autonomy persistence;
- deterministic acceptance/course/hybrid-brain/rig-training diagnostics;
- renderer-neutral modular-art transforms and species attachment metadata.
The diagnostics distinguish mandatory local articulation constraints from optional locomotion guidance. Raw audits preserve the former as plant integrity while disabling teacher, reflex, posture, swing, and course-motion authority; library consumers can inspect both through `GuidanceAuthorityReport`.


Runner owns SDL events, Vulkan resources, shaders, canvas batching, window layout, UI hit testing, fonts, panels, and packaging of executable assets. It links the library once through `Epoch2DWalkEngine::Epoch2DWalkEngine`; core sources are not compiled again into the example.

## Installed CMake usage

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyWalker LANGUAGES CXX)

find_package(Epoch2DWalkEngine 0.7 CONFIG REQUIRED)
add_executable(my_walker main.cpp)
target_link_libraries(my_walker PRIVATE
    Epoch2DWalkEngine::Epoch2DWalkEngine)
target_compile_features(my_walker PRIVATE cxx_std_23)
```

```cpp
#include <Epoch2DWalkEngine/Engine.hpp>

int main()
{
    auto rig = epoch2dwalk::sim::CreatureBlueprint::humanoid();
    epoch2dwalk::sim::Environment world{rig, 42u};
    world.set_course(epoch2dwalk::sim::CourseStage::uneven, 0.25f);
    std::array<float, epoch2dwalk::sim::action_count> action{};
    world.step(action);
}
```

The package also requires `Threads` and `SandHybrid 2.5`; its generated config resolves both before loading the exported target.

## External host and authoring API

The v0.7.50 umbrella header exports `director::TaskGraph`, `director::State`, `integration::TaskCommand`, `integration::ControlRequest`, `integration::CanonicalSnapshot`, `integration::FixedStepResult`, and `integration::DiagnosticEvents`. `integration::fixed_step()` applies a supported task only at an explicit host boundary and otherwise advances the caller-owned physical environment exactly once. Curriculum, autonomous-NPC, and player-guided hosts therefore share the same contacts, materials, equipment state, projectiles, terminal causes, and fixed-step clock.

The Director never owns a renderer or grants motion. Its stable task/challenge identity and selection reason can be sent to another project beside the canonical snapshot and action request. Unsupported construction/tool nodes are rejected rather than simulated with placeholder authority.

`art::Layout` is renderer-neutral, species-owned authoring data. `art::adjust_transform()` applies bounded module anchor, pivot, length, thickness, rotation, flip, and depth settings to the same physical transforms Runner renders in Live, PIP, and Rig Lab. Layout save/load validates schema and species identity; undo/redo is deterministic and bounded.

The public integration API version is `3`. Consumers should compare `integration::api_version` at compile time when they persist or transmit these contracts.

## Release packages

A complete release contains:

- a Windows x64 Runner example runtime ZIP;
- a Linux x86_64 Runner example runtime archive;
- Windows and Linux Epoch2DWalkEngine development packages with the static library, public headers, exported CMake config, and SandHybrid dependency package;
- one exact-commit source archive;
- SHA-256 sidecars and complete per-file manifests for every archive.

Both runtime examples must be independently extracted and launched outside the build tree. Both development packages must configure, compile, link, and run a downstream `find_package` consumer. A source archive is never presented as a runnable release.
