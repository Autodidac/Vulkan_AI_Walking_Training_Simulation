#include "deformable_terrain.hpp"
#include "simulation.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>
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
    return EXIT_SUCCESS;
}
