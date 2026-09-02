#include "rig_training_diagnostic.hpp"

#include "ppo.hpp"

#include <algorithm>
#include <array>
#include <future>
#include <cmath>
#include <optional>

namespace runner::diagnostics
{
    namespace
    {
        struct RigCase
        {
            std::string_view name{};
            sim::CreatureBlueprint blueprint{};
        };

        struct RawPolicyOutcome
        {
            float distance{};
            float stride_events{};
            float shuttle_turns{};
            std::uint32_t rejection_mask{};
            std::uint32_t invalid_runs{};
            sim::InvalidMotion invalid_reason{ sim::InvalidMotion::none };
        };

        [[nodiscard]] float release_distance(
            const sim::CreatureBlueprint& blueprint) noexcept
        {
            if (blueprint.monopedal_gait())
                return 5.0f;
            if (blueprint.avian_gait())
                return 10.0f;
            if (blueprint.presentation_species() == sim::CreatureSpecies::human
                && blueprint.paired_leg_chains())
                return rl::gait_task_mastery_distance(sim::GaitTask::walk);
            return blueprint.paired_leg_chains()
                ? rl::walk_mastery_distance
                : rl::multi_support_release_distance(blueprint);
        }

        [[nodiscard]] float release_gait_cycles(
            const sim::CreatureBlueprint& blueprint) noexcept
        {
            if (blueprint.monopedal_gait())
                return 8.0f;
            if (blueprint.avian_gait())
                return 14.0f;
            if (blueprint.presentation_species() == sim::CreatureSpecies::human
                && blueprint.paired_leg_chains())
                return rl::gait_task_mastery_stride_events(sim::GaitTask::walk);
            return blueprint.paired_leg_chains()
                ? rl::walk_mastery_stride_events
                : rl::multi_support_release_stride_events(blueprint);
        }
        [[nodiscard]] float teacher_release_distance(
            const sim::CreatureBlueprint& blueprint) noexcept
        {
            if (blueprint.presentation_species() == sim::CreatureSpecies::human
                && blueprint.paired_leg_chains())
                return rl::casual_walk_teacher_distance;
            return release_distance(blueprint);
        }

        [[nodiscard]] float teacher_release_gait_cycles(
            const sim::CreatureBlueprint& blueprint) noexcept
        {
            if (blueprint.presentation_species() == sim::CreatureSpecies::human
                && blueprint.paired_leg_chains())
                return rl::casual_walk_teacher_stride_events;
            return release_gait_cycles(blueprint);
        }

        [[nodiscard]] RawPolicyOutcome evaluate_policy(
            const sim::CreatureBlueprint& blueprint,
            std::span<const float> parameters, bool production_controller,
            sim::CourseStage stage = sim::CourseStage::uneven)
        {
            constexpr std::size_t evaluation_agents = 6u;
            rl::PolicyNetwork policy{ 0x7300u };
            policy.parameters().assign(parameters.begin(), parameters.end());

            RawPolicyOutcome outcome{};
            for (std::size_t agent = 0; agent < evaluation_agents; ++agent)
            {
                sim::Environment environment{ blueprint,
                    0xE000u + static_cast<std::uint64_t>(agent) * 4099u };
                environment.set_course(stage, 0.30f);
                environment.set_course_motion_enabled(false);
                environment.set_guidance_mode(production_controller
                    ? sim::GuidanceMode::assisted : sim::GuidanceMode::raw_policy_audit);
                const int maximum_steps = stage == sim::CourseStage::shuttle
                    || static_cast<std::uint8_t>(stage)
                        >= static_cast<std::uint8_t>(sim::CourseStage::hurdles)
                    ? 2400 : 1200;
                for (int step = 0; step < maximum_steps; ++step)
                {
                    const auto raw_action = policy.deterministic_action(
                        environment.observation());
                    const auto action = production_controller
                        ? rl::effective_policy_action(environment, raw_action,
                            stage, 0.0f)
                        : raw_action;
                    if (environment.step(action).terminated)
                        break;
                }
                const rl::StageMotionQualification qualification =
                    rl::stage_motion_qualification(stage, environment);
                if (!qualification.valid || !environment.body_integrity_valid())
                {
                    ++outcome.invalid_runs;
                    outcome.rejection_mask |= qualification.rejection_mask;
                    if (!environment.body_integrity_valid())
                        outcome.rejection_mask |= rl::evidence_bit(
                            rl::MotionEvidenceFailure::invalid_motion);
                    if (outcome.invalid_reason == sim::InvalidMotion::none
                        && environment.invalid_reason() != sim::InvalidMotion::none)
                        outcome.invalid_reason = environment.invalid_reason();
                }
                outcome.distance += environment.distance_travelled();
                outcome.stride_events += static_cast<float>(
                    environment.gait_cycles());
                outcome.shuttle_turns += static_cast<float>(
                    environment.completed_shuttle_turns());
            }
            constexpr float inverse_agents = 1.0f
                / static_cast<float>(evaluation_agents);
            outcome.distance *= inverse_agents;
            outcome.stride_events *= inverse_agents;
            outcome.shuttle_turns *= inverse_agents;
            return outcome;
        }
    }

    bool retained_policy_release_eligible(const RigTrainingResult& result,
        const sim::CreatureBlueprint& blueprint) noexcept
    {
        return result.retained_policy
            && rl::strict_evaluation_quality(result.retained_quality)
            && result.teacher_authority == 0.0f
            && result.retained_update
                >= rl::foundational_walk_teacher_handoff_update(blueprint)
            && std::isfinite(result.retained_probe_distance)
            && std::isfinite(result.retained_probe_stride_events)
            && result.retained_probe_distance >= release_distance(blueprint)
            && result.retained_probe_stride_events
                >= release_gait_cycles(blueprint)
            && result.retained_probe_rejection_mask == 0u
            && result.retained_probe_invalid_runs == 0u
            && result.retained_probe_invalid_reason == sim::InvalidMotion::none
            && !result.rollout_course_motion_enabled
            && result.preview_resets <= 24u;
    }

    RigTrainingResult run_rig_training_case(std::string_view name,
        const sim::CreatureBlueprint& blueprint, std::uint64_t updates)
    {
        updates = std::max<std::uint64_t>(1u, updates);
        sim::Environment teacher{ blueprint, 0x7300u };
        teacher.set_course(sim::CourseStage::uneven, 0.30f);
        teacher.set_course_motion_enabled(false);
        for (int step = 0; step < 1200; ++step)
        {
            const auto action = rl::walking_teacher_action(teacher);
            if (teacher.step(action).terminated)
                break;
        }

        // Human owns two dependent 1,200-update lessons, so let its eight
        // deterministic rollout environments run concurrently. The explicit
        // two-core constructor contract is exercised separately below this
        // aggregate release proof; the other species retain that ceiling.
        const std::size_t diagnostic_workers = name == "human" ? 8u : 2u;
        rl::PpoTrainer trainer{ blueprint, 8u, true, diagnostic_workers };
        trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
        for (std::uint64_t update = 0; update < updates; ++update)
        {
            trainer.train_one_update();
            trainer.step_preview();
        }

        const rl::TrainingMetrics& metrics = trainer.metrics();
        const RawPolicyOutcome raw = evaluate_policy(
            blueprint, trainer.policy().parameters(), false);
        const RawPolicyOutcome retained = trainer.has_best_policy()
            ? evaluate_policy(blueprint,
                trainer.best_policy_parameters(), true)
            : RawPolicyOutcome{};
        bool course_motion_enabled{};
        for (const sim::Environment& environment : trainer.environments())
            course_motion_enabled = course_motion_enabled
                || environment.course_motion_enabled();
        RigTrainingResult result{
            name,
            metrics.mean_episode_distance,
            teacher.distance_travelled(),
            static_cast<float>(teacher.gait_cycles()),
            teacher.elapsed_seconds(),
            teacher.invalid_reason(),
            metrics.evaluation_distance,
            metrics.evaluation_stride_events,
            metrics.evaluation_rejection_mask,
            metrics.evaluation_invalid_runs,
            metrics.evaluation_invalid_reason,
            raw.distance,
            raw.stride_events,
            raw.rejection_mask,
            raw.invalid_runs,
            raw.invalid_reason,
            rl::foundational_walk_teacher_authority(
                trainer.lesson_update(), blueprint),
            trainer.has_best_policy(),
            metrics.best_update,
            metrics.best_evaluation_distance,
            metrics.best_quality_key,
            retained.distance,
            retained.stride_events,
            retained.rejection_mask,
            retained.invalid_runs,
            retained.invalid_reason,
            trainer.preview_reset_count(),
            trainer.preview_last_reset_reason(),
            trainer.maximum_worker_count(),
            course_motion_enabled
        };
        if (name == "human")
        {
            // Reproduce the real lesson boundary: keep the learned forward-walk
            // network, train the appended shuttle skill, then discard any
            // pre-handoff champion and replay the retained production controller
            // with curriculum authority fully removed.
            trainer.set_course(sim::CourseStage::shuttle, 0.30f, false);
            for (std::uint64_t update = 0; update < updates; ++update)
                trainer.train_one_update();

            const rl::TrainingMetrics& shuttle_metrics = trainer.metrics();
            const RawPolicyOutcome shuttle = trainer.has_best_policy()
                ? evaluate_policy(blueprint, trainer.best_policy_parameters(),
                    true, sim::CourseStage::shuttle)
                : RawPolicyOutcome{};
            result.shuttle_lesson_updates = trainer.lesson_update();
            result.shuttle_teacher_authority = rl::lesson_teacher_authority(
                trainer.lesson_update(), sim::CourseStage::shuttle, blueprint);
            result.shuttle_retained_policy = trainer.has_best_policy();
            result.shuttle_retained_update = shuttle_metrics.best_update;
            result.shuttle_retained_quality = shuttle_metrics.best_quality_key;
            result.shuttle_probe_distance = shuttle.distance;
            result.shuttle_probe_stride_events = shuttle.stride_events;
            result.shuttle_probe_turns = shuttle.shuttle_turns;
            result.shuttle_probe_rejection_mask = shuttle.rejection_mask;
            result.shuttle_probe_invalid_runs = shuttle.invalid_runs;
            result.shuttle_probe_invalid_reason = shuttle.invalid_reason;
        }
        return result;
    }

    RigTrainingReport run_rig_training_diagnostic(std::uint64_t updates)
    {
        updates = std::max<std::uint64_t>(1u, updates);
        const std::array cases{
            RigCase{ "human", sim::CreatureBlueprint::humanoid() },
            RigCase{ "chicken", sim::CreatureBlueprint::chicken() },
            RigCase{ "dog", sim::CreatureBlueprint::crawler4() },
            RigCase{ "hexapod", sim::CreatureBlueprint::hexapod() }
        };

        RigTrainingReport report{};
        report.updates = updates;
        std::array<std::future<RigTrainingResult>, 4> cases_in_flight{};
        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            RigCase rig = cases[index];
            cases_in_flight[index] = std::async(std::launch::async,
                [rig = std::move(rig), updates]() mutable
                {
                    return run_rig_training_case(
                        rig.name, rig.blueprint, updates);
                });
        }
        for (std::size_t index = 0; index < cases_in_flight.size(); ++index)
        {
            report.rigs[index] = cases_in_flight[index].get();
        }

        report.passed = updates >= 1200u;
        for (std::size_t index = 0; index < report.rigs.size(); ++index)
        {
            const RigTrainingResult& result = report.rigs[index];
            report.passed = report.passed
                && std::isfinite(result.mean_episode_distance)
                && std::isfinite(result.evaluation_distance)
                && result.teacher_invalid_reason == sim::InvalidMotion::none
                && result.teacher_survival >= 19.9f
                && result.teacher_distance
                    >= teacher_release_distance(cases[index].blueprint)
                && result.teacher_stride_events
                    >= teacher_release_gait_cycles(cases[index].blueprint)
                && result.rollout_workers
                    == (cases[index].name == "human" ? 8u : 2u)
                && retained_policy_release_eligible(
                    result, cases[index].blueprint);
            if (cases[index].name == "human")
            {
                report.passed = report.passed
                    && result.shuttle_lesson_updates >= 1200u
                    && result.shuttle_teacher_authority == 0.0f
                    && result.shuttle_retained_policy
                    && rl::strict_evaluation_quality(
                        result.shuttle_retained_quality)
                    && result.shuttle_probe_distance
                        >= rl::walk_mastery_distance
                    && result.shuttle_probe_stride_events
                        >= rl::walk_mastery_stride_events
                    && result.shuttle_probe_turns >= 1.0f
                    && result.shuttle_probe_rejection_mask == 0u
                    && result.shuttle_probe_invalid_runs == 0u
                    && result.shuttle_probe_invalid_reason
                        == sim::InvalidMotion::none;
            }
        }
        return report;
    }
    static WalkEyeTestProof run_walk_eye_test_candidate(
        std::uint64_t updates, std::uint64_t training_seed)
    {
        constexpr std::size_t evaluation_agents = 6u;
        const sim::CreatureBlueprint blueprint = sim::CreatureBlueprint::humanoid();
        updates = std::max<std::uint64_t>(1u, updates);

        WalkEyeTestProof proof{};
        proof.updates = updates;
        proof.selected_training_seed = training_seed;
        proof.candidate_attempts = 1u;
        rl::PpoTrainer trainer{ blueprint, 8u, true };
        trainer.reset_policy(training_seed, true);
        trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
        for (std::uint64_t update = 0; update < updates; ++update)
            trainer.train_one_update();

        const rl::TrainingMetrics& metrics = trainer.metrics();
        proof.retained_update = metrics.best_update;
        proof.evaluation_count = metrics.evaluation_count;
        proof.evaluation_distance = metrics.evaluation_distance;
        proof.evaluation_stride_events = metrics.evaluation_stride_events;
        proof.evaluation_survival = metrics.evaluation_survival;
        proof.evaluation_quality_key = metrics.evaluation_quality_key;
        proof.evaluation_rejection_mask = metrics.evaluation_rejection_mask;
        proof.evaluation_invalid_runs = metrics.evaluation_invalid_runs;
        proof.evaluation_invalid_reason = metrics.evaluation_invalid_reason;
        proof.evaluation_valid = metrics.evaluation_valid;
        proof.teacher_authority = rl::foundational_walk_teacher_authority(
            trainer.lesson_update(), blueprint);
        if (!trainer.has_best_policy())
            return proof;
        proof.retained_policy_parameters = trainer.best_policy_parameters();

        rl::PolicyNetwork policy{ 0x7300u };
        policy.parameters() = trainer.best_policy_parameters();
        float total_distance{};
        float total_stride_events{};
        bool selected_pose{};
        float selected_final_distance{};
        for (std::size_t agent = 0; agent < evaluation_agents; ++agent)
        {
            const std::uint64_t seed = 0xE000u
                + static_cast<std::uint64_t>(agent) * 4099u;
            sim::Environment environment{ blueprint, seed };
            environment.set_course(sim::CourseStage::uneven, 0.30f);
            environment.set_course_motion_enabled(false);
            std::optional<sim::Environment> first_transfer_pose{};
            for (int step = 0; step < 1200; ++step)
            {
                const auto raw_action = policy.deterministic_action(
                    environment.observation());
                const auto action = rl::effective_policy_action(environment, raw_action,
                    sim::CourseStage::uneven, 0.0f);
                if (environment.step(action).terminated)
                    break;
                const bool left_air = environment.left_foot_phase()
                    == sim::FootContactPhase::airborne;
                const bool right_air = environment.right_foot_phase()
                    == sim::FootContactPhase::airborne;
                if (environment.distance_travelled()
                        >= rl::gait_task_mastery_distance(sim::GaitTask::walk)
                    && environment.alternating_steps() >= 16u
                    && environment.limb_crossings() >= 2u
                    && left_air != right_air)
                {
                    if (!first_transfer_pose.has_value())
                        first_transfer_pose = environment;
                }
            }

            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(sim::CourseStage::uneven, environment);
            const bool strict_valid = qualification.valid
                && environment.body_integrity_valid()
                && environment.course_motion_enabled() == false;
            if (!strict_valid)
            {
                ++proof.retained_invalid_runs;
                proof.retained_rejection_mask |= qualification.rejection_mask;
                if (!environment.body_integrity_valid())
                {
                    proof.retained_rejection_mask |= rl::evidence_bit(
                        rl::MotionEvidenceFailure::invalid_motion);
                }
                if (proof.retained_invalid_reason == sim::InvalidMotion::none
                    && environment.invalid_reason() != sim::InvalidMotion::none)
                {
                    proof.retained_invalid_reason = environment.invalid_reason();
                }
            }
            else if (first_transfer_pose.has_value()
                && (!selected_pose
                    || environment.distance_travelled() > selected_final_distance))
            {
                proof.environment = *first_transfer_pose;
                proof.selected_seed = static_cast<std::uint32_t>(seed);
                proof.displayed_distance = proof.environment.distance_travelled();
                proof.displayed_steps = proof.environment.alternating_steps();
                proof.displayed_crossings = proof.environment.limb_crossings();
                proof.displayed_max_scissor_seconds =
                    proof.environment.maximum_lower_leg_scissor_seconds();
                selected_final_distance = environment.distance_travelled();
                selected_pose = true;
            }
            total_distance += environment.distance_travelled();
            total_stride_events += static_cast<float>(environment.gait_cycles());
        }

        const RawPolicyOutcome raw = evaluate_policy(blueprint,
            proof.retained_policy_parameters, false);
        proof.raw_policy_distance = raw.distance;
        proof.raw_policy_stride_events = raw.stride_events;
        proof.raw_policy_rejection_mask = raw.rejection_mask;
        proof.raw_policy_invalid_runs = raw.invalid_runs;
        proof.raw_policy_invalid_reason = raw.invalid_reason;

        constexpr float inverse_agents = 1.0f
            / static_cast<float>(evaluation_agents);
        proof.retained_distance = total_distance * inverse_agents;
        proof.retained_stride_events = total_stride_events * inverse_agents;
        proof.passed = updates >= 1200u
            && proof.teacher_authority == 0.0f
            && proof.retained_update
                >= rl::foundational_walk_teacher_handoff_update(blueprint)
            && rl::strict_evaluation_quality(metrics.best_quality_key)
            && proof.retained_distance
                >= rl::gait_task_mastery_distance(sim::GaitTask::walk)
            && proof.retained_stride_events
                >= rl::gait_task_mastery_stride_events(sim::GaitTask::walk)
            && proof.retained_invalid_runs == 0u
            && proof.raw_policy_distance
                >= rl::gait_task_mastery_distance(sim::GaitTask::walk)
            && proof.raw_policy_stride_events
                >= rl::gait_task_mastery_stride_events(sim::GaitTask::walk)
            && proof.raw_policy_invalid_runs == 0u
            && proof.raw_policy_rejection_mask == 0u
            && proof.raw_policy_invalid_reason == sim::InvalidMotion::none
            && selected_pose;
        return proof;
    }

    WalkEyeTestProof run_walk_eye_test_proof(std::uint64_t updates)
    {
        WalkEyeTestProof best_failure{};
        bool have_failure{};
        std::uint32_t attempts{};
        for (const std::uint64_t seed : walk_eye_candidate_seeds)
        {
            WalkEyeTestProof candidate = run_walk_eye_test_candidate(updates, seed);
            candidate.candidate_attempts = ++attempts;
            if (candidate.passed)
                return candidate;
            const bool better = !have_failure
                || candidate.raw_policy_invalid_runs < best_failure.raw_policy_invalid_runs
                || (candidate.raw_policy_invalid_runs == best_failure.raw_policy_invalid_runs
                    && candidate.raw_policy_distance > best_failure.raw_policy_distance);
            if (better)
            {
                best_failure = std::move(candidate);
                have_failure = true;
            }
        }
        best_failure.candidate_attempts = attempts;
        return best_failure;
    }

    SpeedWalkGraphProof run_speed_walk_graph_proof(
        std::uint64_t updates_per_lesson)
    {
        updates_per_lesson = std::max<std::uint64_t>(
            1u, updates_per_lesson);
        const sim::CreatureBlueprint blueprint =
            sim::CreatureBlueprint::humanoid();
        rl::PpoTrainer trainer{ blueprint, 8u, true };
        trainer.reset_policy(0x00C0FFEEu, true);
        trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
        for (std::uint64_t update = 0; update < updates_per_lesson; ++update)
            trainer.train_one_update();

        SpeedWalkGraphProof proof{};
        proof.walk_updates = trainer.lesson_update();
        proof.walk_retained_update = trainer.metrics().best_update;
        proof.walk_retained = trainer.has_best_policy()
            && rl::strict_evaluation_quality(
                trainer.metrics().best_quality_key)
            && trainer.restore_best_policy();
        if (!proof.walk_retained)
            return proof;

        trainer.set_gait_task(sim::GaitTask::speed_walk, false);
        for (std::uint64_t update = 0; update < updates_per_lesson; ++update)
            trainer.train_one_update();
        proof.speed_walk_updates = trainer.lesson_update();
        proof.speed_walk_retained_update = trainer.metrics().best_update;
        proof.speed_walk_retained_distance =
            trainer.metrics().best_evaluation_distance;
        const std::uint64_t retained_evaluation_sequence =
            std::max<std::uint64_t>(1u, trainer.metrics().evaluation_count);
        proof.speed_walk_retained = trainer.has_best_policy()
            && rl::strict_evaluation_quality(trainer.metrics().best_quality_key)
            && trainer.evaluate_retained_policy();
        const rl::TrainingMetrics& metrics = trainer.metrics();
        proof.distance = metrics.evaluation_distance;
        proof.stride_events = metrics.evaluation_stride_events;
        proof.speed = metrics.evaluation_speed;
        proof.survival = metrics.evaluation_survival;
        proof.collisions = metrics.evaluation_collisions;
        proof.invalid_runs = metrics.evaluation_invalid_runs;
        proof.rejection_mask = metrics.evaluation_rejection_mask;
        proof.teacher_authority = rl::foundational_walk_teacher_authority(
            trainer.lesson_update(), blueprint);
        proof.raw_evaluation = rl::foundational_walk_uses_raw_evaluation(
            trainer.lesson_update(), trainer.course_stage(), blueprint);
        for (const sim::Environment& environment : trainer.environments())
            proof.course_motion_enabled = proof.course_motion_enabled
                || environment.course_motion_enabled();
        for (std::size_t agent = 0; agent < 6u; ++agent)
        {
            const std::uint64_t seed = rl::evaluation_seed(
                agent, retained_evaluation_sequence);
            sim::Environment environment{ blueprint, seed };
            rl::configure_policy_evaluation_environment(environment,
                sim::CourseStage::uneven, 0.30f, sim::GaitTask::speed_walk,
                sim::GuidanceMode::raw_policy_audit);
            environment.set_equipment_directive(trainer.equipment_directive());
            for (int step = 0; step < 1200; ++step)
            {
                const auto raw_action = trainer.policy().deterministic_action(
                    environment.observation());
                const auto action = rl::effective_policy_action(environment,
                    raw_action, sim::CourseStage::uneven, 0.0f,
                    sim::GuidanceMode::raw_policy_audit);
                if (environment.step(action).terminated)
                    break;
            }
            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(
                    sim::CourseStage::uneven, environment);
            if ((!qualification.valid || !environment.body_integrity_valid())
                && proof.failed_seed == 0u)
            {
                proof.failed_seed = seed;
                proof.failed_distance = environment.distance_travelled();
                proof.failed_survival = environment.elapsed_seconds();
                proof.failed_reason = environment.invalid_reason();
            }
        }
        proof.passed = updates_per_lesson >= 1200u
            && proof.walk_retained
            && proof.walk_retained_update
                >= rl::foundational_walk_teacher_handoff_update(blueprint)
            && proof.speed_walk_retained
            && proof.speed_walk_retained_update
                >= rl::foundational_walk_teacher_handoff_update(blueprint)
            && proof.teacher_authority == 0.0f
            && proof.raw_evaluation && !proof.course_motion_enabled
            && metrics.evaluation_valid
            && metrics.evaluation_quality_key != 0u
            && metrics.evaluation_distance
                >= rl::gait_task_mastery_distance(sim::GaitTask::speed_walk)
            && metrics.evaluation_stride_events
                >= rl::gait_task_mastery_stride_events(sim::GaitTask::speed_walk)
            && metrics.evaluation_speed
                >= rl::gait_task_mastery_speed(sim::GaitTask::speed_walk)
            && metrics.evaluation_survival >= 18.0f
            && metrics.evaluation_collisions <= 1.0f;
        return proof;
    }
}
