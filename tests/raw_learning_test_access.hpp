#pragma once

#include "ppo.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>

namespace runner::rl
{
    namespace detail
    {
        [[nodiscard]] float assisted_action_history_new_weight(
            bool foundational_human_walk,
            float visible_teacher_authority) noexcept;
    }

    struct AssistedIntentRolloutEvidence
    {
        std::uint64_t forward_samples{};
        std::uint64_t reverse_samples{};
        std::uint64_t stop_samples{};
        std::uint64_t observation_mismatches{};
        std::uint64_t elapsed_phase_samples{};
        std::uint64_t teacher_bootstrap_samples{};
        std::uint64_t raw_origin_samples{};
        std::uint64_t optimizer_step_before{};
        std::uint64_t optimizer_step_after{};
        std::uint64_t parameters_before_digest{};
        std::uint64_t parameters_after_digest{};
        std::uint64_t observation_digest{ 1469598103934665603ULL };
        std::uint64_t guided_action_digest{ 1469598103934665603ULL };

        [[nodiscard]] bool operator==(
            const AssistedIntentRolloutEvidence&) const noexcept = default;
    };

    struct FoundationalRolloutScheduleEvidence
    {
        std::array<std::uint64_t, rollout_origin_count> origin_counts{};
        std::uint64_t phase_leak_samples{};
        std::uint64_t raw_guided_action_samples{};
        std::uint64_t assisted_guided_action_samples{};
        std::uint64_t ppo_raw_actor_samples{};
        std::uint64_t ppo_assisted_actor_samples{};
        std::uint64_t self_imitation_updates{};
        std::uint64_t best_anchor_updates{};
        std::uint64_t guided_raw_sources_rejected{};
        std::uint64_t guided_assisted_sources_consumed{};
        bool receipt_present{};

        [[nodiscard]] bool operator==(
            const FoundationalRolloutScheduleEvidence&) const noexcept = default;
    };

    struct EvaluationCadencePoint
    {
        std::uint64_t update{};
        std::uint64_t lesson_update{};
        std::uint64_t evaluation_count{};
        std::uint64_t raw_actor_samples{};
        std::uint64_t assisted_actor_samples{};
        bool receipt_present{};
        bool handoff_reset_complete{};

        [[nodiscard]] bool operator==(
            const EvaluationCadencePoint&) const noexcept = default;
    };

    struct EvaluationCadenceEvidence
    {
        std::uint64_t initial_evaluation_count{};
        std::array<EvaluationCadencePoint, 6u> points{};
        std::uint64_t final_parameters_digest{};

        [[nodiscard]] bool operator==(
            const EvaluationCadenceEvidence&) const noexcept = default;
    };

    struct OneUpdateRawNonRetentionEvidence
    {
        RawPpoReceipt raw_ppo_receipt{};
        PpoTrainer::VerifiedCheckpoint production_checkpoint{};
        std::uint64_t optimizer_step_before{};
        std::uint64_t optimizer_step_after{};
        std::uint64_t parameters_before_digest{};
        std::uint64_t parameters_after_digest{};
        std::uint64_t evaluation_count{};
        std::uint64_t evaluation_quality_key{};
        std::uint32_t evaluation_invalid_runs{};
        std::size_t retained_parameter_count{};
        std::uint64_t retained_update{};

        [[nodiscard]] bool operator==(
            const OneUpdateRawNonRetentionEvidence& other) const noexcept
        {
            return raw_ppo_receipt.provenance_digest()
                    == other.raw_ppo_receipt.provenance_digest()
                && production_checkpoint.valid()
                    == other.production_checkpoint.valid()
                && optimizer_step_before == other.optimizer_step_before
                && optimizer_step_after == other.optimizer_step_after
                && parameters_before_digest == other.parameters_before_digest
                && parameters_after_digest == other.parameters_after_digest
                && evaluation_count == other.evaluation_count
                && evaluation_quality_key == other.evaluation_quality_key
                && evaluation_invalid_runs == other.evaluation_invalid_runs
                && retained_parameter_count == other.retained_parameter_count
                && retained_update == other.retained_update;
        }
    };

    // This header is intentionally test-only and is not in the install set.
    // It crosses the real handoff/reset/rollout/optimizer path; it does not
    // construct evidence or a receipt directly.
    struct RawLearningTestAccess
    {
        [[nodiscard]] static AssistedIntentRolloutEvidence
            one_assisted_intent_rollout(PpoTrainer& trainer)
        {
            trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
            trainer.set_gait_task(sim::GaitTask::walk, false);
            trainer.set_equipment_directive(sim::EquipmentDirective::passive);
            trainer.lesson_update_ = 0u;
            AssistedIntentRolloutEvidence evidence{};
            evidence.optimizer_step_before = trainer.adam_.step;
            evidence.parameters_before_digest =
                parameter_digest(trainer.policy_.parameters());
            trainer.begin_staged_update();

            for (const PpoTrainer::Transition& transition : trainer.rollout_)
            {
                if (transition.stop_requested)
                    ++evidence.stop_samples;
                else if (transition.requested_direction > 0.5f)
                    ++evidence.forward_samples;
                else if (transition.requested_direction < -0.5f)
                    ++evidence.reverse_samples;
                const float expected_direction = transition.stop_requested
                    ? 0.0f : transition.requested_direction;
                if (transition.observation[43] != expected_direction)
                    ++evidence.observation_mismatches;
                if (!transition.foundational_private_phase_scrubbed)
                    ++evidence.elapsed_phase_samples;
                if (transition.origin == RolloutOrigin::teacher_bootstrap)
                    ++evidence.teacher_bootstrap_samples;
                if (transition.origin == RolloutOrigin::raw_policy)
                    ++evidence.raw_origin_samples;
                for (const float observation : transition.observation)
                {
                    evidence.observation_digest ^=
                        std::bit_cast<std::uint32_t>(observation);
                    evidence.observation_digest *= 1099511628211ULL;
                }
                for (const float action : transition.guided_action)
                {
                    evidence.guided_action_digest ^=
                        std::bit_cast<std::uint32_t>(action);
                    evidence.guided_action_digest *= 1099511628211ULL;
                }
            }
            trainer.compute_staged_advantages();
            trainer.optimize_staged_update();
            trainer.finish_staged_update();
            evidence.optimizer_step_after = trainer.adam_.step;
            evidence.parameters_after_digest =
                parameter_digest(trainer.policy_.parameters());
            return evidence;
        }

        [[nodiscard]] static PpoTrainer::VerifiedCheckpoint
            one_real_assisted_update(PpoTrainer& trainer)
        {
            trainer.train_one_update();
            trainer.best_parameters_ = trainer.policy_.parameters();
            trainer.best_policy_guidance_mode_ = sim::GuidanceMode::assisted;
            trainer.best_raw_ppo_receipt_ = {};
            trainer.metrics_.best_update = trainer.metrics_.update;
            trainer.metrics_.best_quality_key = std::max<std::uint64_t>(
                trainer.metrics_.evaluation_quality_key, 1u);
            trainer.metrics_.best_evaluation_score = std::isfinite(
                    trainer.metrics_.evaluation_score)
                ? trainer.metrics_.evaluation_score : 0.0f;
            trainer.metrics_.best_evaluation_distance = std::isfinite(
                    trainer.metrics_.evaluation_distance)
                ? trainer.metrics_.evaluation_distance : 0.0f;
            trainer.preview_policy_.parameters() = trainer.best_parameters_;
            trainer.set_preview_guidance_mode(sim::GuidanceMode::assisted);
            return trainer.verified_checkpoint();
        }

        [[nodiscard]] static PpoTrainer::VerifiedCheckpoint
            one_real_raw_update(PpoTrainer& trainer)
        {
            trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
            trainer.set_gait_task(sim::GaitTask::walk, false);
            trainer.set_equipment_directive(sim::EquipmentDirective::passive);
            trainer.lesson_update_ = foundational_walk_teacher_handoff_update(
                trainer.blueprint_);
            while (foundational_walk_raw_rollout_count(
                    trainer.lesson_update_, sim::CourseStage::uneven,
                    trainer.blueprint_, trainer.environments_.size())
                < trainer.environments_.size())
            {
                ++trainer.lesson_update_;
            }
            trainer.reset_raw_handoff_boundary();
            trainer.train_one_update();
            if (!trainer.last_raw_ppo_receipt_.present())
                return {};

            // Retention quality is orthogonal to the provenance test. Preserve
            // the real evaluation measurements while making the genuine
            // post-update candidate visible to controller-boundary tests.
            trainer.best_parameters_ = trainer.policy_.parameters();
            trainer.best_policy_guidance_mode_ =
                sim::GuidanceMode::raw_policy_audit;
            trainer.best_raw_ppo_receipt_ = trainer.last_raw_ppo_receipt_;
            trainer.metrics_.best_update = trainer.metrics_.update;
            trainer.metrics_.best_quality_key = std::max<std::uint64_t>(
                trainer.metrics_.evaluation_quality_key, 1u);
            trainer.metrics_.best_evaluation_score = std::isfinite(
                    trainer.metrics_.evaluation_score)
                ? trainer.metrics_.evaluation_score : 0.0f;
            trainer.metrics_.best_evaluation_distance = std::isfinite(
                    trainer.metrics_.evaluation_distance)
                ? trainer.metrics_.evaluation_distance : 0.0f;
            trainer.preview_policy_.parameters() = trainer.best_parameters_;
            trainer.set_preview_guidance_mode(
                sim::GuidanceMode::raw_policy_audit);
            return trainer.verified_checkpoint();
        }

        // Cross the real handoff, rollout, optimizer, six-seed evaluation, and
        // retention gate exactly once without manufacturing a champion. This
        // is deliberately evidence about an unretained update, not a shortcut
        // for tests that require a production-qualified Raw controller.
        [[nodiscard]] static OneUpdateRawNonRetentionEvidence
            one_unretained_raw_update(PpoTrainer& trainer)
        {
            trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
            trainer.set_gait_task(sim::GaitTask::walk, false);
            trainer.set_equipment_directive(sim::EquipmentDirective::passive);
            trainer.lesson_update_ = foundational_walk_teacher_handoff_update(
                trainer.blueprint_);
            while (foundational_walk_raw_rollout_count(
                    trainer.lesson_update_, sim::CourseStage::uneven,
                    trainer.blueprint_, trainer.environments_.size())
                < trainer.environments_.size())
            {
                ++trainer.lesson_update_;
            }
            trainer.reset_raw_handoff_boundary();
            OneUpdateRawNonRetentionEvidence evidence{};
            evidence.optimizer_step_before = trainer.adam_.step;
            evidence.parameters_before_digest = parameter_digest(
                trainer.policy_.parameters());
            trainer.train_one_update();
            evidence.optimizer_step_after = trainer.adam_.step;
            evidence.parameters_after_digest = parameter_digest(
                trainer.policy_.parameters());
            evidence.raw_ppo_receipt = trainer.last_raw_ppo_receipt_;
            const PpoTrainer::VerifiedCheckpoint trainer_state =
                trainer.verified_checkpoint();
            if (trainer_state.valid() && trainer.has_best_policy()
                && trainer_state.data().best_guidance_mode
                    == sim::GuidanceMode::raw_policy_audit
                && trainer_state.data().raw_ppo_receipt.present()
                && strict_evaluation_quality(
                    trainer_state.data().metrics.best_quality_key))
            {
                evidence.production_checkpoint = trainer_state;
            }
            evidence.evaluation_count = trainer.metrics_.evaluation_count;
            evidence.evaluation_quality_key =
                trainer.metrics_.evaluation_quality_key;
            evidence.evaluation_invalid_runs =
                trainer.metrics_.evaluation_invalid_runs;
            evidence.retained_parameter_count = trainer.best_parameters_.size();
            evidence.retained_update = trainer.metrics_.best_update;
            return evidence;
        }

        [[nodiscard]] static FoundationalRolloutScheduleEvidence
            foundational_rollout_schedule(PpoTrainer& trainer,
                std::uint64_t lesson_update, bool optimize)
        {
            trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
            trainer.set_gait_task(sim::GaitTask::walk, false);
            trainer.set_equipment_directive(sim::EquipmentDirective::passive);
            trainer.lesson_update_ = lesson_update;
            if (lesson_update >= foundational_walk_teacher_handoff_update(
                    trainer.blueprint_))
            {
                trainer.reset_raw_handoff_boundary();
            }
            trainer.begin_staged_update();

            FoundationalRolloutScheduleEvidence evidence{};
            for (const PpoTrainer::Transition& transition : trainer.rollout_)
            {
                const std::size_t origin =
                    static_cast<std::size_t>(transition.origin);
                if (origin < evidence.origin_counts.size())
                    ++evidence.origin_counts[origin];
                if (!transition.foundational_private_phase_scrubbed)
                    ++evidence.phase_leak_samples;
                const bool guided = std::ranges::any_of(
                    transition.guided_action,
                    [](float action) noexcept { return action != 0.0f; });
                if (transition.origin == RolloutOrigin::raw_policy)
                    evidence.raw_guided_action_samples += guided ? 1u : 0u;
                else
                    evidence.assisted_guided_action_samples += guided ? 1u : 0u;
            }
            if (optimize)
            {
                trainer.compute_staged_advantages();
                trainer.optimize_staged_update();
                trainer.finish_staged_update();
                evidence.guided_raw_sources_rejected =
                    trainer.last_guided_raw_sources_rejected_;
                evidence.guided_assisted_sources_consumed =
                    trainer.last_guided_assisted_sources_consumed_;
                evidence.ppo_raw_actor_samples =
                    trainer.last_ppo_raw_actor_samples_;
                evidence.ppo_assisted_actor_samples =
                    trainer.last_ppo_assisted_actor_samples_;
                evidence.self_imitation_updates =
                    trainer.staged_raw_ppo_evidence_.self_imitation_updates;
                evidence.best_anchor_updates =
                    trainer.staged_raw_ppo_evidence_.best_anchor_updates;
            }
            evidence.receipt_present =
                trainer.last_raw_ppo_receipt_.present();
            return evidence;
        }

        [[nodiscard]] static EvaluationCadenceEvidence
            human_walk_handoff_evaluation_cadence(PpoTrainer& trainer)
        {
            trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
            trainer.set_gait_task(sim::GaitTask::walk, false);
            trainer.set_equipment_directive(sim::EquipmentDirective::passive);
            const std::uint64_t handoff =
                foundational_walk_teacher_handoff_update(trainer.blueprint_);
            trainer.metrics_.update = handoff - 1u;
            trainer.lesson_update_ = handoff - 1u;
            trainer.metrics_.evaluation_count = 37u;

            EvaluationCadenceEvidence evidence{};
            evidence.initial_evaluation_count =
                trainer.metrics_.evaluation_count;
            for (EvaluationCadencePoint& point : evidence.points)
            {
                trainer.train_one_update();
                point.update = trainer.metrics_.update;
                point.lesson_update = trainer.lesson_update_;
                point.evaluation_count = trainer.metrics_.evaluation_count;
                point.raw_actor_samples =
                    trainer.last_ppo_raw_actor_samples_;
                point.assisted_actor_samples =
                    trainer.last_ppo_assisted_actor_samples_;
                point.receipt_present =
                    trainer.last_raw_ppo_receipt_.present();
                point.handoff_reset_complete =
                    trainer.raw_handoff_reset_complete_;
            }
            evidence.final_parameters_digest =
                parameter_digest(trainer.policy_.parameters());
            return evidence;
        }

        [[nodiscard]] static EvaluationCadencePoint
            one_update_evaluation_cadence(PpoTrainer& trainer,
                sim::CourseStage stage, sim::GaitTask gait_task,
                std::uint64_t lesson_update_before)
        {
            trainer.set_course(stage, 0.30f, false);
            trainer.set_gait_task(gait_task, false);
            trainer.set_equipment_directive(sim::EquipmentDirective::passive);
            trainer.metrics_.update = 899u;
            trainer.lesson_update_ = lesson_update_before;
            trainer.metrics_.evaluation_count = 37u;
            trainer.train_one_update();
            return {
                trainer.metrics_.update,
                trainer.lesson_update_,
                trainer.metrics_.evaluation_count,
                trainer.last_ppo_raw_actor_samples_,
                trainer.last_ppo_assisted_actor_samples_,
                trainer.last_raw_ppo_receipt_.present(),
                trainer.raw_handoff_reset_complete_
            };
        }

        [[nodiscard]] static PpoTrainer::CheckpointData reseal(
            PpoTrainer::CheckpointData data)
        {
            data.parameters_digest = parameter_digest(data.parameters);
            data.best_parameters_digest = parameter_digest(data.best_parameters);
            PpoTrainer::seal_checkpoint(
                data, PpoTrainer::CheckpointData::Origin::live_trainer);
            return data;
        }

        // Reproduces the historical EPPO44 writer byte-for-byte so legacy
        // compatibility and exact EOF can be tested without pretending that
        // legacy storage is a verified checkpoint artifact.
        [[nodiscard]] static bool write_legacy_eppo44(
            const std::filesystem::path& path,
            const PpoTrainer::CheckpointData& data)
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            constexpr std::array<char, 8> magic{
                'E', 'P', 'P', 'O', '4', '4', '\0', '\1' };
            const std::uint8_t stage = static_cast<std::uint8_t>(data.stage);
            return output.write(magic.data(),
                       static_cast<std::streamsize>(magic.size()))
                && write_value(output, data.training_semantics)
                && write_value(output, data.rig_signature)
                && write_value(output, data.optimizer_step)
                && write_value(output, data.random_state)
                && write_value(output, stage)
                && write_value(output, data.difficulty)
                && write_value(output, data.lesson_update)
                && write_legacy_metrics(output, data.metrics)
                && write_vector(output, data.parameters)
                && write_vector(output, data.first_moment)
                && write_vector(output, data.second_moment)
                && write_vector(output, data.best_parameters)
                && write_vector(output, data.reward_history)
                && write_vector(output, data.speed_history);
        }

    private:
        template <typename T>
        [[nodiscard]] static bool write_value(std::ofstream& output,
            const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            output.write(reinterpret_cast<const char*>(&value), sizeof(T));
            return static_cast<bool>(output);
        }

        [[nodiscard]] static bool write_vector(std::ofstream& output,
            const std::vector<float>& values)
        {
            const std::uint64_t count = values.size();
            if (!write_value(output, count))
                return false;
            if (!values.empty())
            {
                output.write(reinterpret_cast<const char*>(values.data()),
                    static_cast<std::streamsize>(
                        values.size() * sizeof(float)));
            }
            return static_cast<bool>(output);
        }

        [[nodiscard]] static bool write_legacy_metrics(std::ofstream& output,
            const TrainingMetrics& value)
        {
            return write_value(output, value.update)
                && write_value(output, value.environment_steps)
                && write_value(output, value.total_updates)
                && write_value(output, value.total_environment_steps)
                && write_value(output, value.total_episodes)
                && write_value(output, value.total_valid_episodes)
                && write_value(output, value.total_invalid_episodes)
                && write_value(output, value.total_resets)
                && write_value(output, value.total_alternating_steps)
                && write_value(output, value.total_falls)
                && write_value(output, value.total_collisions)
                && write_value(output, value.total_powered_jumps)
                && write_value(output, value.total_landed_jumps)
                && write_value(output, value.total_landed_flips)
                && write_value(output, value.total_obstacles_passed)
                && write_value(output, value.total_distance)
                && write_value(output, value.total_training_seconds)
                && write_value(output, value.mean_reward)
                && write_value(output, value.mean_episode_distance)
                && write_value(output, value.mean_speed)
                && write_value(output, value.policy_loss)
                && write_value(output, value.value_loss)
                && write_value(output, value.entropy)
                && write_value(output, value.learning_rate)
                && write_value(output, value.evaluation_reward)
                && write_value(output, value.evaluation_distance)
                && write_value(output, value.evaluation_speed)
                && write_value(output, value.evaluation_score)
                && write_value(output, value.evaluation_survival)
                && write_value(output, value.evaluation_collisions)
                && write_value(output, value.evaluation_airborne_ratio)
                && write_value(output, value.evaluation_stride_events)
                && write_value(output, value.evaluation_duck_seconds)
                && write_value(output, value.evaluation_powered_jumps)
                && write_value(output, value.evaluation_jump_landings)
                && write_value(output, value.evaluation_spin_turns)
                && write_value(output, value.evaluation_spin_landings)
                && write_value(output, value.evaluation_obstacles_passed)
                && write_value(output, value.evaluation_stable_stance)
                && write_value(output, value.evaluation_longest_stance)
                && write_value(output, value.evaluation_duck_recoveries)
                && write_value(output, value.evaluation_max_joint_speed)
                && write_value(output, value.evaluation_hand_contacts)
                && write_value(output, value.evaluation_climb_transfers)
                && write_value(output, value.evaluation_climbs)
                && write_value(output, value.evaluation_descents)
                && write_value(output, value.evaluation_shots)
                && write_value(output, value.evaluation_target_hits)
                && write_value(output, value.evaluation_equipment_transitions)
                && write_value(output, value.evaluation_quality_key)
                && write_value(output, value.evaluation_rejection_mask)
                && write_value(output, value.evaluation_invalid_runs)
                && write_value(output, value.evaluation_invalid_reason)
                && write_value(output, value.evaluation_valid)
                && write_value(output, value.best_evaluation_distance)
                && write_value(output, value.best_evaluation_score)
                && write_value(output, value.best_quality_key)
                && write_value(output, value.best_update)
                && write_value(output, value.evaluation_count)
                && write_value(output, value.imitation_samples)
                && write_value(output, value.imitation_weight)
                && write_value(output, value.imitation_source_score);
        }
    };
}
