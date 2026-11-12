#include "integration.hpp"
#include "ppo.hpp"
#include "raw_inference_diagnostic.hpp"
#include "raw_learning_test_access.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    void test_raw_candidate_evaluation_report()
    {
        namespace diagnostics = runner::diagnostics;
        namespace rl = runner::rl;
        namespace sim = runner::sim;
        constexpr std::uint64_t handoff = 900u;
        static_assert(!diagnostics::raw_candidate_evaluation_observed(
            37u, 37u, 905u, handoff));
        static_assert(!diagnostics::raw_candidate_evaluation_observed(
            37u, 38u, handoff, handoff));
        static_assert(diagnostics::raw_candidate_evaluation_observed(
            37u, 38u, 905u, handoff));

        rl::TrainingMetrics metrics{};
        metrics.evaluation_valid = false;
        metrics.evaluation_invalid_runs = 3u;
        metrics.evaluation_invalid_reason = sim::InvalidMotion::fallen;
        metrics.evaluation_rejection_mask =
            rl::evidence_bit(rl::MotionEvidenceFailure::invalid_motion)
            | rl::evidence_bit(rl::MotionEvidenceFailure::missing_progress);
        metrics.evaluation_distance = 2.56795f;
        metrics.evaluation_stride_events = 0.0f;
        metrics.evaluation_survival = 2.06667f;
        metrics.evaluation_quality_key = 0x1122334455667788ULL;
        const diagnostics::RawCandidateEvaluationEvidence captured =
            diagnostics::capture_raw_candidate_evaluation(905u, metrics);
        require(captured.present && captured.update == 905u
                && !captured.valid && captured.invalid_runs == 3u
                && captured.invalid_reason == sim::InvalidMotion::fallen
                && captured.rejection_mask
                    == metrics.evaluation_rejection_mask
                && captured.rejection_summary
                    == "INVALID MOTION + NO PROGRESS"
                && captured.distance == metrics.evaluation_distance
                && captured.stride_events
                    == metrics.evaluation_stride_events
                && captured.survival == metrics.evaluation_survival
                && captured.quality_key == metrics.evaluation_quality_key,
            "Raw candidate evaluation capture lost rejection evidence");

        diagnostics::RawInferenceReport report{};
        report.handoff_update = handoff;
        report.first_raw_candidate = captured;
        report.last_raw_candidate = captured;
        report.last_raw_candidate.update = 1200u;
        std::ostringstream output{};
        diagnostics::write_raw_inference_report(report, output);
        const std::string text = output.str();
        require(text.contains("raw_candidate_first present=1 update=905")
                && text.contains("raw_candidate_last present=1 update=1200")
                && text.contains("invalid_runs=3")
                && text.contains("invalid=FALLEN")
                && text.contains("rejection_mask=0x11")
                && text.contains(
                    "rejection=\"INVALID MOTION + NO PROGRESS\"")
                && text.contains("distance=2.567950")
                && text.contains("strides=0.000000")
                && text.contains("survival=2.066670")
                && text.contains("quality=0x1122334455667788"),
            "Raw candidate formatter omitted exact first/last evidence");
        require(text.contains("replay_skipped_no_retained_policy=1")
                && !text.contains("forward seed=")
                && !text.contains("reverse seed=")
                && !text.contains("stop seed="),
            "no-retention diagnostic emitted misleading zero replay rows");

        diagnostics::RawInferenceReport absent{};
        std::ostringstream absent_output{};
        diagnostics::write_raw_inference_report(absent, absent_output);
        const std::string absent_text = absent_output.str();
        require(absent_text.contains("raw_candidate_first present=0")
                && absent_text.contains("raw_candidate_last present=0"),
            "absent candidate evaluations were not explicit");
    }

    void test_raw_observation_scrub()
    {
        namespace rl = runner::rl;
        namespace sim = runner::sim;
        std::array<float, runner::sim::observation_count> observation{};
        observation.fill(0.25f);
        const auto scrubbed = rl::raw_policy_observation(observation);
        require(scrubbed[rl::elapsed_phase_sine_observation_index] == 0.0f
                && scrubbed[rl::elapsed_phase_cosine_observation_index]
                    == 0.0f
                && scrubbed[0] == observation[0],
            "Raw observation did not isolate elapsed gait phase");

        sim::HumanSupportFeedback support{};
        support.center_of_mass = { 3.0f, 2.4f };
        support.center_of_mass_velocity = { 0.60f, -0.10f };
        support.aggregate_cop_x = 2.55f;
        support.capture_point_x = 3.25f;
        support.capture_error_x = 0.70f;
        support.total_normal_impulse = 18.0f;
        support.load_asymmetry = 0.35f;
        support.valid = true;
        const auto encoded = rl::encode_foundational_human_support_observation(
            observation, support, 1.0f);
        const auto encoded_repeated =
            rl::encode_foundational_human_support_observation(
                observation, support, 1.0f);
        const auto reverse_facing =
            rl::encode_foundational_human_support_observation(
                observation, support, -1.0f);
        sim::HumanSupportFeedback mirrored_support = support;
        mirrored_support.capture_error_x = -support.capture_error_x;
        mirrored_support.load_asymmetry = -support.load_asymmetry;
        const auto mirrored =
            rl::encode_foundational_human_support_observation(
                observation, mirrored_support, -1.0f);
        require(encoded[rl::elapsed_phase_sine_observation_index] == 0.70f
                && encoded[rl::elapsed_phase_cosine_observation_index] == 0.35f
                && encoded_repeated == encoded
                && reverse_facing[rl::elapsed_phase_sine_observation_index]
                    == -0.70f
                && reverse_facing[rl::elapsed_phase_cosine_observation_index]
                    == 0.35f
                && mirrored[rl::elapsed_phase_sine_observation_index] == 0.70f
                && mirrored[rl::elapsed_phase_cosine_observation_index]
                    == -0.35f
                && encoded[0] == observation[0],
            "foundational Human support encoding lost finite repeat/mirror behavior");
        require(observation[rl::elapsed_phase_sine_observation_index] == 0.25f
                && observation[rl::elapsed_phase_cosine_observation_index]
                    == 0.25f
                && support.capture_error_x == 0.70f
                && support.load_asymmetry == 0.35f
                && support.valid,
            "foundational Human support encoding mutated its caller-owned inputs");
        sim::HumanSupportFeedback nonfinite = support;
        nonfinite.capture_error_x = std::numeric_limits<float>::quiet_NaN();
        const auto rejected_nonfinite =
            rl::encode_foundational_human_support_observation(
                observation, nonfinite, 1.0f);
        support.valid = false;
        const auto rejected_invalid =
            rl::encode_foundational_human_support_observation(
                observation, support, 1.0f);
        require(rejected_nonfinite[rl::elapsed_phase_sine_observation_index] == 0.0f
                && rejected_nonfinite[rl::elapsed_phase_cosine_observation_index]
                    == 0.0f
                && rejected_invalid[rl::elapsed_phase_sine_observation_index]
                    == 0.0f
                && rejected_invalid[rl::elapsed_phase_cosine_observation_index]
                    == 0.0f,
            "invalid/non-finite Human support evidence entered policy input");

        sim::Environment human{
            sim::CreatureBlueprint::humanoid(), 0x751010u };
        human.set_course(sim::CourseStage::uneven, 0.30f);
        const auto human_observation = human.observation();
        const auto foundational_assisted =
            rl::foundational_policy_observation(human,
                sim::CourseStage::uneven, sim::GaitTask::walk,
                sim::GuidanceMode::assisted);
        const auto foundational_raw = rl::foundational_policy_observation(human,
            sim::CourseStage::uneven, sim::GaitTask::walk,
            sim::GuidanceMode::raw_policy_audit);
        const auto speed_walk_assisted =
            rl::foundational_policy_observation(human,
                sim::CourseStage::uneven, sim::GaitTask::speed_walk,
                sim::GuidanceMode::assisted);
        const auto speed_walk_raw = rl::foundational_policy_observation(human,
            sim::CourseStage::uneven, sim::GaitTask::speed_walk,
            sim::GuidanceMode::raw_policy_audit);
        const auto balance_assisted = rl::foundational_policy_observation(human,
            sim::CourseStage::balance, sim::GaitTask::walk,
            sim::GuidanceMode::assisted);
        const auto balance_raw = rl::foundational_policy_observation(human,
            sim::CourseStage::balance, sim::GaitTask::walk,
            sim::GuidanceMode::raw_policy_audit);
        sim::Environment quadruped{
            sim::CreatureBlueprint::quadruped(), 0x751010u };
        quadruped.set_course(sim::CourseStage::uneven, 0.30f);
        const auto quadruped_observation = quadruped.observation();
        const auto quadruped_walk_assisted =
            rl::foundational_policy_observation(quadruped,
                sim::CourseStage::uneven, sim::GaitTask::walk,
                sim::GuidanceMode::assisted);
        const auto quadruped_walk_raw = rl::foundational_policy_observation(
            quadruped, sim::CourseStage::uneven, sim::GaitTask::walk,
            sim::GuidanceMode::raw_policy_audit);
        const auto raw_human_observation =
            rl::raw_policy_observation(human_observation);
        const auto raw_quadruped_observation =
            rl::raw_policy_observation(quadruped_observation);
        require(foundational_assisted
                    == rl::encode_foundational_human_support_observation(
                        human_observation, human.human_support_feedback(),
                        human.facing_direction())
                && foundational_raw == foundational_assisted
                && speed_walk_assisted == human_observation
                && balance_assisted == human_observation
                && quadruped_walk_assisted == quadruped_observation
                && speed_walk_raw == raw_human_observation
                && balance_raw == raw_human_observation
                && quadruped_walk_raw == raw_quadruped_observation
                && speed_walk_raw[rl::elapsed_phase_sine_observation_index]
                    == 0.0f
                && speed_walk_raw[rl::elapsed_phase_cosine_observation_index]
                    == 0.0f
                && balance_raw[rl::elapsed_phase_sine_observation_index]
                    == 0.0f
                && balance_raw[rl::elapsed_phase_cosine_observation_index]
                    == 0.0f
                && quadruped_walk_raw[
                    rl::elapsed_phase_sine_observation_index] == 0.0f
                && quadruped_walk_raw[
                    rl::elapsed_phase_cosine_observation_index] == 0.0f,
            "scoped foundational guidance leaked phase/support across a gait, stage, topology, or Raw boundary");

        sim::Environment repeated_human{
            sim::CreatureBlueprint::humanoid(), 0x751010u };
        repeated_human.set_course(sim::CourseStage::uneven, 0.30f);
        std::array<float, sim::action_count> zero{};
        for (int frame = 0; frame < 8; ++frame)
        {
            static_cast<void>(human.step(zero));
            static_cast<void>(repeated_human.step(zero));
        }
        const sim::HumanSupportFeedback maintained =
            human.human_support_feedback();
        const sim::HumanSupportFeedback maintained_repeated =
            repeated_human.human_support_feedback();
        require(maintained.valid
                && maintained.total_normal_impulse > 0.0f
                && std::isfinite(maintained.center_of_mass.x)
                && std::isfinite(maintained.center_of_mass.y)
                && std::isfinite(maintained.center_of_mass_velocity.x)
                && std::isfinite(maintained.center_of_mass_velocity.y)
                && std::isfinite(maintained.aggregate_cop_x)
                && std::isfinite(maintained.capture_point_x)
                && std::isfinite(maintained.capture_error_x)
                && std::isfinite(maintained.total_normal_impulse)
                && std::isfinite(maintained.load_asymmetry)
                && maintained.center_of_mass.x
                    == maintained_repeated.center_of_mass.x
                && maintained.center_of_mass.y
                    == maintained_repeated.center_of_mass.y
                && maintained.center_of_mass_velocity.x
                    == maintained_repeated.center_of_mass_velocity.x
                && maintained.center_of_mass_velocity.y
                    == maintained_repeated.center_of_mass_velocity.y
                && maintained.aggregate_cop_x
                    == maintained_repeated.aggregate_cop_x
                && maintained.capture_point_x
                    == maintained_repeated.capture_point_x
                && maintained.capture_error_x
                    == maintained_repeated.capture_error_x
                && maintained.total_normal_impulse
                    == maintained_repeated.total_normal_impulse
                && maintained.load_asymmetry
                    == maintained_repeated.load_asymmetry
                && maintained.valid == maintained_repeated.valid,
            "always-maintained Human support feedback was invalid, non-finite, or non-repeatable");
    }

    void test_raw_player_intent_curriculum()
    {
        namespace rl = runner::rl;
        namespace sim = runner::sim;
        const auto forward = rl::raw_player_intent_curriculum(0u, 64u);
        const auto reverse = rl::raw_player_intent_curriculum(1u, 64u);
        const auto moving_before_stop =
            rl::raw_player_intent_curriculum(2u, 63u);
        const auto stopped = rl::raw_player_intent_curriculum(2u, 64u);
        const auto resumed = rl::raw_player_intent_curriculum(2u, 128u);
        require(forward.direction == 1.0f && !forward.stop
                && reverse.direction == -1.0f && !reverse.stop
                && moving_before_stop.direction == 1.0f
                && !moving_before_stop.stop
                && stopped.direction == 0.0f && stopped.stop
                && resumed.direction == 1.0f && !resumed.stop,
            "Raw player-intent curriculum lost signed move/stop ordering");

        std::uint64_t forward_samples{};
        std::uint64_t reverse_samples{};
        std::uint64_t stop_samples{};
        for (std::size_t environment_index = 0u;
            environment_index < 8u; ++environment_index)
        {
            for (std::size_t step = 0u; step < 128u; ++step)
            {
                const auto intent = rl::raw_player_intent_curriculum(
                    environment_index, step);
                const auto repeated = rl::raw_player_intent_curriculum(
                    environment_index, step);
                require(intent.direction == repeated.direction
                        && intent.stop == repeated.stop,
                    "Raw player-intent curriculum was not deterministic");
                if (intent.stop)
                {
                    require(intent.direction == 0.0f,
                        "Raw stop cohort retained a travel direction");
                    ++stop_samples;
                }
                else if (intent.direction > 0.5f)
                    ++forward_samples;
                else if (intent.direction < -0.5f)
                    ++reverse_samples;
                else
                    require(false,
                        "Raw moving cohort omitted its signed direction");
            }
        }
        require(forward_samples == 384u && reverse_samples == 384u
                && stop_samples == 256u,
            "Raw player-intent curriculum did not cover its exact cohorts");

        sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
            0x75104u };
        environment.set_course(sim::CourseStage::uneven, 0.30f);
        environment.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
        require(environment.set_locomotion_intent(1.0f, false)
                && environment.observation()[43] == 1.0f
                && environment.set_locomotion_intent(-1.0f, false)
                && environment.observation()[43] == -1.0f
                && environment.set_locomotion_intent(0.0f, true)
                && environment.observation()[43] == 0.0f,
            "Raw observation did not expose signed move/stop intent");
        const float prior_direction = environment.locomotion_direction();
        require(!environment.set_locomotion_intent(
                    std::numeric_limits<float>::infinity(), false)
                && environment.locomotion_direction() == prior_direction,
            "non-finite Raw player intent mutated task state");

        sim::Environment forward_environment{
            sim::CreatureBlueprint::humanoid(), 0x751070u };
        sim::Environment reverse_environment{
            sim::CreatureBlueprint::humanoid(), 0x751070u };
        sim::Environment stop_environment{
            sim::CreatureBlueprint::humanoid(), 0x751070u };
        sim::Environment assisted_environment{
            sim::CreatureBlueprint::humanoid(), 0x751070u };
        for (sim::Environment* sample : { &forward_environment,
                 &reverse_environment, &stop_environment,
                 &assisted_environment })
        {
            sample->set_course(sim::CourseStage::uneven, 0.30f);
            sample->set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
            std::array<float, sim::action_count> zero{};
            for (std::size_t step = 0u; step < 36u; ++step)
                static_cast<void>(sample->step(zero));
        }
        assisted_environment.set_guidance_mode(sim::GuidanceMode::assisted);
        require(forward_environment.set_locomotion_intent(1.0f, false)
                && reverse_environment.set_locomotion_intent(-1.0f, false)
                && stop_environment.set_locomotion_intent(0.0f, true)
                && assisted_environment.set_locomotion_intent(1.0f, false),
            "teacher intent fixture rejected finite signed/stop tasks");
        const auto forward_teacher =
            rl::raw_walking_teacher_action(forward_environment);
        const auto reverse_teacher =
            rl::raw_walking_teacher_action(reverse_environment);
        const auto stop_teacher = rl::raw_walking_teacher_action(stop_environment);
        const auto stop_balance = rl::balance_teacher_action(stop_environment);
        const auto assisted_teacher =
            rl::raw_walking_teacher_action(assisted_environment);
        require(assisted_teacher == forward_teacher,
            "Assisted imitation and Raw diagnostic selected different Human gait targets");
        const auto assisted_policy_input =
            rl::foundational_policy_observation(assisted_environment,
                sim::CourseStage::uneven, sim::GaitTask::walk,
                sim::GuidanceMode::assisted);
        const auto raw_policy_input =
            rl::foundational_policy_observation(forward_environment,
                sim::CourseStage::uneven, sim::GaitTask::walk,
                sim::GuidanceMode::raw_policy_audit);
        const auto expected_foundational_input =
            rl::encode_foundational_human_support_observation(
                forward_environment.observation(),
                forward_environment.human_support_feedback(),
                forward_environment.facing_direction());
        require((forward_environment.observation()[
                    rl::elapsed_phase_sine_observation_index] != 0.0f
                    || forward_environment.observation()[
                        rl::elapsed_phase_cosine_observation_index] != 0.0f)
                && assisted_policy_input == raw_policy_input
                && raw_policy_input == expected_foundational_input
                && std::isfinite(raw_policy_input[
                    rl::elapsed_phase_sine_observation_index])
                && std::isfinite(raw_policy_input[
                    rl::elapsed_phase_cosine_observation_index]),
            "Assisted and Raw foundational policy inputs crossed a phase/distribution boundary");
        bool signed_teacher_differs{};
        for (std::size_t action = 0u;
            action < sim::anatomy_action_count; ++action)
        {
            require(std::isfinite(forward_teacher[action])
                    && std::isfinite(reverse_teacher[action])
                    && stop_teacher[action] == stop_balance[action],
                "signed/stop teacher emitted non-finite action or drove a stop");
            signed_teacher_differs = signed_teacher_differs
                || forward_teacher[action] != reverse_teacher[action];
        }
        require(signed_teacher_differs,
            "walking teacher ignored opposed signed locomotion intent");
        rl::PpoTrainer first{ sim::CreatureBlueprint::humanoid(), 8u, false };
        rl::PpoTrainer repeated{ sim::CreatureBlueprint::humanoid(), 8u, false };
        first.reset_policy(0x751071u, true);
        repeated.reset_policy(0x751071u, true);
        const rl::AssistedIntentRolloutEvidence assisted =
            rl::RawLearningTestAccess::one_assisted_intent_rollout(first);
        const rl::AssistedIntentRolloutEvidence assisted_repeat =
            rl::RawLearningTestAccess::one_assisted_intent_rollout(repeated);
        require(assisted.forward_samples == 384u
                && assisted.reverse_samples == 384u
                && assisted.stop_samples == 256u
                && assisted.observation_mismatches == 0u
                && assisted.elapsed_phase_samples == 0u
                && assisted.teacher_bootstrap_samples == 1024u
                && assisted.raw_origin_samples == 0u,
            "Assisted bootstrap did not receive the exact sanitized signed/stop observation graph");
        require(assisted.optimizer_step_after
                    > assisted.optimizer_step_before
                && assisted.parameters_after_digest
                    != assisted.parameters_before_digest,
            "sanitized Assisted rollout was not admitted to the real optimizer");
        require(assisted == assisted_repeat,
            "Assisted sanitized observation and teacher graph was not repeatable");
    }

    void test_foundational_mixed_rollout_schedule()
    {
        namespace rl = runner::rl;
        namespace sim = runner::sim;
        constexpr std::size_t cohort_size = 8u;
        constexpr std::uint64_t samples_per_environment = 128u;
        const sim::CreatureBlueprint human = sim::CreatureBlueprint::humanoid();
        const std::uint64_t handoff =
            rl::foundational_walk_teacher_handoff_update(human);
        const std::uint64_t fade_begin =
            rl::foundational_walk_teacher_fade_begin_update(human);
        const std::uint64_t pre_mixed = fade_begin;
        const std::uint64_t first_mixed = fade_begin + 1u;
        const std::uint64_t midpoint =
            fade_begin + (handoff - fade_begin) / 2u;
        const std::uint64_t final_mixed = handoff - 1u;
        const std::uint64_t whole_raw = handoff;
        require(rl::foundational_walk_raw_rollout_count(
                    pre_mixed, sim::CourseStage::uneven,
                    human, cohort_size) == 0u
                && rl::foundational_walk_raw_rollout_count(
                    first_mixed, sim::CourseStage::uneven,
                    human, cohort_size) == 1u
                && rl::foundational_walk_raw_rollout_count(
                    midpoint, sim::CourseStage::uneven,
                    human, cohort_size) == 4u
                && rl::foundational_walk_raw_rollout_count(
                    final_mixed, sim::CourseStage::uneven,
                    human, cohort_size) == cohort_size - 1u
                && rl::foundational_walk_raw_rollout_count(
                    whole_raw, sim::CourseStage::uneven,
                    human, cohort_size) == cohort_size,
            "foundational teacher-fade Raw cohort schedule lost an exact boundary");
        const float first_weight = rl::guided_rollout_imitation_weight(
            first_mixed, sim::CourseStage::uneven, &human);
        const float midpoint_weight = rl::guided_rollout_imitation_weight(
            midpoint, sim::CourseStage::uneven, &human);
        const float final_weight = rl::guided_rollout_imitation_weight(
            final_mixed, sim::CourseStage::uneven, &human);
        require(first_weight > midpoint_weight
                && midpoint_weight > final_weight && final_weight > 0.0f,
            "Assisted consolidation demonstrations do not decay monotonically");

        const std::array boundaries{
            pre_mixed, first_mixed, midpoint, final_mixed, whole_raw };
        for (const std::uint64_t update : boundaries)
        {
            rl::PpoTrainer first{ human, cohort_size, false };
            rl::PpoTrainer repeated{ human, cohort_size, false };
            first.reset_policy(0x751100u + update, true);
            repeated.reset_policy(0x751100u + update, true);
            const auto evidence =
                rl::RawLearningTestAccess::foundational_rollout_schedule(
                    first, update, false);
            const auto repeated_evidence =
                rl::RawLearningTestAccess::foundational_rollout_schedule(
                    repeated, update, false);
            const std::uint64_t raw_environments =
                rl::foundational_walk_raw_rollout_count(
                    update, sim::CourseStage::uneven, human, cohort_size);
            const std::uint64_t raw_samples =
                raw_environments * samples_per_environment;
            const std::uint64_t assisted_samples =
                cohort_size * samples_per_environment - raw_samples;
            const std::uint64_t observed_assisted =
                evidence.origin_counts[static_cast<std::size_t>(
                    rl::RolloutOrigin::assisted_policy)]
                + evidence.origin_counts[static_cast<std::size_t>(
                    rl::RolloutOrigin::teacher_bootstrap)]
                + evidence.origin_counts[static_cast<std::size_t>(
                    rl::RolloutOrigin::guided_policy)];
            require(evidence.origin_counts[static_cast<std::size_t>(
                        rl::RolloutOrigin::raw_policy)] == raw_samples
                    && observed_assisted == assisted_samples
                    && evidence.phase_leak_samples == 0u
                    && evidence.raw_guided_action_samples == 0u
                    && (assisted_samples == 0u
                        || evidence.assisted_guided_action_samples > 0u)
                    && !evidence.receipt_present,
                "mixed consolidation crossed Raw/Assisted action or receipt authority");
            require(evidence == repeated_evidence,
                "mixed consolidation origin schedule was not repeated-seed deterministic");
        }

        const std::array mixed_boundaries{
            first_mixed, midpoint, final_mixed };
        for (const std::uint64_t update : mixed_boundaries)
        {
            rl::PpoTrainer mixed{ human, cohort_size, false };
            mixed.reset_policy(0x7511A11u + update, true);
            const auto optimized =
                rl::RawLearningTestAccess::foundational_rollout_schedule(
                    mixed, update, true);
            const std::uint64_t raw_samples =
                rl::foundational_walk_raw_rollout_count(
                    update, sim::CourseStage::uneven, human, cohort_size)
                * samples_per_environment;
            require(optimized.ppo_raw_actor_samples == raw_samples
                    && optimized.ppo_assisted_actor_samples == 0u
                    && optimized.self_imitation_updates == 0u
                    && optimized.best_anchor_updates == 0u,
                "mixed PPO actor update admitted an Assisted-origin action");
            require(optimized.guided_raw_sources_rejected == raw_samples
                    && optimized.guided_assisted_sources_consumed > 0u
                    && !optimized.receipt_present,
                "mixed imitation consumed a Raw-origin target or issued premature Raw provenance");
        }

        rl::PpoTrainer assisted{ human, cohort_size, false };
        assisted.reset_policy(0x7511A55u, true);
        const auto assisted_optimized =
            rl::RawLearningTestAccess::foundational_rollout_schedule(
                assisted, pre_mixed, true);
        require(assisted_optimized.ppo_raw_actor_samples == 0u
                && assisted_optimized.ppo_assisted_actor_samples
                    == cohort_size * samples_per_environment
                && assisted_optimized.self_imitation_updates > 0u
                && !assisted_optimized.receipt_present,
            "pre-handoff Assisted optimizer behavior was not preserved");

        rl::PpoTrainer raw{ human, cohort_size, false };
        raw.reset_policy(0x7511A99u, true);
        const auto raw_optimized =
            rl::RawLearningTestAccess::foundational_rollout_schedule(
                raw, whole_raw, true);
        require(raw_optimized.ppo_raw_actor_samples
                    == cohort_size * samples_per_environment
                && raw_optimized.ppo_assisted_actor_samples == 0u,
            "whole-Raw PPO actor sample origin was not isolated");
        require(raw_optimized.self_imitation_updates == 0u
                && raw_optimized.best_anchor_updates == 0u
                && raw_optimized.guided_assisted_sources_consumed == 0u,
            "whole-Raw optimizer admitted an Assisted prior, anchor, or imitation sample");
    }

    void test_assisted_action_history_authority()
    {
        namespace rl = runner::rl;
        constexpr std::array authorities{
            1.0f, 0.75f, 0.50f, 0.25f, 0.0f };
        constexpr std::array expected{
            0.60f, 0.70f, 0.80f, 0.90f, 1.00f };
        float previous = 0.0f;
        for (std::size_t index = 0u; index < authorities.size(); ++index)
        {
            const float weight =
                rl::detail::assisted_action_history_new_weight(
                    true, authorities[index]);
            const float repeated =
                rl::detail::assisted_action_history_new_weight(
                    true, authorities[index]);
            require(std::abs(weight - expected[index]) < 1.0e-6f
                    && weight == repeated
                    && (index == 0u || weight > previous),
                "Human Uneven action-history smoothing did not converge monotonically with visible authority");
            previous = weight;
        }
        require(rl::detail::assisted_action_history_new_weight(
                    true, -1.0f) == 1.0f
                && rl::detail::assisted_action_history_new_weight(
                    true, 2.0f) == 0.60f
                && rl::detail::assisted_action_history_new_weight(
                    true, std::numeric_limits<float>::infinity()) == 0.60f
                && rl::detail::assisted_action_history_new_weight(
                    true, std::numeric_limits<float>::quiet_NaN()) == 0.60f,
            "Human Uneven action-history smoothing did not bound adversarial authority");
        for (const float authority : authorities)
        {
            require(rl::detail::assisted_action_history_new_weight(
                        false, authority) == 0.60f,
                "Human history convergence leaked into another species, stage, or gait");
        }
    }

    void test_raw_handoff_evaluation_cadence()
    {
        namespace rl = runner::rl;
        namespace sim = runner::sim;
        constexpr std::uint64_t initial_evaluations = 37u;
        constexpr std::uint64_t samples_per_environment = 128u;
        const sim::CreatureBlueprint human = sim::CreatureBlueprint::humanoid();
        const std::uint64_t handoff =
            rl::foundational_walk_teacher_handoff_update(human);

        rl::PpoTrainer first{ human, 8u, true, 1u };
        rl::PpoTrainer repeated{ human, 8u, true, 1u };
        first.reset_policy(0x751190u, true);
        repeated.reset_policy(0x751190u, true);
        const rl::EvaluationCadenceEvidence evidence =
            rl::RawLearningTestAccess::
                human_walk_handoff_evaluation_cadence(first);
        const rl::EvaluationCadenceEvidence repeated_evidence =
            rl::RawLearningTestAccess::
                human_walk_handoff_evaluation_cadence(repeated);
        require(evidence == repeated_evidence,
            "Raw handoff evaluation cadence was not repeated-seed deterministic");
        require(evidence.initial_evaluation_count == initial_evaluations,
            "Raw handoff cadence fixture lost its evaluation baseline");
        for (std::size_t index = 0u; index < evidence.points.size(); ++index)
        {
            const rl::EvaluationCadencePoint& point = evidence.points[index];
            const bool first_post_handoff_evaluation =
                index + 1u == evidence.points.size();
            const std::uint64_t expected_evaluations =
                initial_evaluations
                + (first_post_handoff_evaluation ? 1u : 0u);
            require(point.update == handoff + index
                    && point.lesson_update == handoff + index
                    && point.evaluation_count == expected_evaluations
                    && point.handoff_reset_complete,
                "Human Walk evaluated at its reset boundary or lost the ordinary update-905 cadence");
            const std::uint64_t expected_raw_samples = index == 0u
                ? 7u * samples_per_environment
                : 8u * samples_per_environment;
            require(point.raw_actor_samples == expected_raw_samples
                    && point.assisted_actor_samples == 0u
                    && point.receipt_present
                        == first_post_handoff_evaluation,
                "Raw handoff cadence did not cross the real mixed/whole-Raw optimizer path");
        }

        rl::PpoTrainer quadruped{
            sim::CreatureBlueprint::quadruped(), 8u, true, 1u };
        quadruped.reset_policy(0x751191u, true);
        const rl::EvaluationCadencePoint non_human =
            rl::RawLearningTestAccess::one_update_evaluation_cadence(
                quadruped, sim::CourseStage::uneven,
                sim::GaitTask::walk, 899u);
        require(non_human.update == 900u
                && non_human.evaluation_count
                    == initial_evaluations + 1u
                && non_human.raw_actor_samples == 0u
                && non_human.assisted_actor_samples
                    == 8u * samples_per_environment,
            "non-Human update 900 incorrectly inherited the Human Raw handoff skip");

        rl::PpoTrainer speed_walk{ human, 8u, true, 1u };
        speed_walk.reset_policy(0x751192u, true);
        const rl::EvaluationCadencePoint non_walk =
            rl::RawLearningTestAccess::one_update_evaluation_cadence(
                speed_walk, sim::CourseStage::uneven,
                sim::GaitTask::speed_walk, handoff - 1u);
        require(non_walk.update == 900u
                && non_walk.lesson_update == handoff
                && non_walk.evaluation_count
                    == initial_evaluations + 1u
                && non_walk.raw_actor_samples == 0u
                && non_walk.assisted_actor_samples
                    == 8u * samples_per_environment,
            "Human non-Walk update 900 incorrectly inherited the Walk handoff skip");
    }

    [[nodiscard]] std::vector<char> read_bytes(
        const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return { std::istreambuf_iterator<char>{ input },
            std::istreambuf_iterator<char>{} };
    }

    void write_bytes(const std::filesystem::path& path,
        const std::vector<char>& bytes)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(output), "could not write corruption fixture");
    }

    void test_eppo51_and_integration_provenance()
    {
        namespace integration = runner::integration;
        namespace rl = runner::rl;
        namespace sim = runner::sim;
        const sim::CreatureBlueprint rig = sim::CreatureBlueprint::humanoid();
        rl::PpoTrainer trainer{ rig, 8u, true, 1u };
        const rl::PpoTrainer::VerifiedCheckpoint raw_artifact =
            rl::RawLearningTestAccess::one_real_raw_update(trainer);
        require(raw_artifact.valid(),
            "real Raw staged PPO update did not issue an artifact");
        const rl::PpoTrainer::CheckpointData& raw = raw_artifact.data();
        require(raw.raw_ppo_receipt.present()
                && raw.raw_ppo_receipt.origin_counts()[static_cast<std::size_t>(
                    rl::RolloutOrigin::raw_policy)]
                    == raw.raw_ppo_receipt.expected_sample_count()
                && raw.raw_ppo_receipt.self_imitation_updates() == 0u
                && raw.raw_ppo_receipt.best_anchor_updates() == 0u
                && raw.raw_ppo_receipt.elapsed_phase_samples() == 0u
                && raw.raw_ppo_receipt.forward_intent_samples() == 384u
                && raw.raw_ppo_receipt.reverse_intent_samples() == 384u
                && raw.raw_ppo_receipt.stop_intent_samples() == 256u
                && raw.raw_ppo_receipt.environments_reset_at_handoff()
                && raw.raw_ppo_receipt.actions_reset_at_handoff()
                && raw.raw_ppo_receipt.episodes_reset_at_handoff()
                && raw.raw_ppo_receipt.whole_cohort_raw()
                && raw.raw_ppo_receipt.active_actor_before_digest()
                    != raw.raw_ppo_receipt.active_actor_after_digest(),
            "trainer-issued receipt did not preserve full Raw evidence");

        const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / "runner-raw-learning-provenance";
        std::error_code filesystem_error{};
        std::filesystem::remove_all(directory, filesystem_error);
        std::filesystem::create_directories(directory, filesystem_error);
        require(!filesystem_error, "could not create provenance test directory");
        const std::filesystem::path checkpoint = directory / "valid.eppo";
        std::string error{};
        require(rl::PpoTrainer::write_checkpoint_data(raw, checkpoint, error),
            "valid EPPO51 checkpoint was not written");

        rl::PpoTrainer::VerifiedCheckpoint loaded_artifact{};
        require(rl::PpoTrainer::read_verified_checkpoint(
                    checkpoint, loaded_artifact, error)
                && loaded_artifact.valid()
                && loaded_artifact.data().parameters_digest
                    == rl::parameter_digest(loaded_artifact.data().parameters)
                && loaded_artifact.data().best_parameters_digest
                    == rl::parameter_digest(
                        loaded_artifact.data().best_parameters),
            "EPPO51 checkpoint provenance did not round-trip");
        rl::PpoTrainer resumed{ rig, 8u, false };
        require(resumed.apply_verified_checkpoint(loaded_artifact, error),
            "verified EPPO51 checkpoint did not resume");

        rl::PpoTrainer context_trainer{ rig, 2u, false };
        context_trainer.set_course(
            sim::CourseStage::equipment_targets, 0.40f, false);
        context_trainer.set_gait_task(sim::GaitTask::run, false);
        context_trainer.set_equipment_directive(
            sim::EquipmentDirective::fire_and_correct);
        const rl::PpoTrainer::VerifiedCheckpoint context_artifact =
            rl::RawLearningTestAccess::one_real_assisted_update(context_trainer);
        require(context_artifact.valid(),
            "trainer did not issue nondefault execution-context artifact");
        rl::PpoTrainer::CheckpointData context_data = context_artifact.data();
        context_data.metrics.evaluation_max_backward_brace_seconds = 0.375f;
        context_data = rl::RawLearningTestAccess::reseal(
            std::move(context_data));
        const std::filesystem::path context_path = directory / "context.eppo";
        const rl::PpoTrainer::CheckpointData logical_copy = context_data;
        require(rl::PpoTrainer::write_checkpoint_data(
                    logical_copy, context_path, error),
            "logical checkpoint copy lost its portable artifact seal");
        rl::PpoTrainer::VerifiedCheckpoint loaded_context{};
        require(rl::PpoTrainer::read_verified_checkpoint(
                    context_path, loaded_context, error)
                && loaded_context.data().stage
                    == sim::CourseStage::equipment_targets
                && loaded_context.data().gait_task == sim::GaitTask::run
                && loaded_context.data().equipment_directive
                    == sim::EquipmentDirective::fire_and_correct
                && loaded_context.data().metrics
                    .evaluation_max_backward_brace_seconds == 0.375f,
            "EPPO51 lost execution context or backward-brace evidence");
        rl::PpoTrainer context_resumed{ rig, 2u, false };
        context_resumed.set_course(sim::CourseStage::uneven, 0.30f, false);
        context_resumed.set_gait_task(sim::GaitTask::walk, false);
        context_resumed.set_equipment_directive(
            sim::EquipmentDirective::passive);
        require(context_resumed.apply_verified_checkpoint(loaded_context, error)
                && context_resumed.course_stage()
                    == sim::CourseStage::equipment_targets
                && context_resumed.gait_task() == sim::GaitTask::run
                && context_resumed.equipment_directive()
                    == sim::EquipmentDirective::fire_and_correct
                && context_resumed.metrics()
                    .evaluation_max_backward_brace_seconds == 0.375f,
            "resume leaked stale gait/equipment/metric context");

        for (const std::uint32_t old_semantics : std::array<std::uint32_t, 6>{
                0x0007'5109u, 0x0007'5110u, 0x0007'5111u,
                0x0007'5112u, 0x0007'5113u, 0x0007'5114u })
        {
            rl::PpoTrainer::CheckpointData transfer_only = context_data;
            transfer_only.training_semantics = old_semantics;
            transfer_only = rl::RawLearningTestAccess::reseal(
                std::move(transfer_only));
            rl::PpoTrainer v0751_resume{ rig, 2u, false };
            require(!v0751_resume.apply_checkpoint_data(
                        transfer_only, error, false)
                    && v0751_resume.apply_checkpoint_data(
                        std::move(transfer_only), error, true),
                "historical weights silently resumed under current load-qualified semantics");
        }

        sim::Environment environment{ rig, 0x75130002u };
        environment.set_course(raw.stage, raw.difficulty);
        environment.set_gait_task(raw.gait_task);
        environment.set_equipment_directive(raw.equipment_directive);
        integration::RetainedPolicyController controller{};
        require(controller.load_retained_best(loaded_artifact, error)
                && controller.valid_for(environment,
                    sim::GuidanceMode::raw_policy_audit)
                && controller.provenance().schema
                    == integration::retained_policy_provenance_schema_version
                && controller.provenance().raw_receipt_verified,
            "integration rejected a verified Raw retained policy");
        environment.set_course(sim::CourseStage::equipment_targets, 0.30f);
        require(rl::action_mask_bits(rl::action_mask_for(
                    environment, environment.course_stage()))
                    != raw.raw_ppo_receipt.action_mask_bits()
                && !controller.valid_for(environment,
                    sim::GuidanceMode::raw_policy_audit),
            "Raw controller ignored stage/action-mask context mismatch");
        environment.set_course(raw.stage, raw.difficulty);
        environment.set_gait_task(sim::GaitTask::run);
        require(!controller.valid_for(environment,
                sim::GuidanceMode::raw_policy_audit),
            "Raw controller ignored gait context mismatch");
        environment.set_gait_task(raw.gait_task);
        environment.set_equipment_directive(
            sim::EquipmentDirective::safe_carry_walk);
        require(!controller.valid_for(environment,
                sim::GuidanceMode::raw_policy_audit),
            "Raw controller ignored equipment context mismatch");
        environment.set_equipment_directive(raw.equipment_directive);
        require(controller.valid_for(environment,
                sim::GuidanceMode::raw_policy_audit),
            "Raw controller did not recover in its receipt-bound context");

        std::vector<char> bytes = read_bytes(checkpoint);
        require(bytes.size() > 32u, "EPPO51 fixture is unexpectedly small");
        std::vector<char> corrupt = bytes;
        corrupt[corrupt.size() / 2u] ^= static_cast<char>(0x01);
        const std::filesystem::path bitflip = directory / "bitflip.eppo";
        write_bytes(bitflip, corrupt);
        rl::PpoTrainer::CheckpointData loaded{};
        require(!rl::PpoTrainer::read_checkpoint_data(bitflip, loaded, error),
            "bit-flipped EPPO51 checkpoint was accepted");

        corrupt = bytes;
        corrupt.pop_back();
        const std::filesystem::path truncated = directory / "truncated.eppo";
        write_bytes(truncated, corrupt);
        require(!rl::PpoTrainer::read_checkpoint_data(truncated, loaded, error),
            "truncated EPPO51 checkpoint was accepted");

        corrupt = bytes;
        corrupt.push_back('X');
        const std::filesystem::path appended = directory / "appended.eppo";
        write_bytes(appended, corrupt);
        require(!rl::PpoTrainer::read_checkpoint_data(appended, loaded, error),
            "EPPO51 checkpoint with appended content was accepted");

        corrupt = bytes;
        constexpr std::array<char, 8> nonexistent_magic{
            'E', 'P', 'P', 'O', '4', '5', '\0', '\1' };
        std::copy(nonexistent_magic.begin(), nonexistent_magic.end(), corrupt.begin());
        const std::filesystem::path nonexistent = directory / "eppo45.eppo";
        write_bytes(nonexistent, corrupt);
        require(!rl::PpoTrainer::read_checkpoint_data(nonexistent, loaded, error),
            "fabricated EPPO45 format was accepted");

        rl::PpoTrainer::CheckpointData nonfinite = raw;
        nonfinite.parameters[0] = std::numeric_limits<float>::quiet_NaN();
        nonfinite = rl::RawLearningTestAccess::reseal(std::move(nonfinite));
        require(!rl::PpoTrainer::write_checkpoint_data(
                nonfinite, directory / "nonfinite.eppo", error),
            "non-finite checkpoint parameters were written");

        rl::PpoTrainer::CheckpointData exhausted_counter = raw;
        exhausted_counter.optimizer_step =
            std::numeric_limits<std::uint64_t>::max();
        exhausted_counter = rl::RawLearningTestAccess::reseal(
            std::move(exhausted_counter));
        require(!rl::PpoTrainer::write_checkpoint_data(
                exhausted_counter, directory / "counter.eppo", error),
            "overflow-sentinel checkpoint counter was written");

        rl::PpoTrainer::CheckpointData invalid_context = raw;
        invalid_context.gait_task = static_cast<sim::GaitTask>(
            sim::gait_task_count);
        invalid_context = rl::RawLearningTestAccess::reseal(
            std::move(invalid_context));
        require(!rl::PpoTrainer::write_checkpoint_data(
                invalid_context, directory / "enum.eppo", error),
            "out-of-range checkpoint context enum was written");

        rl::PpoTrainer::CheckpointData invalid_rng = raw;
        invalid_rng.random_state = 0u;
        invalid_rng = rl::RawLearningTestAccess::reseal(
            std::move(invalid_rng));
        require(!rl::PpoTrainer::write_checkpoint_data(
                invalid_rng, directory / "rng.eppo", error),
            "zero checkpoint RNG state was written");

        const auto rejects_metrics = [&](rl::PpoTrainer::CheckpointData invalid,
            const char* filename) -> bool
        {
            invalid = rl::RawLearningTestAccess::reseal(std::move(invalid));
            return !rl::PpoTrainer::write_checkpoint_data(
                invalid, directory / filename, error);
        };
        rl::PpoTrainer::CheckpointData invalid_metrics = raw;
        invalid_metrics.metrics.total_distance = -1.0;
        require(rejects_metrics(std::move(invalid_metrics), "negative-total.eppo"),
            "negative lifetime distance was written");
        invalid_metrics = raw;
        invalid_metrics.metrics.evaluation_collisions = -1.0f;
        require(rejects_metrics(std::move(invalid_metrics), "negative-count.eppo"),
            "negative count-valued evaluation evidence was written");
        invalid_metrics = raw;
        invalid_metrics.metrics.evaluation_airborne_ratio = 1.01f;
        require(rejects_metrics(std::move(invalid_metrics), "ratio.eppo"),
            "out-of-range airborne ratio was written");
        invalid_metrics = raw;
        invalid_metrics.metrics.imitation_samples = 0u;
        invalid_metrics.metrics.imitation_weight = 0.10f;
        require(rejects_metrics(std::move(invalid_metrics), "imitation.eppo"),
            "stale imitation weight without samples was written");
        invalid_metrics = raw;
        invalid_metrics.metrics.evaluation_valid = true;
        invalid_metrics.metrics.evaluation_invalid_reason =
            sim::InvalidMotion::collapsed_posture;
        invalid_metrics.metrics.evaluation_rejection_mask = 0u;
        require(rejects_metrics(std::move(invalid_metrics), "validity.eppo"),
            "contradictory evaluation validity was written");
        invalid_metrics = raw;
        invalid_metrics.metrics.total_episodes = 2u;
        invalid_metrics.metrics.total_valid_episodes = 2u;
        invalid_metrics.metrics.total_invalid_episodes = 1u;
        require(rejects_metrics(std::move(invalid_metrics), "episodes.eppo"),
            "contradictory episode totals were written");
        invalid_metrics = raw;
        invalid_metrics.metrics.update =
            invalid_metrics.metrics.total_updates + 1u;
        require(rejects_metrics(std::move(invalid_metrics), "updates.eppo"),
            "lesson update greater than lifetime updates was written");
        invalid_metrics = raw;
        invalid_metrics.metrics.evaluation_shots = 1.0f;
        invalid_metrics.metrics.evaluation_target_hits = 2.0f;
        require(rejects_metrics(std::move(invalid_metrics), "hits.eppo"),
            "target hits greater than shots were written");

        rl::PpoTrainer::CheckpointData fabricated{};
        require(!rl::PpoTrainer::write_checkpoint_data(
                fabricated, directory / "fabricated.eppo", error),
            "ordinary caller manufactured a trusted checkpoint artifact");

        const std::filesystem::path legacy_path = directory / "legacy-eppo44.eppo";
        require(rl::RawLearningTestAccess::write_legacy_eppo44(
                    legacy_path, trainer.checkpoint_data()),
            "could not write authentic EPPO44 grammar fixture");
        rl::PpoTrainer::CheckpointData legacy{};
        require(rl::PpoTrainer::read_checkpoint_data(
                    legacy_path, legacy, error),
            "authentic EPPO44 checkpoint was not recognized");
        rl::PpoTrainer::VerifiedCheckpoint legacy_artifact{};
        require(!rl::PpoTrainer::read_verified_checkpoint(
                    legacy_path, legacy_artifact, error)
                && !legacy_artifact.valid(),
            "legacy EPPO44 storage became a verified artifact");
        rl::PpoTrainer legacy_resume{ rig, 2u, false };
        require(!legacy_resume.apply_checkpoint_data(legacy, error, false)
                && legacy_resume.apply_checkpoint_data(legacy, error, true),
            "EPPO44 bypassed explicit unverified weight transfer");
        std::vector<char> legacy_bytes = read_bytes(legacy_path);
        legacy_bytes.push_back('X');
        const std::filesystem::path legacy_appended =
            directory / "legacy-appended.eppo";
        write_bytes(legacy_appended, legacy_bytes);
        require(!rl::PpoTrainer::read_checkpoint_data(
                legacy_appended, legacy, error),
            "EPPO44 checkpoint with appended content was accepted");

        integration::ControlAuthorityReport old_schema{};
        old_schema.retained_policy = controller.provenance();
        old_schema.retained_policy.schema = 2u;
        require(!integration::valid_control_authority_report(old_schema),
            "old retained-policy provenance schema remained valid");
        std::filesystem::remove_all(directory, filesystem_error);
    }
}

int main()
{
    try
    {
        static_assert(runner::rl::training_semantics_version == 0x0007'5115u);
        static_assert(runner::integration::api_version == 7u);
        static_assert(runner::integration::canonical_snapshot_schema_version
            == 5u);
        static_assert(
            runner::integration::retained_policy_provenance_schema_version
            == 3u);
        test_raw_candidate_evaluation_report();
        test_raw_observation_scrub();
        test_raw_player_intent_curriculum();
        test_foundational_mixed_rollout_schedule();
        test_assisted_action_history_authority();
        test_raw_handoff_evaluation_cadence();
        test_eppo51_and_integration_provenance();
        std::cout << "Raw learning provenance tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Raw learning provenance tests failed: "
            << error.what() << '\n';
        return 1;
    }
}
