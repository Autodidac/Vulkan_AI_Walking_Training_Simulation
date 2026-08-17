#include "ppo.hpp"
#include "rig_training_diagnostic.hpp"
#include "simulation.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static float guide_backward_pose(Environment& environment) noexcept
        {
            const std::uint16_t root = environment.blueprint_.root_node;
            const std::uint16_t torso = environment.blueprint_.torso_node;
            const std::uint16_t head = environment.blueprint_.head_node;
            const Vec2 root_position = environment.particles_[root].position;
            environment.particles_[torso].position = root_position + Vec2{ -0.42f, 1.0f };
            environment.particles_[head].position = root_position + Vec2{ -0.48f, 1.72f };
            environment.particles_[torso].previous = environment.particles_[torso].position;
            environment.particles_[head].previous = environment.particles_[head].position;
            const float before = environment.particles_[torso].position.x - root_position.x;
            environment.stabilize_balance_posture();
            const float after = environment.particles_[torso].position.x - root_position.x;
            return after - before;
        }
    };
}

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner v0.7.37 locomotion/terrain failure: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    std::size_t phase_group(const runner::sim::CreatureBlueprint& rig,
        const runner::sim::MotorConstraint& motor)
    {
        return runner::rl::multi_support_phase_group(rig, motor);
    }
}

int main()
{
    using namespace runner;

    const Vec2 backward_authored{ -0.36f, 1.0f };
    require(sim::directional_backward_brace_ratio(backward_authored,
            backward_authored, 1.0f) > sim::backward_brace_activation_ratio,
        "an authored backward rest axis waived ground-relative posture truth");
    require(sim::directional_backward_brace_ratio({ 0.0f, 1.0f },
            { 0.14f, 1.0f }, 1.0f) == 0.0f,
        "a natural forward posture was classified as a backward brace");
    require(sim::directional_backward_brace_ratio({ 0.0f, 1.0f },
            { 0.14f, 1.0f }, -1.0f) > 0.10f,
        "travel reversal did not reverse posture truth");

    for (const float hz : std::array{ 20.0f, 60.0f, 240.0f })
    {
        float seconds = 0.0f;
        const float dt = 1.0f / hz;
        for (int step = 0; step < static_cast<int>(hz * 1.0f); ++step)
            seconds = sim::contiguous_condition_seconds(true, seconds, dt);
        require(std::abs(seconds - 1.0f) < 1.0e-4f,
            "backward-brace dwell changed with render cadence");
    }

    sim::CreatureBlueprint edited = sim::CreatureBlueprint::humanoid();
    edited.nodes[edited.torso_node].x = edited.nodes[edited.root_node].x - 0.35f;
    edited.nodes[edited.head_node].x = edited.nodes[edited.root_node].x - 0.42f;
    edited.rebuild_rest_lengths();
    sim::Environment edited_walk{ edited, 0x73701u };
    edited_walk.set_course(sim::CourseStage::uneven, 0.30f);
    require(sim::EnvironmentTestAccess::guide_backward_pose(edited_walk) > 0.0f,
        "the physical locomotion guide reinforced an edited backward rest axis");

    const sim::CreatureBlueprint quadruped = sim::CreatureBlueprint::quadruped();
    const std::array<std::size_t, 8> quadruped_groups{ 0u, 0u, 1u, 1u,
        0u, 0u, 1u, 1u };
    for (std::size_t index = 0; index < quadruped.active_motor_count; ++index)
    {
        require(std::isfinite(quadruped.support_branch_center_x(
                quadruped.motors[index])),
            "quadruped support branch lost its authored endpoint");
        require(phase_group(quadruped, quadruped.motors[index])
                == quadruped_groups[index],
            "quadruped did not derive diagonal phases from authored topology");
    }
    const sim::CreatureBlueprint hexapod = sim::CreatureBlueprint::hexapod();
    const std::array<std::size_t, 6> hexapod_groups{ 0u, 1u, 0u, 1u, 0u, 1u };
    for (std::size_t index = 0; index < hexapod.active_motor_count; ++index)
        require(phase_group(hexapod, hexapod.motors[index]) == hexapod_groups[index],
            "hexapod did not derive alternating tripod phases from authored topology");

    require(!rl::multi_support_progress_truth(1.20f, 34u, 20.0f),
        "inflated contact cycling was accepted as multi-support progress");
    require(rl::multi_support_progress_truth(2.0f, 20u, 8.0f),
        "useful multi-support incremental progress was rejected");

    diagnostics::RigTrainingResult retained{};
    retained.retained_policy = true;
    retained.retained_quality = rl::strict_evaluation_quality_bit | 1u;
    retained.retained_update = rl::foundational_walk_teacher_handoff_update(quadruped);
    retained.retained_probe_stride_events = rl::multi_support_release_stride_events(quadruped);
    retained.retained_probe_distance = 2.1616f;
    require(!diagnostics::retained_policy_release_eligible(retained, quadruped),
        "the packaged v0.7.36 two-metre quadruped still passed release");
    retained.retained_probe_distance = rl::multi_support_release_distance(quadruped);
    require(diagnostics::retained_policy_release_eligible(retained, quadruped),
        "a strict useful-distance quadruped replay failed the release contract");

    locomotion::Signals hole{};
    hole.uprightness = 0.88f;
    hole.left_supported = true;
    hole.right_supported = true;
    hole.left_support_x = -0.28f;
    hole.right_support_x = 0.28f;
    hole.left_escape_rise = 0.24f;
    hole.right_escape_rise = 0.38f;
    hole.zero_progress_seconds = 0.75f;
    hole.gait_cycles = 18u;
    hole.requested_direction = 1.0f;
    const locomotion::Plan hole_plan = locomotion::plan(hole);
    require(hole_plan.intent == locomotion::Intent::escape
            && hole_plan.direction < 0.0f && !hole_plan.brake,
        "the runtime brain did not choose the lower hole exit after a real stall");
    hole.zero_progress_seconds = 0.20f;
    require(locomotion::plan(hole).intent != locomotion::Intent::escape,
        "ordinary slow footing falsely triggered deterministic hole escape");

    locomotion::Signals falling = hole;
    falling.left_escape_rise = 0.0f;
    falling.right_escape_rise = 0.0f;
    falling.zero_progress_seconds = 0.0f;
    falling.incoming_density = 0.90f;
    falling.incoming_time_to_impact = 0.55f;
    falling.incoming_velocity_x = 2.0f;
    falling.free_space_direction = -1.0f;
    const locomotion::Plan dodge = locomotion::plan(falling);
    require(dodge.intent == locomotion::Intent::flee && dodge.direction < 0.0f,
        "the runtime brain did not react to a newly observed falling hazard");
    falling.requested_direction = 0.0f;
    falling.turning = false;
    require(locomotion::plan(falling).intent == locomotion::Intent::flee,
        "an idle rig held still under an imminent falling hazard");
    falling.requested_direction = 1.0f;
    falling.turning = true;
    require(locomotion::plan(falling).intent == locomotion::Intent::flee,
        "a turning rig held still under an imminent falling hazard");

    for (const float hz : std::array{ 20.0f, 60.0f, 240.0f })
    {
        locomotion::Signals cadence = hole;
        cadence.zero_progress_seconds = 0.0f;
        const float dt = 1.0f / hz;
        for (int step = 0; step < static_cast<int>(hz * 0.75f); ++step)
            cadence.zero_progress_seconds += dt;
        const locomotion::Plan cadence_plan = locomotion::plan(cadence);
        require(cadence_plan.intent == locomotion::Intent::escape
                && cadence_plan.direction < 0.0f,
            "hybrid stuck recovery changed with render cadence");
    }

    for (const std::uint64_t seed : std::array<std::uint64_t, 4>{ 1u, 7u, 0x737u, 0x737737u })
    {
        sim::DeformableTerrain terrain{};
        terrain.reset(seed, 0.72f);
        std::size_t transitions = 0u;
        for (std::size_t index = 0; index + 1u < sim::DeformableTerrain::cell_count;
            ++index)
        {
            const auto& current = terrain.cells()[index];
            const auto& next = terrain.cells()[index + 1u];
            if (current.region != next.region)
                ++transitions;
            if (current.region == sim::TerrainRegion::hole
                || next.region == sim::TerrainRegion::hole)
                continue;
            require(std::abs(current.height - next.height) < 0.11f,
                "ordinary material boundary created a curb or one-cell spike");
        }
        require(transitions <= 10u,
            "terrain tail fragmented into repeated short material slabs");

        sim::DeformableTerrain repeated{};
        repeated.reset(seed, 0.72f);
        for (std::size_t index = 0; index < sim::DeformableTerrain::cell_count; ++index)
            require(terrain.cells()[index].height == repeated.cells()[index].height
                    && terrain.cells()[index].region == repeated.cells()[index].region,
                "coherent terrain field changed for a repeated seed");
    }

    std::cout << "Runner v0.7.37 locomotion and terrain truth passed\n";
    return EXIT_SUCCESS;
}
