#include <Epoch2DWalkEngine/Engine.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>

static_assert(sandhybrid::library_api_version == 4u);
static_assert(epoch2dwalk::director::State::schema_version == 2u);
static_assert(epoch2dwalk::director::physical_trial_evidence_schema_version == 1u);
static_assert(epoch2dwalk::diagnostics::raw_inference_replay_seeds.size() == 6u);
static_assert(std::is_same_v<
    decltype(&epoch2dwalk::diagnostics::run_raw_inference_diagnostic),
    epoch2dwalk::diagnostics::RawInferenceReport (*)(std::uint64_t)>);

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
    request.mode = epoch2dwalk::integration::ControlMode::training;
    request.joint_control =
        epoch2dwalk::integration::JointControlSource::expert_joint_actions;
    request.guidance = epoch2dwalk::integration::GuidanceRequest::raw_policy;
    const epoch2dwalk::integration::FixedStepResult result =
        epoch2dwalk::integration::fixed_step(environment, request, 7u, 1u);
    if (!std::isfinite(result.reward)
        || !epoch2dwalk::integration::canonical_snapshot_valid(result.snapshot)
        || result.snapshot.schema
            != epoch2dwalk::integration::canonical_snapshot_schema_version
        || result.snapshot.integration_api
            != epoch2dwalk::integration::api_version
        || !epoch2dwalk::integration::valid_action_layout(
            result.snapshot.action_layout)
        || !result.snapshot.action_layout.active[
            epoch2dwalk::sim::human_left_ankle_action]
        || !result.snapshot.action_layout.active[
            epoch2dwalk::sim::human_right_ankle_action]
        || result.snapshot.trial_id != 7u || result.snapshot.fixed_tick != 1u
        || !result.control_applied || result.authority.optional_guidance_active)
        return 4;

    const auto graph = epoch2dwalk::director::default_training_graph();
    const auto events = epoch2dwalk::integration::diagnostic_events(
        nullptr, result, graph.nodes[0].stable_id);
    if (events.span().empty() || result.snapshot.stable_challenge_id == 0u)
        return 5;
    if (!graph.valid())
        return 6;
    epoch2dwalk::director::Evidence scalar_water{};
    for (std::size_t index = 0u; index < 3u; ++index)
        scalar_water.mastery[index] = graph.nodes[index].minimum_evidence;
    const auto scalar_water_completion = epoch2dwalk::director::reduce_evidence(
        graph, scalar_water,
        { graph.nodes[3].stable_id, 3u,
            epoch2dwalk::director::TrialOutcome::completed,
            graph.nodes[3].minimum_evidence, {} },
        epoch2dwalk::director::Profile::curriculum);
    if (scalar_water_completion.accepted)
        return 12;
    epoch2dwalk::director::Evidence evidence{};
    evidence.mastery[0] = 1.0f;
    const auto decision = epoch2dwalk::director::select(graph, evidence,
        epoch2dwalk::director::Profile::player_guided);
    if (!decision.selected())
        return 7;
    epoch2dwalk::sim::Environment directed{ rig, 0xE20Eu };
    directed.set_raw_authority_telemetry_enabled(true);
    epoch2dwalk::integration::ControlRequest directed_request{};
    directed_request.mode = epoch2dwalk::integration::ControlMode::player_guided;
    directed_request.task_graph = graph;
    directed_request.evidence = evidence;
    directed_request.selected_task = epoch2dwalk::integration::task_command(
        graph, evidence, decision,
        epoch2dwalk::director::Profile::player_guided);
    directed_request.player_intent =
        epoch2dwalk::integration::player_intent_for(
            directed_request.selected_task);
    directed_request.joint_control =
        epoch2dwalk::integration::JointControlSource::retained_policy;
    directed_request.guidance =
        epoch2dwalk::integration::GuidanceRequest::raw_policy;
    epoch2dwalk::rl::PpoTrainer trainer{ directed.blueprint(), 8u, false };
    const epoch2dwalk::rl::PpoTrainer::VerifiedCheckpoint checkpoint =
        trainer.verified_checkpoint();
    if (!checkpoint.valid()
        || checkpoint.data().rig_signature != directed.blueprint().signature()
        || checkpoint.data().training_semantics
            != epoch2dwalk::rl::training_semantics_version)
        return 8;
    epoch2dwalk::integration::RetainedPolicyController controller{};
    std::string controller_error{};
    if (controller.load_retained_best(checkpoint, controller_error))
        return 9;
    const epoch2dwalk::rl::PpoTrainer::VerifiedCheckpoint forged{};
    if (controller.load_retained_best(forged, controller_error))
        return 10;
    const auto directed_result = epoch2dwalk::integration::fixed_step(
        directed, directed_request, controller, 8u, 1u);
    if (directed_result.control_applied
        || directed_result.control_rejection
            != epoch2dwalk::integration::ControlRejection::retained_policy_mismatch)
        return 11;

    std::cout << "Epoch2DWalkEngine 0.7.51 library API, task graph, physical fixed-step request, physical-evidence gate, and fail-closed retained artifact boundary passed\n";
    return 0;
}
