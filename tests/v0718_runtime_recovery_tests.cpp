#include "autonomy.hpp"
#include "ppo.hpp"
#include "ui_layout.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "Runner v0.7.18 regression failed: " << message << '\n';
            std::exit(1);
        }
    }
}

int main()
{
    using namespace runner;
    require(rl::stage_minimum_fresh_updates(sim::CourseStage::balance) == 80u,
        "Stand fresh-work gate changed unexpectedly");
    require(!rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 10u, 3u),
        "update-10 policy reset remains possible");
    require(!rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 80u, 12u),
        "nursery reset occurs as soon as Stand dwell completes");
    require(rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 200u, 12u),
        "extended nursery reset can never activate");
    require(rl::stage_minimum_fresh_updates(sim::CourseStage::duck_press)
            == rl::crouch_teacher_handoff_update
            && !rl::stage_fresh_work_complete(sim::CourseStage::duck_press,
                rl::crouch_teacher_handoff_update - 1u, 4u, 8u)
            && !rl::stage_fresh_work_complete(sim::CourseStage::duck_press,
                rl::crouch_teacher_handoff_update, 3u, 8u)
            && !rl::stage_fresh_work_complete(sim::CourseStage::duck_press,
                rl::crouch_teacher_handoff_update, 4u, 7u)
            && rl::stage_fresh_work_complete(sim::CourseStage::duck_press,
                rl::crouch_teacher_handoff_update, 4u, 8u),
        "Crouch readiness is late, under-evidenced, or off its exact boundary");

    require(ui_layout::course_reference_marker_spacing_m(
            ui_layout::DistanceUnits::metric) == 10.0f,
        "metric markers are not visible near the start");
    require(std::abs(ui_layout::course_reference_marker_spacing_m(
            ui_layout::DistanceUnits::imperial) - 15.24f) < 0.0001f,
        "imperial marker spacing is not 50 feet");

    require(std::abs(sim::terrain_relative_distance(0.0f, 0.0f, 6.0f))
            < 0.0001f,
        "static-world distance still credits terrain progress");
    require(std::abs(sim::terrain_relative_frame_progress(
            1.0f, 1.0f, 1.25f, 0.02f)) < 0.0001f,
        "static-world frame progress still contains treadmill motion");

    sim::Environment walking{ sim::CreatureBlueprint::biped(), 0x718u };
    walking.set_course(sim::CourseStage::uneven, 0.30f);
    const std::array<float, sim::action_count> neutral{};
    for (int frame = 0; frame < 12; ++frame)
        (void)walking.step(neutral);
    require(walking.distance_travelled() < 0.02f,
        "idle rig receives distance credit from a moving course");
    require(!walking.course_motion_enabled() && walking.course_speed() == 0.0f,
        "course motion can still be enabled through the runtime API");
    const auto teacher = rl::walking_teacher_action(walking);
    require(std::abs(teacher[0] - teacher[2]) > 0.08f,
        "paired support chains have no opposite-phase sagittal separation");
    const auto assisted = rl::effective_policy_action(
        walking, neutral, sim::CourseStage::uneven);
    require(std::abs(assisted[0] - assisted[2]) > 0.08f,
        "Walk assistance has no useful left/right sagittal separation");

    std::cout << "Runner v0.7.18 runtime recovery contracts passed\n";
    return 0;
}
