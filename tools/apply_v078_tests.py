from pathlib import Path
R=Path(__file__).resolve().parents[1]
def x(p,o,n):
 q=R/p; t=q.read_text(); c=t.count(o)
 if c!=1: raise RuntimeError(f'{p}: {c} matches')
 q.write_text(t.replace(o,n,1))
x('CMakeLists.txt','project(Runner VERSION 0.7.7 LANGUAGES CXX)','project(Runner VERSION 0.7.8 LANGUAGES CXX)')
x('tests/core_tests.cpp','        static_assert(sim::observation_count == 40);\n','        static_assert(sim::observation_count == 50);\n')
x('CMakeLists.txt','''    add_test(NAME Runner.Core COMMAND RunnerCoreTests)

    add_executable(RunnerConcurrencyBenchmark tests/concurrency_benchmark.cpp)
''','''    add_test(NAME Runner.Core COMMAND RunnerCoreTests)

    add_executable(RunnerDeformableTerrainTests tests/deformable_terrain_tests.cpp)
    target_link_libraries(RunnerDeformableTerrainTests PRIVATE Runner::Core)
    target_compile_features(RunnerDeformableTerrainTests PRIVATE cxx_std_23)
    set_target_properties(RunnerDeformableTerrainTests PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    runner_enable_warnings(RunnerDeformableTerrainTests)
    add_test(NAME Runner.DeformableTerrain COMMAND RunnerDeformableTerrainTests)
    set_tests_properties(Runner.DeformableTerrain PROPERTIES TIMEOUT 45)

    add_executable(RunnerConcurrencyBenchmark tests/concurrency_benchmark.cpp)
''')
x('src/app.cpp','        std::filesystem::path autosave_policy_path{ "runner-v076-autosave.eppo" };\n','        std::filesystem::path autosave_policy_path{ "runner-v078-autosave.eppo" };\n')
x('src/app.cpp','        std::filesystem::path autosave_rig_path{ "runner-v076-evolved.rig" };\n','        std::filesystem::path autosave_rig_path{ "runner-v078-evolved.rig" };\n')
x('src/app.cpp','        std::filesystem::path autosave_state_path{ "runner-v076-autonomy.state" };\n','        std::filesystem::path autosave_state_path{ "runner-v078-autonomy.state" };\n')
x('src/app.cpp','''            const std::string pip_metrics = environment.course_stage() == sim::CourseStage::balance
                ? std::format("UPDATE {}  STANCE {:.1f}/{:.1f}S  SPIN {:.2f}  ARMS {:.0f} DEG",
                    trainer.metrics().update,
                    environment.longest_stable_stance_seconds(),
                    rl::standing_mastery_seconds,
                    environment.uncontrolled_spin_turns(),
                    environment.maximum_upper_body_motor_deviation() * 180.0f / pi)
                : std::format("UPDATE {}  CROUCH {:.1f}S  {:.2f}M  STEPS {}  PASSED {}",
                    trainer.metrics().update,
                    environment.crouch_walk_seconds(), environment.crouch_walk_distance(),
                    environment.gait_cycles(), environment.obstacles_passed());
''','''            const std::string pip_metrics = environment.course_stage() == sim::CourseStage::balance
                ? std::format("UPDATE {}  STANCE {:.1f}/{:.1f}S  SPIN {:.2f}  ARMS {:.0f} DEG",
                    trainer.metrics().update,
                    environment.longest_stable_stance_seconds(),
                    rl::standing_mastery_seconds,
                    environment.uncontrolled_spin_turns(),
                    environment.maximum_upper_body_motor_deviation() * 180.0f / pi)
                : environment.course_stage() == sim::CourseStage::moving_hazards
                    ? std::format("UPDATE {}  BURIAL {:.2f}M  IMPACT {:.1f}S  MATERIAL {}",
                        trainer.metrics().update, environment.burial_depth(),
                        environment.incoming_time_to_impact(), environment.material_event_count())
                    : std::format("UPDATE {}  CROUCH {:.1f}S  {:.2f}M  STEPS {}  PASSED {}",
                        trainer.metrics().update,
                        environment.crouch_walk_seconds(), environment.crouch_walk_distance(),
                        environment.gait_cycles(), environment.obstacles_passed());
''')
(R/'tests/deformable_terrain_tests.cpp').write_text(r'''#include "deformable_terrain.hpp"
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
''')
(R/'RELEASE_NOTES_v0.7.8.md').write_text('''# Runner v0.7.8

- Replaces sine-only uneven ground with a deterministic 224-cell, 56 m deformable sand heightfield shared by physics, observations, evaluation, replay, and both live/PIP rendering.
- Foot loading compacts loose cells, retains natural slip on soft support, displaces conserved volume into adjacent mounds, and relaxes over-steep slopes without creating or deleting terrain mass.
- Adds persistent falling sand, rocks, and debris; sand deposits into the terrain while rocks and debris bounce, roll, settle, and transfer impact velocity.
- Expands policy observations from 40 to 50 with firmness, looseness, burial depth, escape direction, incoming velocity, time-to-impact, density, obstruction mask, and surface slope.
- Adds burial/obstruction tracking, escape shaping, honest sustained no-escape termination, and PIP material telemetry.
- Adds seeded conservation, compaction, slope-collapse, material-spawn, observation-finiteness, and anti-tunneling regressions.
- Invalidates earlier policy/autonomy state with training semantics v0.7.8 and RUNAUTONOMY 13.
''')
Path(__file__).unlink()
