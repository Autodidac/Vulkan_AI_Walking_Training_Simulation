#include <Epoch2DWalkEngine/Engine.hpp>

#include "course_geometry.hpp"
#include "raw_learning_test_access.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Epoch2DWalkEngine v0.7.51 Runner-example Director challenge test failed: "
                  << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    [[nodiscard]] runner::director::TaskGraph isolated_graph_for(
        runner::director::TaskGraph graph,
        std::uint16_t task_index) noexcept
    {
        graph.nodes[task_index].prerequisite_count = 0u;
        return graph;
    }

    [[nodiscard]] runner::director::PhysicalTrialEvidence water_proof(
        const runner::director::TaskNode& node, std::uint64_t trial_id) noexcept
    {
        namespace director = runner::director;
        director::PhysicalTrialEvidence proof{};
        proof.schema = director::physical_trial_evidence_schema_version;
        proof.kind = director::PhysicalEvidenceKind::water_crossing;
        proof.trial_id = trial_id;
        proof.stable_task_id = node.stable_id;
        proof.stable_challenge_id = node.stable_challenge_id;
        proof.first_tick = 1u;
        proof.last_tick = 50u;
        proof.water = { 2u, 10u, 11u, 24u, 40u, 45u, 0.72f, 0.68f };
        return proof;
    }

    [[nodiscard]] runner::director::PhysicalTrialEvidence equipment_proof(
        const runner::director::TaskNode& node, std::uint64_t trial_id) noexcept
    {
        namespace director = runner::director;
        director::PhysicalTrialEvidence proof{};
        proof.schema = director::physical_trial_evidence_schema_version;
        proof.kind = director::PhysicalEvidenceKind::equipment_correction;
        proof.trial_id = trial_id;
        proof.stable_task_id = node.stable_id;
        proof.stable_challenge_id = node.stable_challenge_id;
        proof.first_tick = 1u;
        proof.last_tick = 50u;
        proof.equipment = { 4u, 10u, 20u, 21u, 40u,
            0.40f, 0.35f, 0.60f, -0.05f, 0.0f, 2u, 1u, 1u, true };
        return proof;
    }

    void test_physical_mastery_reducer_is_fail_closed()
    {
        namespace director = runner::director;
        const director::TaskGraph graph = director::default_training_graph();

        director::Evidence water_ready{};
        for (std::size_t index = 0u; index < 3u; ++index)
            water_ready.mastery[index] = graph.nodes[index].minimum_evidence;
        require(director::evidence_valid(graph, water_ready),
            "ordinary prerequisites did not form valid water evidence");
        const director::TaskNode& wet = graph.nodes[3];
        const director::TrialEvidence scalar_water{ wet.stable_id, 3u,
            director::TrialOutcome::completed, wet.minimum_evidence, {} };
        require(!director::reduce_evidence(graph, water_ready, scalar_water,
                    director::Profile::curriculum).accepted,
            "caller-supplied scalar mastery completed a water node");

        director::TrialEvidence crossed_water = scalar_water;
        crossed_water.physical = water_proof(wet, 0x751300u);
        const director::EvidenceReduction water_first = director::reduce_evidence(
            graph, water_ready, crossed_water, director::Profile::curriculum);
        const director::EvidenceReduction water_repeated = director::reduce_evidence(
            graph, water_ready, crossed_water, director::Profile::curriculum);
        require(water_first.accepted && water_repeated.accepted
                && water_first.evidence.mastery == water_repeated.evidence.mastery
                && water_first.evidence.physical_mastery
                    == water_repeated.evidence.physical_mastery
                && director::task_complete(graph, water_first.evidence, 3u),
            "ordered water proof was not accepted deterministically");
        const director::EvidenceReduction replayed_water =
            director::reduce_evidence(graph, water_first.evidence,
                crossed_water, director::Profile::curriculum);
        require(!replayed_water.accepted
                && replayed_water.evidence.physical_mastery
                    == water_first.evidence.physical_mastery,
            "completed trial receipt replay mutated mastered evidence");
        director::TrialEvidence failed_with_completion_receipt = crossed_water;
        failed_with_completion_receipt.outcome = director::TrialOutcome::failed;
        failed_with_completion_receipt.measured_mastery = 0.50f;
        const director::EvidenceReduction failed_water =
            director::reduce_evidence(graph, water_ready,
                failed_with_completion_receipt, director::Profile::curriculum);
        require(!failed_water.accepted
                && failed_water.evidence.mastery == water_ready.mastery
                && failed_water.evidence.physical_mastery
                    == water_ready.physical_mastery,
            "failed trial carried a completion receipt into mastery");

        for (int adversary = 0; adversary < 7; ++adversary)
        {
            director::TrialEvidence forged = crossed_water;
            if (adversary == 0)
                forged.physical.stable_challenge_id ^= 1u;
            else if (adversary == 1)
                forged.physical.water.far_shore_dry_tick =
                    forged.physical.water.shore_exit_tick;
            else if (adversary == 2)
                forged.physical.water.maximum_depth = 0.0f;
            else if (adversary == 3)
                forged.physical.water.maximum_submersion =
                    std::numeric_limits<float>::infinity();
            else if (adversary == 4)
                forged.physical.trial_id = 0u;
            else if (adversary == 5)
                forged.physical.water.swimming_tick =
                    forged.physical.last_tick + 1u;
            else
                forged.physical.stable_task_id ^= 1u;
            const director::EvidenceReduction rejected = director::reduce_evidence(
                graph, water_ready, forged, director::Profile::curriculum);
            require(!rejected.accepted
                    && rejected.evidence.mastery == water_ready.mastery,
                "forged, shallow, non-finite, or out-of-order water proof mutated mastery");
        }

        director::Evidence equipment_ready{};
        equipment_ready.mastery[0] = graph.nodes[0].minimum_evidence;
        equipment_ready.mastery[1] = graph.nodes[1].minimum_evidence;
        equipment_ready.mastery[10] = graph.nodes[10].minimum_evidence;
        equipment_ready.mastery[11] = graph.nodes[11].minimum_evidence;
        require(director::evidence_valid(graph, equipment_ready),
            "ordinary prerequisites did not form valid equipment evidence");

        for (std::uint16_t index = 12u; index <= 15u; ++index)
        {
            const director::TaskNode& node = graph.nodes[index];
            const director::TrialEvidence scalar{ node.stable_id, index,
                director::TrialOutcome::completed, node.minimum_evidence, {} };
            require(!director::reduce_evidence(graph, equipment_ready, scalar,
                        director::Profile::autonomous_npc).accepted,
                "hit/transition-style scalar completed a physical equipment node");

            director::TrialEvidence physical = scalar;
            physical.physical = equipment_proof(node, 0x751400u + index);
            if (index == 12u)
            {
                for (int adversary = 0; adversary < 8; ++adversary)
                {
                    director::TrialEvidence forged = physical;
                    if (adversary == 0)
                        forged.physical.equipment.shots_missed = 0u;
                    else if (adversary == 1)
                        forged.physical.equipment.applied_correction = 0.30f;
                    else if (adversary == 2)
                        forged.physical.equipment.applied_correction = 0.05f;
                    else if (adversary == 3)
                        forged.physical.equipment.improved_shot_error = 0.70f;
                    else if (adversary == 4)
                        forged.physical.equipment.physical_aim_tick = 21u;
                    else if (adversary == 5)
                        forged.physical.equipment.observed_miss_error =
                            std::numeric_limits<float>::quiet_NaN();
                    else if (adversary == 6)
                        forged.physical.equipment.correction_tick =
                            forged.physical.equipment.miss_tick;
                    else
                    {
                        forged.physical.equipment.shots_fired =
                            std::numeric_limits<std::uint32_t>::max();
                        forged.physical.equipment.shots_missed =
                            std::numeric_limits<std::uint32_t>::max();
                    }
                    require(!director::reduce_evidence(graph, equipment_ready,
                                forged, director::Profile::autonomous_npc).accepted,
                        "synthetic-only, unbounded, wrong-sign, unimproved, out-of-order, or non-finite equipment proof was accepted");
                }
            }
            const director::EvidenceReduction accepted = director::reduce_evidence(
                graph, equipment_ready, physical,
                director::Profile::autonomous_npc);
            require(accepted.accepted
                    && director::task_complete(graph, accepted.evidence, index)
                    && accepted.evidence.physical_mastery[index]
                        == physical.physical,
                "ordered physical miss/correction proof did not master its bound node");
            equipment_ready = accepted.evidence;
        }

        director::Evidence scalar_only = equipment_ready;
        scalar_only.physical_mastery[15] = {};
        require(!director::evidence_valid(graph, scalar_only)
                && !director::select(graph, scalar_only,
                    director::Profile::autonomous_npc).selected(),
            "persistable evidence accepted completed physical mastery without its receipt");
    }

    void test_every_enabled_node_reaches_physics()
    {
        namespace director = runner::director;
        namespace integration = runner::integration;
        namespace rl = runner::rl;
        namespace sim = runner::sim;

        constexpr director::TaskGraph graph = director::default_training_graph();
        static_assert(graph.valid());
        static_assert(graph.nodes[7].challenge == director::ChallengeKind::ramp);
        static_assert(graph.nodes[8].challenge == director::ChallengeKind::step);
        static_assert(!graph.nodes[16].enabled && !graph.nodes[17].enabled
            && !graph.nodes[20].enabled);

        for (std::uint16_t index = 0u; index < graph.count; ++index)
        {
            const director::TaskNode& node = graph.nodes[index];
            if (!node.enabled)
            {
                require(!integration::task_node_has_physical_binding(node),
                    "disabled future node reports a physical binding");
                continue;
            }
            require(node.challenge != director::ChallengeKind::moving_target,
                "static range is mislabeled as a moving target");
            require(integration::task_node_has_physical_binding(node),
                "enabled graph node has no physical binding");

            const director::TaskGraph isolated = isolated_graph_for(graph, index);
            director::Evidence evidence{};
            evidence.requested_goal = index;
            const director::Decision decision = director::select(
                isolated, evidence, director::Profile::player_guided);
            const integration::TaskCommand command = integration::task_command(
                isolated, evidence, decision, director::Profile::player_guided);
            require(decision.task_index == index && command.selected(),
                "requested eligible graph node did not produce a command");

            sim::Environment environment{
                sim::CreatureBlueprint::humanoid(), 0x751000u + index };
            const integration::TaskApplication applied = integration::apply_task(
                environment, command, isolated, evidence,
                director::Profile::player_guided);
            require(!applied.rejected && applied.challenge_present
                    && applied.stable_challenge_id == node.stable_challenge_id,
                "accepted command did not materialize a challenge");

            const integration::DesiredTaskState desired =
                integration::desired_task_state(node.task, node.challenge);
            const integration::ChallengeResolution repeated =
                integration::resolve_challenge(environment, desired);
            require(repeated.present
                    && repeated.stable_id == applied.stable_challenge_id
                    && repeated.authority == applied.challenge_authority,
                "environment did not reproduce the selected challenge identity");
            if (repeated.authority == integration::ChallengeAuthority::terrain_interval
                || repeated.authority
                    == integration::ChallengeAuthority::terrain_boundary)
            {
                require(environment.terrain().challenge_at(repeated.course_x).stable_id
                        == repeated.stable_id,
                    "terrain-bound command points at a different physical interval");
            }
            else if (repeated.authority
                == integration::ChallengeAuthority::course_feature)
            {
                bool feature_seen{};
                for (const sim::CourseFeature& feature : environment.course_features())
                    feature_seen = feature_seen || feature.kind == desired.feature;
                require(feature_seen,
                    "feature-bound command points at an absent obstacle");
            }
            else if (repeated.authority
                == integration::ChallengeAuthority::equipment_target)
            {
                require(environment.equipment_target().active,
                    "range-bound command points at an inactive target");
            }
        }
    }

    void test_forged_prerequisite_and_profile_commands_are_rejected()
    {
        namespace director = runner::director;
        namespace integration = runner::integration;
        namespace rl = runner::rl;
        namespace sim = runner::sim;

        constexpr director::TaskGraph graph = director::default_training_graph();
        const director::TaskGraph isolated = isolated_graph_for(graph, 15u);
        director::Evidence evidence{};
        evidence.requested_goal = 15u;
        const director::Decision decision = director::select(
            isolated, evidence, director::Profile::autonomous_npc);
        const integration::TaskCommand authentic = integration::task_command(
            isolated, evidence, decision, director::Profile::autonomous_npc);
        require(authentic.selected(), "eligible command factory rejected graph output");

        sim::Environment environment{ sim::CreatureBlueprint::humanoid(), 0x751101u };
        environment.set_course(sim::CourseStage::uneven, 0.30f);
        integration::TaskCommand forged_id = authentic;
        forged_id.stable_id ^= 0x100u;
        integration::TaskCommand forged_task = authentic;
        forged_task.task = director::TaskKind::construction;
        integration::TaskCommand forged_challenge = authentic;
        forged_challenge.challenge = director::ChallengeKind::moving_target;
        require(integration::apply_task(environment, forged_id, isolated, evidence,
                    director::Profile::autonomous_npc).rejected
                && integration::apply_task(environment, forged_task, isolated, evidence,
                    director::Profile::autonomous_npc).rejected
                && integration::apply_task(environment, forged_challenge, isolated,
                    evidence, director::Profile::autonomous_npc).rejected
                && environment.course_stage() == sim::CourseStage::uneven,
            "forged command mutated the environment");

        director::Evidence no_prerequisites{};
        const integration::TaskCommand forged_prerequisite{
            graph.nodes[15].stable_id, 15u, graph.nodes[15].task,
            graph.nodes[15].challenge, director::Profile::autonomous_npc };
        require(integration::apply_task(environment, forged_prerequisite, graph,
                    no_prerequisites, director::Profile::autonomous_npc).rejected,
            "command bypassed graph prerequisites");

        integration::ControlRequest request{};
        request.mode = integration::ControlMode::player_guided;
        request.task_graph = isolated;
        request.evidence = evidence;
        request.selected_task = authentic;
        request.player_intent = integration::player_intent_for(authentic);
        request.joint_control = integration::JointControlSource::retained_policy;
        request.guidance = integration::GuidanceRequest::raw_policy;
        rl::PpoTrainer retained_trainer{
            environment.blueprint(), 8u, true, 1u };
        retained_trainer.reset_policy(0x751102u, true);
        const rl::PpoTrainer::VerifiedCheckpoint retained_checkpoint =
            rl::RawLearningTestAccess::one_real_raw_update(retained_trainer);
        require(retained_checkpoint.valid(),
            "trainer-owned Raw test artifact was not issued");
        integration::RetainedPolicyController retained_controller{};
        std::string retained_error{};
        require(retained_controller.load_retained_best(retained_checkpoint,
                    retained_error),
            "test retained policy fixture was rejected");
        const integration::FixedStepResult mismatch = integration::fixed_step(
            environment, request, retained_controller, 1u, 1u);
        require(mismatch.task_rejected && !mismatch.selected_challenge_present
                && environment.course_stage() == sim::CourseStage::uneven,
            "control mode accepted a command authenticated for another profile");

        director::Evidence walk_evidence{};
        walk_evidence.mastery[0] = graph.nodes[0].minimum_evidence;
        walk_evidence.requested_goal = 1u;
        const director::Decision player_decision = director::select(
            graph, walk_evidence, director::Profile::player_guided);
        request.task_graph = graph;
        request.evidence = walk_evidence;
        request.selected_task = integration::task_command(graph, walk_evidence,
            player_decision, director::Profile::player_guided);
        request.player_intent = integration::player_intent_for(
            request.selected_task);
        const integration::FixedStepResult accepted = integration::fixed_step(
            environment, request, retained_controller, 2u, 1u);
        require(!accepted.task_rejected && accepted.task_applied
                && accepted.selected_challenge_present
                && accepted.selected_challenge_id
                    == graph.nodes[1].stable_challenge_id
                && environment.course_stage() == sim::CourseStage::uneven,
            "authenticated profile command did not reach physical Walk control");
    }

    void test_seed_stable_course_geometry()
    {
        namespace director = runner::director;
        namespace sim = runner::sim;

        static_assert(runner::course_geometry::authored_span_meters == 90.0f);
        static_assert(runner::course_geometry::authored_center_meters == 45.0f);
        static_assert(sim::DeformableTerrain::period
            == runner::course_geometry::authored_span_meters);

        constexpr std::array<std::uint64_t, 6u> seeds{
            1u, 2u, 3u, 0x7500u, 0xffffu, 0x12345678u };
        for (const std::uint64_t seed : seeds)
        {
            sim::DeformableTerrain terrain{};
            terrain.reset(seed, 0.55f);
            const auto wet = terrain.challenge_for(
                director::ChallengeKind::waterlogged_cells);
            const auto shallow = terrain.challenge_for(
                director::ChallengeKind::shallow_water);
            const auto shore = terrain.challenge_for(director::ChallengeKind::shore);
            const auto gap = terrain.challenge_for(director::ChallengeKind::gap);
            const auto step = terrain.challenge_for(director::ChallengeKind::step);
            const auto ramp = terrain.challenge_for(director::ChallengeKind::ramp);
            require(wet && shallow && shore && gap && step && ramp,
                "authored course omitted a required physical challenge");
            require(wet->stable_id
                    == sim::DeformableTerrain::waterlogged_cells_challenge_id
                    && wet->region == sim::TerrainRegion::waterlogged
                    && shallow->stable_id
                        == sim::DeformableTerrain::shallow_water_challenge_id
                    && shallow->region == sim::TerrainRegion::shallow_water,
                "seed swapped waterlogged and shallow semantic IDs");
            require(terrain.challenge_at(shore->center()).stable_id
                    == sim::DeformableTerrain::shore_challenge_id
                    && terrain.challenge_at(gap->center()).stable_id
                        == sim::DeformableTerrain::gap_challenge_id
                    && terrain.challenge_at(step->center()).stable_id
                        == sim::DeformableTerrain::step_challenge_id
                    && terrain.challenge_at(ramp->center()).stable_id
                        == sim::DeformableTerrain::ramp_challenge_id,
                "challenge resolver does not reach the authored terrain interval");

            const float step_height = terrain.height_at(step->center());
            const float ramp_height = terrain.height_at(ramp->center());
            require(step_height > terrain.height_at(step->begin
                        - sim::DeformableTerrain::fine_cell_spacing) + 0.12f
                    && ramp_height > terrain.height_at(ramp->begin
                        - sim::DeformableTerrain::fine_cell_spacing) + 0.24f,
                "step or ramp is metadata without collision geometry");
            terrain.reset(seed, 0.55f);
            require(std::abs(terrain.height_at(step->center()) - step_height)
                        < 1.0e-7f
                    && std::abs(terrain.height_at(ramp->center()) - ramp_height)
                        < 1.0e-7f,
                "repeated seed changed authored challenge geometry");
        }
    }
}

int main()
{
    test_physical_mastery_reducer_is_fail_closed();
    test_every_enabled_node_reaches_physics();
    test_forged_prerequisite_and_profile_commands_are_rejected();
    test_seed_stable_course_geometry();
    std::cout << "Epoch2DWalkEngine v0.7.51 Runner-example Director challenge binding tests passed\n";
    return EXIT_SUCCESS;
}
