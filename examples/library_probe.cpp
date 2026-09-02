#include <Epoch2DWalkEngine/Engine.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

int main()
{
    const epoch2dwalk::sim::CreatureBlueprint rig =
        epoch2dwalk::sim::CreatureBlueprint::humanoid();
    if (!rig.valid() || rig.presentation_species()
            != epoch2dwalk::sim::CreatureSpecies::human)
        return 1;

    epoch2dwalk::sim::Environment environment{ rig, 0xE20Du };
    environment.set_course(epoch2dwalk::sim::CourseStage::uneven, 0.25f);
    const auto observation = environment.observation();
    if (observation.size() != epoch2dwalk::sim::observation_count)
        return 2;
    for (const float value : observation)
    {
        if (!std::isfinite(value))
            return 3;
    }

    epoch2dwalk::integration::ControlRequest request{};
    request.mode = epoch2dwalk::integration::ControlMode::player_guided;
    const epoch2dwalk::integration::FixedStepResult result =
        epoch2dwalk::integration::fixed_step(environment, request, 7u, 1u);
    if (!std::isfinite(result.reward) || !result.snapshot.valid
        || result.snapshot.trial_id != 7u || result.snapshot.fixed_tick != 1u)
        return 4;

    const auto graph = epoch2dwalk::director::default_training_graph();
    const auto events = epoch2dwalk::integration::diagnostic_events(
        nullptr, result, graph.nodes[0].stable_id);
    if (events.span().empty() || result.snapshot.stable_challenge_id == 0u)
        return 5;
    if (!graph.valid())
        return 6;
    epoch2dwalk::director::Evidence evidence{};
    evidence.mastery[0] = 1.0f;
    const auto decision = epoch2dwalk::director::select(graph, evidence,
        epoch2dwalk::director::Profile::player_guided);
    if (!decision.selected())
        return 7;
    epoch2dwalk::sim::Environment directed{ rig, 0xE20Eu };
    epoch2dwalk::integration::ControlRequest directed_request{};
    directed_request.mode = epoch2dwalk::integration::ControlMode::player_guided;
    directed_request.selected_task = epoch2dwalk::integration::task_command(
        graph, decision);
    const auto directed_result = epoch2dwalk::integration::fixed_step(
        directed, directed_request, 8u, 1u);
    if (!directed_result.task_applied || directed_result.task_rejected
        || directed_result.stable_task_id != decision.stable_id
        || directed.course_stage() != epoch2dwalk::sim::CourseStage::uneven
        || directed.gait_task() != epoch2dwalk::sim::GaitTask::walk)
        return 8;

    std::cout << "Epoch2DWalkEngine 0.7.50 library API, task graph, and external host fixed-step passed\n";
    return 0;
}
