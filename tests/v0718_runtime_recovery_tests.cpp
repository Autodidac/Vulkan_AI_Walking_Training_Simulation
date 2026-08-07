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
    require(rl::stage_minimum_fresh_updates(sim::CourseStage::balance) == 120u,
        "Stand fresh-work gate changed unexpectedly");
    require(!rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 10u, 3u),
        "update-10 policy reset remains possible");
    require(!rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 120u, 12u),
        "nursery reset occurs as soon as Stand dwell completes");
    require(rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 240u, 12u),
        "extended nursery reset can never activate");

    require(ui_layout::course_reference_marker_spacing_m(
            ui_layout::DistanceUnits::metric) == 10.0f,
        "metric markers are not visible near the start");
    require(std::abs(ui_layout::course_reference_marker_spacing_m(
            ui_layout::DistanceUnits::imperial) - 15.24f) < 0.0001f,
        "imperial marker spacing is not 50 feet");

    require(std::abs(sim::terrain_relative_distance(0.0f, 0.0f, 6.0f) - 6.0f)
            < 0.0001f,
        "moving-course distance ignores terrain progress");
    require(std::abs(sim::terrain_relative_frame_progress(
            1.0f, 1.0f, 1.25f, 0.02f) - 0.025f) < 0.0001f,
        "camera-centered treadmill motion has zero logical progress");

    sim::Environment walking{ sim::CreatureBlueprint::biped(), 0x718u };
    walking.set_course(sim::CourseStage::uneven, 0.30f);
    const std::array<float, sim::action_count> neutral{};
    for (int frame = 0; frame < 12; ++frame)
        (void)walking.step(neutral);
    require(walking.distance_travelled() > 0.10f,
        "moving Walk course still reports zero terrain-relative distance");
    require(walking.forward_speed() > 0.20f,
        "moving Walk course still reports zero ground-relative speed");
    const auto teacher = rl::walking_teacher_action(walking);
    require(teacher[0] * teacher[2] <= 0.0f,
        "paired hips are not driven in opposite sagittal phases");
    const auto assisted = rl::effective_policy_action(
        walking, neutral, sim::CourseStage::uneven);
    require(std::abs(assisted[0] - assisted[2]) > 0.08f,
        "Walk assistance has no useful left/right sagittal separation");

    std::cout << "Runner v0.7.18 runtime recovery contracts passed\n";
    return 0;
}
