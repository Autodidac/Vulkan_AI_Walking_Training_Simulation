#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace runner::rl
{
    class PpoTrainer;

    enum class RolloutOrigin : std::uint8_t
    {
        raw_policy,
        assisted_policy,
        teacher_bootstrap,
        guided_policy,
        unspecified,
        count
    };

    inline constexpr std::size_t rollout_origin_count =
        static_cast<std::size_t>(RolloutOrigin::count);
    inline constexpr std::uint32_t raw_ppo_receipt_schema_version = 3u;
    inline constexpr std::uint32_t raw_observation_scrub_schema_version = 2u;
    inline constexpr std::size_t elapsed_phase_sine_observation_index = 38u;
    inline constexpr std::size_t elapsed_phase_cosine_observation_index = 39u;

    template <std::size_t Count>
    [[nodiscard]] inline std::array<float, Count> raw_policy_observation(
        std::array<float, Count> observation) noexcept
    {
        static_assert(Count > elapsed_phase_cosine_observation_index);
        observation[elapsed_phase_sine_observation_index] = 0.0f;
        observation[elapsed_phase_cosine_observation_index] = 0.0f;
        return observation;
    }

    [[nodiscard]] inline std::uint64_t provenance_hash_bytes(
        std::span<const std::byte> bytes,
        std::uint64_t hash = 1469598103934665603ULL) noexcept
    {
        for (const std::byte byte : bytes)
        {
            hash ^= std::to_integer<std::uint8_t>(byte);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    [[nodiscard]] inline std::uint64_t provenance_hash_u64(
        std::uint64_t hash, std::uint64_t value) noexcept
    {
        for (unsigned shift = 0; shift < 64u; shift += 8u)
        {
            hash ^= static_cast<std::uint8_t>(value >> shift);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    [[nodiscard]] inline std::uint64_t parameter_digest(
        std::span<const float> parameters) noexcept
    {
        std::uint64_t hash = provenance_hash_u64(1469598103934665603ULL,
            static_cast<std::uint64_t>(parameters.size()));
        for (const float value : parameters)
            hash = provenance_hash_u64(hash,
                static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value)));
        return hash;
    }

    namespace detail
    {
        // Staging state is not an authority object. Only PpoTrainer may turn
        // it into an immutable receipt after the real rollout/update path.
        struct RawPpoUpdateEvidence
        {
            std::uint32_t schema{ raw_ppo_receipt_schema_version };
            std::uint32_t training_semantics{};
            std::uint64_t rig_signature{};
            std::uint64_t handoff_update{};
            std::uint64_t rollout_lesson_update{};
            std::uint64_t optimizer_update{};
            std::uint64_t global_update{};
            std::uint64_t optimizer_step_before{};
            std::uint64_t optimizer_step_after{};
            std::uint64_t expected_sample_count{};
            std::array<std::uint64_t, rollout_origin_count> origin_counts{};
            std::uint64_t self_imitation_updates{};
            std::uint64_t best_anchor_updates{};
            std::uint64_t elapsed_phase_samples{};
            std::uint64_t forward_intent_samples{};
            std::uint64_t reverse_intent_samples{};
            std::uint64_t stop_intent_samples{};
            double absolute_advantage_sum{};
            std::uint64_t parameters_before_digest{};
            std::uint64_t parameters_after_digest{};
            std::uint64_t active_actor_before_digest{};
            std::uint64_t active_actor_after_digest{};
            std::uint64_t evaluated_parameters_digest{};
            std::uint64_t current_parameters_digest{};
            std::uint32_t action_mask_bits{};
            std::uint32_t observation_scrub_schema{
                raw_observation_scrub_schema_version };
            std::uint8_t stage{};
            std::uint8_t gait_task{};
            std::uint8_t equipment_directive{};
            bool environments_reset_at_handoff{};
            bool actions_reset_at_handoff{};
            bool episodes_reset_at_handoff{};
            bool whole_cohort_raw{};
        };
    }

    struct RawPpoReceiptContext
    {
        std::uint32_t training_semantics{};
        std::uint64_t rig_signature{};
        std::uint64_t best_update{};
        std::uint64_t checkpoint_optimizer_step{};
        std::uint64_t retained_parameters_digest{};
        std::uint64_t retained_active_actor_digest{};
        std::uint32_t action_mask_bits{};
        std::uint8_t stage{};
        std::uint8_t gait_task{};
        std::uint8_t equipment_directive{};
    };

    class RawPpoReceipt
    {
    public:
        RawPpoReceipt() = default;
        RawPpoReceipt(const RawPpoReceipt&) = default;
        RawPpoReceipt& operator=(const RawPpoReceipt&) = default;

        [[nodiscard]] std::uint32_t schema() const noexcept { return schema_; }
        [[nodiscard]] std::uint32_t training_semantics() const noexcept { return training_semantics_; }
        [[nodiscard]] std::uint64_t rig_signature() const noexcept { return rig_signature_; }
        [[nodiscard]] std::uint64_t handoff_update() const noexcept { return handoff_update_; }
        [[nodiscard]] std::uint64_t rollout_lesson_update() const noexcept { return rollout_lesson_update_; }
        [[nodiscard]] std::uint64_t optimizer_update() const noexcept { return optimizer_update_; }
        [[nodiscard]] std::uint64_t global_update() const noexcept { return global_update_; }
        [[nodiscard]] std::uint64_t optimizer_step_before() const noexcept { return optimizer_step_before_; }
        [[nodiscard]] std::uint64_t optimizer_step() const noexcept { return optimizer_step_; }
        [[nodiscard]] std::uint64_t expected_sample_count() const noexcept { return expected_sample_count_; }
        [[nodiscard]] const std::array<std::uint64_t, rollout_origin_count>& origin_counts() const noexcept { return origin_counts_; }
        [[nodiscard]] std::uint64_t self_imitation_updates() const noexcept { return self_imitation_updates_; }
        [[nodiscard]] std::uint64_t best_anchor_updates() const noexcept { return best_anchor_updates_; }
        [[nodiscard]] std::uint64_t elapsed_phase_samples() const noexcept { return elapsed_phase_samples_; }
        [[nodiscard]] std::uint64_t forward_intent_samples() const noexcept { return forward_intent_samples_; }
        [[nodiscard]] std::uint64_t reverse_intent_samples() const noexcept { return reverse_intent_samples_; }
        [[nodiscard]] std::uint64_t stop_intent_samples() const noexcept { return stop_intent_samples_; }
        [[nodiscard]] double absolute_advantage_sum() const noexcept { return absolute_advantage_sum_; }
        [[nodiscard]] std::uint64_t parameters_before_digest() const noexcept { return parameters_before_digest_; }
        [[nodiscard]] std::uint64_t parameters_after_digest() const noexcept { return parameters_after_digest_; }
        [[nodiscard]] std::uint64_t active_actor_before_digest() const noexcept { return active_actor_before_digest_; }
        [[nodiscard]] std::uint64_t active_actor_after_digest() const noexcept { return active_actor_after_digest_; }
        [[nodiscard]] std::uint64_t evaluated_parameters_digest() const noexcept { return evaluated_parameters_digest_; }
        [[nodiscard]] std::uint64_t retained_parameters_digest() const noexcept { return retained_parameters_digest_; }
        [[nodiscard]] std::uint32_t action_mask_bits() const noexcept { return action_mask_bits_; }
        [[nodiscard]] std::uint32_t observation_scrub_schema() const noexcept { return observation_scrub_schema_; }
        [[nodiscard]] std::uint8_t stage() const noexcept { return stage_; }
        [[nodiscard]] std::uint8_t gait_task() const noexcept { return gait_task_; }
        [[nodiscard]] std::uint8_t equipment_directive() const noexcept { return equipment_directive_; }
        [[nodiscard]] bool environments_reset_at_handoff() const noexcept { return environments_reset_at_handoff_; }
        [[nodiscard]] bool actions_reset_at_handoff() const noexcept { return actions_reset_at_handoff_; }
        [[nodiscard]] bool episodes_reset_at_handoff() const noexcept { return episodes_reset_at_handoff_; }
        [[nodiscard]] bool whole_cohort_raw() const noexcept { return whole_cohort_raw_; }
        [[nodiscard]] std::uint64_t provenance_digest() const noexcept { return provenance_digest_; }
        [[nodiscard]] bool present() const noexcept { return provenance_digest_ != 0u; }

    private:
        friend class PpoTrainer;
        friend std::uint64_t raw_ppo_receipt_digest(const RawPpoReceipt&) noexcept;
        friend bool raw_ppo_receipt_valid(const RawPpoReceipt&,
            const RawPpoReceiptContext&) noexcept;

        [[nodiscard]] static RawPpoReceipt issue(
            const detail::RawPpoUpdateEvidence& evidence) noexcept;

        std::uint32_t schema_{ raw_ppo_receipt_schema_version };
        std::uint32_t training_semantics_{};
        std::uint64_t rig_signature_{};
        std::uint64_t handoff_update_{};
        std::uint64_t rollout_lesson_update_{};
        std::uint64_t optimizer_update_{};
        std::uint64_t global_update_{};
        std::uint64_t optimizer_step_before_{};
        std::uint64_t optimizer_step_{};
        std::uint64_t expected_sample_count_{};
        std::array<std::uint64_t, rollout_origin_count> origin_counts_{};
        std::uint64_t self_imitation_updates_{};
        std::uint64_t best_anchor_updates_{};
        std::uint64_t elapsed_phase_samples_{};
        std::uint64_t forward_intent_samples_{};
        std::uint64_t reverse_intent_samples_{};
        std::uint64_t stop_intent_samples_{};
        double absolute_advantage_sum_{};
        std::uint64_t parameters_before_digest_{};
        std::uint64_t parameters_after_digest_{};
        std::uint64_t active_actor_before_digest_{};
        std::uint64_t active_actor_after_digest_{};
        std::uint64_t evaluated_parameters_digest_{};
        std::uint64_t retained_parameters_digest_{};
        std::uint32_t action_mask_bits_{};
        std::uint32_t observation_scrub_schema_{ raw_observation_scrub_schema_version };
        std::uint8_t stage_{};
        std::uint8_t gait_task_{};
        std::uint8_t equipment_directive_{};
        bool environments_reset_at_handoff_{};
        bool actions_reset_at_handoff_{};
        bool episodes_reset_at_handoff_{};
        bool whole_cohort_raw_{};
        std::uint64_t provenance_digest_{};
    };

    [[nodiscard]] inline bool checked_add(
        std::uint64_t& sum, std::uint64_t value) noexcept
    {
        if (value > std::numeric_limits<std::uint64_t>::max() - sum)
            return false;
        sum += value;
        return true;
    }

    [[nodiscard]] inline std::uint64_t raw_ppo_receipt_digest(
        const RawPpoReceipt& receipt) noexcept
    {
        std::uint64_t hash = 1469598103934665603ULL;
        const auto add = [&hash](std::uint64_t value) noexcept
        {
            hash = provenance_hash_u64(hash, value);
        };
        add(receipt.schema_);
        add(receipt.training_semantics_);
        add(receipt.rig_signature_);
        add(receipt.handoff_update_);
        add(receipt.rollout_lesson_update_);
        add(receipt.optimizer_update_);
        add(receipt.global_update_);
        add(receipt.optimizer_step_before_);
        add(receipt.optimizer_step_);
        add(receipt.expected_sample_count_);
        for (const std::uint64_t count : receipt.origin_counts_)
            add(count);
        add(receipt.self_imitation_updates_);
        add(receipt.best_anchor_updates_);
        add(receipt.elapsed_phase_samples_);
        add(receipt.forward_intent_samples_);
        add(receipt.reverse_intent_samples_);
        add(receipt.stop_intent_samples_);
        add(std::bit_cast<std::uint64_t>(receipt.absolute_advantage_sum_));
        add(receipt.parameters_before_digest_);
        add(receipt.parameters_after_digest_);
        add(receipt.active_actor_before_digest_);
        add(receipt.active_actor_after_digest_);
        add(receipt.evaluated_parameters_digest_);
        add(receipt.retained_parameters_digest_);
        add(receipt.action_mask_bits_);
        add(receipt.observation_scrub_schema_);
        add(receipt.stage_);
        add(receipt.gait_task_);
        add(receipt.equipment_directive_);
        add(receipt.environments_reset_at_handoff_ ? 1u : 0u);
        add(receipt.actions_reset_at_handoff_ ? 1u : 0u);
        add(receipt.episodes_reset_at_handoff_ ? 1u : 0u);
        add(receipt.whole_cohort_raw_ ? 1u : 0u);
        return hash;
    }

    inline RawPpoReceipt RawPpoReceipt::issue(
        const detail::RawPpoUpdateEvidence& evidence) noexcept
    {
        RawPpoReceipt receipt{};
        std::uint64_t sample_count{};
        bool counts_valid = true;
        for (const std::uint64_t count : evidence.origin_counts)
            counts_valid = counts_valid && checked_add(sample_count, count);
        const std::uint64_t raw_samples = evidence.origin_counts[
            static_cast<std::size_t>(RolloutOrigin::raw_policy)];
        std::uint64_t intent_samples{};
        const bool intent_counts_valid = checked_add(intent_samples,
                evidence.forward_intent_samples)
            && checked_add(intent_samples, evidence.reverse_intent_samples)
            && checked_add(intent_samples, evidence.stop_intent_samples);
        if (!counts_valid
            || !intent_counts_valid
            || evidence.schema != raw_ppo_receipt_schema_version
            || evidence.training_semantics == 0u
            || evidence.rig_signature == 0u
            || evidence.handoff_update == 0u
            || evidence.rollout_lesson_update < evidence.handoff_update
            || evidence.rollout_lesson_update
                == std::numeric_limits<std::uint64_t>::max()
            || evidence.optimizer_update != evidence.rollout_lesson_update + 1u
            || evidence.global_update == 0u
            || evidence.optimizer_step_before
                == std::numeric_limits<std::uint64_t>::max()
            || evidence.optimizer_step_after <= evidence.optimizer_step_before
            || evidence.expected_sample_count == 0u
            || evidence.expected_sample_count > (1u << 20u)
            || raw_samples != evidence.expected_sample_count
            || sample_count != evidence.expected_sample_count
            || evidence.self_imitation_updates != 0u
            || evidence.best_anchor_updates != 0u
            || evidence.elapsed_phase_samples != 0u
            || evidence.forward_intent_samples == 0u
            || evidence.reverse_intent_samples == 0u
            || evidence.stop_intent_samples == 0u
            || intent_samples != evidence.expected_sample_count
            || !std::isfinite(evidence.absolute_advantage_sum)
            || evidence.absolute_advantage_sum <= 1.0e-12
            || evidence.absolute_advantage_sum > 1.0e12
            || evidence.parameters_before_digest == 0u
            || evidence.parameters_after_digest == 0u
            || evidence.parameters_before_digest == evidence.parameters_after_digest
            || evidence.active_actor_before_digest == 0u
            || evidence.active_actor_after_digest == 0u
            || evidence.active_actor_before_digest == evidence.active_actor_after_digest
            || evidence.parameters_after_digest != evidence.evaluated_parameters_digest
            || evidence.parameters_after_digest != evidence.current_parameters_digest
            || evidence.action_mask_bits == 0u
            || evidence.observation_scrub_schema != raw_observation_scrub_schema_version
            || !evidence.environments_reset_at_handoff
            || !evidence.actions_reset_at_handoff
            || !evidence.episodes_reset_at_handoff
            || !evidence.whole_cohort_raw)
            return receipt;

        receipt.training_semantics_ = evidence.training_semantics;
        receipt.rig_signature_ = evidence.rig_signature;
        receipt.handoff_update_ = evidence.handoff_update;
        receipt.rollout_lesson_update_ = evidence.rollout_lesson_update;
        receipt.optimizer_update_ = evidence.optimizer_update;
        receipt.global_update_ = evidence.global_update;
        receipt.optimizer_step_before_ = evidence.optimizer_step_before;
        receipt.optimizer_step_ = evidence.optimizer_step_after;
        receipt.expected_sample_count_ = evidence.expected_sample_count;
        receipt.origin_counts_ = evidence.origin_counts;
        receipt.self_imitation_updates_ = evidence.self_imitation_updates;
        receipt.best_anchor_updates_ = evidence.best_anchor_updates;
        receipt.elapsed_phase_samples_ = evidence.elapsed_phase_samples;
        receipt.forward_intent_samples_ = evidence.forward_intent_samples;
        receipt.reverse_intent_samples_ = evidence.reverse_intent_samples;
        receipt.stop_intent_samples_ = evidence.stop_intent_samples;
        receipt.absolute_advantage_sum_ = evidence.absolute_advantage_sum;
        receipt.parameters_before_digest_ = evidence.parameters_before_digest;
        receipt.parameters_after_digest_ = evidence.parameters_after_digest;
        receipt.active_actor_before_digest_ = evidence.active_actor_before_digest;
        receipt.active_actor_after_digest_ = evidence.active_actor_after_digest;
        receipt.evaluated_parameters_digest_ = evidence.evaluated_parameters_digest;
        receipt.retained_parameters_digest_ = evidence.parameters_after_digest;
        receipt.action_mask_bits_ = evidence.action_mask_bits;
        receipt.observation_scrub_schema_ = evidence.observation_scrub_schema;
        receipt.stage_ = evidence.stage;
        receipt.gait_task_ = evidence.gait_task;
        receipt.equipment_directive_ = evidence.equipment_directive;
        receipt.environments_reset_at_handoff_ = evidence.environments_reset_at_handoff;
        receipt.actions_reset_at_handoff_ = evidence.actions_reset_at_handoff;
        receipt.episodes_reset_at_handoff_ = evidence.episodes_reset_at_handoff;
        receipt.whole_cohort_raw_ = evidence.whole_cohort_raw;
        receipt.provenance_digest_ = raw_ppo_receipt_digest(receipt);
        return receipt;
    }

    [[nodiscard]] inline bool raw_ppo_receipt_valid(
        const RawPpoReceipt& receipt,
        const RawPpoReceiptContext& context) noexcept
    {
        std::uint64_t sample_count{};
        bool counts_valid = true;
        for (const std::uint64_t count : receipt.origin_counts_)
            counts_valid = counts_valid && checked_add(sample_count, count);
        const std::uint64_t raw_samples = receipt.origin_counts_[
            static_cast<std::size_t>(RolloutOrigin::raw_policy)];
        std::uint64_t intent_samples{};
        const bool intent_counts_valid = checked_add(intent_samples,
                receipt.forward_intent_samples_)
            && checked_add(intent_samples, receipt.reverse_intent_samples_)
            && checked_add(intent_samples, receipt.stop_intent_samples_);
        return counts_valid
            && intent_counts_valid
            && receipt.schema_ == raw_ppo_receipt_schema_version
            && receipt.training_semantics_ == context.training_semantics
            && receipt.rig_signature_ == context.rig_signature
            && receipt.handoff_update_ > 0u
            && receipt.rollout_lesson_update_ >= receipt.handoff_update_
            && receipt.rollout_lesson_update_
                != std::numeric_limits<std::uint64_t>::max()
            && receipt.optimizer_update_ == receipt.rollout_lesson_update_ + 1u
            && receipt.global_update_ == context.best_update
            && receipt.optimizer_step_before_ < receipt.optimizer_step_
            && receipt.optimizer_step_ <= context.checkpoint_optimizer_step
            && receipt.expected_sample_count_ > 0u
            && receipt.expected_sample_count_ <= (1u << 20u)
            && raw_samples == receipt.expected_sample_count_
            && sample_count == receipt.expected_sample_count_
            && receipt.self_imitation_updates_ == 0u
            && receipt.best_anchor_updates_ == 0u
            && receipt.elapsed_phase_samples_ == 0u
            && receipt.forward_intent_samples_ > 0u
            && receipt.reverse_intent_samples_ > 0u
            && receipt.stop_intent_samples_ > 0u
            && intent_samples == receipt.expected_sample_count_
            && std::isfinite(receipt.absolute_advantage_sum_)
            && receipt.absolute_advantage_sum_ > 1.0e-12
            && receipt.absolute_advantage_sum_ <= 1.0e12
            && receipt.parameters_before_digest_ != 0u
            && receipt.parameters_after_digest_ != 0u
            && receipt.parameters_before_digest_ != receipt.parameters_after_digest_
            && receipt.active_actor_before_digest_ != 0u
            && receipt.active_actor_after_digest_ != 0u
            && receipt.active_actor_before_digest_ != receipt.active_actor_after_digest_
            && receipt.parameters_after_digest_ == receipt.evaluated_parameters_digest_
            && receipt.parameters_after_digest_ == receipt.retained_parameters_digest_
            && receipt.retained_parameters_digest_ == context.retained_parameters_digest
            && receipt.active_actor_after_digest_ == context.retained_active_actor_digest
            && receipt.action_mask_bits_ == context.action_mask_bits
            && receipt.observation_scrub_schema_ == raw_observation_scrub_schema_version
            && receipt.stage_ == context.stage
            && receipt.gait_task_ == context.gait_task
            && receipt.equipment_directive_ == context.equipment_directive
            && receipt.environments_reset_at_handoff_
            && receipt.actions_reset_at_handoff_
            && receipt.episodes_reset_at_handoff_
            && receipt.whole_cohort_raw_
            && receipt.provenance_digest_ != 0u
            && receipt.provenance_digest_ == raw_ppo_receipt_digest(receipt);
    }
}
