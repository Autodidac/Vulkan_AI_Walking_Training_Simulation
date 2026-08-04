from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
PIN = "99dd8acddfa9be1402981052b39cbf6284ed99ae"


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def patch_cmake() -> None:
    text = read("CMakeLists.txt")
    text = replace_once(text,
        "project(Runner VERSION 0.7.13 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.14 LANGUAGES CXX)",
        "Runner project version")
    text = replace_once(text,
        "find_package(Threads REQUIRED)\n\nadd_library(RunnerCore STATIC",
        f'''find_package(Threads REQUIRED)

include(FetchContent)
set(SANDHYBRID_BUILD_APP OFF CACHE BOOL "" FORCE)
set(SANDHYBRID_BUILD_VULKAN_RUNTIME OFF CACHE BOOL "" FORCE)
set(SANDHYBRID_WARNINGS_AS_ERRORS ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(runner_sandhybrid
    GIT_REPOSITORY https://github.com/Autodidac/EpochSimEngine.git
    GIT_TAG {PIN}
    GIT_SHALLOW FALSE
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE FALSE)
FetchContent_MakeAvailable(runner_sandhybrid)
if(NOT TARGET SandHybrid::SandHybrid)
    message(FATAL_ERROR "Pinned SandHybrid core target was not created")
endif()

add_library(RunnerCore STATIC''',
        "pinned SandHybrid FetchContent block")
    text = replace_once(text,
        "target_link_libraries(RunnerCore PUBLIC Threads::Threads)",
        "target_link_libraries(RunnerCore PUBLIC Threads::Threads SandHybrid::SandHybrid)",
        "RunnerCore SandHybrid link")
    text = replace_once(text,
        "target_compile_features(RunnerCore PUBLIC cxx_std_23)",
        f'''target_compile_features(RunnerCore PUBLIC cxx_std_23)
target_compile_definitions(RunnerCore PUBLIC
    RUNNER_SANDHYBRID_SOURCE_COMMIT="{PIN}")''',
        "SandHybrid pin compile definition")
    text = replace_once(text,
        '''        COMMAND ${{CMAKE_COMMAND}} -E copy_directory "${{CMAKE_CURRENT_SOURCE_DIR}}/assets" "$<TARGET_FILE_DIR:Runner>/assets"
        COMMAND ${{CMAKE_COMMAND}} -E copy_directory "${{RUNNER_SHADER_OUTPUT_DIR}}" "$<TARGET_FILE_DIR:Runner>/shaders"
        VERBATIM)'''.replace("${{", "${").replace("}}", "}"),
        '''        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:Runner>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${RUNNER_SHADER_OUTPUT_DIR}" "$<TARGET_FILE_DIR:Runner>/shaders"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Runner>/docs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/SANDHYBRID_INTEGRATION_BRIDGE.md"
            "$<TARGET_FILE_DIR:Runner>/docs/SANDHYBRID_INTEGRATION_BRIDGE.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${runner_sandhybrid_SOURCE_DIR}/missioncache.md"
            "$<TARGET_FILE_DIR:Runner>/docs/SandHybrid-missioncache.md"
        VERBATIM)''',
        "package document copy")
    text = replace_once(text,
        '''    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION assets)
endif()''',
        '''    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION assets)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/SANDHYBRID_INTEGRATION_BRIDGE.md"
        DESTINATION docs)
    install(FILES "${runner_sandhybrid_SOURCE_DIR}/missioncache.md"
        DESTINATION docs RENAME SandHybrid-missioncache.md)
endif()''',
        "install combined project ledgers")
    marker = '''    add_executable(RunnerDeformableTerrainTests tests/deformable_terrain_tests.cpp)
    target_link_libraries(RunnerDeformableTerrainTests PRIVATE Runner::Core)
    target_compile_features(RunnerDeformableTerrainTests PRIVATE cxx_std_23)
    set_target_properties(RunnerDeformableTerrainTests PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    runner_enable_warnings(RunnerDeformableTerrainTests)
    add_test(NAME Runner.DeformableTerrain COMMAND RunnerDeformableTerrainTests)
    set_tests_properties(Runner.DeformableTerrain PROPERTIES TIMEOUT 45)
'''
    addition = marker + '''
    add_executable(RunnerSandHybridIntegrationTests
        tests/sandhybrid_integration_tests.cpp)
    target_link_libraries(RunnerSandHybridIntegrationTests PRIVATE Runner::Core)
    target_compile_features(RunnerSandHybridIntegrationTests PRIVATE cxx_std_23)
    set_target_properties(RunnerSandHybridIntegrationTests PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    runner_enable_warnings(RunnerSandHybridIntegrationTests)
    add_test(NAME Runner.SandHybridIntegration COMMAND RunnerSandHybridIntegrationTests)
    set_tests_properties(Runner.SandHybridIntegration PROPERTIES TIMEOUT 60)
'''
    text = replace_once(text, marker, addition, "SandHybrid integration test target")
    write("CMakeLists.txt", text)


def patch_terrain_header() -> None:
    text = read("src/deformable_terrain.hpp")
    text = replace_once(text,
        "static constexpr float fine_cell_spacing = 0.125f;",
        "static constexpr float fine_cell_spacing = 0.140625f;",
        "fine-cell world scale")
    text = replace_once(text,
        "static constexpr float world_bottom = -4.0f;",
        "static constexpr float world_bottom = -4.5f;",
        "terrain vertical origin")
    text = replace_once(text,
        "static_assert(macro_tile_size == 1.0f);",
        "static_assert(macro_tile_size == 1.125f);",
        "macro tile scale contract")
    text = replace_once(text,
        '''                if (!target->occupied())
                    set_cell(*target, material, structural, 0.0f);
                const float capacity = (1.0f - target->fill) * fine_cell_spacing;''',
        '''                if (!target->occupied())
                {
                    target->material_id = static_cast<std::uint8_t>(material);
                    target->flags = structural ? FineCell::structural_flag : 0u;
                    target->fill = 0.0f;
                }
                const float capacity = (1.0f - target->fill) * fine_cell_spacing;''',
        "empty fine-cell volume insertion")
    old_volume = '''        [[nodiscard]] float total_height_volume() const noexcept
        {
            float result = 0.0f;
            for (const FineCell& cell : fine_cells_)
                result += cell.fill * fine_cell_spacing;
            return result;
        }'''
    new_volume = '''        [[nodiscard]] float total_height_volume() const noexcept
        {
            double result = 0.0;
            for (const Cell& column : cells_)
                result += static_cast<double>(column.height);
            return static_cast<float>(result);
        }'''
    text = replace_once(text, old_volume, new_volume,
        "precision-safe represented terrain volume")
    write("src/deformable_terrain.hpp", text)


def patch_simulation_api() -> None:
    text = read("src/simulation.hpp")
    text = replace_once(text,
        '''        [[nodiscard]] float terrain_firmness_at(float x) const noexcept;
        [[nodiscard]] float terrain_looseness_at(float x) const noexcept;''',
        '''        [[nodiscard]] float terrain_firmness_at(float x) const noexcept;
        [[nodiscard]] float terrain_looseness_at(float x) const noexcept;
        [[nodiscard]] const DeformableTerrain& terrain() const noexcept { return terrain_; }''',
        "live terrain accessor")
    write("src/simulation.hpp", text)


def patch_live_renderer() -> None:
    text = read("src/app.cpp")
    start = text.index("        void draw_course_ground(const sim::Environment& environment, Rect viewport,\n")
    end = text.index("        void draw_course_reference(const sim::Environment& environment, Rect viewport,\n", start)
    replacement = '''        void draw_course_ground(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            const sim::DeformableTerrain& terrain = environment.terrain();
            const float half_view = viewport.size.x * 0.5f / scale;
            const float left = camera - half_view - sim::DeformableTerrain::macro_tile_size;
            const float right = camera + half_view + sim::DeformableTerrain::macro_tile_size;
            const int first_macro = static_cast<int>(std::floor(
                left / sim::DeformableTerrain::macro_tile_size));
            const int last_macro = static_cast<int>(std::ceil(
                right / sim::DeformableTerrain::macro_tile_size));

            auto material_color = [](sandhybrid::Material material)
            {
                const sandhybrid::Rgb8 source = sandhybrid::material_editor_color(
                    static_cast<std::uint32_t>(material));
                const std::uint32_t packed = (static_cast<std::uint32_t>(source.r) << 16u)
                    | (static_cast<std::uint32_t>(source.g) << 8u)
                    | static_cast<std::uint32_t>(source.b);
                return rgb(packed);
            };
            auto draw_world_cell = [&](float x0, float y0, float x1, float y1,
                sandhybrid::Material material)
            {
                const Vec2 minimum = world_to_screen({ x0, y0 }, viewport, camera, scale);
                const Vec2 maximum = world_to_screen({ x1, y1 }, viewport, camera, scale);
                canvas.quad({ minimum.x, maximum.y }, { maximum.x, minimum.y },
                    material_color(material));
            };

            for (int world_macro = first_macro; world_macro <= last_macro; ++world_macro)
            {
                const auto wrapped_macro = static_cast<std::size_t>((
                    world_macro % static_cast<int>(sim::DeformableTerrain::macro_columns)
                    + static_cast<int>(sim::DeformableTerrain::macro_columns))
                    % static_cast<int>(sim::DeformableTerrain::macro_columns));
                const float macro_x0 = static_cast<float>(world_macro)
                    * sim::DeformableTerrain::macro_tile_size;
                for (std::size_t macro_y = 0;
                    macro_y < sim::DeformableTerrain::macro_rows; ++macro_y)
                {
                    const sim::DeformableTerrain::MacroTile& tile =
                        terrain.macro_tile(wrapped_macro, macro_y);
                    if (tile.occupied_mask == 0u)
                        continue;
                    const float macro_y0 = sim::DeformableTerrain::world_bottom
                        + static_cast<float>(macro_y)
                            * sim::DeformableTerrain::macro_tile_size;
                    if (tile.macro_ready)
                    {
                        draw_world_cell(macro_x0, macro_y0,
                            macro_x0 + sim::DeformableTerrain::macro_tile_size,
                            macro_y0 + sim::DeformableTerrain::macro_tile_size,
                            tile.uniform_material);
                        continue;
                    }

                    for (std::size_t local_y = 0;
                        local_y < sim::DeformableTerrain::macro_cell_side; ++local_y)
                    {
                        for (std::size_t local_x = 0;
                            local_x < sim::DeformableTerrain::macro_cell_side; ++local_x)
                        {
                            const std::size_t fine_x = wrapped_macro
                                * sim::DeformableTerrain::macro_cell_side + local_x;
                            const std::size_t fine_y = macro_y
                                * sim::DeformableTerrain::macro_cell_side + local_y;
                            const sim::DeformableTerrain::FineCell& cell =
                                terrain.fine_cell(fine_x, fine_y);
                            if (!cell.occupied())
                                continue;
                            const float x0 = macro_x0 + static_cast<float>(local_x)
                                * sim::DeformableTerrain::fine_cell_spacing;
                            const float y0 = sim::DeformableTerrain::row_world_bottom(fine_y);
                            draw_world_cell(x0, y0,
                                x0 + sim::DeformableTerrain::fine_cell_spacing,
                                y0 + sim::DeformableTerrain::fine_cell_spacing * cell.fill,
                                cell.material());
                        }
                    }
                }
            }

            std::vector<Vec2> surface{};
            const int first_column = static_cast<int>(std::floor(
                left / sim::DeformableTerrain::fine_cell_spacing));
            const int last_column = static_cast<int>(std::ceil(
                right / sim::DeformableTerrain::fine_cell_spacing));
            surface.reserve(static_cast<std::size_t>(std::max(0,
                last_column - first_column + 1)));
            for (int column = first_column; column <= last_column; ++column)
            {
                const float x = static_cast<float>(column)
                    * sim::DeformableTerrain::fine_cell_spacing;
                surface.push_back(world_to_screen({ x, environment.ground_height_at(x) },
                    viewport, camera, scale));
            }
            if (surface.size() >= 2u)
                canvas.polyline(surface, 1.5f, rgb(0x5d6870, 0.72f));
        }

'''
    text = text[:start] + replacement + text[end:]
    write("src/app.cpp", text)


def patch_package_diagnostic() -> None:
    text = read("src/main.cpp")
    text = replace_once(text,
        '''        const std::array required_files{
            std::filesystem::path{ RUNNER_SHADER_DIRECTORY } / "flat.vert.spv",
            std::filesystem::path{ RUNNER_SHADER_DIRECTORY } / "flat.frag.spv"
        };''',
        '''        const std::array required_files{
            std::filesystem::path{ RUNNER_SHADER_DIRECTORY } / "flat.vert.spv",
            std::filesystem::path{ RUNNER_SHADER_DIRECTORY } / "flat.frag.spv",
            std::filesystem::path{ "docs" } / "SANDHYBRID_INTEGRATION_BRIDGE.md",
            std::filesystem::path{ "docs" } / "SandHybrid-missioncache.md"
        };''',
        "combined package documents")
    write("src/main.cpp", text)


def patch_versioned_state() -> None:
    text = read("src/ppo.hpp")
    text = replace_once(text,
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1300u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1400u;",
        "combined training semantics")
    write("src/ppo.hpp", text)

    text = read("src/app.cpp")
    for old, new in (
        ("runner-v0713-autosave.eppo", "runner-v0714-autosave.eppo"),
        ("runner-v0713-evolved.rig", "runner-v0714-evolved.rig"),
        ("runner-v0713-autonomy.state", "runner-v0714-autonomy.state"),
    ):
        text = replace_once(text, old, new, f"state path {old}")
    write("src/app.cpp", text)


def patch_repository_audit() -> None:
    text = read("tools/repository_audit.cmake")
    text = replace_once(text,
        "foreach(required IN ITEMS CHANGELOG.md missioncache.md README.md)",
        "foreach(required IN ITEMS CHANGELOG.md missioncache.md README.md docs/SANDHYBRID_INTEGRATION_BRIDGE.md)",
        "integration bridge repository requirement")
    text = text.replace("project(Runner VERSION 0.7.13 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.14 LANGUAGES CXX)")
    text = text.replace("CMake project version is not 0.7.13",
        "CMake project version is not 0.7.14")
    text = replace_once(text,
        '''foreach(reference IN ITEMS "CHANGELOG.md" "missioncache.md" "--diagnose-acceptance")''',
        '''foreach(reference IN ITEMS "CHANGELOG.md" "missioncache.md" "SANDHYBRID_INTEGRATION_BRIDGE.md" "--diagnose-acceptance")''',
        "README integration reference audit")
    write("tools/repository_audit.cmake", text)


def patch_docs() -> None:
    readme = read("README.md")
    readme = replace_once(readme,
        "Runner 0.7.13 is a C++23 SDL3/Vulkan locomotion laboratory",
        "Runner 0.7.14 is a combined C++23 SDL3/Vulkan locomotion and SandHybrid live-map laboratory",
        "README version and combined identity")
    old_terrain = '''A deterministic deformable sand heightfield drives collision, observations, evaluation, replay, live rendering, and the training PIP. Foot pressure compacts and displaces material. Falling sand deposits into terrain; rocks and debris bounce, roll, settle, and transfer impact velocity. Burial, obstruction, incoming material, and escape direction are observable, and no-escape burial terminates honestly.'''
    new_terrain = f'''RunnerCore links the complete platform-neutral `SandHybrid::SandHybrid` library pinned at `{PIN}`. The live map uses canonical fine cells, SandHybrid material identity, derived 8×8 macro-tile metadata, and 64×64 dirty-section scheduling. A primary humanoid is approximately 3–5 macro tiles tall. Full uniform 8×8 regions promote immediately; any changed or partial cell demotes immediately. Sand keeps irregular blob/pixel edges while structural stone may form a true vertical face or 90-degree ledge. Collision, observations, evaluation, replay, preview rendering, pressure, deposits, burial, and material impacts consume this same map state.'''
    readme = replace_once(readme, old_terrain, new_terrain,
        "README live-map terrain model")
    readme = replace_once(readme,
        '''- [`missioncache.md`](missioncache.md) is the single authoritative mission ledger with status, acceptance criteria, and immutable release evidence.''',
        '''- [`missioncache.md`](missioncache.md) is Runner's single authoritative mission ledger with status, acceptance criteria, and immutable release evidence.
- [`docs/SANDHYBRID_INTEGRATION_BRIDGE.md`](docs/SANDHYBRID_INTEGRATION_BRIDGE.md) pins the SandHybrid library and preserves ownership of both canonical ledgers.''',
        "README integration bridge reference")
    write("README.md", readme)

    changelog = read("CHANGELOG.md")
    marker = "## [0.7.13] - 2026-08-04\n"
    entry = f'''## [0.7.14] - 2026-08-04

### Added

- Linked the complete platform-neutral `SandHybrid::SandHybrid` library at pinned commit `{PIN}` into RunnerCore.
- Added a live canonical fine-cell terrain with derived 8×8 macro metadata and SandHybrid 64×64 dirty-section scheduling.
- Added package-preserved integration ownership and upstream mission-ledger bridge documentation.

### Changed

- Rescaled terrain so chicken, biped, and humanoid bodies occupy approximately 3–5 macro tiles.
- Replaced the smooth preview fill with the same macro/fine pixel terrain used by collision and training.
- Preserved irregular granular edges while allowing deterministic structural 90-degree ledges.
- Folded the validated v0.7.13 toe command/hinge rate gates into the combined release and isolated v0.7.14 learned state.

'''
    changelog = replace_once(changelog, marker, entry + marker,
        "v0.7.14 changelog entry")
    write("CHANGELOG.md", changelog)

    cache = read("missioncache.md")
    cache = re.sub(r"^\*\*Target:\*\*.*$", "**Target:** Runner v0.7.14",
        cache, count=1, flags=re.MULTILINE)
    cache = re.sub(r"^\*\*Release state:\*\*.*$",
        "**Release state:** IMPLEMENTING — validated toe-rate correction is folded forward; Runner now integrates the pinned SandHybrid core and replaces its preview heightfield with the live 8x8-cell/macro training map.",
        cache, count=1, flags=re.MULTILINE)
    marker_text = "## v0.7.14 SandHybrid live-map integration"
    if marker_text not in cache:
        cache = cache.rstrip() + f'''

{marker_text}

### WALK-SANDLIB-129 — Link the complete platform-neutral SandHybrid library
**Status:** IMPLEMENTED — VALIDATION REQUIRED

RunnerCore links `SandHybrid::SandHybrid` pinned at `{PIN}` with SandHybrid native startup and Vulkan runtime disabled. Runner retains its SDL3/Vulkan application ownership while using SandHybrid material, terrain-generation, and sparse-section contracts. The pin, API identity, Linux/Windows builds, and static linkage must be package-verified.

### WALK-TERRAINSCALE-130 — Make rigs 3–5 macro tiles tall
**Status:** IMPLEMENTED — VALIDATION REQUIRED

One macro tile is exactly 8×8 fine cells. Chicken, biped, and humanoid authored height must remain between three and five macro tiles. The camera and collision world use the same scale; terrain may not be enlarged only cosmetically.

### WALK-HYBRIDMAP-131 — Canonical cells with instant macro promotion and demotion
**Status:** IMPLEMENTED — VALIDATION REQUIRED

Fine cells remain authoritative. A full uniform 8×8 region promotes immediately to derived macro metadata; changing or partially filling one cell demotes it immediately. Pressure, deposit, settling, material identity, structural state, volume conservation, promotion telemetry, and demotion telemetry require deterministic tests.

### WALK-LIVEMAP-132 — Train and render against the same SandHybrid map
**Status:** IMPLEMENTED — VALIDATION REQUIRED

Collision, observations, burial, material impacts, preview, training PIP, and live rendering consume one terrain state. The renderer batches macro-ready tiles and draws fine cells only for partial/mixed regions. No separate decorative heightfield is allowed.

### WALK-BRIDGE-133 — Preserve both canonical mission ledgers
**Status:** IMPLEMENTED — VALIDATION REQUIRED

`docs/SANDHYBRID_INTEGRATION_BRIDGE.md` pins the upstream commit and ownership boundary. Runner packaging includes that bridge and the pinned SandHybrid `missioncache.md`. No upstream `OPEN`, `PARTIAL`, `REGRESSION`, or `DEFERRED` mission is copied into history, renamed away, or marked complete by integration.

### WALK-CLIMB-134 — Reachable ledge climb and backward controlled descent
**Status:** OPEN — CARRIED FORWARD, NOT ORPHANED

After the combined live map is accepted, add a hard-wall curriculum where a rig climbs without jumping when its hands can reach a ledge at any ledge height, and turns backward to lower itself from a ledge when the remaining fall is no greater than its standing height. Completion requires hand/ledge contact, support transfer, no powered takeoff, and controlled feet-first recovery.

### WALK-RELEASE-135 — Publish the combined Runner v0.7.14 package
**Status:** BLOCKED — IMPLEMENTATION VALIDATION AND PACKAGE ACCEPTANCE REQUIRED

Require pinned-library retrieval, Linux warnings-as-errors, full Windows SDL3/Vulkan build, SandHybrid API/scale/macro/fine/volume contracts, all existing Runner tests, all seven Stand and static Crouch gates, build-tree/installed/extracted diagnostics, both ledgers in the package, checksum/manifest audit, and clean branch/PR state.
'''
    write("missioncache.md", cache)


def remove_superseded_publishers() -> None:
    for relative in (
        ".github/workflows/publish-runner-v0713.yml",
        "tools/publish_v0713.ps1",
        "tools/patch_publish_v0713.py",
        "tools/patch_publish_selectstring_v0713.py",
        "PUBLISH_V0713.trigger",
    ):
        path = ROOT / relative
        if path.exists():
            path.unlink()


def main() -> None:
    patch_cmake()
    patch_terrain_header()
    patch_simulation_api()
    patch_live_renderer()
    patch_package_diagnostic()
    patch_versioned_state()
    patch_repository_audit()
    patch_docs()
    remove_superseded_publishers()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
