#include "deformable_terrain.hpp"
#include "simulation.hpp"

#include <sandhybrid/library.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner SandHybrid integration test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    [[nodiscard]] float blueprint_height(const runner::sim::CreatureBlueprint& blueprint)
    {
        float minimum = std::numeric_limits<float>::max();
        float maximum = std::numeric_limits<float>::lowest();
        for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
        {
            const float radius = index < blueprint.radii.size()
                ? blueprint.radii[index] : 0.12f;
            minimum = std::min(minimum, blueprint.nodes[index].y - radius);
            maximum = std::max(maximum, blueprint.nodes[index].y + radius);
        }
        return maximum - minimum;
    }

    void require_actor_scale(const runner::sim::CreatureBlueprint& blueprint,
        std::string_view name)
    {
        const float tiles = runner::sim::DeformableTerrain::actor_height_in_macro_tiles(
            blueprint_height(blueprint));
        if (tiles >= 3.0f && tiles <= 5.0f)
            return;
        std::cerr << name << " occupies " << tiles
            << " macro tiles; expected 3..5\n";
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    using runner::sim::DeformableTerrain;

    require(sandhybrid::library_api_version == 3u,
        "unexpected SandHybrid library API version");
    require(sandhybrid::library_name == "SandHybrid",
        "wrong linked SandHybrid library identity");
    require(sandhybrid::core_library_capabilities.native_startup_owned_by_consumer,
        "SandHybrid attempted to own Runner startup");
    require(!sandhybrid::core_library_capabilities.vulkan_required,
        "platform-neutral SandHybrid core unexpectedly requires its own Vulkan runtime");

    require(DeformableTerrain::macro_cell_side == 8u,
        "macro tile is not exactly 8x8 fine cells");
    require(std::abs(DeformableTerrain::macro_tile_size
            - DeformableTerrain::fine_cell_spacing * 8.0f) < 1.0e-7f,
        "macro and fine-cell scale diverged");
    require_actor_scale(runner::sim::CreatureBlueprint::chicken(), "chicken");
    require_actor_scale(runner::sim::CreatureBlueprint::biped(), "biped");
    require_actor_scale(runner::sim::CreatureBlueprint::humanoid(), "humanoid");

    DeformableTerrain terrain{};
    terrain.reset(0x7145A11Du, 0.72f);
    require(terrain.resident_section_count() > 0u,
        "SandHybrid sparse-section scheduler received no live terrain");
    require(terrain.macro_ready_count() > 0u,
        "no full 8x8 region promoted to macro metadata");
    require(terrain.hard_ledge_count() > 0u,
        "deterministic map contains no structural 90-degree ledge");
    require(terrain.irregular_boundary_count() > 0u,
        "terrain surface snapped entirely to macro-tile stair steps");

    std::size_t selected = DeformableTerrain::macro_tile_count;
    for (std::size_t index = 0; index < terrain.macro_tiles().size(); ++index)
    {
        const auto& tile = terrain.macro_tiles()[index];
        if (tile.macro_ready && tile.uniform_material != sandhybrid::Material::empty)
        {
            selected = index;
            break;
        }
    }
    require(selected < DeformableTerrain::macro_tile_count,
        "no deterministic macro tile available for promotion/demotion proof");

    const std::size_t macro_x = selected % DeformableTerrain::macro_columns;
    const std::size_t macro_y = selected / DeformableTerrain::macro_columns;
    const auto original = terrain.macro_tile(macro_x, macro_y);
    const std::size_t fine_x = macro_x * DeformableTerrain::macro_cell_side + 3u;
    const std::size_t fine_y = macro_y * DeformableTerrain::macro_cell_side + 3u;
    const auto original_cell = terrain.fine_cell(fine_x, fine_y);

    require(terrain.erase_cell(fine_x, fine_y),
        "single fine cell could not demote a full macro tile");
    require(!terrain.macro_tile(macro_x, macro_y).macro_ready,
        "macro tile did not demote immediately after one cell changed");
    require(terrain.macro_demotions() > 0u,
        "macro demotion telemetry did not advance");

    require(terrain.paint_cell(fine_x, fine_y,
            original_cell.material(), original_cell.structural()),
        "erased fine cell could not be restored");
    require(terrain.macro_tile(macro_x, macro_y).macro_ready,
        "restored uniform 8x8 region did not promote immediately");
    require(terrain.macro_tile(macro_x, macro_y).uniform_material
            == original.uniform_material,
        "macro promotion changed material identity");
    require(terrain.macro_promotions() > 0u,
        "macro promotion telemetry did not advance");

    const float volume = terrain.total_height_volume();
    terrain.apply_pressure(12.0f, 2.1f, 0.7f, 1.0f / 60.0f);
    terrain.deposit(18.0f, 0.12f, 0.22f);
    const float expected = volume + 0.12f;
    require(std::abs(terrain.total_height_volume() - expected) < 5.0e-4f,
        "fine-cell pressure/deposit path lost represented volume");
    for (int frame = 0; frame < 180; ++frame)
        terrain.step(1.0f / 60.0f);
    require(std::abs(terrain.total_height_volume() - expected) < 1.0e-3f,
        "granular settling leaked fine-cell volume");

    return EXIT_SUCCESS;
}
