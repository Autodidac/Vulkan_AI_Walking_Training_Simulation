#include "autonomy.hpp"
#include "pixel_art.hpp"
#include "ppo.hpp"
#include "simulation.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner v0.7.17 eye-test contract failed: "
                  << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv)
{
    using namespace runner;
    const sim::CreatureBlueprint biped = sim::CreatureBlueprint::biped();
    require(biped.additional_left_contact_nodes.empty()
            && biped.additional_right_contact_nodes.empty(),
        "biped still has heel/ball/toe contact arrays");
    require(biped.left_contact_node != biped.right_contact_node,
        "stub supports are fused");
    require(biped.is_support_seed(biped.left_contact_node)
            && biped.is_support_seed(biped.right_contact_node),
        "stub supports are not semantic contacts");
    require(biped.radii[biped.left_contact_node] >= 0.104f
            && biped.radii[biped.right_contact_node] >= 0.104f,
        "stub supports are too small to carry the authored stance");

    const sim::DuckPressProfile upright =
        sim::duck_press_profile(6.5f, 0.5f, 4.8f, false);
    const sim::DuckPressProfile quadruped =
        sim::duck_press_profile(6.5f, 0.5f, 3.2f, true);
    require(quadruped.bottom_y > 2.65f,
        "horizontal press target still crushes the body plan");
    require((3.2f - quadruped.bottom_y) < (4.8f - upright.bottom_y),
        "quadruped press drop is not shallower than biped drop");

    require(!rl::stage_fresh_work_complete(sim::CourseStage::uneven,
            419u, 8u, 8u),
        "walk can master before minimum fresh updates");
    require(!rl::stage_fresh_work_complete(sim::CourseStage::uneven,
            420u, 7u, 8u),
        "walk can master before minimum fresh episodes");
    require(rl::stage_fresh_work_complete(sim::CourseStage::uneven,
            420u, 8u, 8u),
        "valid fresh walk work is rejected");

    require(sim::sagittal_gait_evidence(
            12u, 10u, 8.0f, 12.0f, 1.05f),
        "sustained sagittal gait does not qualify");
    require(!sim::sagittal_gait_evidence(
            2u, 1u, 1.0f, 2.0f, 1.05f),
        "two steps incorrectly qualify");
    require(sim::crab_walking_motion(
            8u, 0u, 2.0f, 8.0f, 1.80f),
        "wide lateral crab gait is not rejected");
    require(!sim::crab_walking_motion(
            12u, 10u, 8.0f, 12.0f, 1.05f),
        "normal sagittal gait is marked as crab walking");

    sim::Environment quad{ sim::CreatureBlueprint::quadruped(), 0x717200u };
    quad.set_course(sim::CourseStage::duck_press, 0.45f);
    bool terminated = false;
    for (int frame = 0; frame < 900 && !quad.duck_press_completed(); ++frame)
    {
        const auto action = rl::effective_policy_action(
            quad, {}, sim::CourseStage::duck_press);
        const sim::StepResult result = quad.step(action, 1.0f / 60.0f);
        terminated = result.terminated;
        if (terminated)
            break;
    }
    require(!terminated, "quadruped terminates under the press");
    require(quad.duck_press_completed(),
        "quadruped does not hold and recover from the press");
    require(quad.duck_recoveries() >= 1u
            && quad.stable_stance_seconds() >= 1.0f,
        "quadruped recovery is not stably held");

    if (argc > 1)
    {
        art::PixelArt foot{};
        std::string error{};
        require(art::load_p3_pixel_art(
                std::filesystem::path(argv[1]) / "optional"
                    / "runner_armor_concepts" / "runtime"
                    / "foot_side.ppm",
                foot, error),
            "runtime foot atlas does not load");
        require(foot.width == 64 && foot.height == 40,
            "runtime foot atlas dimensions changed");
    }

    std::cout << "Runner v0.7.17 eye-test contracts passed\n";
    return EXIT_SUCCESS;
}
