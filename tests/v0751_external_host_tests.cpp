#include "ai_director_io.hpp"
#include "integration.hpp"
#include "raw_learning_test_access.hpp"

#if defined(near) || defined(far) || defined(min) || defined(max)
#error "Epoch2DWalkEngine public headers leaked Windows identifier macros"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    runner::director::State valid_director_state(
        runner::director::Profile profile = runner::director::Profile::curriculum)
    {
        runner::director::State state{};
        state.graph = runner::director::default_training_graph();
        state.profile = profile;
        state.current = runner::director::select(state.graph, state.evidence,
            state.profile);
        return state;
    }

    runner::director::PhysicalTrialEvidence persisted_water_proof(
        const runner::director::TaskNode& node) noexcept
    {
        namespace director = runner::director;
        director::PhysicalTrialEvidence proof{};
        proof.schema = director::physical_trial_evidence_schema_version;
        proof.kind = director::PhysicalEvidenceKind::water_crossing;
        proof.trial_id = 0x751501u;
        proof.stable_task_id = node.stable_id;
        proof.stable_challenge_id = node.stable_challenge_id;
        proof.first_tick = 1u;
        proof.last_tick = 60u;
        proof.water = { 2u, 10u, 12u, 25u, 45u, 55u, 0.75f, 0.70f };
        return proof;
    }

    void test_strict_graph_and_evidence_reducer()
    {
        namespace director = runner::director;
        director::State state = valid_director_state();
        require(state.valid(), "canonical Director state was rejected");

        director::TaskGraph duplicate = state.graph;
        duplicate.nodes[1].stable_id = duplicate.nodes[0].stable_id;
        require(!duplicate.valid(), "duplicate stable task IDs were accepted");

        director::TaskGraph inconsistent_challenge = state.graph;
        inconsistent_challenge.nodes[1].stable_challenge_id ^= 0x40u;
        require(!inconsistent_challenge.valid(),
            "one challenge kind accepted two stable physical identities");

        director::TaskGraph aliased_challenge = state.graph;
        aliased_challenge.nodes[2].stable_challenge_id =
            aliased_challenge.nodes[0].stable_challenge_id;
        require(!aliased_challenge.valid(),
            "different challenge kinds shared one stable physical identity");

        director::TaskGraph cycle = state.graph;
        cycle.nodes[0].prerequisites[0] = 1u;
        cycle.nodes[0].prerequisite_count = 1u;
        require(!cycle.valid(), "cyclic task prerequisites were accepted");

        director::TaskGraph nonfinite = state.graph;
        nonfinite.nodes[0].difficulty =
            std::numeric_limits<float>::infinity();
        require(!nonfinite.valid(), "non-finite task difficulty was accepted");

        director::TaskGraph unsupported = state.graph;
        unsupported.nodes[20].enabled = true;
        require(!unsupported.valid(),
            "enabled task without a physical implementation was accepted");

        director::TaskGraph dead_end = state.graph;
        dead_end.nodes[0].enabled = false;
        require(!dead_end.valid(),
            "enabled task depending on a disabled prerequisite was accepted");

        director::Evidence hidden = state.evidence;
        hidden.mastery[state.graph.count] = 0.25f;
        require(!director::evidence_valid(state.graph, hidden),
            "out-of-graph evidence retained hidden task authority");

        director::State inconsistent = state;
        inconsistent.current.stable_id ^= 0x10u;
        require(!inconsistent.valid(),
            "Director state accepted a forged current selection");

        const director::TaskNode& first = state.graph.nodes[0];
        const director::TrialEvidence progress{ first.stable_id, 0u,
            director::TrialOutcome::progress, first.minimum_evidence * 0.5f };
        const director::EvidenceReduction progress_a = director::reduce_evidence(
            state.graph, state.evidence, progress, state.profile);
        const director::EvidenceReduction progress_b = director::reduce_evidence(
            state.graph, state.evidence, progress, state.profile);
        require(progress_a.accepted && progress_b.accepted
                && progress_a.evidence.mastery == progress_b.evidence.mastery
                && progress_a.next.task_index == progress_b.next.task_index
                && progress_a.evidence.mastery[0] == progress.measured_mastery,
            "pure evidence reduction was not deterministic");

        const director::TrialEvidence stall{ first.stable_id, 0u,
            director::TrialOutcome::stalled, progress.measured_mastery };
        const director::EvidenceReduction stalled = director::reduce_evidence(
            state.graph, progress_a.evidence, stall, state.profile);
        require(stalled.accepted && stalled.evidence.failures[0] == 1u
                && stalled.evidence.first_stall == 0u
                && stalled.next.reason == director::SelectionReason::unresolved_stall,
            "stalled trial evidence was not reduced truthfully");

        director::Evidence requested = stalled.evidence;
        requested.requested_goal = 0u;
        const director::TrialEvidence complete{ first.stable_id, 0u,
            director::TrialOutcome::completed, first.minimum_evidence };
        const director::EvidenceReduction completed = director::reduce_evidence(
            state.graph, requested, complete, state.profile);
        require(completed.accepted
                && completed.evidence.mastery[0] >= first.minimum_evidence
                && completed.evidence.first_stall == director::invalid_task_index
                && completed.evidence.requested_goal == director::invalid_task_index,
            "completed evidence did not clear its stall/request authority");

        director::TrialEvidence forged = complete;
        forged.stable_task_id ^= 1u;
        const director::EvidenceReduction rejected = director::reduce_evidence(
            state.graph, state.evidence, forged, state.profile);
        require(!rejected.accepted
                && rejected.evidence.mastery == state.evidence.mastery,
            "forged trial identity mutated Director evidence");

        director::TrialEvidence failed_as_complete = complete;
        failed_as_complete.outcome = director::TrialOutcome::failed;
        const director::EvidenceReduction failed_rejected =
            director::reduce_evidence(state.graph, state.evidence,
                failed_as_complete, state.profile);
        require(!failed_rejected.accepted
                && failed_rejected.evidence.mastery == state.evidence.mastery,
            "failed trial was allowed to grant completion evidence");
    }

    void test_atomic_director_state()
    {
        namespace director = runner::director;
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / "epoch2dwalk-v0751-external-host";
        const std::filesystem::path path = directory / "director.state";
        const std::filesystem::path replacement = directory / "replacement.tmp";
        std::error_code cleanup_error{};
        std::filesystem::remove_all(directory, cleanup_error);
        std::filesystem::create_directories(directory);

        director::State state = valid_director_state();
        std::string error{};
        require(director::save_state(path, state, error),
            "valid Director state was not saved");
        director::State loaded{};
        require(director::load_state(path, loaded, error) && loaded.valid(),
            "atomically saved Director state did not reload");

        const director::TaskNode& first = state.graph.nodes[0];
        const director::EvidenceReduction reduction = director::reduce_evidence(
            state.graph, state.evidence,
            { first.stable_id, 0u, director::TrialOutcome::completed,
                first.minimum_evidence },
            state.profile);
        require(reduction.accepted, "replacement Director evidence was rejected");
        state.evidence = reduction.evidence;
        state.current = reduction.next;
        require(director::save_state(path, state, error),
            "existing Director state was not atomically replaced");
        loaded = {};
        require(director::load_state(path, loaded, error)
                && loaded.valid()
                && loaded.evidence.mastery[0] == first.minimum_evidence
                && loaded.current.task_index == state.current.task_index,
            "atomic Director replacement retained stale accepted state");

        std::ifstream before_stream(path, std::ios::binary);
        const std::string before{ std::istreambuf_iterator<char>{ before_stream },
            std::istreambuf_iterator<char>{} };
        require(!runner::persistence::atomic_replace_file(replacement, path, error),
            "missing temporary file falsely replaced accepted state");
        std::ifstream after_stream(path, std::ios::binary);
        const std::string after{ std::istreambuf_iterator<char>{ after_stream },
            std::istreambuf_iterator<char>{} };
        require(!before.empty() && after == before,
            "failed atomic replacement damaged accepted state");
        before_stream.close();
        after_stream.close();

        class CommaDecimal final : public std::numpunct<char>
        {
        protected:
            [[nodiscard]] char do_decimal_point() const override { return ','; }
        };
        const std::locale original_locale{};
        const std::locale comma_locale{ original_locale, new CommaDecimal{} };
        std::locale::global(comma_locale);
        const bool locale_save = director::save_state(path, state, error);
        std::locale::global(original_locale);
        require(locale_save,
            "locale-invariant Director state could not be saved");
        std::locale::global(comma_locale);
        loaded = {};
        const bool locale_load = director::load_state(path, loaded, error);
        std::locale::global(original_locale);
        require(locale_load && loaded.valid()
                && loaded.evidence.mastery[0] == first.minimum_evidence,
            "Director state grammar depended on the external host locale");

        director::State physical_state = valid_director_state();
        for (std::size_t index = 0u; index < 3u; ++index)
            physical_state.evidence.mastery[index] =
                physical_state.graph.nodes[index].minimum_evidence;
        const director::TaskNode& water = physical_state.graph.nodes[3];
        const director::TrialEvidence water_trial{ water.stable_id, 3u,
            director::TrialOutcome::completed, water.minimum_evidence,
            persisted_water_proof(water) };
        const director::EvidenceReduction water_reduction =
            director::reduce_evidence(physical_state.graph,
                physical_state.evidence, water_trial, physical_state.profile);
        require(water_reduction.accepted,
            "physical persistence fixture did not pass the reducer");
        physical_state.evidence = water_reduction.evidence;
        physical_state.current = water_reduction.next;
        require(physical_state.valid()
                && director::save_state(path, physical_state, error),
            "physical Director receipt was not serializable");
        loaded = {};
        require(director::load_state(path, loaded, error)
                && loaded.valid()
                && loaded.evidence.physical_mastery[3]
                    == physical_state.evidence.physical_mastery[3],
            "physical Director receipt did not round-trip exactly");

        std::ifstream physical_stream(path, std::ios::binary);
        const std::string physical_bytes{
            std::istreambuf_iterator<char>{ physical_stream },
            std::istreambuf_iterator<char>{} };
        physical_stream.close();
        require(director::save_state(path, physical_state, error),
            "repeated physical Director save failed");
        std::ifstream repeated_stream(path, std::ios::binary);
        const std::string repeated_bytes{
            std::istreambuf_iterator<char>{ repeated_stream },
            std::istreambuf_iterator<char>{} };
        require(!physical_bytes.empty() && repeated_bytes == physical_bytes,
            "same physical Director state serialized differently on repeat");
        repeated_stream.close();

        director::State forged_physical = physical_state;
        forged_physical.evidence.physical_mastery[3].stable_challenge_id ^= 1u;
        require(!forged_physical.valid()
                && !director::save_state(path, forged_physical, error),
            "wrong-challenge physical receipt was serialized");

        {
            std::ofstream trailing_output(path,
                std::ios::binary | std::ios::app);
            trailing_output << "FORGED TRAILING RECORD\n";
            require(static_cast<bool>(trailing_output),
                "could not author trailing-record Director adversary");
        }
        loaded = physical_state;
        require(!director::load_state(path, loaded, error)
                && loaded.evidence.physical_mastery[3]
                    == physical_state.evidence.physical_mastery[3],
            "trailing Director data was accepted or mutated destination state");

        const std::filesystem::path legacy = directory / "director-v1.state";
        {
            std::ofstream legacy_output(legacy, std::ios::binary | std::ios::trunc);
            legacy_output << "EPOCH2DDIRECTOR 1\n";
        }
        loaded = physical_state;
        require(!director::load_state(legacy, loaded, error)
                && loaded.evidence.physical_mastery[3]
                    == physical_state.evidence.physical_mastery[3],
            "legacy scalar-only Director schema migrated or mutated state");
        std::filesystem::remove_all(directory, cleanup_error);
    }

    runner::integration::RetainedPolicyController assisted_test_controller_for(
        const runner::sim::Environment& environment, std::uint64_t seed)
    {
        runner::rl::PpoTrainer trainer{
            environment.blueprint(), 8u, true, 1u };
        trainer.reset_policy(seed, true);
        require(trainer.rig_signature() == environment.blueprint().signature(),
            "trainer checkpoint did not sign the canonical runtime rig");
        const runner::rl::PpoTrainer::VerifiedCheckpoint checkpoint =
            runner::rl::RawLearningTestAccess::one_real_assisted_update(trainer);
        require(checkpoint.valid(), "trainer did not issue an Assisted test artifact");
        runner::integration::RetainedPolicyController controller{};
        std::string error{};
        require(controller.load_retained_best(checkpoint, error),
            "valid Assisted test artifact was rejected");
        return controller;
    }

    void test_one_update_raw_non_retention()
    {
        namespace integration = runner::integration;
        namespace rl = runner::rl;
        namespace sim = runner::sim;

        const auto run = []()
        {
            rl::PpoTrainer trainer{ sim::CreatureBlueprint::humanoid(),
                8u, true, 1u };
            trainer.reset_policy(0x751E02u, true);
            return rl::RawLearningTestAccess::one_unretained_raw_update(trainer);
        };

        const rl::OneUpdateRawNonRetentionEvidence first = run();
        const rl::OneUpdateRawNonRetentionEvidence repeated = run();
        const rl::RawPpoReceipt& receipt = first.raw_ppo_receipt;
        require(first == repeated,
            "same-seed one-update Raw non-retention evidence changed");
        require(receipt.present()
                && receipt.origin_counts()[static_cast<std::size_t>(
                    rl::RolloutOrigin::raw_policy)]
                    == receipt.expected_sample_count()
                && receipt.expected_sample_count() == 1024u
                && receipt.forward_intent_samples() == 384u
                && receipt.reverse_intent_samples() == 384u
                && receipt.stop_intent_samples() == 256u
                && receipt.self_imitation_updates() == 0u
                && receipt.best_anchor_updates() == 0u
                && receipt.environments_reset_at_handoff()
                && receipt.actions_reset_at_handoff()
                && receipt.episodes_reset_at_handoff()
                && receipt.whole_cohort_raw(),
            "one-update fixture did not cross the genuine Raw producer path");
        require(first.optimizer_step_after > first.optimizer_step_before
                && first.parameters_after_digest
                    != first.parameters_before_digest
                && receipt.optimizer_step_before()
                    == first.optimizer_step_before
                && receipt.optimizer_step() == first.optimizer_step_after
                && receipt.parameters_before_digest()
                    == first.parameters_before_digest
                && receipt.parameters_after_digest()
                    == first.parameters_after_digest
                && receipt.evaluated_parameters_digest()
                    == first.parameters_after_digest,
            "one-update Raw optimizer/provenance evidence was not authentic");
        require(first.evaluation_count == 1u
                && first.evaluation_invalid_runs > 0u
                && !rl::strict_evaluation_quality(
                    first.evaluation_quality_key)
                && !rl::policy_candidate_retainable(sim::CourseStage::uneven,
                    first.evaluation_quality_key)
                && first.retained_parameter_count == 0u
                && first.retained_update == 0u
                && !first.production_checkpoint.valid(),
            "one random Raw update was mislabeled as production retention");

        integration::RetainedPolicyController controller{};
        std::string error{};
        require(!controller.load_retained_best(
                    first.production_checkpoint, error)
                && !controller.provenance().valid
                && error
                    == "Retained policy requires an issued verified checkpoint artifact.",
            "external controller accepted an explicitly unretained Raw update");
    }

    void test_topology_masked_policy_actions()
    {
        namespace integration = runner::integration;
        namespace rl = runner::rl;
        namespace sim = runner::sim;

        static_assert(sim::anatomy_action_count == 12u);
        static_assert(sim::equipment_action_count == 3u);
        static_assert(sim::action_count == 15u);
        static_assert(sim::observation_count == 84u);
        static_assert(integration::api_version == 7u);
        static_assert(integration::action_layout_schema_version == 2u);
        static_assert(integration::canonical_snapshot_schema_version == 5u);

        sim::Environment human{ sim::CreatureBlueprint::humanoid(), 0x751A00u };
        sim::Environment dog{ sim::CreatureBlueprint::crawler4(), 0x751A01u };
        sim::Environment chicken{ sim::CreatureBlueprint::chicken(), 0x751A02u };
        sim::Environment hex{ sim::CreatureBlueprint::hexapod(), 0x751A03u };
        const rl::ActionMask human_mask = rl::action_mask_for(human,
            sim::CourseStage::uneven);
        const rl::ActionMask dog_mask = rl::action_mask_for(dog,
            sim::CourseStage::uneven);
        const rl::ActionMask chicken_mask = rl::action_mask_for(chicken,
            sim::CourseStage::uneven);
        const rl::ActionMask hex_mask = rl::action_mask_for(hex,
            sim::CourseStage::uneven);
        require(human_mask[sim::human_left_ankle_action]
                && human_mask[sim::human_right_ankle_action]
                && !dog_mask[sim::human_left_ankle_action]
                && !dog_mask[sim::human_right_ankle_action]
                && !chicken_mask[sim::human_left_ankle_action]
                && !chicken_mask[sim::human_right_ankle_action]
                && !hex_mask[sim::human_left_ankle_action]
                && !hex_mask[sim::human_right_ankle_action],
            "species topology did not isolate the Human ankle outputs");
        const rl::ActionMask human_equipment = rl::action_mask_for(human,
            sim::CourseStage::equipment_targets);
        for (std::size_t index = sim::anatomy_action_count;
            index < sim::action_count; ++index)
        {
            require(human_equipment[index] && !human_mask[index]
                    && !dog_mask[index] && !chicken_mask[index]
                    && !hex_mask[index],
                "equipment action gating escaped its capable staged topology");
        }

        rl::ActionMask sparse{};
        sparse[0] = true;
        sparse[2] = true;
        sparse[sim::equipment_state_action] = true;
        rl::ActionMask compact{};
        compact[0] = true;
        compact[1] = true;
        compact[2] = true;
        rl::PolicyNetwork sparse_policy{ 0x751A10u };
        rl::PolicyNetwork compact_policy{ 0x751A10u };
        std::fill(sparse_policy.parameters().begin(),
            sparse_policy.parameters().end(), 0.0f);
        compact_policy.parameters() = sparse_policy.parameters();
        sparse_policy.set_action_mask(sparse);
        compact_policy.set_action_mask(compact);
        std::array<float, sim::observation_count> observation{};
        const auto sparse_evaluation = sparse_policy.evaluate(observation);
        const auto compact_evaluation = compact_policy.evaluate(observation);
        std::uint64_t sparse_rng = 0x751A11u;
        std::uint64_t compact_rng = sparse_rng;
        float sparse_log_probability{};
        float compact_log_probability{};
        const auto sparse_sample = sparse_policy.sample_action(
            sparse_evaluation, sparse_rng, sparse_log_probability);
        const auto compact_sample = compact_policy.sample_action(
            compact_evaluation, compact_rng, compact_log_probability);
        require(sparse_rng == compact_rng
                && sparse_sample[0] == compact_sample[0]
                && sparse_sample[2] == compact_sample[1]
                && sparse_sample[sim::equipment_state_action]
                    == compact_sample[2],
            "inactive outputs consumed stochastic-policy RNG");
        for (std::size_t index = 0; index < sparse.size(); ++index)
            require(sparse[index] || sparse_sample[index] == 0.0f,
                "stochastic policy emitted an inactive action");
        const auto sparse_deterministic = sparse_policy.deterministic_action(
            observation);
        for (std::size_t index = 0; index < sparse.size(); ++index)
            require(sparse[index] || sparse_deterministic[index] == 0.0f,
                "deterministic policy emitted an inactive action");

        std::array<float, sim::action_count> clean_action{};
        clean_action[0] = 0.1f;
        clean_action[2] = -0.2f;
        clean_action[sim::equipment_state_action] = 0.3f;
        std::array<float, sim::action_count> poisoned_action = clean_action;
        for (std::size_t index = 0; index < sparse.size(); ++index)
            if (!sparse[index])
                poisoned_action[index] = 1000.0f + static_cast<float>(index);
        require(sparse_policy.log_probability(clean_action, sparse_evaluation)
                == sparse_policy.log_probability(poisoned_action,
                    sparse_evaluation),
            "inactive actions changed policy likelihood");

        rl::PolicyNetwork first_gradient{ 0x751A12u };
        rl::PolicyNetwork second_gradient{ 0x751A12u };
        first_gradient.set_action_mask(sparse);
        second_gradient.set_action_mask(sparse);
        const auto first_evaluation = first_gradient.evaluate(observation);
        const auto second_evaluation = second_gradient.evaluate(observation);
        const float old_log_probability = first_gradient.log_probability(
            clean_action, first_evaluation);
        float first_policy_loss{};
        float first_value_loss{};
        float first_entropy{};
        float second_policy_loss{};
        float second_value_loss{};
        float second_entropy{};
        first_gradient.zero_gradients();
        second_gradient.zero_gradients();
        first_gradient.accumulate_gradient(observation, clean_action,
            old_log_probability, 0.5f, first_evaluation.value,
            0.2f, 0.5f, 0.01f, first_policy_loss, first_value_loss,
            first_entropy);
        second_gradient.accumulate_gradient(observation, poisoned_action,
            old_log_probability, 0.5f, second_evaluation.value,
            0.2f, 0.5f, 0.01f, second_policy_loss, second_value_loss,
            second_entropy);
        require(first_gradient.gradients() == second_gradient.gradients()
                && first_policy_loss == second_policy_loss
                && first_value_loss == second_value_loss
                && first_entropy == second_entropy,
            "inactive outputs affected PPO loss, entropy, or gradients");

        rl::PolicyNetwork first_imitation{ 0x751A13u };
        rl::PolicyNetwork second_imitation{ 0x751A13u };
        first_imitation.set_action_mask(sparse);
        second_imitation.set_action_mask(sparse);
        float first_imitation_loss{};
        float second_imitation_loss{};
        first_imitation.zero_gradients();
        second_imitation.zero_gradients();
        first_imitation.accumulate_imitation_gradient(observation, clean_action,
            0.5f, first_imitation_loss);
        second_imitation.accumulate_imitation_gradient(observation,
            poisoned_action, 0.5f, second_imitation_loss);
        require(first_imitation.gradients() == second_imitation.gradients()
                && first_imitation_loss == second_imitation_loss,
            "inactive outputs affected imitation loss or gradients");

        std::array<float, sim::action_count> poisoned_policy{};
        poisoned_policy.fill(0.75f);
        const auto dog_effective = rl::effective_policy_action(dog,
            poisoned_policy, sim::CourseStage::uneven, 0.0f,
            sim::GuidanceMode::raw_policy_audit);
        const auto human_effective = rl::effective_policy_action(human,
            poisoned_policy, sim::CourseStage::uneven, 0.0f,
            sim::GuidanceMode::raw_policy_audit);
        require(dog_effective[sim::human_left_ankle_action] == 0.0f
                && dog_effective[sim::human_right_ankle_action] == 0.0f
                && human_effective[sim::human_left_ankle_action] == 0.75f
                && human_effective[sim::human_right_ankle_action] == 0.75f
                && human_effective[sim::equipment_state_action] == 0.0f,
            "effective actions did not preserve the topology/task mask");
    }

    void test_checkpoint_action_mask_transitions()
    {
        namespace rl = runner::rl;
        namespace sim = runner::sim;

        const auto run = []()
        {
            rl::PpoTrainer trainer{ sim::CreatureBlueprint::humanoid(),
                8u, false };
            rl::PpoTrainer::CheckpointData equipment =
                trainer.checkpoint_data();
            equipment.stage = sim::CourseStage::equipment_targets;
            equipment.difficulty = 0.45f;
            std::fill(equipment.first_moment.begin(),
                equipment.first_moment.end(), 1.0f);
            std::fill(equipment.second_moment.begin(),
                equipment.second_moment.end(), 2.0f);
            equipment = rl::RawLearningTestAccess::reseal(
                std::move(equipment));
            std::string error{};
            require(trainer.apply_checkpoint_data(equipment, error),
                "equipment-stage checkpoint restore was rejected");
            const rl::ActionMask equipment_mask = rl::action_mask_for(
                trainer.preview(), sim::CourseStage::equipment_targets);
            const rl::PpoTrainer::CheckpointData equipment_restored =
                trainer.checkpoint_data();
            require(trainer.policy().action_mask() == equipment_mask
                    && trainer.preview_action_mask() == equipment_mask,
                "checkpoint restore left retained/preview masks on the old stage");
            require(std::ranges::all_of(equipment_restored.first_moment,
                        [](float value) noexcept { return value == 1.0f; })
                    && std::ranges::all_of(equipment_restored.second_moment,
                        [](float value) noexcept { return value == 2.0f; }),
                "inactive-to-active checkpoint restore cleared optimizer state");

            rl::PpoTrainer::CheckpointData balance = equipment_restored;
            balance.stage = sim::CourseStage::balance;
            balance.difficulty = 0.25f;
            std::fill(balance.first_moment.begin(), balance.first_moment.end(),
                3.0f);
            std::fill(balance.second_moment.begin(), balance.second_moment.end(),
                4.0f);
            balance = rl::RawLearningTestAccess::reseal(std::move(balance));
            require(trainer.apply_checkpoint_data(std::move(balance), error),
                "balance-stage checkpoint restore was rejected");
            const rl::ActionMask balance_mask = rl::action_mask_for(
                trainer.preview(), sim::CourseStage::balance);
            const rl::PpoTrainer::CheckpointData balance_restored =
                trainer.checkpoint_data();
            require(trainer.policy().action_mask() == balance_mask
                    && trainer.preview_action_mask() == balance_mask,
                "checkpoint restore did not deactivate staged equipment outputs");
            constexpr std::size_t state_values_per_action =
                rl::PolicyNetwork::hidden_size + 2u;
            constexpr std::size_t expected_cleared =
                sim::equipment_action_count * state_values_per_action;
            require(static_cast<std::size_t>(std::count(
                        balance_restored.first_moment.begin(),
                        balance_restored.first_moment.end(), 0.0f))
                        == expected_cleared
                    && static_cast<std::size_t>(std::count(
                        balance_restored.second_moment.begin(),
                        balance_restored.second_moment.end(), 0.0f))
                        == expected_cleared
                    && static_cast<std::size_t>(std::count(
                        balance_restored.first_moment.begin(),
                        balance_restored.first_moment.end(), 3.0f))
                        + expected_cleared
                        == balance_restored.first_moment.size()
                    && static_cast<std::size_t>(std::count(
                        balance_restored.second_moment.begin(),
                        balance_restored.second_moment.end(), 4.0f))
                        + expected_cleared
                        == balance_restored.second_moment.size(),
                "active-to-inactive transition did not clear exactly the disabled heads");

            const std::vector<float> first_before_activation =
                balance_restored.first_moment;
            const std::vector<float> second_before_activation =
                balance_restored.second_moment;
            trainer.set_course(sim::CourseStage::equipment_targets, 0.45f,
                true);
            const rl::PpoTrainer::CheckpointData reactivated =
                trainer.checkpoint_data();
            require(reactivated.first_moment == first_before_activation
                    && reactivated.second_moment == second_before_activation,
                "inactive-to-active transition disturbed retained optimizer state");
            return balance_restored;
        };

        const rl::PpoTrainer::CheckpointData first = run();
        const rl::PpoTrainer::CheckpointData second = run();
        require(first.first_moment == second.first_moment
                && first.second_moment == second.second_moment,
            "repeated checkpoint mask transitions were not deterministic");
    }

    bool same_physical_pose(const runner::sim::Environment& environment,
        const std::vector<runner::sim::Particle>& expected) noexcept
    {
        const auto& actual = environment.particles();
        if (actual.size() != expected.size())
            return false;
        for (std::size_t index = 0; index < actual.size(); ++index)
        {
            if (actual[index].position.x != expected[index].position.x
                || actual[index].position.y != expected[index].position.y
                || actual[index].previous.x != expected[index].previous.x
                || actual[index].previous.y != expected[index].previous.y)
                return false;
        }
        return true;
    }

    void test_locomotion_intent_has_no_root_authority()
    {
        namespace sim = runner::sim;
        sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
            0x751E00u };
        const std::vector<sim::Particle> initial_pose = environment.particles();
        const float initial_elapsed = environment.elapsed_seconds();
        const float initial_progress = environment.course_progress();

        require(environment.set_locomotion_intent(-1.0f, false)
                && environment.locomotion_intent_direction() == -1.0f
                && environment.locomotion_direction() == -1.0f
                && same_physical_pose(environment, initial_pose)
                && environment.elapsed_seconds() == initial_elapsed
                && environment.course_progress() == initial_progress,
            "signed intent directly moved or forced the physical rig");
        require(environment.set_locomotion_intent(0.0f, true)
                && environment.locomotion_stop_requested()
                && environment.locomotion_direction() == 0.0f
                && same_physical_pose(environment, initial_pose)
                && environment.elapsed_seconds() == initial_elapsed
                && environment.course_progress() == initial_progress,
            "stop intent directly moved or forced the physical rig");

        const float accepted_direction = environment.locomotion_intent_direction();
        require(!environment.set_locomotion_intent(
                    std::numeric_limits<float>::quiet_NaN(), false)
                && environment.locomotion_intent_direction()
                    == accepted_direction
                && environment.locomotion_stop_requested()
                && same_physical_pose(environment, initial_pose),
            "rejected locomotion intent mutated accepted physical state");
    }

    void test_canonical_authority_snapshot()
    {
        namespace integration = runner::integration;
        namespace sim = runner::sim;

        const auto run = []()
        {
            sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
                0x751E10u };
            environment.set_raw_authority_telemetry_enabled(true);
            integration::ControlRequest request{};
            request.mode = integration::ControlMode::training;
            request.joint_control =
                integration::JointControlSource::expert_joint_actions;
            request.guidance = integration::GuidanceRequest::raw_policy;
            const runner::rl::ActionMask action_mask =
                runner::rl::action_mask_for(environment,
                    environment.course_stage());
            for (std::size_t index = 0; index < request.actions.size(); ++index)
                if (action_mask[index])
                    request.actions[index] = index % 2u == 0u
                        ? 0.35f : -0.20f;
            integration::FixedStepResult step = integration::fixed_step(
                environment, request, 0x751u, 7u);
            return std::pair{ std::move(environment), std::move(step) };
        };

        const auto [first_environment, first_step] = run();
        const auto [second_environment, second_step] = run();
        const integration::CanonicalSnapshot& first = first_step.snapshot;
        const integration::CanonicalSnapshot& second = second_step.snapshot;
        require(first_step.control_applied && second_step.control_applied
                && first.schema == integration::canonical_snapshot_schema_version
                && integration::canonical_snapshot_valid(first)
                && integration::canonical_snapshot_valid(second),
            "schema-5 physical authority snapshots were not self-validating");
        require(integration::valid_action_layout(first.action_layout)
                && first.action_layout.observation_count == 84u
                && first.action_layout.action_count == 15u
                && first.action_layout.anatomy_action_count == 12u
                && first.action_layout.equipment_action_count == 3u
                && first.action_layout.roles[sim::human_left_ankle_action]
                    == integration::ActionRole::anatomy_motor
                && first.action_layout.roles[sim::equipment_state_action]
                    == integration::ActionRole::equipment_state
                && first.action_layout.roles[sim::equipment_aim_action]
                    == integration::ActionRole::equipment_aim
                && first.action_layout.roles[sim::equipment_trigger_action]
                    == integration::ActionRole::equipment_trigger
                && first.action_layout.active[sim::human_left_ankle_action]
                && first.action_layout.active[sim::human_right_ankle_action]
                && !first.action_layout.active[sim::equipment_state_action],
            "snapshot omitted or mislabeled the canonical action layout");
        require(first.root_node_index
                    == first_environment.blueprint().root_node
                && first.torso_node_index
                    == first_environment.blueprint().torso_node
                && first.left_contact_node_index
                    == first_environment.blueprint().left_contact_node
                && first.right_contact_node_index
                    == first_environment.blueprint().right_contact_node
                && first.root_pose.position.x == first.root_position.x
                && first.root_pose.position.y == first.root_position.y
                && first.root_pose.velocity.x == first.root_velocity.x
                && first.root_pose.velocity.y == first.root_velocity.y
                && first.left_contact_pose.grounded
                    == first.pose[first.left_contact_node_index].grounded
                && first.right_contact_pose.grounded
                    == first.pose[first.right_contact_node_index].grounded
                && first.left_contact_phase
                    == first_environment.left_foot_phase()
                && first.right_contact_phase
                    == first_environment.right_foot_phase()
                && first.bone_count
                    == first_environment.blueprint().bones.size()
                && first.pose_count == 19u && first.motor_count == 12u,
            "snapshot omitted or misidentified a semantic physical pose");
        require(first.total_particle_mass > 0.0f
                && std::isfinite(first.center_of_mass_position.x)
                && std::isfinite(first.center_of_mass_position.y)
                && std::isfinite(first.center_of_mass_velocity.x)
                && std::isfinite(first.center_of_mass_velocity.y),
            "snapshot did not expose a finite mass-weighted center of mass");
        require(first.raw_authority_telemetry_enabled
                && first.raw_authority.root_drive_corrections == 0u
                && first.raw_authority.torso_drive_corrections == 0u
                && first.raw_authority.course_motion_corrections == 0u,
            "raw snapshot recorded hidden root, torso, or course authority");
        for (std::size_t index = 0; index < first.motor_count; ++index)
        {
            const sim::MotorDiagnostic diagnostic =
                first_environment.motor_diagnostic(index);
            require(first.raw_authority.commanded_actions[index]
                        == std::clamp(first.requested_actions[index], -1.0f, 1.0f)
                    && first.raw_authority.actuator_actions[index]
                        == first.applied_motor_actions[index]
                    && first.applied_motor_actions[index]
                        == first.motors[index].applied_action
                    && first.motors[index].action_slot == index
                    && first.motors[index].reference_node == diagnostic.parent
                    && first.motors[index].pivot_node == diagnostic.pivot
                    && first.motors[index].driven_node == diagnostic.driven
                    && first.action_layout.active[index]
                        == first.motors[index].enabled,
                "raw requested/actuator/motor action provenance diverged");
        }
        require(first.motors[sim::human_left_ankle_action].reference_node == 3u
                && first.motors[sim::human_left_ankle_action].pivot_node
                    == sim::human_left_ankle_node
                && first.motors[sim::human_left_ankle_action].driven_node
                    == sim::human_left_ball_node
                && first.motors[sim::human_right_ankle_action].reference_node == 5u
                && first.motors[sim::human_right_ankle_action].pivot_node
                    == sim::human_right_ankle_node
                && first.motors[sim::human_right_ankle_action].driven_node
                    == sim::human_right_ball_node,
            "external motor layout did not expose the canonical ankle chains");

        require(first.pose_count == second.pose_count
                && first.motor_count == second.motor_count
                && first_environment.blueprint().signature()
                    == second_environment.blueprint().signature()
                && first.center_of_mass_position.x
                    == second.center_of_mass_position.x
                && first.center_of_mass_position.y
                    == second.center_of_mass_position.y
                && first.center_of_mass_velocity.x
                    == second.center_of_mass_velocity.x
                && first.center_of_mass_velocity.y
                    == second.center_of_mass_velocity.y
                && first.raw_authority.total_center_velocity_delta_x
                    == second.raw_authority.total_center_velocity_delta_x
                && first.raw_authority.total_center_velocity_delta_y
                    == second.raw_authority.total_center_velocity_delta_y,
            "same-seed authority snapshots diverged");
        for (std::size_t index = 0; index < first.pose_count; ++index)
        {
            require(integration::same_pose_particle(first.pose[index],
                    second.pose[index]),
                "same-seed particle snapshots diverged");
        }
        for (std::size_t index = 0; index < first.raw_authority.sources.size();
            ++index)
        {
            const sim::AuthoritySourceTelemetry& a =
                first.raw_authority.sources[index];
            const sim::AuthoritySourceTelemetry& b =
                second.raw_authority.sources[index];
            require(a.center_velocity_delta_x == b.center_velocity_delta_x
                    && a.center_velocity_delta_y == b.center_velocity_delta_y
                    && a.maximum_absolute_center_velocity_delta_x
                        == b.maximum_absolute_center_velocity_delta_x
                    && a.maximum_absolute_center_velocity_delta_y
                        == b.maximum_absolute_center_velocity_delta_y
                    && a.positive_horizontal_specific_energy
                        == b.positive_horizontal_specific_energy
                    && a.center_position_delta_x == b.center_position_delta_x
                    && a.center_position_delta_y == b.center_position_delta_y
                    && a.center_history_delta_x == b.center_history_delta_x
                    && a.center_history_delta_y == b.center_history_delta_y
                    && a.root_position_delta_x == b.root_position_delta_x
                    && a.root_position_delta_y == b.root_position_delta_y
                    && a.torso_position_delta_x == b.torso_position_delta_x
                    && a.torso_position_delta_y == b.torso_position_delta_y
                    && a.applications == b.applications,
                "same-seed per-source authority ledger diverged");
        }

        integration::CanonicalSnapshot corrupt = first;
        corrupt.schema = 1u;
        require(!integration::canonical_snapshot_valid(corrupt),
            "stale canonical snapshot schema was accepted");
        corrupt = first;
        corrupt.action_layout.schema = 0u;
        require(!integration::canonical_snapshot_valid(corrupt),
            "stale action-layout schema was accepted");
        corrupt = first;
        corrupt.action_layout.roles[sim::equipment_state_action] =
            integration::ActionRole::equipment_aim;
        require(!integration::canonical_snapshot_valid(corrupt),
            "forged action role was accepted");
        corrupt = first;
        corrupt.action_layout.active[sim::human_left_ankle_action] = false;
        require(!integration::canonical_snapshot_valid(corrupt),
            "snapshot hid a canonical powered ankle output");
        corrupt = first;
        corrupt.requested_actions[sim::equipment_state_action] = 0.25f;
        require(!integration::canonical_snapshot_valid(corrupt),
            "snapshot retained a command in an inactive action slot");
        corrupt = first;
        corrupt.requested_actions[0] = 1.01f;
        require(!integration::canonical_snapshot_valid(corrupt),
            "snapshot accepted an out-of-range requested joint action");
        corrupt = first;
        corrupt.applied_motor_actions[0] = -1.01f;
        require(!integration::canonical_snapshot_valid(corrupt),
            "snapshot accepted an out-of-range applied joint action");
        corrupt = first;
        corrupt.applied_motor_actions[0] = std::clamp(
            corrupt.applied_motor_actions[0] + 0.125f, -1.0f, 1.0f);
        require(!integration::canonical_snapshot_valid(corrupt),
            "published applied action diverged from its motor snapshot");
        corrupt = first;
        corrupt.motors[0].applied_action = 1.01f;
        corrupt.applied_motor_actions[0] = 1.01f;
        require(!integration::canonical_snapshot_valid(corrupt),
            "motor snapshot accepted an out-of-range applied action");
        corrupt = first;
        corrupt.motors[sim::human_left_ankle_action].pivot_node =
            static_cast<std::uint16_t>(first.pose_count);
        require(!integration::canonical_snapshot_valid(corrupt),
            "snapshot accepted an out-of-range motor node identity");
        corrupt = first;
        corrupt.left_contact_node_index = integration::invalid_snapshot_node_index;
        require(!integration::canonical_snapshot_valid(corrupt),
            "malformed canonical contact index was accepted");
        corrupt = first;
        corrupt.center_of_mass_position.x =
            std::numeric_limits<float>::quiet_NaN();
        require(!integration::canonical_snapshot_valid(corrupt),
            "non-finite canonical center of mass was accepted");
        corrupt = first;
        require(corrupt.pose_count < corrupt.pose.size(),
            "authority snapshot left no hidden-tail test capacity");
        corrupt.pose[corrupt.pose_count].radius = 1.0f;
        require(!integration::canonical_snapshot_valid(corrupt),
            "hidden canonical pose tail data was accepted");
        corrupt = first;
        corrupt.raw_authority.sources[0].root_position_delta_x =
            std::numeric_limits<float>::quiet_NaN();
        require(!integration::canonical_snapshot_valid(corrupt),
            "non-finite raw authority delta was accepted");
        corrupt = first;
        corrupt.raw_authority.sources[0]
            .maximum_absolute_center_velocity_delta_x = -0.01f;
        require(!integration::canonical_snapshot_valid(corrupt),
            "negative maximum source authority delta was accepted");
        corrupt = first;
        corrupt.raw_authority.sources[0]
            .positive_horizontal_specific_energy =
                std::numeric_limits<float>::infinity();
        require(!integration::canonical_snapshot_valid(corrupt),
            "non-finite source authority energy was accepted");
        corrupt = first;
        corrupt.raw_authority.maximum_bone_error_ratio = 0.1f;
        corrupt.raw_authority.maximum_bone_error_index = corrupt.bone_count;
        require(!integration::canonical_snapshot_valid(corrupt),
            "out-of-range authority bone evidence was accepted");
        corrupt = first;
        corrupt.raw_authority.maximum_bone_error_ratio = 0.0f;
        corrupt.raw_authority.maximum_bone_error_index = 0u;
        require(!integration::canonical_snapshot_valid(corrupt),
            "zero bone error retained a forged source index");
        corrupt = first;
        corrupt.raw_authority.support_traction_capacity[0] = -1.0f;
        require(!integration::canonical_snapshot_valid(corrupt),
            "negative authority traction capacity was accepted");
        corrupt = first;
        corrupt.raw_authority.support_traction_capacity[0] = 0.25f;
        corrupt.raw_authority.support_traction_used[0] = 0.30f;
        require(!integration::canonical_snapshot_valid(corrupt),
            "traction use above finite contact capacity was accepted");
        corrupt = first;
        corrupt.raw_authority.support_normal_impulse[0] = -0.01f;
        require(!integration::canonical_snapshot_valid(corrupt),
            "negative support normal impulse was accepted");
        corrupt = first;
        corrupt.raw_authority.support_tangential_impulse[0] =
            std::numeric_limits<float>::quiet_NaN();
        require(!integration::canonical_snapshot_valid(corrupt),
            "non-finite support tangential impulse was accepted");
        corrupt = first;
        corrupt.raw_authority.friction_work_joules = 1.0f;
        corrupt.raw_authority.friction_positive_work_joules = 0.0f;
        require(!integration::canonical_snapshot_valid(corrupt),
            "friction ledger accepted less positive work than net work");
        corrupt = first;
        corrupt.raw_authority.commanded_actions[0] =
            std::clamp(corrupt.raw_authority.commanded_actions[0] + 0.125f,
                -1.0f, 1.0f);
        require(!integration::canonical_snapshot_valid(corrupt),
            "command telemetry diverged from the accepted host request");
        corrupt = first;
        corrupt.raw_authority.actuator_actions[0] =
            std::clamp(corrupt.raw_authority.actuator_actions[0] + 0.125f,
                -1.0f, 1.0f);
        require(!integration::canonical_snapshot_valid(corrupt),
            "actuator telemetry diverged from the physical motor snapshot");
        corrupt = first;
        corrupt.raw_authority.commanded_actions[0] = 1.01f;
        require(!integration::canonical_snapshot_valid(corrupt),
            "raw authority accepted an out-of-range commanded action");
        sim::Environment telemetry_disabled_environment{
            sim::CreatureBlueprint::humanoid(), 0x751E12u };
        std::array<float, sim::action_count> disabled_actions{};
        disabled_actions[0] = 0.5f;
        const sim::StepResult disabled_step = telemetry_disabled_environment.step(
            disabled_actions, 1.0f / 60.0f);
        static_cast<void>(disabled_step);
        require(telemetry_disabled_environment.raw_authority_telemetry()
                    .commanded_actions[0] == 0.5f,
            "disabled-telemetry adversary did not carry an internal payload");
        integration::CanonicalSnapshot telemetry_disabled =
            integration::snapshot(telemetry_disabled_environment, 0x751u, 8u);
        require(integration::canonical_snapshot_valid(telemetry_disabled)
                && !telemetry_disabled.raw_authority_telemetry_enabled
                && integration::empty_raw_authority_telemetry(
                    telemetry_disabled.raw_authority),
            "disabled raw authority payload escaped the public snapshot");
        telemetry_disabled.raw_authority.root_drive_corrections = 1u;
        require(!integration::canonical_snapshot_valid(telemetry_disabled),
            "disabled telemetry retained hidden authority evidence");
        corrupt = first;
        corrupt.root_cell.cell.material = sandhybrid::material_count;
        require(!integration::canonical_snapshot_valid(corrupt),
            "out-of-range canonical material was accepted");
        corrupt = first;
        corrupt.control_authority.lesson_teacher =
            std::numeric_limits<float>::quiet_NaN();
        require(!integration::canonical_snapshot_valid(corrupt),
            "non-finite guidance authority was accepted");
        corrupt = first;
        corrupt.control_authority.applied_guidance = sim::GuidanceMode::assisted;
        require(!integration::canonical_snapshot_valid(corrupt),
            "accepted Raw request reported mismatched applied guidance");
        corrupt = first;
        corrupt.control_authority.joint_control =
            integration::JointControlSource::retained_policy;
        require(!integration::canonical_snapshot_valid(corrupt),
            "accepted retained authority omitted retained provenance");
    }

    void test_external_control_authority()
    {
        namespace director = runner::director;
        namespace integration = runner::integration;
        namespace sim = runner::sim;

        sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
            0x751E01u };
        const float initial_elapsed = environment.elapsed_seconds();
        integration::ControlRequest ambiguous{};
        const integration::FixedStepResult rejected = integration::fixed_step(
            environment, ambiguous, 1u, 1u);
        require(!rejected.control_applied
                && rejected.control_rejection
                    == integration::ControlRejection::unspecified_guidance
                && environment.elapsed_seconds() == initial_elapsed,
            "ambiguous external authority mutated physics");

        integration::ControlRequest expert{};
        expert.mode = integration::ControlMode::training;
        expert.joint_control = integration::JointControlSource::expert_joint_actions;
        expert.guidance = integration::GuidanceRequest::raw_policy;
        const integration::FixedStepResult expert_step = integration::fixed_step(
            environment, expert, 2u, 1u);
        require(expert_step.control_applied
                && expert_step.control_rejection
                    == integration::ControlRejection::none
                && expert_step.authority.request_accepted
                && expert_step.authority.applied_guidance
                    == sim::GuidanceMode::raw_policy_audit
                && !expert_step.authority.optional_guidance_active,
            "explicit raw expert control did not disclose its authority");

        integration::ControlRequest invalid = expert;
        invalid.actions[0] = std::numeric_limits<float>::quiet_NaN();
        const float before_invalid = environment.elapsed_seconds();
        const integration::FixedStepResult invalid_step = integration::fixed_step(
            environment, invalid, 3u, 1u);
        require(!invalid_step.control_applied
                && invalid_step.control_rejection
                    == integration::ControlRejection::invalid_expert_action
                && environment.elapsed_seconds() == before_invalid,
            "non-finite expert action reached caller-owned physics");

        const std::vector<sim::Particle> before_inactive_pose =
            environment.particles();
        integration::ControlRequest inactive = expert;
        inactive.actions[sim::equipment_state_action] = 0.5f;
        const integration::FixedStepResult inactive_step =
            integration::fixed_step(environment, inactive, 3u, 2u);
        require(!inactive_step.control_applied
                && inactive_step.control_rejection
                    == integration::ControlRejection::inactive_expert_action
                && environment.elapsed_seconds() == before_invalid
                && same_physical_pose(environment, before_inactive_pose),
            "inactive expert action mutated caller-owned physics");

        integration::ControlRequest out_of_range = expert;
        out_of_range.actions[0] = 1.01f;
        const integration::FixedStepResult out_of_range_step =
            integration::fixed_step(environment, out_of_range, 3u, 3u);
        require(!out_of_range_step.control_applied
                && out_of_range_step.control_rejection
                    == integration::ControlRejection::invalid_expert_action
                && environment.elapsed_seconds() == before_invalid
                && same_physical_pose(environment, before_inactive_pose),
            "out-of-range expert action mutated caller-owned physics");

        director::TaskGraph graph = director::default_training_graph();
        director::Evidence evidence{};
        evidence.mastery[0] = 1.0f;
        const director::Decision decision = director::select(graph, evidence,
            director::Profile::player_guided);
        require(decision.selected(), "player task graph produced no intent");

        integration::ControlRequest player{};
        player.mode = integration::ControlMode::player_guided;
        player.task_graph = graph;
        player.evidence = evidence;
        player.selected_task = integration::task_command(graph, evidence,
            decision, director::Profile::player_guided);
        player.player_intent = integration::player_intent_for(
            player.selected_task);
        player.joint_control = integration::JointControlSource::retained_policy;
        player.guidance = integration::GuidanceRequest::raw_policy;
        integration::RetainedPolicyController unretained_controller{};
        const std::vector<sim::Particle> before_player_pose =
            environment.particles();
        const float before_player_elapsed = environment.elapsed_seconds();

        const integration::FixedStepResult missing_controller_step =
            integration::fixed_step(environment, player, 3u, 4u);
        require(!missing_controller_step.control_applied
                && missing_controller_step.control_rejection
                    == integration::ControlRejection::retained_policy_required
                && environment.elapsed_seconds() == before_player_elapsed
                && same_physical_pose(environment, before_player_pose),
            "player-guided control ran without a retained controller");

        const integration::FixedStepResult unretained_controller_step =
            integration::fixed_step(environment, player, unretained_controller,
                3u, 5u);
        require(!unretained_controller_step.control_applied
                && unretained_controller_step.control_rejection
                    == integration::ControlRejection::retained_policy_mismatch
                && environment.elapsed_seconds() == before_player_elapsed
                && same_physical_pose(environment, before_player_pose),
            "player-guided control accepted an unretained controller");

        integration::ControlRequest mixed_player = player;
        mixed_player.actions[0] = 0.25f;
        const integration::FixedStepResult mixed_player_step =
            integration::fixed_step(environment, mixed_player,
                unretained_controller, 3u, 6u);
        require(!mixed_player_step.control_applied
                && mixed_player_step.control_rejection
                    == integration::ControlRejection::mixed_joint_authority
                && environment.elapsed_seconds() == before_player_elapsed
                && same_physical_pose(environment, before_player_pose),
            "retained-policy request also carried expert joint authority");

        const integration::FixedStepResult mixed_expert_step =
            integration::fixed_step(environment, expert,
                unretained_controller, 3u, 7u);
        require(!mixed_expert_step.control_applied
                && mixed_expert_step.control_rejection
                    == integration::ControlRejection::mixed_joint_authority
                && environment.elapsed_seconds() == before_player_elapsed
                && same_physical_pose(environment, before_player_pose),
            "expert request also carried a retained-controller authority source");

        integration::ControlRequest player_expert = player;
        player_expert.joint_control =
            integration::JointControlSource::expert_joint_actions;
        const integration::FixedStepResult player_expert_step =
            integration::fixed_step(environment, player_expert, 4u, 1u);
        require(!player_expert_step.control_applied
                && player_expert_step.control_rejection
                    == integration::ControlRejection::retained_policy_required
                && environment.elapsed_seconds() == before_player_elapsed
                && same_physical_pose(environment, before_player_pose),
            "player-guided mode bypassed retained AI joint control");

        integration::ControlRequest assisted_player = player;
        assisted_player.guidance = integration::GuidanceRequest::assisted;
        const integration::FixedStepResult assisted_player_step =
            integration::fixed_step(environment, assisted_player,
                unretained_controller, 4u, 2u);
        require(!assisted_player_step.control_applied
                && assisted_player_step.control_rejection
                    == integration::ControlRejection::player_guidance_not_raw
                && environment.elapsed_seconds() == before_player_elapsed
                && same_physical_pose(environment, before_player_pose),
            "player-guided mode admitted hidden Assisted authority");

        integration::RetainedPolicyController assisted_controller =
            assisted_test_controller_for(environment, 0x751E04u);
        const integration::FixedStepResult assisted_retained_step =
            integration::fixed_step(environment, player, assisted_controller,
                4u, 3u);
        require(!assisted_retained_step.control_applied
                && assisted_retained_step.control_rejection
                    == integration::ControlRejection::
                        retained_player_guidance_not_raw
                && environment.elapsed_seconds() == before_player_elapsed
                && same_physical_pose(environment, before_player_pose),
            "player-guided mode admitted Assisted retained-policy provenance");

        integration::ControlRequest unsupported = player;
        unsupported.player_intent.use_tool = true;
        const integration::FixedStepResult unsupported_step =
            integration::fixed_step(environment, unsupported,
                unretained_controller, 4u, 4u);
        require(!unsupported_step.control_applied
                && unsupported_step.control_rejection
                    == integration::ControlRejection::unsupported_player_intent
                && environment.elapsed_seconds() == before_player_elapsed
                && same_physical_pose(environment, before_player_pose),
            "unimplemented player tool intent was silently accepted");

        integration::ControlRequest conflicted_stop = player;
        conflicted_stop.player_intent.stop = true;
        conflicted_stop.player_intent.travel_direction = 1.0f;
        const integration::FixedStepResult conflicted_stop_step =
            integration::fixed_step(environment, conflicted_stop,
                unretained_controller, 4u, 5u);
        require(!conflicted_stop_step.control_applied
                && conflicted_stop_step.control_rejection
                    == integration::ControlRejection::invalid_player_intent
                && environment.elapsed_seconds() == before_player_elapsed
                && same_physical_pose(environment, before_player_pose),
            "contradictory player stop intent mutated caller-owned physics");

        integration::ControlRequest nonfinite_player = player;
        nonfinite_player.player_intent.travel_direction =
            std::numeric_limits<float>::quiet_NaN();
        const integration::FixedStepResult nonfinite_player_step =
            integration::fixed_step(environment, nonfinite_player,
                unretained_controller, 4u, 6u);
        require(!nonfinite_player_step.control_applied
                && nonfinite_player_step.control_rejection
                    == integration::ControlRejection::invalid_player_intent
                && environment.elapsed_seconds() == before_player_elapsed
                && same_physical_pose(environment, before_player_pose),
            "non-finite player intent mutated caller-owned physics");

        require(environment.set_locomotion_intent(0.0f, true),
            "external-authority stop-state fixture was rejected");
        const std::vector<sim::Particle> stopped_pose = environment.particles();
        const float stopped_elapsed = environment.elapsed_seconds();

        integration::ControlRequest rejected_handoff = expert;
        rejected_handoff.mode = integration::ControlMode::autonomous;
        rejected_handoff.actions[0] =
            std::numeric_limits<float>::quiet_NaN();
        const integration::FixedStepResult rejected_handoff_step =
            integration::fixed_step(environment, rejected_handoff, 5u, 1u);
        require(!rejected_handoff_step.control_applied
                && rejected_handoff_step.control_rejection
                    == integration::ControlRejection::invalid_expert_action
                && environment.elapsed_seconds() == stopped_elapsed
                && environment.locomotion_intent_direction() == 0.0f
                && environment.locomotion_stop_requested()
                && same_physical_pose(environment, stopped_pose),
            "rejected autonomous handoff partially cleared player stop state");

        integration::ControlRequest autonomous_handoff = expert;
        autonomous_handoff.mode = integration::ControlMode::autonomous;
        const integration::FixedStepResult autonomous_handoff_step =
            integration::fixed_step(environment, autonomous_handoff, 5u, 2u);
        require(autonomous_handoff_step.control_applied
                && autonomous_handoff_step.snapshot
                    .locomotion_intent_direction == 1.0f
                && !autonomous_handoff_step.snapshot
                    .locomotion_stop_requested
                && environment.locomotion_direction() == 1.0f,
            "autonomous handoff inherited stale player stop state");

        integration::CanonicalSnapshot stalled =
            autonomous_handoff_step.snapshot;
        stalled.zero_progress_seconds = sim::zero_progress_reset_seconds;
        require(integration::trial_stalled(stalled),
            "canonical zero-progress stall was not classified");
        integration::FixedStepResult stalled_step{};
        stalled_step.snapshot = stalled;
        const integration::DiagnosticEvents events =
            integration::diagnostic_events(nullptr, stalled_step);
        require(std::ranges::any_of(events.span(),
                [](const integration::DiagnosticEvent& event) noexcept
                {
                    return event.kind
                        == integration::DiagnosticKind::trial_stalled;
                }),
            "classified trial stall did not emit a diagnostic event");
    }

    void test_nonhuman_director_compatibility()
    {
        namespace director = runner::director;
        namespace integration = runner::integration;
        namespace sim = runner::sim;

        sim::Environment dog{ sim::CreatureBlueprint::crawler4(), 0x751E03u };
        require(!dog.equipment_capable(),
            "non-Human compatibility test unexpectedly has manipulators");
        const director::TaskGraph source = director::default_training_graph();
        const director::TaskGraph compatible =
            integration::compatible_task_graph(dog, source);
        require(compatible.valid(),
            "species-filtered Director graph became malformed");

        director::TaskGraph forged_compatible = compatible;
        const director::ChallengeKind forged_kind =
            forged_compatible.nodes[0].challenge;
        for (std::size_t index = 0u; index < forged_compatible.count; ++index)
        {
            if (forged_compatible.nodes[index].challenge == forged_kind)
                forged_compatible.nodes[index].stable_challenge_id ^= 0x4000u;
        }
        require(forged_compatible.valid()
                && !integration::evidence_for_compatible_graph(source,
                    forged_compatible, director::Evidence{}).has_value(),
            "compatible-graph reduction accepted a forged challenge identity");
        for (std::size_t index = 0; index < compatible.count; ++index)
        {
            const integration::DesiredTaskState desired =
                integration::desired_task_state(compatible.nodes[index].task,
                    compatible.nodes[index].challenge);
            require(!compatible.nodes[index].enabled || !desired.equipment_task,
                "non-Human Director retained a Human weapon task");
        }

        director::Evidence completed{};
        for (std::size_t index = 0; index < compatible.count; ++index)
        {
            if (compatible.nodes[index].enabled)
                completed.mastery[index] = 1.0f;
        }
        const director::Decision decision = director::select(compatible,
            completed, director::Profile::autonomous_npc);
        require(!director::evidence_valid(compatible, completed)
                && !decision.selected()
                && decision.reason == director::SelectionReason::no_eligible_task,
            "non-Human Director accepted scalar-only physical mastery");

        director::Evidence stale_request{};
        stale_request.requested_goal = 10u;
        stale_request.first_stall = 13u;
        const director::Decision compatible_decision =
            integration::select_for_environment(dog, source, stale_request,
                director::Profile::autonomous_npc);
        require(compatible_decision.selected()
                && compatible_decision.task_index == 0u,
            "non-Human Director dead-ended on stale Human equipment evidence");
    }
}

int main()
{
    try
    {
        test_strict_graph_and_evidence_reducer();
        test_atomic_director_state();
        test_one_update_raw_non_retention();
        test_topology_masked_policy_actions();
        test_checkpoint_action_mask_transitions();
        test_locomotion_intent_has_no_root_authority();
        test_canonical_authority_snapshot();
        test_nonhuman_director_compatibility();
        test_external_control_authority();
        std::cout << "Epoch2DWalkEngine v0.7.51 Runner-example external host authority tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Epoch2DWalkEngine v0.7.51 Runner-example external host authority tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
