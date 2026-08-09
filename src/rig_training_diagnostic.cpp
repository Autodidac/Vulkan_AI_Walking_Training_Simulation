#include "rig_training_diagnostic.hpp"

#include "ppo.hpp"

#include <algorithm>
#include <array>
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
            std::uint32_t rejection_mask{};
            std::uint32_t invalid_runs{};
            sim::InvalidMotion invalid_reason{ sim::InvalidMotion::none };
        };

        [[nodiscard]] RawPolicyOutcome evaluate_policy(
            const sim::CreatureBlueprint& blueprint,
            std::span<const float> parameters, bool production_controller)
        {
            constexpr std::size_t evaluation_agents = 6u;
            rl::PolicyNetwork policy{ 0x7300u };
            policy.parameters().assign(parameters.begin(), parameters.end());

            RawPolicyOutcome outcome{};
            for (std::size_t agent = 0; agent < evaluation_agents; ++agent)
            {
                sim::Environment environment{ blueprint,
                    0xE000u + static_cast<std::uint64_t>(agent) * 4099u };
                environment.set_course(sim::CourseStage::uneven, 0.30f);
                environment.set_course_motion_enabled(false);
                for (int step = 0; step < 1200; ++step)
                {
                    const auto raw_action = policy.deterministic_action(
                        environment.observation());
                    const auto action = production_controller
                        ? rl::effective_policy_action(environment, raw_action,
                            sim::CourseStage::uneven, 0.0f)
                        : raw_action;
                    if (environment.step(action).terminated)
                        break;
                }
                const rl::StageMotionQualification qualification =
                    rl::stage_motion_qualification(
                        sim::CourseStage::uneven, environment);
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
            }
            constexpr float inverse_agents = 1.0f
                / static_cast<float>(evaluation_agents);
            outcome.distance *= inverse_agents;
            outcome.stride_events *= inverse_agents;
            return outcome;
        }
    }

    RigTrainingReport run_rig_training_diagnostic(std::uint64_t updates)
    {
        updates = std::max<std::uint64_t>(1u, updates);
        const std::array cases{
            RigCase{ "biped", sim::CreatureBlueprint::biped() },
            RigCase{ "humanoid", sim::CreatureBlueprint::humanoid() },
            RigCase{ "quadruped", sim::CreatureBlueprint::quadruped() },
            RigCase{ "crawler4", sim::CreatureBlueprint::crawler4() },
            RigCase{ "hexapod", sim::CreatureBlueprint::hexapod() }
        };

        RigTrainingReport report{};
        report.updates = updates;
        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            const RigCase& rig = cases[index];
            sim::Environment teacher{ rig.blueprint, 0x7300u + index * 4099u };
            teacher.set_course(sim::CourseStage::uneven, 0.30f);
            teacher.set_course_motion_enabled(false);
            for (int step = 0; step < 1200; ++step)
            {
                const auto action = rl::walking_teacher_action(teacher);
                if (teacher.step(action).terminated)
                    break;
            }

            rl::PpoTrainer trainer{ rig.blueprint, 8u, true };
            trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
            for (std::uint64_t update = 0; update < updates; ++update)
            {
                trainer.train_one_update();
                for (std::uint32_t frame = 0; frame < 60u; ++frame)
                    trainer.step_preview();
            }

            const rl::TrainingMetrics& metrics = trainer.metrics();
            const RawPolicyOutcome raw = evaluate_policy(
                rig.blueprint, trainer.policy().parameters(), false);
            const RawPolicyOutcome retained = trainer.has_best_policy()
                ? evaluate_policy(rig.blueprint,
                    trainer.best_policy_parameters(), true)
                : RawPolicyOutcome{};
            bool course_motion_enabled{};
            for (const sim::Environment& environment : trainer.environments())
                course_motion_enabled = course_motion_enabled
                    || environment.course_motion_enabled();
            report.rigs[index] = {
                rig.name,
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
                    metrics.update, rig.blueprint),
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
                course_motion_enabled
            };
        }

        report.passed = updates >= 1200u;
        for (std::size_t index = 0; index < report.rigs.size(); ++index)
        {
            const RigTrainingResult& result = report.rigs[index];
            const bool paired_legs = cases[index].blueprint.paired_leg_chains();
            report.passed = report.passed
                && std::isfinite(result.mean_episode_distance)
                && std::isfinite(result.evaluation_distance)
                && result.teacher_invalid_reason == sim::InvalidMotion::none
                && result.teacher_survival >= 19.9f
                && result.teacher_distance >= 1.0f
                && result.teacher_stride_events >= 3.0f
                && result.retained_policy
                && rl::strict_evaluation_quality(result.retained_quality)
                && result.teacher_authority == 0.0f
                && result.retained_update
                    >= rl::foundational_walk_teacher_handoff_update(
                        cases[index].blueprint)
                && result.retained_probe_distance >= (paired_legs ? rl::walk_mastery_distance : 1.0f)
                && result.retained_probe_stride_events >= (paired_legs
                    ? rl::walk_mastery_stride_events : 12.0f)
                && result.retained_probe_invalid_runs == 0u
                && !result.rollout_course_motion_enabled
                && result.preview_resets <= 24u;
        }
        return report;
    }
    WalkEyeTestProof run_walk_eye_test_proof(std::uint64_t updates)
    {
        constexpr std::size_t evaluation_agents = 6u;
        const sim::CreatureBlueprint blueprint = sim::CreatureBlueprint::humanoid();
        updates = std::max<std::uint64_t>(1u, updates);

        WalkEyeTestProof proof{};
        proof.updates = updates;
        rl::PpoTrainer trainer{ blueprint, 8u, true };
        trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
        for (std::uint64_t update = 0; update < updates; ++update)
            trainer.train_one_update();

        const rl::TrainingMetrics& metrics = trainer.metrics();
        proof.retained_update = metrics.best_update;
        proof.teacher_authority = rl::foundational_walk_teacher_authority(
            metrics.update, blueprint);
        if (!trainer.has_best_policy())
            return proof;

        rl::PolicyNetwork policy{ 0x7300u };
        policy.parameters() = trainer.best_policy_parameters();
        float total_distance{};
        float total_stride_events{};
        bool selected_pose{};
        for (std::size_t agent = 0; agent < evaluation_agents; ++agent)
        {
            const std::uint64_t seed = 0xE000u
                + static_cast<std::uint64_t>(agent) * 4099u;
            sim::Environment environment{ blueprint, seed };
            environment.set_course(sim::CourseStage::uneven, 0.30f);
            environment.set_course_motion_enabled(false);
            std::optional<sim::Environment> latest_transfer_pose{};
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
                if (environment.distance_travelled() >= 18.0f
                    && environment.alternating_steps() >= 16u
                    && environment.limb_crossings() >= 2u
                    && left_air != right_air)
                {
                    latest_transfer_pose = environment;
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
            }
            else if (latest_transfer_pose.has_value()
                && (!selected_pose
                    || latest_transfer_pose->distance_travelled()
                        > proof.displayed_distance))
            {
                proof.environment = *latest_transfer_pose;
                proof.selected_seed = static_cast<std::uint32_t>(seed);
                proof.displayed_distance = proof.environment.distance_travelled();
                proof.displayed_steps = proof.environment.alternating_steps();
                proof.displayed_crossings = proof.environment.limb_crossings();
                selected_pose = true;
            }
            total_distance += environment.distance_travelled();
            total_stride_events += static_cast<float>(environment.gait_cycles());
        }

        constexpr float inverse_agents = 1.0f
            / static_cast<float>(evaluation_agents);
        proof.retained_distance = total_distance * inverse_agents;
        proof.retained_stride_events = total_stride_events * inverse_agents;
        proof.passed = updates >= 1200u
            && proof.teacher_authority == 0.0f
            && proof.retained_update
                >= rl::foundational_walk_teacher_handoff_update(blueprint)
            && rl::strict_evaluation_quality(metrics.best_quality_key)
            && proof.retained_distance >= rl::walk_mastery_distance
            && proof.retained_stride_events >= rl::walk_mastery_stride_events
            && proof.retained_invalid_runs == 0u
            && selected_pose;
        return proof;
    }
}
