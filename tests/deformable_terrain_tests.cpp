#include "deformable_terrain.hpp"
#include "simulation.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>
namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static Vec2 root_position(const Environment& environment) noexcept
        {
            return environment.particles_[environment.blueprint_.root_node].position;
        }

        static Vec2 node_position(const Environment& environment,
            std::uint16_t node) noexcept
        {
            return environment.particles_[node].position;
        }

        static void deposit_world(Environment& environment, float world_x,
            float amount, float firmness) noexcept
        {
            environment.terrain_.deposit(terrain_sample_x(world_x, environment.course_progress()),
                amount, firmness);
        }

        static void refresh_material_metrics(Environment& environment, float dt) noexcept
        {
            environment.update_material_metrics(dt);
        }

        static void set_terrain_progress(Environment& environment, float progress) noexcept
        {
            environment.elapsed_seconds_ = progress;
            // Static-world fixtures move the physical rig to the requested
            // course location. Terrain itself never translates under it.
            for (Particle& particle : environment.particles_)
            {
                particle.position.x += progress;
                particle.previous.x += progress;
            }
            environment.previous_pelvis_.x += progress;
            environment.previous_root_for_path_.x += progress;
        }

        static void add_material(Environment& environment, MaterialParticle item)
        {
            environment.material_particles_.push_back(item);
            environment.rebuild_course_features();
        }

        static void prepare_granular_event(Environment& environment,
            std::uint32_t prior_sequence) noexcept
        {
            environment.clear_dynamic_materials();
            environment.shuttle_state_.phase = ShuttlePhase::traverse;
            environment.shuttle_state_.facing_direction = 1.0f;
            environment.shuttle_state_.locomotion_direction = 1.0f;
            environment.shuttle_state_.completed_turns = 2u;
            environment.distance_travelled_ = 24.0f;
            environment.shuttle_distance_travelled_ = 24.0f;
            environment.alternating_steps_ = 20u;
            environment.limb_crossings_ = 20u;
            environment.longest_stable_stance_seconds_ = 3.0f;
            environment.material_event_sequence_ = prior_sequence;
            environment.elapsed_seconds_ = 20.0f + static_cast<float>(prior_sequence);
            environment.next_material_event_seconds_ = environment.elapsed_seconds_;
            environment.update_materials(1.0f / 60.0f);
            environment.rebuild_course_features();
            environment.append_dynamic_material_features();
            environment.next_material_event_seconds_ = 1.0e6f;
        }

        static void tick_granular(Environment& environment, int steps) noexcept
        {
            for (int step = 0; step < steps; ++step)
            {
                environment.elapsed_seconds_ += 1.0f / 60.0f;
                environment.update_materials(1.0f / 60.0f);
                environment.rebuild_course_features();
                environment.append_dynamic_material_features();
            }
        }

        static void advance_granular_render_schedule(Environment& environment,
            int render_hz, int seconds) noexcept
        {
            int accumulator = 0;
            for (int frame = 0; frame < render_hz * seconds; ++frame)
            {
                accumulator += 60;
                while (accumulator >= render_hz)
                {
                    tick_granular(environment, 1);
                    accumulator -= render_hz;
                }
            }
        }
        static double terrain_volume(const Environment& environment) noexcept
        {
            return environment.terrain_.total_height_volume();
        }
        static std::vector<float> launch_support_clearances(
            const Environment& environment)
        {
            std::vector<float> result{};
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (!environment.blueprint_.is_support_seed(index))
                    continue;
                const Particle& support = environment.particles_[index];
                const float contact_height = environment.ground_height_at(support.position.x)
                    + ground_contact_offset(true, support.radius);
                result.push_back(support.position.y - contact_height);
            }
            return result;
        }
    };
}

namespace { void require(bool ok, std::string_view message) { if (ok) return; std::cerr << "Runner deformable terrain test failed: " << message << '\n'; std::exit(EXIT_FAILURE); } }
int main()
{
    using namespace runner;
    static_assert(sizeof(sim::DeformableTerrain) < 128u * 1024u);
    static_assert(sizeof(sim::Environment) < 256u * 1024u);
    constexpr float transform_world_x = 3.25f;
    constexpr float transform_progress = 11.75f;
    constexpr float transform_source_x = sim::terrain_sample_x(
        transform_world_x, transform_progress);
    constexpr float transform_round_trip_error =
        sim::terrain_world_x(transform_source_x, transform_progress)
        - transform_world_x;
    static_assert(transform_round_trip_error > -1.0e-6f
        && transform_round_trip_error < 1.0e-6f);
    sim::DeformableTerrain first{}, second{};
    first.reset(0x12345678u, 0.72f); second.reset(0x12345678u, 0.72f);
    for (std::size_t i=0; i<sim::DeformableTerrain::cell_count; ++i)
    {
        require(std::abs(first.cells()[i].height-second.cells()[i].height)<1.0e-7f, "same seed changed height");
        require(std::abs(first.cells()[i].firmness-second.cells()[i].firmness)<1.0e-7f, "same seed changed firmness");
    }
    std::array<bool, sandhybrid::material_count> canonical_materials{};
    std::size_t canonical_occupied_cells = 0u;
    std::size_t underground_air_cells = 0u;
    for (std::size_t index = 0; index < first.fine_cells().size(); ++index)
    {
        const auto& lhs = first.fine_cells()[index];
        const auto& rhs = second.fine_cells()[index];
        require(lhs.same_state(rhs),
            "same seed changed canonical SandHybrid material cells");
        const auto material_index = static_cast<std::size_t>(lhs.material());
        if (lhs.fill > 1.0e-6f
            && lhs.material() != sandhybrid::Material::empty
            && lhs.material() != sandhybrid::Material::atmosphere)
        {
            ++canonical_occupied_cells;
            if (material_index < canonical_materials.size())
                canonical_materials[material_index] = true;
        }
        else if (index / sim::DeformableTerrain::cell_count < 152u
            && lhs.material() == sandhybrid::Material::atmosphere)
        {
            ++underground_air_cells;
        }
    }
    require(sim::DeformableTerrain::world_bottom <= -25.0f
            && sim::DeformableTerrain::world_top >= 25.0f,
        "canonical 640x360 map does not straddle the authored surface");
    require(canonical_occupied_cells
            > sim::DeformableTerrain::cell_count * 140u,
        "canonical map leaves the visible underground viewport as fallback fill");
    require(canonical_materials[static_cast<std::size_t>(sandhybrid::Material::sand)]
            && canonical_materials[static_cast<std::size_t>(sandhybrid::Material::dirt)]
            && canonical_materials[static_cast<std::size_t>(sandhybrid::Material::silt)]
            && canonical_materials[static_cast<std::size_t>(sandhybrid::Material::stone)]
            && canonical_materials[static_cast<std::size_t>(sandhybrid::Material::grass)],
        "canonical map omitted a SandHybrid surface or substrate layer");
    require(canonical_materials[static_cast<std::size_t>(sandhybrid::Material::steel)]
            && canonical_materials[static_cast<std::size_t>(sandhybrid::Material::glass)]
            && canonical_materials[static_cast<std::size_t>(sandhybrid::Material::water)]
            && canonical_materials[static_cast<std::size_t>(sandhybrid::Material::crystal)]
            && canonical_materials[static_cast<std::size_t>(sandhybrid::Material::power_cell)]
            && underground_air_cells > 8u * 8u * 16u,
        "canonical training map omitted constructed rooms, waterworks, or cavern cells");
    std::size_t grass_surface_columns{};
    for (std::size_t column = 0u;
        column < sim::DeformableTerrain::cell_count; ++column)
    {
        const auto& surface = first.cells()[column];
        if (surface.region == sim::TerrainRegion::firm
            && surface.surface_material == sandhybrid::Material::grass)
            ++grass_surface_columns;
        require(surface.region == sim::TerrainRegion::firm
                || surface.surface_material != sandhybrid::Material::grass,
            "grass veneer replaced a loose, wet, or gap challenge material");
    }
    require(grass_surface_columns > 24u,
        "connected-world map omitted its canonical firm-ground grass veneer");

    const auto natural_material = [](sandhybrid::Material material) noexcept
    {
        return material == sandhybrid::Material::sand
            || material == sandhybrid::Material::dirt
            || material == sandhybrid::Material::silt
            || material == sandhybrid::Material::stone
            || material == sandhybrid::Material::mud
            || material == sandhybrid::Material::iron_ore;
    };
    std::size_t geology_transitions{};
    for (std::size_t row = 4u; row < 150u; ++row)
    {
        for (std::size_t column = 1u;
            column < sim::DeformableTerrain::cell_count; ++column)
        {
            const auto& left = first.fine_cell(column - 1u, row);
            const auto& right = first.fine_cell(column, row);
            if (left.occupied() && right.occupied()
                && natural_material(left.material())
                && natural_material(right.material())
                && left.material() != right.material())
                ++geology_transitions;
        }
    }
    const bool natural_geology_density = geology_transitions > 900u
        && geology_transitions < 18'000u;
    if (!natural_geology_density)
        std::cerr << "Observed geology transitions: " << geology_transitions
                  << '\n';
    require(natural_geology_density,
        "canonical subsurface is still a flat slab or became checker noise");

    // The facility, waterworks, natural gallery, and construction bay must be
    // one orthogonally connected canonical-air network.  This catches the old
    // presentation-only boxes and sealed cavern even when every material was
    // individually present in the map.
    constexpr std::size_t network_left = 24u;
    constexpr std::size_t network_right = 632u;
    constexpr std::size_t network_bottom = 64u;
    constexpr std::size_t network_top = 152u;
    constexpr std::size_t network_cell_count =
        sim::DeformableTerrain::cell_count
        * sim::DeformableTerrain::vertical_cell_count;
    const auto network_index = [](std::size_t column,
        std::size_t row) noexcept
    {
        return row * sim::DeformableTerrain::cell_count + column;
    };
    const auto open_at = [&](std::size_t column, std::size_t row) noexcept
    {
        return column >= network_left && column < network_right
            && row >= network_bottom && row < network_top
            && !first.fine_cell(column, row).occupied();
    };
    constexpr std::size_t start_column = 64u;
    constexpr std::size_t start_row = 116u;
    require(open_at(start_column, start_row),
        "west operations gallery lost its canonical-air interior");
    std::vector<std::uint8_t> visited(network_cell_count, 0u);
    std::vector<std::size_t> pending{};
    pending.push_back(network_index(start_column, start_row));
    visited[pending.back()] = 1u;
    std::size_t connected_open_cells{};
    while (!pending.empty())
    {
        const std::size_t index = pending.back();
        pending.pop_back();
        ++connected_open_cells;
        const std::size_t column = index
            % sim::DeformableTerrain::cell_count;
        const std::size_t row = index
            / sim::DeformableTerrain::cell_count;
        const std::array<std::array<std::ptrdiff_t, 2>, 4> neighbors{{
            {{ -1, 0 }}, {{ 1, 0 }}, {{ 0, -1 }}, {{ 0, 1 }} }};
        for (const auto& offset : neighbors)
        {
            const auto next_column = static_cast<std::ptrdiff_t>(column)
                + offset[0];
            const auto next_row = static_cast<std::ptrdiff_t>(row) + offset[1];
            if (next_column < 0 || next_row < 0)
                continue;
            const auto candidate_column = static_cast<std::size_t>(next_column);
            const auto candidate_row = static_cast<std::size_t>(next_row);
            if (!open_at(candidate_column, candidate_row))
                continue;
            const std::size_t candidate = network_index(
                candidate_column, candidate_row);
            if (visited[candidate] != 0u)
                continue;
            visited[candidate] = 1u;
            pending.push_back(candidate);
        }
    }
    const auto connected = [&](std::size_t column, std::size_t row) noexcept
    {
        return visited[network_index(column, row)] != 0u;
    };
    const std::array<bool, 4> network_portals{
        connected(190u, 128u), connected(320u, 105u),
        connected(410u, 116u), connected(560u, 124u) };
    if (!std::ranges::all_of(network_portals,
            [](bool connected_portal) { return connected_portal; }))
    {
        std::cerr << "Observed connected portals: waterworks="
            << network_portals[0] << " west-cavern=" << network_portals[1]
            << " east-cavern=" << network_portals[2]
            << " construction=" << network_portals[3]
            << " component-cells=" << connected_open_cells
            << " open=" << open_at(190u, 128u) << '/'
            << open_at(320u, 105u) << '/' << open_at(410u, 116u) << '/'
            << open_at(560u, 124u) << '\n';
    }
    require(std::ranges::all_of(network_portals,
                [](bool connected_portal) { return connected_portal; }),
        "lab, waterworks, cavern, and construction bay are disconnected");
    require(connected_open_cells > 7'000u
            && connected_open_cells < 22'000u,
        "connected training facility is missing or became a screen-sized void");

    std::size_t maximum_network_horizontal_run{};
    std::size_t maximum_network_vertical_run{};
    std::size_t maximum_network_horizontal_row{};
    std::size_t maximum_network_vertical_column{};
    for (std::size_t row = network_bottom; row < network_top; ++row)
    {
        std::size_t run{};
        for (std::size_t column = network_left;
            column < network_right; ++column)
        {
            run = connected(column, row) ? run + 1u : 0u;
            if (run > maximum_network_horizontal_run)
            {
                maximum_network_horizontal_run = run;
                maximum_network_horizontal_row = row;
            }
        }
    }
    for (std::size_t column = network_left; column < network_right; ++column)
    {
        std::size_t run{};
        for (std::size_t row = network_bottom; row < network_top; ++row)
        {
            run = connected(column, row) ? run + 1u : 0u;
            if (run > maximum_network_vertical_run)
            {
                maximum_network_vertical_run = run;
                maximum_network_vertical_column = column;
            }
        }
    }
    if (maximum_network_horizontal_run
            > runner::course_geometry::maximum_cavern_open_span_cells
        || maximum_network_vertical_run
            > runner::course_geometry::maximum_facility_clear_height_cells)
    {
        std::cerr << "Observed connected open runs: horizontal="
            << maximum_network_horizontal_run << "@row"
            << maximum_network_horizontal_row << " vertical="
            << maximum_network_vertical_run << "@column"
            << maximum_network_vertical_column << '\n';
    }
    require(maximum_network_horizontal_run
                <= runner::course_geometry::maximum_cavern_open_span_cells
            && maximum_network_vertical_run
                >= runner::course_geometry::minimum_facility_clear_height_cells
            && maximum_network_vertical_run
                <= runner::course_geometry::maximum_facility_clear_height_cells,
        "connected facility regressed to a giant rectangular compartment");
    require(first.fine_cell(40u, 104u).structural()
            && first.fine_cell(40u, 104u).authored()
            && first.fine_cell(40u, 104u).material()
                == sandhybrid::Material::steel,
        "natural room boundary lost its canonical authored steel support");

    const auto shallow_water = first.challenge_for(
        director::ChallengeKind::shallow_water);
    require(shallow_water.has_value(),
        "surface progression lost the shallow-water challenge");
    const std::size_t lake_column = static_cast<std::size_t>(std::lround(
        shallow_water->center() / sim::DeformableTerrain::fine_cell_spacing));
    std::size_t lake_surface_row{};
    for (std::size_t row = 0u;
        row < sim::DeformableTerrain::vertical_cell_count; ++row)
        if (first.fine_cell(lake_column, row).occupied())
            lake_surface_row = row;
    const auto& lake_surface_cell = first.fine_cell(
        lake_column, lake_surface_row);
    const auto& lake_aquifer_cell = first.fine_cell(
        lake_column, lake_surface_row - 4u);
    require(lake_surface_cell.material() != sandhybrid::Material::water
            && lake_aquifer_cell.material() == sandhybrid::Material::water
            && lake_aquifer_cell.authored()
            && !lake_aquifer_cell.structural(),
        "graded surface lake is not coupled to canonical capped groundwater");
    require(canonical_materials[static_cast<std::size_t>(
                sandhybrid::Material::conveyor)]
            && canonical_materials[static_cast<std::size_t>(
                sandhybrid::Material::smelter)]
            && canonical_materials[static_cast<std::size_t>(
                sandhybrid::Material::assembler)]
            && canonical_materials[static_cast<std::size_t>(
                sandhybrid::Material::factory_core)]
            && canonical_materials[static_cast<std::size_t>(
                sandhybrid::Material::gold)]
            && canonical_materials[static_cast<std::size_t>(
                sandhybrid::Material::aluminum)],
        "constructed labs lost their aligned machinery/material language");

    sim::DeformableTerrain alternate_substrate{};
    alternate_substrate.reset(0x87654321u, 0.72f);
    bool found_seeded_deep_transition = false;
    for (std::size_t row = 0;
        row + 24u < sim::DeformableTerrain::vertical_cell_count; ++row)
    {
        for (std::size_t column = 0;
            column < sim::DeformableTerrain::cell_count; ++column)
        {
            const auto& lhs = first.fine_cell(column, row);
            const auto& rhs = alternate_substrate.fine_cell(column, row);
            if (lhs.material() != rhs.material())
            {
                found_seeded_deep_transition = true;
                break;
            }
        }
        if (found_seeded_deep_transition)
            break;
    }
    require(found_seeded_deep_transition,
        "different seeds produced an identical canonical substrate map");
    bool found_exact_cell_transition = false;
    for (std::size_t i = 0; i + 1u < sim::DeformableTerrain::cell_count; ++i)
    {
        const auto& left = first.cells()[i];
        const auto& right = first.cells()[i + 1u];
        if (left.region == right.region && std::abs(left.height - right.height) < 1.0e-5f)
            continue;
        const float boundary = (static_cast<float>(i) + 0.5f) * sim::DeformableTerrain::cell_spacing;
        const float before = boundary - 1.0e-4f;
        const float after = boundary + 1.0e-4f;
        require(std::abs(first.height_at(before) - left.height) < 1.0e-7f && std::abs(first.height_at(after) - right.height) < 1.0e-7f, "terrain height interpolated between fixed material cells");
        require(std::abs(first.firmness_at(before) - left.firmness) < 1.0e-7f && std::abs(first.firmness_at(after) - right.firmness) < 1.0e-7f, "terrain firmness interpolated between fixed material cells");
        require(std::abs(first.looseness_at(before) - left.loose_fraction) < 1.0e-7f && std::abs(first.looseness_at(after) - right.loose_fraction) < 1.0e-7f, "terrain looseness interpolated between fixed material cells");
        require(std::abs(first.water_depth_at(before) - left.water_depth) < 1.0e-7f && std::abs(first.water_depth_at(after) - right.water_depth) < 1.0e-7f, "water depth interpolated between fixed material cells");
        require(std::abs(first.water_surface_at(before) - left.water_surface) < 1.0e-7f && std::abs(first.water_surface_at(after) - right.water_surface) < 1.0e-7f, "water surface diverged from exact material cells");
        found_exact_cell_transition = true;
        break;
    }
    require(found_exact_cell_transition, "seed did not expose an exact-cell terrain transition");
    for (std::uint64_t seed = 1u; seed <= 16u; ++seed)
    {
        sim::DeformableTerrain varied{};
        varied.reset(seed, 0.72f);
        require(varied.region_at(2.25f) == sim::TerrainRegion::dry_sand,
            "active sand does not begin shortly after the launch transition");
        std::array<bool, 5> regions{};
        std::array<sim::TerrainRegion, 3> middle{};
        std::size_t middle_count{};
        float maximum_shallow_depth{};
        float minimum_hole_height{ std::numeric_limits<float>::infinity() };
        sim::TerrainRegion last = sim::TerrainRegion::dry_sand;
        for (float sample = 2.25f; sample < sim::DeformableTerrain::period - 5.0f;
            sample += 0.25f)
        {
            const sim::TerrainRegion region = varied.region_at(sample);
            regions[static_cast<std::size_t>(region)] = true;
            if (region == sim::TerrainRegion::shallow_water)
                maximum_shallow_depth = std::max(
                    maximum_shallow_depth, varied.water_depth_at(sample));
            if (region == sim::TerrainRegion::hole)
                minimum_hole_height = std::min(
                    minimum_hole_height, varied.height_at(sample));
            if (middle_count < middle.size() && region != last
                && region != sim::TerrainRegion::dry_sand
                && region != sim::TerrainRegion::hole)
                middle[middle_count++] = region;
            last = region;
        }
        require(std::ranges::all_of(regions, [](bool present) { return present; }),
            "seeded terrain omitted a firm, sand, mud, water, or hole region");
        require(middle_count == middle.size(),
            "seeded terrain middle-band order could not be observed");
        require(middle == std::array{
                sim::TerrainRegion::firm,
                sim::TerrainRegion::waterlogged,
                sim::TerrainRegion::shallow_water },
            "course no longer progresses from dry recovery through saturated "
            "ground into open water");
        require(maximum_shallow_depth > 0.05f,
            "shallow-water band has no physical interior water depth");
        require(minimum_hole_height < -0.20f,
            "authored hole region has no physical collision depression");
        for (std::size_t column = 0;
            static_cast<float>(column) * sim::DeformableTerrain::fine_cell_spacing
                <= 8.0f; ++column)
        {
            const std::size_t next = (column + 1u)
                % sim::DeformableTerrain::cell_count;
            if (varied.cells()[column].region == sim::TerrainRegion::hole
                || varied.cells()[next].region == sim::TerrainRegion::hole)
                continue;
            const float boundary_delta = std::abs(varied.cells()[column].height
                - varied.cells()[next].height);
            require(boundary_delta < 0.11f,
                "foundation terrain contains an invisible curb-sized ledge");
        }
        const auto challenges = varied.terrain_challenges();
        require(challenges[0].region == sim::TerrainRegion::firm
                && challenges[1].region == sim::TerrainRegion::dry_sand
                && challenges[2].region == sim::TerrainRegion::firm
                && challenges[3].region == sim::TerrainRegion::waterlogged
                && challenges[4].region == sim::TerrainRegion::shallow_water
                && challenges[5].stable_id
                    == sim::DeformableTerrain::post_lake_recovery_challenge_id
                && challenges[5].region == sim::TerrainRegion::firm
                && challenges[6].region == sim::TerrainRegion::hole,
            "terrain challenge graph lost its progressive approach/lake/"
            "recovery/gap sequence");
        require(challenges[3].length() >= 6.0f
                && challenges[4].length() >= 7.0f
                && challenges[5].length() >= 3.25f
                && challenges[6].length() >= 5.0f,
            "a physical terrain lesson or post-lake recovery runway is too short");
        for (std::size_t left = 0u; left < challenges.size(); ++left)
        {
            require(challenges[left].stable_id != 0u
                    && challenges[left].sequence == left
                    && challenges[left].end > challenges[left].begin,
                "terrain challenge identity, order, or extent is invalid");
            for (std::size_t right = left + 1u; right < challenges.size(); ++right)
                require(challenges[left].stable_id != challenges[right].stable_id,
                    "terrain challenge graph reused a stable physical identity");
        }

        const auto shallow = varied.challenge_for(
            director::ChallengeKind::shallow_water);
        require(shallow.has_value(),
            "progressive terrain graph omitted its open-water lesson");
        const float lake_center = shallow->center();
        const float lake_depth = varied.water_depth_at(lake_center);
        require(lake_depth >= 0.24f
                && std::abs(varied.water_surface_at(lake_center - 0.50f)
                    - varied.water_surface_at(lake_center + 0.50f)) < 1.0e-6f,
            "lake lost its full-depth, level canonical-cell interior");
        require(varied.region_at(challenges[5].center())
                    == sim::TerrainRegion::firm
                && varied.water_depth_at(challenges[5].center()) == 0.0f,
            "far shore does not provide a real dry recovery plant before the gap");

        require(!varied.fine_cell(64u, 116u).occupied()
                && !varied.fine_cell(320u, 105u).occupied()
                && !varied.fine_cell(410u, 116u).occupied()
                && !varied.fine_cell(560u, 124u).occupied(),
            "a different seed sealed a stable facility/cavern portal");
        const std::size_t seeded_lake_column = static_cast<std::size_t>(
            std::lround(lake_center
                / sim::DeformableTerrain::fine_cell_spacing));
        std::size_t seeded_lake_surface_row{};
        for (std::size_t row = 0u;
            row < sim::DeformableTerrain::vertical_cell_count; ++row)
            if (varied.fine_cell(seeded_lake_column, row).occupied())
                seeded_lake_surface_row = row;
        require(seeded_lake_surface_row >= 4u
                && varied.fine_cell(seeded_lake_column,
                    seeded_lake_surface_row).material()
                    != sandhybrid::Material::water
                && varied.fine_cell(seeded_lake_column,
                    seeded_lake_surface_row - 4u).material()
                    == sandhybrid::Material::water,
            "a different seed separated the graded lake from its capped aquifer");

        for (std::size_t column = 0u;
            column + 1u < sim::DeformableTerrain::cell_count; ++column)
        {
            const auto material = varied.cells()[column].surface_material;
            require(material == sandhybrid::Material::grass
                    || material == sandhybrid::Material::dirt
                    || material == sandhybrid::Material::sand
                    || material == sandhybrid::Material::mud
                    || material == sandhybrid::Material::stone,
                "an underground facility protrudes into walk-course collision");
            const float rise = std::abs(varied.cells()[column + 1u].height
                - varied.cells()[column].height);
            if (rise
                > runner::course_geometry::traversable_neighbor_rise_meters)
            {
                std::cerr << "terrain seam seed=" << seed
                    << " column=" << column
                    << " x=" << static_cast<float>(column)
                        * sim::DeformableTerrain::fine_cell_spacing
                    << " rise=" << rise
                    << " left=" << varied.cells()[column].height
                    << " right=" << varied.cells()[column + 1u].height
                    << " regions="
                    << sim::terrain_region_name(varied.cells()[column].region)
                    << '/' << sim::terrain_region_name(
                        varied.cells()[column + 1u].region) << '\n';
            }
            require(rise
                    <= runner::course_geometry::traversable_neighbor_rise_meters,
                "natural course surface contains a curb-sized hidden seam");
        }

        for (const director::ChallengeKind kind : {
            director::ChallengeKind::step, director::ChallengeKind::ramp })
        {
            const auto challenge = varied.challenge_for(kind);
            require(challenge.has_value(),
                "step/ramp overlay disappeared from the connected course");
            const std::size_t column = static_cast<std::size_t>(std::lround(
                challenge->center() / sim::DeformableTerrain::fine_cell_spacing))
                % sim::DeformableTerrain::cell_count;
            const sim::DeformableTerrain::FineCell* top = nullptr;
            for (std::size_t row = 0u;
                row < sim::DeformableTerrain::vertical_cell_count; ++row)
            {
                const auto& candidate = varied.fine_cell(column, row);
                if (candidate.occupied())
                    top = &candidate;
            }
            require(top != nullptr && top->authored() && !top->structural()
                    && varied.region_at(challenge->center())
                        == sim::TerrainRegion::firm,
                "walk/run step or ramp is a structural wall or loose compound test");
        }
    }

    // Difficulty changes physical magnitude, never ordering, shore topology,
    // or the deterministic cell construction for an identical input tuple.
    for (const float difficulty : { 0.0f, 1.0f })
    {
        constexpr std::array<std::uint64_t, 3> adversarial_seeds{
            1ULL, 0x8000000000000000ULL, 0xffffffffffffffffULL };
        for (const std::uint64_t seed : adversarial_seeds)
        {
            sim::DeformableTerrain extreme_a{};
            sim::DeformableTerrain extreme_b{};
            extreme_a.reset(seed, difficulty);
            extreme_b.reset(seed, difficulty);
            const auto shallow = extreme_a.challenge_for(
                director::ChallengeKind::shallow_water);
            require(shallow.has_value()
                    && extreme_a.water_depth_at(shallow->center())
                        >= 0.10f + difficulty * 0.16f,
                "adversarial seed/difficulty removed the lake interior");
            require(extreme_a.terrain_challenges()
                    == extreme_b.terrain_challenges(),
                "repeated seed/difficulty changed challenge construction");
            for (std::size_t column = 0u;
                column < sim::DeformableTerrain::cell_count; ++column)
            {
                require(extreme_a.cells()[column].height
                            == extreme_b.cells()[column].height
                        && extreme_a.cells()[column].water_depth
                            == extreme_b.cells()[column].water_depth
                        && extreme_a.fine_cell(column, 0u).same_state(
                            extreme_b.fine_cell(column, 0u)),
                    "repeated adversarial terrain tuple changed canonical state");
            }
        }
    }

    const auto same_terrain = [](const sim::DeformableTerrain& subject,
        const auto& cells, const auto& fine_cells)
    {
        for (std::size_t index = 0; index < cells.size(); ++index)
        {
            const auto& before = cells[index];
            const auto& after = subject.cells()[index];
            if (before.height != after.height || before.rest_height != after.rest_height
                || before.firmness != after.firmness
                || before.loose_fraction != after.loose_fraction
                || before.water_surface != after.water_surface
                || before.water_depth != after.water_depth
                || before.surface_material != after.surface_material
                || before.region != after.region)
                return false;
        }
        for (std::size_t index = 0; index < fine_cells.size(); ++index)
        {
            const auto& before = fine_cells[index];
            const auto& after = subject.fine_cells()[index];
            if (!before.same_state(after))
                return false;
        }
        return true;
    };
    for (const float difficulty : { 0.30f, 1.0f })
    {
        sim::DeformableTerrain immutable_sand{};
        immutable_sand.reset(0x73130u, difficulty);
        const auto authored_cells = immutable_sand.cells();
        const auto authored_fine_cells = immutable_sand.fine_cells();
        for (int step = 0; step < 600; ++step)
        {
            for (const float sample : { 3.5f, 7.25f, 12.0f, 18.0f })
                immutable_sand.apply_pressure(sample, 4.0f, 5.0f, 1.0f / 20.0f);
            immutable_sand.step(1.0f / 20.0f);
        }
        require(same_terrain(immutable_sand, authored_cells, authored_fine_cells),
            "authored macro/fine cells changed under repeated contact pressure");
    }

    sim::DeformableTerrain protected_launch{};
    protected_launch.reset(0xA731u, 0.95f);
    const auto launch_cells = protected_launch.cells();
    const auto launch_fine_cells = protected_launch.fine_cells();
    constexpr std::array<float, 7> launch_pressure_points{
        -0.01f, 0.0f, 0.50f, 0.699f, 0.701f,
        sim::DeformableTerrain::period - 0.01f,
        sim::DeformableTerrain::period - 0.699f };
    for (int step = 0; step < 600; ++step)
    {
        for (const float sample : launch_pressure_points)
        {
            protected_launch.apply_pressure(sample, 4.0f, 5.0f, 1.0f / 20.0f);
            protected_launch.deposit(sample, 0.25f, 0.0f);
        }
        protected_launch.step(1.0f / 20.0f);
    }
    for (std::size_t column = 0; column < sim::DeformableTerrain::cell_count; ++column)
    {
        const float course_x = static_cast<float>(column)
            * sim::DeformableTerrain::fine_cell_spacing;
        if (!sim::DeformableTerrain::launch_pad_at(course_x))
            continue;
        const auto& before = launch_cells[column];
        const auto& after = protected_launch.cells()[column];
        require(before.height == after.height && before.rest_height == after.rest_height
                && before.firmness == after.firmness
                && before.loose_fraction == after.loose_fraction
                && before.water_surface == after.water_surface
                && before.water_depth == after.water_depth
                && before.surface_material == after.surface_material
                && before.region == after.region,
            "launch column changed under pressure, deposit, or relaxation");
        for (std::size_t row = 0; row < sim::DeformableTerrain::vertical_cell_count; ++row)
        {
            const std::size_t fine_index = row * sim::DeformableTerrain::cell_count + column;
            const auto& fine_before = launch_fine_cells[fine_index];
            const auto& fine_after = protected_launch.fine_cells()[fine_index];
            require(fine_before.same_state(fine_after),
                "launch fine cell changed under adversarial load");
        }
    }

    constexpr float x=3.5f;
    const auto immutable_cells = first.cells();
    const auto immutable_fine_cells = first.fine_cells();
    const double volume=first.total_height_volume();
    for(int i=0;i<240;++i){first.apply_pressure(x,4.0f,5.0f,1.0f/20.0f);first.step(1.0f/20.0f);}
    require(same_terrain(first,immutable_cells,immutable_fine_cells),
        "foot contact changed immutable authored pixel cells");
    require(std::abs(first.total_height_volume()-volume)<2.0e-5f,
        "immutable contact path changed terrain volume");
    const double deposited=first.total_height_volume(); first.deposit(18.0f,0.12f,0.20f);
    require(std::abs((first.total_height_volume()-deposited)-0.12f)<2.0e-4f,"deposit lost volume");
    const float slope=first.maximum_neighbor_delta();
    for(int i=0;i<240;++i) first.step(1.0f/60.0f);
    require(first.maximum_neighbor_delta()<=slope+1.0e-5f,"collapse increased maximum slope");
    if (std::abs(first.total_height_volume()-(deposited+0.12f))>=8.0e-4f)
        std::cerr << "volume expected=" << (deposited+0.12f)
            << " actual=" << first.total_height_volume() << std::endl;
    require(std::abs(first.total_height_volume()-(deposited+0.12f))<8.0e-4f,"collapse leaked volume");
    sim::DeformableTerrain firm_truth{};
    firm_truth.reset(0x733001u, 0.85f);
    float firm_x = 0.0f;
    bool found_firm = false;
    for (std::size_t column = 0; column < sim::DeformableTerrain::cell_count; ++column)
    {
        const float sample = static_cast<float>(column)
            * sim::DeformableTerrain::fine_cell_spacing;
        if (!sim::DeformableTerrain::launch_pad_at(sample)
            && firm_truth.region_at(sample) == sim::TerrainRegion::firm)
        {
            firm_x = sample;
            found_firm = true;
            break;
        }
    }
    require(found_firm, "material-truth test could not find firm dirt");
    const float firm_height = firm_truth.height_at(firm_x);
    const double firm_volume = firm_truth.total_height_volume();
    const sandhybrid::Material firm_material = firm_truth.surface_material_at(firm_x);
    for (int iteration = 0; iteration < 120; ++iteration)
        firm_truth.apply_pressure(firm_x, 4.0f, 5.0f, 1.0f / 60.0f);
    require(std::abs(firm_truth.height_at(firm_x) - firm_height) < 1.0e-7f
            && std::abs(firm_truth.total_height_volume() - firm_volume) < 1.0e-6f
            && firm_truth.surface_material_at(firm_x) == firm_material,
        "firm dirt deformed or converted under support pressure");

    sim::DeformableTerrain excavated_a{}, excavated_b{};
    excavated_a.reset(0x733002u, 0.85f);
    excavated_b.reset(0x733002u, 0.85f);
    constexpr float excavation_x = 3.5f;
    const auto excavation_cells = excavated_a.cells();
    const auto excavation_fine_cells = excavated_a.fine_cells();
    require(excavated_a.excavate(excavation_x, 2.00f, 0.35f) == 0.0f
            && same_terrain(excavated_a, excavation_cells, excavation_fine_cells),
        "excavation modified immutable authored course cells");
    excavated_a.deposit(excavation_x, 0.35f, 0.18f);
    excavated_b.deposit(excavation_x, 0.35f, 0.18f);
    const float removed_a = excavated_a.excavate(excavation_x, 2.00f, 0.35f);
    const float removed_b = excavated_b.excavate(excavation_x, 2.00f, 0.35f);
    require(removed_a > 0.0f && std::abs(removed_a - removed_b) < 1.0e-7f,
        "explicit dropped-cell excavation failed or was nondeterministic");
    require(std::abs(excavated_a.height_at(excavation_x)
            - excavated_b.height_at(excavation_x)) < 1.0e-7f,
        "repeated-seed dropped-cell excavation changed the resulting surface");
    require(excavated_a.excavate(0.0f, 1.0f, 0.5f) == 0.0f
            && excavated_a.excavate(std::numeric_limits<float>::quiet_NaN(),
                1.0f, 0.5f) == 0.0f,
        "launch or non-finite excavation bypassed its guard");

    sim::DeformableTerrain deposited_truth{};
    deposited_truth.reset(0x733003u, 0.85f);
    deposited_truth.deposit(firm_x, 0.24f, 0.18f, sandhybrid::Material::sand);
    require(deposited_truth.surface_material_at(firm_x) == sandhybrid::Material::sand
            && deposited_truth.region_at(firm_x) == sim::TerrainRegion::dry_sand,
        "sand deposition did not update the authoritative material label");

    sim::Environment granular{ sim::CreatureBlueprint::humanoid(), 0x733100u };
    granular.set_course(sim::CourseStage::moving_hazards, 0.75f);
    granular.set_course_motion_enabled(false);
    require(granular.material_event_count() == 0u
            && granular.material_particles().empty()
            && !granular.granular_block_present(),
        "granular hazards spawned before gait and shuttle readiness");
    const double granular_volume = sim::EnvironmentTestAccess::terrain_volume(granular);
    sim::EnvironmentTestAccess::prepare_granular_event(granular, 0u);
    require(granular.material_event_count() == 1u
            && !granular.material_particles().empty()
            && std::ranges::all_of(granular.material_particles(),
                [](const sim::MaterialParticle& particle)
                { return particle.kind == sim::MaterialKind::sand; })
            && granular.granular_hazard_active(),
        "first ready granular event is not an active falling-sand burst");
    sim::EnvironmentTestAccess::tick_granular(granular, 150);
    require(granular.material_particles().empty()
            && sim::EnvironmentTestAccess::terrain_volume(granular) > granular_volume
            && granular.granular_hazard_safe(),
        "falling sand did not settle, stack, and become safely traversable");

    sim::EnvironmentTestAccess::prepare_granular_event(granular, 1u);
    require(std::ranges::all_of(granular.material_particles(),
            [](const sim::MaterialParticle& particle)
            { return particle.kind == sim::MaterialKind::dirt; }),
        "second granular event did not preserve falling dirt material");
    sim::EnvironmentTestAccess::prepare_granular_event(granular, 2u);
    require(granular.material_event_count() == 3u
            && granular.granular_hazard_active()
            && !granular.material_particles().empty()
            && std::ranges::any_of(granular.material_particles(),
                [](const sim::MaterialParticle& particle)
                { return particle.kind == sim::MaterialKind::sand; })
            && std::ranges::any_of(granular.material_particles(),
                [](const sim::MaterialParticle& particle)
                { return particle.kind == sim::MaterialKind::dirt; }),
        "falling-cell cascade did not expose an unsafe mixed-material window");
    sim::EnvironmentTestAccess::tick_granular(granular, 240);
    require(granular.granular_hazard_safe(),
        "falling-cell cascade never settled into safe traversable terrain");
    sim::EnvironmentTestAccess::prepare_granular_event(granular, 3u);
    require(granular.granular_block_present()
            && granular.granular_hazard_active()
            && std::ranges::any_of(granular.course_features(),
                [](const sim::CourseFeature& feature)
                { return feature.marker_sequence >= 50'000; }),
        "moving block did not enter collision, render, and observation state");
    sim::EnvironmentTestAccess::tick_granular(granular, 300);
    require(granular.granular_block_present()
            && granular.granular_hazard_safe(),
        "moving block did not settle into a persistent traversable obstacle");
    const auto granular_fingerprint = [](const sim::Environment& subject)
    {
        std::vector<float> result{
            static_cast<float>(subject.material_event_count()),
            static_cast<float>(subject.material_particles().size()),
            static_cast<float>(subject.granular_hazard_active()),
            static_cast<float>(subject.granular_hazard_safe()),
            static_cast<float>(subject.granular_block_present()),
            static_cast<float>(
                sim::EnvironmentTestAccess::terrain_volume(subject)) };
        for (const sim::MaterialParticle& particle : subject.material_particles())
            result.insert(result.end(), { static_cast<float>(particle.kind),
                particle.position.x, particle.position.y,
                particle.velocity.x, particle.velocity.y, particle.radius });
        for (const sim::CourseFeature& feature : subject.course_features())
        {
            if (feature.marker_sequence < 50'000)
                continue;
            result.insert(result.end(), { feature.center.x, feature.center.y,
                feature.velocity.x, feature.velocity.y,
                feature.half_extent.x, feature.half_extent.y });
        }
        return result;
    };
    for (const std::uint32_t sequence : { 0u, 2u, 3u })
    {
        sim::Environment at_20(sim::CreatureBlueprint::humanoid(), 0x733333u);
        sim::Environment at_60(sim::CreatureBlueprint::humanoid(), 0x733333u);
        sim::Environment at_240(sim::CreatureBlueprint::humanoid(), 0x733333u);
        for (sim::Environment* subject : { &at_20, &at_60, &at_240 })
        {
            subject->set_course(sim::CourseStage::moving_hazards, 0.80f);
            subject->set_course_motion_enabled(false);
            sim::EnvironmentTestAccess::prepare_granular_event(*subject, sequence);
        }
        sim::EnvironmentTestAccess::advance_granular_render_schedule(at_20, 20, 5);
        sim::EnvironmentTestAccess::advance_granular_render_schedule(at_60, 60, 5);
        sim::EnvironmentTestAccess::advance_granular_render_schedule(at_240, 240, 5);
        require(granular_fingerprint(at_20) == granular_fingerprint(at_60)
                && granular_fingerprint(at_60) == granular_fingerprint(at_240),
            "granular fixed-step state differs at 20/60/240 Hz render cadence");
    }
    const std::array<sim::CreatureBlueprint, 8> launch_rigs{
        sim::CreatureBlueprint::humanoid(), sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::scaffold(), sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::quadruped(), sim::CreatureBlueprint::crawler4(),
        sim::CreatureBlueprint::hexapod(), sim::CreatureBlueprint::monoped() };
    for (std::size_t rig_index = 0; rig_index < launch_rigs.size(); ++rig_index)
    {
        for (std::uint64_t seed = 1u; seed <= 5u; ++seed)
        {
            sim::Environment launch_environment(launch_rigs[rig_index],
                0x731000u + static_cast<std::uint64_t>(rig_index) * 101u + seed);
            launch_environment.set_course(sim::CourseStage::uneven, 0.95f);
            const std::vector<float> clearances =
                sim::EnvironmentTestAccess::launch_support_clearances(launch_environment);
            require(!clearances.empty(), "rig has no authored launch support");
            float minimum_clearance = clearances.front();
            for (const float clearance : clearances)
            {
                require(clearance >= -1.0e-6f, "launch support penetrated terrain");
                minimum_clearance = std::min(minimum_clearance, clearance);
            }
            require(std::abs(minimum_clearance) <= 1.0e-5f,
                "no authored support was placed on launch collision");
        }
    }

    sim::Environment environment(sim::CreatureBlueprint::quadruped(),0x5a17u);
    environment.set_course(sim::CourseStage::moving_hazards,0.80f);
    std::array<float,sim::action_count> idle{};
    for(int frame=0;frame<90;++frame) static_cast<void>(environment.step(idle));
    const float sync_world_x=1.75f;
    const float sync_source_x=sim::terrain_sample_x(sync_world_x,environment.course_progress());
    require(std::abs(environment.ground_height_at(sync_world_x)
        - environment.terrain().height_at(sync_source_x))<1.0e-6f,
        "render/world terrain transform disagrees with collision sampling");
    for(int frame=0;frame<240;++frame) static_cast<void>(environment.step(idle));
    require(environment.material_event_count()==0u,"material pressure spawned before real safe-runway progress");
    const auto observation=environment.observation(); static_assert(observation.size()==sim::observation_count);
    for(std::size_t i=40;i<observation.size();++i) require(std::isfinite(observation[i]),"non-finite material observation");
    require(environment.terrain_firmness_at(0.0f)>=0.0f && environment.terrain_firmness_at(0.0f)<=1.0f,"firmness out of range");
    require(environment.burial_depth()>=0.0f,"negative burial depth");
    for(const sim::MaterialParticle& item:environment.material_particles())
        require(item.position.y+item.radius>=environment.ground_height_at(item.position.x)-0.005f,"material tunneled below terrain");

    sim::Environment deterministic_a(sim::CreatureBlueprint::quadruped(),0x778899u);
    sim::Environment deterministic_b(sim::CreatureBlueprint::quadruped(),0x778899u);
    deterministic_a.set_course(sim::CourseStage::moving_hazards,0.90f);
    deterministic_b.set_course(sim::CourseStage::moving_hazards,0.90f);
    for(int frame=0;frame<480;++frame)
    {
        static_cast<void>(deterministic_a.step(idle));
        static_cast<void>(deterministic_b.step(idle));
    }
    require(deterministic_a.material_event_count()==deterministic_b.material_event_count(),
        "seeded repeated material events are not deterministic");
    require(deterministic_a.material_particles().size()==deterministic_b.material_particles().size(),
        "seeded material population diverged");
    for(std::size_t i=0;i<deterministic_a.material_particles().size();++i)
    {
        const auto& a=deterministic_a.material_particles()[i];
        const auto& b=deterministic_b.material_particles()[i];
        require(std::abs(a.position.x-b.position.x)<1.0e-5f
                && std::abs(a.position.y-b.position.y)<1.0e-5f,
            "seeded material trajectory diverged");
    }

    sim::Environment escape(sim::CreatureBlueprint::quadruped(),0xE5CA9Eu);
    escape.set_course(sim::CourseStage::moving_hazards,0.75f);
    sim::EnvironmentTestAccess::set_terrain_progress(escape, 6.0f);
    const Vec2 escape_root=sim::EnvironmentTestAccess::root_position(escape);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-1.35f,3.50f,0.22f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.85f,3.50f,0.22f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.35f,2.50f,0.18f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x+0.10f,1.75f,0.20f);
    sim::EnvironmentTestAccess::refresh_material_metrics(escape,1.0f/60.0f);
    require(escape.burial_depth()>0.05f,"partial burial scenario produced no burial depth");
    if (escape.free_space_direction() <= 0.0f)
        std::cerr << "partial burial diagnostic root=" << escape_root.x
            << " burial=" << escape.burial_depth()
            << " free=" << escape.free_space_direction()
            << " ground=" << escape.ground_height_at(escape_root.x - 1.50f)
                << ',' << escape.ground_height_at(escape_root.x - 0.75f)
                << ',' << escape.ground_height_at(escape_root.x)
                << ',' << escape.ground_height_at(escape_root.x + 0.75f)
                << ',' << escape.ground_height_at(escape_root.x + 1.50f)
            << '\n';
    require(escape.free_space_direction()>0.0f,"partial burial did not identify the open escape side");

    sim::Environment trapped(sim::CreatureBlueprint::quadruped(),0xB091EDu);
    trapped.set_course(sim::CourseStage::moving_hazards,0.90f);
    sim::EnvironmentTestAccess::set_terrain_progress(trapped, 6.0f);
    const Vec2 trapped_root=sim::EnvironmentTestAccess::root_position(trapped);
    const Vec2 trapped_head=sim::EnvironmentTestAccess::node_position(
        trapped,trapped.blueprint().head_node);
    const Vec2 trapped_torso=sim::EnvironmentTestAccess::node_position(
        trapped,trapped.blueprint().torso_node);
    constexpr std::array<float,7> burial_offsets{-1.05f,-0.70f,-0.35f,0.0f,0.35f,0.70f,1.05f};
    for(float offset:burial_offsets)
        sim::EnvironmentTestAccess::deposit_world(trapped,trapped_root.x+offset,22.0f,0.20f);
    sim::EnvironmentTestAccess::deposit_world(trapped,trapped_head.x,26.0f,0.18f);
    sim::EnvironmentTestAccess::deposit_world(trapped,trapped_torso.x,26.0f,0.18f);
    for(int frame=0;frame<180;++frame)
        sim::EnvironmentTestAccess::refresh_material_metrics(trapped,1.0f/60.0f);
    require(trapped.invalid_reason()==sim::InvalidMotion::buried_no_escape,
        "complete terrain burial does not terminate honestly");

    sim::Environment impact(sim::CreatureBlueprint::quadruped(),0x1A2B3Cu);
    impact.set_course(sim::CourseStage::moving_hazards,0.85f);
    const Vec2 impact_root=sim::EnvironmentTestAccess::root_position(impact);
    sim::EnvironmentTestAccess::add_material(impact,{sim::MaterialKind::rock,
        impact_root+Vec2{0.10f,1.60f},{-0.20f,-5.50f},0.24f,0.94f,true});
    sim::EnvironmentTestAccess::add_material(impact,{sim::MaterialKind::sand,
        impact_root+Vec2{-0.65f,1.85f},{0.35f,-4.80f},0.07f,0.42f,true});
    for(int frame=0;frame<180;++frame)
        static_cast<void>(impact.step(idle));
    require(std::isfinite(impact.burial_depth())
            && std::isfinite(impact.incoming_time_to_impact()),
        "direct and glancing impact produced invalid material state");
    for(const sim::MaterialParticle& item:impact.material_particles())
        require(item.position.y+item.radius>=impact.ground_height_at(item.position.x)-0.005f,
            "direct impact tunneled material below terrain");
    return EXIT_SUCCESS;
}
