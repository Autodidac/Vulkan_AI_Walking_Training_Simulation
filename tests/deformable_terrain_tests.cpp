#include "deformable_terrain.hpp"
#include "simulation.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
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
            environment.terrain_.deposit(world_x + environment.course_progress(),
                amount, firmness);
        }

        static void refresh_material_metrics(Environment& environment, float dt) noexcept
        {
            environment.update_material_metrics(dt);
        }

        static void add_material(Environment& environment, MaterialParticle item)
        {
            environment.material_particles_.push_back(item);
            environment.rebuild_course_features();
        }
    };
}

namespace { void require(bool ok, std::string_view message) { if (ok) return; std::cerr << "Runner deformable terrain test failed: " << message << '\n'; std::exit(EXIT_FAILURE); } }
int main()
{
    using namespace runner;
    sim::DeformableTerrain first{}, second{};
    first.reset(0x12345678u, 0.72f); second.reset(0x12345678u, 0.72f);
    for (std::size_t i=0; i<sim::DeformableTerrain::cell_count; ++i)
    {
        require(std::abs(first.cells()[i].height-second.cells()[i].height)<1.0e-7f, "same seed changed height");
        require(std::abs(first.cells()[i].firmness-second.cells()[i].firmness)<1.0e-7f, "same seed changed firmness");
    }
    constexpr float x=12.5f;
    const float volume=first.total_height_volume(), height=first.height_at(x), firmness=first.firmness_at(x);
    first.apply_pressure(x,2.4f,0.65f,1.0f/60.0f);
    require(first.height_at(x)<height,"pressure did not compact sand");
    require(first.firmness_at(x)>firmness,"pressure did not firm sand");
    require(std::abs(first.total_height_volume()-volume)<2.0e-5f,"pressure did not conserve volume");
    const float deposited=first.total_height_volume(); first.deposit(18.0f,0.12f,0.20f);
    require(std::abs((first.total_height_volume()-deposited)-0.12f)<2.0e-4f,"deposit lost volume");
    const float slope=first.maximum_neighbor_delta();
    for(int i=0;i<240;++i) first.step(1.0f/60.0f);
    require(first.maximum_neighbor_delta()<=slope+1.0e-5f,"collapse increased maximum slope");
    require(std::abs(first.total_height_volume()-(deposited+0.12f))<8.0e-4f,"collapse leaked volume");
    sim::Environment environment(sim::CreatureBlueprint::quadruped(),0x5a17u);
    environment.set_course(sim::CourseStage::moving_hazards,0.80f);
    std::array<float,sim::action_count> idle{};
    for(int frame=0;frame<240;++frame) static_cast<void>(environment.step(idle));
    require(environment.material_event_count()>0u,"moving hazards spawned no persistent material");
    const auto observation=environment.observation(); static_assert(observation.size()==50u);
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
    const Vec2 escape_root=sim::EnvironmentTestAccess::root_position(escape);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-1.05f,14.0f,0.22f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.35f,10.0f,0.18f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x+0.10f,7.0f,0.20f);
    sim::EnvironmentTestAccess::refresh_material_metrics(escape,1.0f/60.0f);
    require(escape.burial_depth()>0.05f,"partial burial scenario produced no burial depth");
    require(escape.free_space_direction()>0.0f,"partial burial did not identify the open escape side");

    sim::Environment trapped(sim::CreatureBlueprint::quadruped(),0xB091EDu);
    trapped.set_course(sim::CourseStage::moving_hazards,0.90f);
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
