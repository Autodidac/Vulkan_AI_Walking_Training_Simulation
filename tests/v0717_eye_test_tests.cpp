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
    const sim::CreatureBlueprint humanoid = sim::CreatureBlueprint::humanoid();
    const std::uint64_t humanoid_handoff =
        rl::foundational_walk_teacher_handoff_update(humanoid);
    require(!rl::foundational_walk_consolidation_active(
            humanoid_handoff - 1u, sim::CourseStage::uneven, humanoid),
        "walk consolidation starts before zero-authority handoff");
    require(rl::foundational_walk_consolidation_active(
            humanoid_handoff, sim::CourseStage::uneven, humanoid)
            && rl::foundational_walk_teacher_authority(
                humanoid_handoff, humanoid) == 0.0f
            && rl::guided_rollout_imitation_weight(
                humanoid_handoff, sim::CourseStage::uneven, &humanoid) > 0.0f,
        "walk consolidation is not anchored to zero-authority handoff");
    require(rl::foundational_walk_consolidation_active(
            humanoid_handoff + rl::foundational_walk_consolidation_updates - 1u,
            sim::CourseStage::uneven, humanoid),
        "walk consolidation drops its final bounded update");
    require(!rl::foundational_walk_consolidation_active(
            humanoid_handoff + rl::foundational_walk_consolidation_updates,
            sim::CourseStage::uneven, humanoid)
            && rl::guided_rollout_imitation_weight(
                humanoid_handoff + rl::foundational_walk_consolidation_updates,
                sim::CourseStage::uneven, &humanoid) == 0.0f
            && !rl::foundational_walk_consolidation_active(
                humanoid_handoff, sim::CourseStage::crouch_walk, humanoid),
        "walk consolidation leaks past its boundary or into another lesson");

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
    require(!sim::crab_walking_motion(
            44u, 39u, 50.0f, 36.0f, 2.15f),
        "a valid sagittal gait was rejected for ending at maximum stride");
    require(!sim::crab_walking_motion(
            67u, 19u, 47.0f, 36.0f, 2.12f),
        "a sustained casual stride was rejected for sampling crossings below every transfer");
    require(sim::crab_walking_motion(
            67u, 3u, 47.0f, 36.0f, 2.12f),
        "a wide high-count gait with almost no passing phases evades crab rejection");

    require(sim::sagittal_crossing_shaping_reward(true, true, true) > 0.0f
            && sim::sagittal_crossing_shaping_reward(true, true, false) == 0.0f
            && sim::sagittal_crossing_shaping_reward(true, false, true) == 0.0f,
        "sagittal crossing reward leaks outside a paired forward transfer");
    require(sim::lateral_crab_shaping_penalty(
            true, true, 8u, 0u, 2.0f, 8.0f, 1.80f) > 0.0f
            && sim::lateral_crab_shaping_penalty(
                true, true, 12u, 10u, 8.0f, 12.0f, 1.05f) == 0.0f
            && sim::lateral_crab_shaping_penalty(
                false, true, 8u, 0u, 2.0f, 8.0f, 1.80f) == 0.0f,
        "lateral gait shaping does not match the hard crab evidence gate");
    require(sim::strict_segment_crossing({ -0.5f, 1.0f }, { 0.5f, 0.0f },
            { 0.5f, 1.0f }, { -0.5f, 0.0f })
            && !sim::strict_segment_crossing({ -0.5f, 1.0f }, { -0.5f, 0.0f },
                { 0.5f, 1.0f }, { 0.5f, 0.0f })
            && sim::awkward_paired_passing_pose(0.80f, 0.10f, 1.40f)
            && !sim::awkward_paired_passing_pose(0.80f, 0.60f, 1.40f)
            && !sim::awkward_paired_passing_pose(0.35f, 0.45f, 1.40f)
            && !sim::awkward_paired_passing_pose(0.80f, 0.10f, 0.0f)
            && sim::lower_leg_scissor_shaping_penalty(
                true, true, sim::sustained_scissor_limit_seconds) > 0.0f
            && sim::lower_leg_scissor_shaping_penalty(false, true, 1.0f) == 0.0f,
        "persistent lower-leg scissoring is not geometrically isolated and shaped");

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
        require(foot.width == 32 && foot.height == 28,
            "runtime foot atlas dimensions changed");
    }

    std::cout << "Runner v0.7.17 eye-test contracts passed\n";
    return EXIT_SUCCESS;
}
