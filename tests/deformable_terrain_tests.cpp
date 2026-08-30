#include "deformable_terrain.hpp"
#include "simulation.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
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
            environment.elapsed_seconds_ = progress / environment.course_speed();
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
        static float terrain_volume(const Environment& environment) noexcept
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
    std::array<sim::TerrainRegion, 3> previous_middle{};
    bool saw_distinct_middle_order = false;
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
        if (seed > 1u && middle != previous_middle)
            saw_distinct_middle_order = true;
        previous_middle = middle;
    }
    require(saw_distinct_middle_order,
        "terrain material-band order is fixed across seeds");

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
    const float volume=first.total_height_volume();
    for(int i=0;i<240;++i){first.apply_pressure(x,4.0f,5.0f,1.0f/20.0f);first.step(1.0f/20.0f);}
    require(same_terrain(first,immutable_cells,immutable_fine_cells),
        "foot contact changed immutable authored pixel cells");
    require(std::abs(first.total_height_volume()-volume)<2.0e-5f,
        "immutable contact path changed terrain volume");
    const float deposited=first.total_height_volume(); first.deposit(18.0f,0.12f,0.20f);
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
    const float firm_volume = firm_truth.total_height_volume();
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
    const float granular_volume = sim::EnvironmentTestAccess::terrain_volume(granular);
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
            sim::EnvironmentTestAccess::terrain_volume(subject) };
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
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-1.05f,14.0f,0.22f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.35f,10.0f,0.18f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x+0.10f,7.0f,0.20f);
    sim::EnvironmentTestAccess::refresh_material_metrics(escape,1.0f/60.0f);
    require(escape.burial_depth()>0.05f,"partial burial scenario produced no burial depth");
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
