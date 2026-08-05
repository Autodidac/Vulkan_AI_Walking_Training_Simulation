from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def patch_simulation_header() -> None:
    text = read("src/simulation.hpp")
    marker = """    [[nodiscard]] inline bool stage_requires_forward_gait(CourseStage stage) noexcept
"""
    helpers = """    [[nodiscard]] inline float terrain_sample_x(float world_x,
        float course_progress) noexcept
    {
        return world_x + course_progress;
    }

    [[nodiscard]] inline float terrain_world_x(float terrain_x,
        float course_progress) noexcept
    {
        return terrain_x - course_progress;
    }

"""
    if "terrain_sample_x" not in text:
        text = replace_once(text, marker, helpers + marker,
            "canonical terrain coordinate helpers")
    write("src/simulation.hpp", text)


def patch_simulation_source() -> None:
    text = read("src/simulation.cpp")
    replacements = (
        ("return terrain_.height_at(x + course_progress());",
         "return terrain_.height_at(terrain_sample_x(x, course_progress()));",
         "ground height terrain mapping"),
        ("? terrain_.firmness_at(x + course_progress()) : 1.0f;",
         "? terrain_.firmness_at(terrain_sample_x(x, course_progress())) : 1.0f;",
         "terrain firmness mapping"),
        ("? terrain_.looseness_at(x + course_progress()) : 0.0f;",
         "? terrain_.looseness_at(terrain_sample_x(x, course_progress())) : 0.0f;",
         "terrain looseness mapping"),
        ("terrain_.deposit(item.position.x + course_progress(),",
         "terrain_.deposit(terrain_sample_x(item.position.x, course_progress()),",
         "material deposit terrain mapping"),
        ("terrain_.apply_pressure(particle.position.x + course_progress(), load, slip, dt);",
         "terrain_.apply_pressure(terrain_sample_x(particle.position.x, course_progress()),\n                load, slip, dt);",
         "support pressure terrain mapping"),
    )
    for old, new, label in replacements:
        text = replace_once(text, old, new, label)
    write("src/simulation.cpp", text)


def patch_renderer() -> None:
    path = ROOT / "src/app.cpp"
    text = path.read_text(encoding="utf-8")
    start_marker = """        void draw_course_ground(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
"""
    end_marker = """        void draw_course_reference(const sim::Environment& environment, Rect viewport,
"""
    start = text.index(start_marker)
    end = text.index(end_marker, start)
    replacement = """        void draw_course_ground(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            const sim::DeformableTerrain& terrain = environment.terrain();
            const float half_view = viewport.size.x * 0.5f / scale;
            const float left = camera - half_view - sim::DeformableTerrain::macro_tile_size;
            const float right = camera + half_view + sim::DeformableTerrain::macro_tile_size;

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
            auto draw_world_color = [&](float x0, float y0, float x1, float y1,
                Color color)
            {
                const Vec2 minimum = world_to_screen({ x0, y0 }, viewport, camera, scale);
                const Vec2 maximum = world_to_screen({ x1, y1 }, viewport, camera, scale);
                canvas.quad({ minimum.x, maximum.y }, { maximum.x, minimum.y }, color);
            };

            if (!sim::stage_uses_deformable_terrain(environment.course_stage()))
            {
                const Vec2 surface = world_to_screen({ camera, 0.0f }, viewport, camera, scale);
                canvas.quad({ viewport.position.x, surface.y },
                    viewport.position + viewport.size, rgb(0x4d392c));

                const float cell = sim::DeformableTerrain::fine_cell_spacing;
                const int first_cell = static_cast<int>(std::floor(left / cell));
                const int last_cell = static_cast<int>(std::ceil(right / cell));
                for (int column = first_cell; column <= last_cell; ++column)
                {
                    const float x0 = static_cast<float>(column) * cell;
                    const std::uint32_t hash = static_cast<std::uint32_t>(column) * 2654435761u;
                    const Color upper = (hash & 1u) == 0u
                        ? rgb(0x77543a) : rgb(0x6b4a34);
                    const Color lower = (hash & 2u) == 0u
                        ? rgb(0x604330) : rgb(0x593d2d);
                    draw_world_color(x0, -cell, x0 + cell, 0.0f, upper);
                    draw_world_color(x0, -cell * 2.0f, x0 + cell, -cell, lower);
                }
                return;
            }

            const float progress = environment.course_progress();
            const float source_left = sim::terrain_sample_x(left, progress);
            const float source_right = sim::terrain_sample_x(right, progress);
            const int first_source_macro = static_cast<int>(std::floor(
                source_left / sim::DeformableTerrain::macro_tile_size));
            const int last_source_macro = static_cast<int>(std::ceil(
                source_right / sim::DeformableTerrain::macro_tile_size));

            for (int source_macro = first_source_macro;
                source_macro <= last_source_macro; ++source_macro)
            {
                const auto wrapped_macro = static_cast<std::size_t>((
                    source_macro % static_cast<int>(sim::DeformableTerrain::macro_columns)
                    + static_cast<int>(sim::DeformableTerrain::macro_columns))
                    % static_cast<int>(sim::DeformableTerrain::macro_columns));
                const float source_macro_x0 = static_cast<float>(source_macro)
                    * sim::DeformableTerrain::macro_tile_size;
                const float macro_x0 = sim::terrain_world_x(source_macro_x0, progress);
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
                    const float macro_y1 = macro_y0
                        + sim::DeformableTerrain::macro_tile_size;
                    bool near_surface = false;
                    for (std::size_t local_x = 0;
                        local_x < sim::DeformableTerrain::macro_cell_side; ++local_x)
                    {
                        const float sample_x = macro_x0
                            + (static_cast<float>(local_x) + 0.5f)
                                * sim::DeformableTerrain::fine_cell_spacing;
                        if (macro_y1 >= environment.ground_height_at(sample_x)
                            - sim::DeformableTerrain::fine_cell_spacing * 3.0f)
                        {
                            near_surface = true;
                            break;
                        }
                    }
                    if (tile.macro_ready && !tile.active && !near_surface)
                    {
                        draw_world_cell(macro_x0, macro_y0,
                            macro_x0 + sim::DeformableTerrain::macro_tile_size,
                            macro_y1, tile.uniform_material);
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
        }

"""
    text = text[:start] + replacement + text[end:]
    write("src/app.cpp", text)


def patch_tests() -> None:
    text = read("tests/deformable_terrain_tests.cpp")
    text = replace_once(text,
        "environment.terrain_.deposit(world_x + environment.course_progress(),",
        "environment.terrain_.deposit(terrain_sample_x(world_x, environment.course_progress()),",
        "test terrain deposit mapping")
    marker = """    static_assert(sizeof(sim::Environment) < 256u * 1024u);
"""
    addition = """    static_assert(sizeof(sim::Environment) < 256u * 1024u);
    constexpr float transform_world_x = 3.25f;
    constexpr float transform_progress = 11.75f;
    constexpr float transform_source_x = sim::terrain_sample_x(
        transform_world_x, transform_progress);
    static_assert(std::abs(sim::terrain_world_x(transform_source_x,
        transform_progress) - transform_world_x) < 1.0e-6f);
"""
    text = replace_once(text, marker, addition,
        "terrain coordinate round-trip test")
    marker = """    sim::Environment environment(sim::CreatureBlueprint::quadruped(),0x5a17u);
    environment.set_course(sim::CourseStage::moving_hazards,0.80f);
    std::array<float,sim::action_count> idle{};
"""
    addition = """    sim::Environment environment(sim::CreatureBlueprint::quadruped(),0x5a17u);
    environment.set_course(sim::CourseStage::moving_hazards,0.80f);
    std::array<float,sim::action_count> idle{};
    for(int frame=0;frame<90;++frame) static_cast<void>(environment.step(idle));
    const float sync_world_x=1.75f;
    const float sync_source_x=sim::terrain_sample_x(sync_world_x,environment.course_progress());
    require(std::abs(environment.ground_height_at(sync_world_x)
        - environment.terrain().height_at(sync_source_x))<1.0e-6f,
        "render/world terrain transform disagrees with collision sampling");
"""
    text = replace_once(text, marker, addition,
        "live terrain synchronization test")
    write("tests/deformable_terrain_tests.cpp", text)


def patch_documents() -> None:
    mission = read("missioncache.md")
    marker = """- [x] Render exposed, active, and near-surface terrain as fine granular cells while retaining deep inactive uniform macro tiles.
"""
    addition = marker + """- [x] Render flat lessons from the same y=0 collision plane instead of showing the hidden deformable course map.
- [x] Use one canonical world-to-terrain treadmill transform for collision, pressure, deposits, sampling, and rendering.
"""
    mission = replace_once(mission, marker, addition,
        "terrain synchronization mission entries")
    write("missioncache.md", mission)

    changelog = read("CHANGELOG.md")
    marker = """- Preserved fine granular cells at exposed and active terrain while restricting macro-tile quads to deep inactive uniform material.
"""
    addition = marker + """- Synchronized rendered terrain with the treadmill-space collision map and rendered flat lessons from their actual y=0 collision plane.
"""
    changelog = replace_once(changelog, marker, addition,
        "terrain synchronization changelog entry")
    write("CHANGELOG.md", changelog)

    doc = read("docs/RUNNER_V0715_VIEWPORT_RECOVERY.md")
    paragraph = """
The renderer and physics now use the same canonical treadmill coordinate conversion. Deformable terrain source coordinates are shifted into world space by the exact inverse of the collision sampler, so pressure marks, deposited material, hills, feet, and obstacles remain locked together. Balance, static crouch, jump, and flip lessons use a flat y=0 collision plane and therefore render a flat compacted surface instead of exposing the unrelated deformable map.
"""
    if "canonical treadmill coordinate conversion" not in doc:
        doc = doc.rstrip() + paragraph + "\n"
    write("docs/RUNNER_V0715_VIEWPORT_RECOVERY.md", doc)


def patch_package_docs() -> None:
    text = read("CMakeLists.txt")
    old = """        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/SANDHYBRID_INTEGRATION_BRIDGE.md"
            "$<TARGET_FILE_DIR:Runner>/docs/SANDHYBRID_INTEGRATION_BRIDGE.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${runner_sandhybrid_SOURCE_DIR}/missioncache.md"
"""
    new = """        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/SANDHYBRID_INTEGRATION_BRIDGE.md"
            "$<TARGET_FILE_DIR:Runner>/docs/SANDHYBRID_INTEGRATION_BRIDGE.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md"
            "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${runner_sandhybrid_SOURCE_DIR}/missioncache.md"
"""
    text = replace_once(text, old, new,
        "build-tree terrain synchronization document")
    old = """    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/SANDHYBRID_INTEGRATION_BRIDGE.md"
        DESTINATION docs)
"""
    new = """    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/SANDHYBRID_INTEGRATION_BRIDGE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md"
        DESTINATION docs)
"""
    text = replace_once(text, old, new,
        "installed terrain synchronization document")
    write("CMakeLists.txt", text)


def main() -> None:
    patch_simulation_header()
    patch_simulation_source()
    patch_renderer()
    patch_tests()
    patch_documents()
    patch_package_docs()


if __name__ == "__main__":
    main()
