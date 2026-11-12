#include "ppo.hpp"
#include "atomic_file.hpp"

#include <algorithm>

#include <array>
#include <cmath>
#include <fstream>
#include <format>
#include <iterator>
#include <type_traits>

namespace runner::rl
{
    namespace
    {
        constexpr std::array<char, 8> checkpoint_magic{ 'E', 'P', 'P', 'O', '5', '1', '\0', '\1' };
        constexpr std::array<char, 8> v0744_checkpoint_magic{ 'E', 'P', 'P', 'O', '4', '4', '\0', '\1' };
        constexpr std::array<char, 8> v0743_checkpoint_magic{ 'E', 'P', 'P', 'O', '4', '3', '\0', '\1' };
        constexpr std::array<char, 8> v0742_checkpoint_magic{ 'E', 'P', 'P', 'O', '4', '2', '\0', '\1' };
        constexpr std::array<char, 8> v0741_checkpoint_magic{ 'E', 'P', 'P', 'O', '4', '1', '\0', '\1' };
        constexpr std::array<char, 8> v0740_checkpoint_magic{ 'E', 'P', 'P', 'O', '4', '0', '\0', '\1' };
        constexpr std::array<char, 8> v0739_checkpoint_magic{ 'E', 'P', 'P', 'O', '3', '9', '\0', '\1' };
        constexpr std::array<char, 8> v0738_checkpoint_magic{ 'E', 'P', 'P', 'O', '3', '8', '\0', '\1' };
        constexpr std::array<char, 8> v0737_checkpoint_magic{ 'E', 'P', 'P', 'O', '3', '7', '\0', '\1' };
        constexpr std::array<char, 8> v0736_checkpoint_magic{ 'E', 'P', 'P', 'O', '3', '6', '\0', '\1' };
        constexpr std::array<char, 8> v0735_checkpoint_magic{ 'E', 'P', 'P', 'O', '3', '5', '\0', '\1' };
        constexpr std::array<char, 8> v0734_checkpoint_magic{ 'E', 'P', 'P', 'O', '3', '4', '\0', '\1' };
        constexpr std::array<char, 8> v0733_checkpoint_magic{ 'E', 'P', 'P', 'O', '3', '3', '\0', '\1' };
        constexpr std::array<char, 8> v0732_checkpoint_magic{ 'E', 'P', 'P', 'O', '3', '2', '\0', '\1' };
        constexpr std::array<char, 8> v0731_checkpoint_magic{ 'E', 'P', 'P', 'O', '3', '1', '\0', '\1' };
        constexpr std::array<char, 8> v0730_checkpoint_magic{ 'E', 'P', 'P', 'O', '2', '9', '\0', '\1' };
        constexpr std::array<char, 8> v0727_checkpoint_magic{ 'E', 'P', 'P', 'O', '2', '8', '\0', '\1' };
        constexpr std::size_t v0727_input_size = 50u;
        constexpr std::size_t v0727_output_size = 8u;
        constexpr std::size_t v0727_parameter_count = 8'017u;

        template <typename T>
        bool write_value(std::ofstream& output, const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            output.write(reinterpret_cast<const char*>(&value), sizeof(T));
            return static_cast<bool>(output);
        }

        bool write_flag(std::ofstream& output, bool value)
        {
            const std::uint8_t encoded = value ? 1u : 0u;
            return write_value(output, encoded);
        }

        template <typename T>
        bool read_value(std::ifstream& input, T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            input.read(reinterpret_cast<char*>(&value), sizeof(T));
            return static_cast<bool>(input);
        }

        bool read_flag(std::ifstream& input, bool& value)
        {
            std::uint8_t encoded{};
            if (!read_value(input, encoded) || encoded > 1u)
                return false;
            value = encoded != 0u;
            return true;
        }

        bool write_vector(std::ofstream& output, const std::vector<float>& values)
        {
            const std::uint64_t count = values.size();
            if (!write_value(output, count))
                return false;
            if (values.empty())
                return true;
            output.write(reinterpret_cast<const char*>(values.data()),
                static_cast<std::streamsize>(values.size() * sizeof(float)));
            return static_cast<bool>(output);
        }

        bool read_vector(std::ifstream& input, std::vector<float>& values, std::size_t maximum)
        {
            std::uint64_t count{};
            if (!read_value(input, count) || count > maximum)
                return false;
            values.resize(static_cast<std::size_t>(count));
            if (values.empty())
                return true;
            input.read(reinterpret_cast<char*>(values.data()),
                static_cast<std::streamsize>(values.size() * sizeof(float)));
            return static_cast<bool>(input);
        }

        [[nodiscard]] bool finite_vector(std::span<const float> values) noexcept
        {
            return std::ranges::all_of(values, [](float value) noexcept
            {
                return std::isfinite(value) && std::abs(value) <= 1.0e6f;
            });
        }

        [[nodiscard]] bool bounded_finite(double value) noexcept
        {
            return std::isfinite(value) && std::abs(value) <= 1.0e12;
        }

        [[nodiscard]] bool finite_or_negative_infinity(float value) noexcept
        {
            return (std::isfinite(value) && std::abs(value) <= 1.0e9f)
                || value == -std::numeric_limits<float>::infinity();
        }

        [[nodiscard]] bool metrics_valid(const TrainingMetrics& value) noexcept
        {
            const std::array ordinary{
                value.mean_reward, value.mean_episode_distance, value.mean_speed,
                value.policy_loss, value.value_loss, value.entropy,
                value.learning_rate, value.evaluation_reward,
                value.evaluation_distance, value.evaluation_speed,
                value.evaluation_survival, value.evaluation_collisions,
                value.evaluation_airborne_ratio, value.evaluation_stride_events,
                value.evaluation_duck_seconds, value.evaluation_powered_jumps,
                value.evaluation_jump_landings, value.evaluation_spin_turns,
                value.evaluation_spin_landings, value.evaluation_obstacles_passed,
                value.evaluation_stable_stance, value.evaluation_longest_stance,
                value.evaluation_duck_recoveries,
                value.evaluation_max_joint_speed,
                value.evaluation_max_backward_brace_seconds,
                value.evaluation_hand_contacts, value.evaluation_climb_transfers,
                value.evaluation_climbs, value.evaluation_descents,
                value.evaluation_shots, value.evaluation_target_hits,
                value.evaluation_equipment_transitions, value.imitation_weight
            };
            const std::array nonnegative{
                value.evaluation_survival, value.evaluation_collisions,
                value.evaluation_airborne_ratio,
                value.evaluation_stride_events, value.evaluation_duck_seconds,
                value.evaluation_powered_jumps,
                value.evaluation_jump_landings, value.evaluation_spin_turns,
                value.evaluation_spin_landings,
                value.evaluation_obstacles_passed,
                value.evaluation_stable_stance,
                value.evaluation_longest_stance,
                value.evaluation_duck_recoveries,
                value.evaluation_max_joint_speed,
                value.evaluation_max_backward_brace_seconds,
                value.evaluation_hand_contacts,
                value.evaluation_climb_transfers, value.evaluation_climbs,
                value.evaluation_descents, value.evaluation_shots,
                value.evaluation_target_hits,
                value.evaluation_equipment_transitions
            };
            if (!bounded_finite(value.total_distance)
                || !bounded_finite(value.total_training_seconds)
                || value.total_distance < 0.0
                || value.total_training_seconds < 0.0
                || !std::ranges::all_of(ordinary, [](float scalar) noexcept
                    {
                        return std::isfinite(scalar)
                            && std::abs(scalar) <= 1.0e9f;
                    })
                || !finite_or_negative_infinity(value.evaluation_score)
                || !finite_or_negative_infinity(value.best_evaluation_distance)
                || !finite_or_negative_infinity(value.best_evaluation_score)
                || !finite_or_negative_infinity(value.imitation_source_score)
                || value.learning_rate < 0.0f || value.learning_rate > 1.0f
                || std::ranges::any_of(nonnegative, [](float scalar) noexcept
                    {
                        return scalar < 0.0f;
                    })
                || value.evaluation_airborne_ratio > 1.0f
                || value.imitation_weight < 0.0f
                || value.imitation_weight > 0.18f
                || static_cast<std::uint8_t>(value.evaluation_invalid_reason)
                    > static_cast<std::uint8_t>(
                        sim::InvalidMotion::structural_compression))
                return false;
            const std::array counters{
                value.update, value.environment_steps, value.total_updates,
                value.total_environment_steps, value.total_episodes,
                value.total_valid_episodes, value.total_invalid_episodes,
                value.total_resets, value.total_alternating_steps,
                value.total_falls, value.total_collisions,
                value.total_powered_jumps, value.total_landed_jumps,
                value.total_landed_flips, value.total_obstacles_passed,
                value.evaluation_quality_key, value.best_quality_key,
                value.best_update, value.evaluation_count
            };
            if (std::ranges::any_of(counters, [](std::uint64_t counter) noexcept
            {
                return counter == std::numeric_limits<std::uint64_t>::max();
            }) || value.evaluation_invalid_runs
                    == std::numeric_limits<std::uint32_t>::max())
                return false;
            const bool episode_totals_valid =
                value.total_valid_episodes <= value.total_episodes
                && value.total_invalid_episodes
                    <= value.total_episodes - value.total_valid_episodes;
            const bool evaluation_state_valid =
                (!value.evaluation_valid
                    || (value.evaluation_count > 0u
                        && value.evaluation_invalid_reason
                            == sim::InvalidMotion::none
                        && value.evaluation_rejection_mask == 0u))
                && (value.evaluation_invalid_reason
                        == sim::InvalidMotion::none
                    || !value.evaluation_valid)
                && (value.evaluation_rejection_mask == 0u
                    || !value.evaluation_valid);
            const bool imitation_state_valid = value.imitation_samples == 0u
                ? value.imitation_weight == 0.0f
                    && value.imitation_source_score
                        == -std::numeric_limits<float>::infinity()
                : value.imitation_weight >= 0.040f
                    && std::isfinite(value.imitation_source_score);
            return episode_totals_valid
                && value.update <= value.total_updates
                && value.environment_steps <= value.total_environment_steps
                && value.evaluation_count <= value.total_updates
                && value.evaluation_target_hits
                    <= value.evaluation_shots + 1.0e-5f
                && evaluation_state_valid
                && imitation_state_valid;
        }

        [[nodiscard]] ActionMask mask_from_bits(std::uint32_t bits) noexcept
        {
            ActionMask result{};
            for (std::size_t index = 0; index < result.size(); ++index)
                result[index] = (bits & (std::uint32_t{ 1u } << index)) != 0u;
            return result;
        }

        [[nodiscard]] RawPpoReceiptContext receipt_context_for(
            const PpoTrainer::CheckpointData& data) noexcept
        {
            const ActionMask mask = mask_from_bits(data.action_mask_bits);
            return RawPpoReceiptContext{
                .training_semantics = data.training_semantics,
                .rig_signature = data.rig_signature,
                .best_update = data.metrics.best_update,
                .checkpoint_optimizer_step = data.optimizer_step,
                .retained_parameters_digest = data.best_parameters_digest,
                .retained_active_actor_digest =
                    PolicyNetwork::active_actor_parameter_digest(
                        data.best_parameters, mask),
                .action_mask_bits = data.action_mask_bits,
                .stage = static_cast<std::uint8_t>(data.stage),
                .gait_task = static_cast<std::uint8_t>(data.gait_task),
                .equipment_directive =
                    static_cast<std::uint8_t>(data.equipment_directive)
            };
        }

        [[nodiscard]] bool checkpoint_payload_valid(
            const PpoTrainer::CheckpointData& data, bool require_current,
            std::string& error)
        {
            const std::size_t expected = PolicyNetwork{}.parameter_count();
            const std::uint32_t valid_action_bits =
                (std::uint32_t{ 1u } << sim::action_count) - 1u;
            if (data.training_semantics == 0u || data.rig_signature == 0u
                || data.parameters.size() != expected
                || (require_current && (data.first_moment.size() != expected
                    || data.second_moment.size() != expected
                    || (!data.best_parameters.empty()
                        && data.best_parameters.size() != expected)))
                || !finite_vector(data.parameters)
                || !finite_vector(data.first_moment)
                || !finite_vector(data.second_moment)
                || !finite_vector(data.best_parameters)
                || !finite_vector(data.reward_history)
                || !finite_vector(data.speed_history)
                || !std::isfinite(data.difficulty)
                || data.difficulty < 0.10f || data.difficulty > 1.0f
                || static_cast<std::size_t>(data.stage) >= sim::course_stage_count
                || static_cast<std::size_t>(data.gait_task) >= sim::gait_task_count
                || static_cast<std::size_t>(data.equipment_directive)
                    >= sim::equipment_directive_count
                || data.action_mask_bits == 0u
                || (data.action_mask_bits & ~valid_action_bits) != 0u
                || data.optimizer_step == std::numeric_limits<std::uint64_t>::max()
                || data.random_state == 0u
                || data.lesson_update == std::numeric_limits<std::uint64_t>::max()
                || !metrics_valid(data.metrics)
                || data.parameters_digest != parameter_digest(data.parameters)
                || data.best_parameters_digest
                    != parameter_digest(data.best_parameters))
            {
                error = "Checkpoint contains invalid values, dimensions, context, or digests.";
                return false;
            }
            if (data.best_parameters.empty())
            {
                if (data.best_guidance_mode != sim::GuidanceMode::assisted
                    || data.raw_ppo_receipt.present()
                    || data.metrics.best_update != 0u
                    || data.metrics.best_quality_key != 0u)
                {
                    error = "Empty retained policy carries stale quality or provenance.";
                    return false;
                }
                return true;
            }
            if (data.metrics.best_update == 0u
                || data.metrics.best_update > data.metrics.update
                || data.metrics.best_quality_key == 0u
                || !std::isfinite(data.metrics.best_evaluation_score)
                || !std::isfinite(data.metrics.best_evaluation_distance))
            {
                error = "Retained policy quality evidence is inconsistent.";
                return false;
            }
            if (data.best_guidance_mode == sim::GuidanceMode::raw_policy_audit)
            {
                if (!raw_ppo_receipt_valid(data.raw_ppo_receipt,
                        receipt_context_for(data)))
                {
                    error = "Raw retained policy provenance is inconsistent.";
                    return false;
                }
            }
            else if (data.best_guidance_mode != sim::GuidanceMode::assisted
                || data.raw_ppo_receipt.present())
            {
                error = "Retained policy guidance or receipt is invalid.";
                return false;
            }
            return true;
        }

        [[nodiscard]] std::uint64_t metrics_digest(
            const TrainingMetrics& value) noexcept
        {
            std::uint64_t hash = 0xA4D7'924C'51E8'6B03ULL;
            const auto add = [&hash](std::uint64_t scalar) noexcept
            {
                hash = provenance_hash_u64(hash, scalar);
            };
            const auto add_float = [&add](float scalar) noexcept
            {
                add(std::bit_cast<std::uint32_t>(scalar));
            };
            const auto add_double = [&add](double scalar) noexcept
            {
                add(std::bit_cast<std::uint64_t>(scalar));
            };
            add(value.update);
            add(value.environment_steps);
            add(value.total_updates);
            add(value.total_environment_steps);
            add(value.total_episodes);
            add(value.total_valid_episodes);
            add(value.total_invalid_episodes);
            add(value.total_resets);
            add(value.total_alternating_steps);
            add(value.total_falls);
            add(value.total_collisions);
            add(value.total_powered_jumps);
            add(value.total_landed_jumps);
            add(value.total_landed_flips);
            add(value.total_obstacles_passed);
            add_double(value.total_distance);
            add_double(value.total_training_seconds);
            add_float(value.mean_reward);
            add_float(value.mean_episode_distance);
            add_float(value.mean_speed);
            add_float(value.policy_loss);
            add_float(value.value_loss);
            add_float(value.entropy);
            add_float(value.learning_rate);
            add_float(value.evaluation_reward);
            add_float(value.evaluation_distance);
            add_float(value.evaluation_speed);
            add_float(value.evaluation_score);
            add_float(value.evaluation_survival);
            add_float(value.evaluation_collisions);
            add_float(value.evaluation_airborne_ratio);
            add_float(value.evaluation_stride_events);
            add_float(value.evaluation_duck_seconds);
            add_float(value.evaluation_powered_jumps);
            add_float(value.evaluation_jump_landings);
            add_float(value.evaluation_spin_turns);
            add_float(value.evaluation_spin_landings);
            add_float(value.evaluation_obstacles_passed);
            add_float(value.evaluation_stable_stance);
            add_float(value.evaluation_longest_stance);
            add_float(value.evaluation_duck_recoveries);
            add_float(value.evaluation_max_joint_speed);
            add_float(value.evaluation_max_backward_brace_seconds);
            add_float(value.evaluation_hand_contacts);
            add_float(value.evaluation_climb_transfers);
            add_float(value.evaluation_climbs);
            add_float(value.evaluation_descents);
            add_float(value.evaluation_shots);
            add_float(value.evaluation_target_hits);
            add_float(value.evaluation_equipment_transitions);
            add(value.evaluation_quality_key);
            add(value.evaluation_rejection_mask);
            add(value.evaluation_invalid_runs);
            add(static_cast<std::uint8_t>(value.evaluation_invalid_reason));
            add(value.evaluation_valid ? 1u : 0u);
            add_float(value.best_evaluation_distance);
            add_float(value.best_evaluation_score);
            add(value.best_quality_key);
            add(value.best_update);
            add(value.evaluation_count);
            add(value.imitation_samples);
            add_float(value.imitation_weight);
            add_float(value.imitation_source_score);
            return hash;
        }

        [[nodiscard]] std::uint64_t checkpoint_public_digest(
            const PpoTrainer::CheckpointData& data,
            std::uint8_t origin) noexcept
        {
            std::uint64_t hash = provenance_hash_u64(
                0x8D41'58A7'75B1'C3E9ULL, origin);
            const auto add = [&hash](std::uint64_t value) noexcept
            {
                hash = provenance_hash_u64(hash, value);
            };
            add(data.training_semantics);
            add(data.rig_signature);
            add(parameter_digest(data.parameters));
            add(parameter_digest(data.first_moment));
            add(parameter_digest(data.second_moment));
            add(parameter_digest(data.best_parameters));
            add(parameter_digest(data.reward_history));
            add(parameter_digest(data.speed_history));
            add(data.parameters_digest);
            add(data.best_parameters_digest);
            add(data.whole_content_digest);
            add(data.optimizer_step);
            add(data.random_state);
            add(data.lesson_update);
            add(static_cast<std::uint8_t>(data.best_guidance_mode));
            add(static_cast<std::uint8_t>(data.stage));
            add(static_cast<std::uint8_t>(data.gait_task));
            add(static_cast<std::uint8_t>(data.equipment_directive));
            add(data.action_mask_bits);
            add(std::bit_cast<std::uint32_t>(data.difficulty));
            add(data.raw_ppo_receipt.provenance_digest());
            add(metrics_digest(data.metrics));
            return hash;
        }

        bool write_raw_receipt(std::ofstream& output,
            const RawPpoReceipt& value)
        {
            const auto write_bool = [&output](bool flag)
            {
                const std::uint8_t encoded = flag ? 1u : 0u;
                return write_value(output, encoded);
            };
            return write_value(output, value.schema())
                && write_value(output, value.training_semantics())
                && write_value(output, value.rig_signature())
                && write_value(output, value.handoff_update())
                && write_value(output, value.rollout_lesson_update())
                && write_value(output, value.optimizer_update())
                && write_value(output, value.global_update())
                && write_value(output, value.optimizer_step_before())
                && write_value(output, value.optimizer_step())
                && write_value(output, value.expected_sample_count())
                && write_value(output, value.origin_counts())
                && write_value(output, value.self_imitation_updates())
                && write_value(output, value.best_anchor_updates())
                && write_value(output, value.elapsed_phase_samples())
                && write_value(output, value.forward_intent_samples())
                && write_value(output, value.reverse_intent_samples())
                && write_value(output, value.stop_intent_samples())
                && write_value(output, value.absolute_advantage_sum())
                && write_value(output, value.parameters_before_digest())
                && write_value(output, value.parameters_after_digest())
                && write_value(output, value.active_actor_before_digest())
                && write_value(output, value.active_actor_after_digest())
                && write_value(output, value.evaluated_parameters_digest())
                && write_value(output, value.retained_parameters_digest())
                && write_value(output, value.action_mask_bits())
                && write_value(output, value.observation_scrub_schema())
                && write_value(output, value.stage())
                && write_value(output, value.gait_task())
                && write_value(output, value.equipment_directive())
                && write_bool(value.environments_reset_at_handoff())
                && write_bool(value.actions_reset_at_handoff())
                && write_bool(value.episodes_reset_at_handoff())
                && write_bool(value.whole_cohort_raw())
                && write_value(output, value.provenance_digest());
        }

        [[nodiscard]] bool checkpoint_prefix_digest(
            const std::filesystem::path& path, std::uintmax_t suffix_size,
            std::uint64_t& digest) noexcept
        {
            std::error_code size_error{};
            const std::uintmax_t size = std::filesystem::file_size(
                path, size_error);
            if (size_error || size < suffix_size
                || size - suffix_size > static_cast<std::uintmax_t>(
                    std::numeric_limits<std::streamsize>::max()))
                return false;
            std::ifstream input(path, std::ios::binary);
            if (!input)
                return false;
            std::vector<std::byte> bytes(static_cast<std::size_t>(
                size - suffix_size));
            if (!bytes.empty())
            {
                input.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                if (!input)
                    return false;
            }
            digest = provenance_hash_bytes(bytes);
            return true;
        }

        bool write_metrics(std::ofstream& output, const TrainingMetrics& value)
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
                && write_value(output, value.evaluation_max_backward_brace_seconds)
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
                && write_flag(output, value.evaluation_valid)
                && write_value(output, value.best_evaluation_distance)
                && write_value(output, value.best_evaluation_score)
                && write_value(output, value.best_quality_key)
                && write_value(output, value.best_update)
                && write_value(output, value.evaluation_count)
                && write_value(output, value.imitation_samples)
                && write_value(output, value.imitation_weight)
                && write_value(output, value.imitation_source_score);
        }

        bool read_metrics(std::ifstream& input, TrainingMetrics& value)
        {
            return read_value(input, value.update)
                && read_value(input, value.environment_steps)
                && read_value(input, value.total_updates)
                && read_value(input, value.total_environment_steps)
                && read_value(input, value.total_episodes)
                && read_value(input, value.total_valid_episodes)
                && read_value(input, value.total_invalid_episodes)
                && read_value(input, value.total_resets)
                && read_value(input, value.total_alternating_steps)
                && read_value(input, value.total_falls)
                && read_value(input, value.total_collisions)
                && read_value(input, value.total_powered_jumps)
                && read_value(input, value.total_landed_jumps)
                && read_value(input, value.total_landed_flips)
                && read_value(input, value.total_obstacles_passed)
                && read_value(input, value.total_distance)
                && read_value(input, value.total_training_seconds)
                && read_value(input, value.mean_reward)
                && read_value(input, value.mean_episode_distance)
                && read_value(input, value.mean_speed)
                && read_value(input, value.policy_loss)
                && read_value(input, value.value_loss)
                && read_value(input, value.entropy)
                && read_value(input, value.learning_rate)
                && read_value(input, value.evaluation_reward)
                && read_value(input, value.evaluation_distance)
                && read_value(input, value.evaluation_speed)
                && read_value(input, value.evaluation_score)
                && read_value(input, value.evaluation_survival)
                && read_value(input, value.evaluation_collisions)
                && read_value(input, value.evaluation_airborne_ratio)
                && read_value(input, value.evaluation_stride_events)
                && read_value(input, value.evaluation_duck_seconds)
                && read_value(input, value.evaluation_powered_jumps)
                && read_value(input, value.evaluation_jump_landings)
                && read_value(input, value.evaluation_spin_turns)
                && read_value(input, value.evaluation_spin_landings)
                && read_value(input, value.evaluation_obstacles_passed)
                && read_value(input, value.evaluation_stable_stance)
                && read_value(input, value.evaluation_longest_stance)
                && read_value(input, value.evaluation_duck_recoveries)
                && read_value(input, value.evaluation_max_joint_speed)
                && read_value(input, value.evaluation_max_backward_brace_seconds)
                && read_value(input, value.evaluation_hand_contacts)
                && read_value(input, value.evaluation_climb_transfers)
                && read_value(input, value.evaluation_climbs)
                && read_value(input, value.evaluation_descents)
                && read_value(input, value.evaluation_shots)
                && read_value(input, value.evaluation_target_hits)
                && read_value(input, value.evaluation_equipment_transitions)
                && read_value(input, value.evaluation_quality_key)
                && read_value(input, value.evaluation_rejection_mask)
                && read_value(input, value.evaluation_invalid_runs)
                && read_value(input, value.evaluation_invalid_reason)
                && read_flag(input, value.evaluation_valid)
                && read_value(input, value.best_evaluation_distance)
                && read_value(input, value.best_evaluation_score)
                && read_value(input, value.best_quality_key)
                && read_value(input, value.best_update)
                && read_value(input, value.evaluation_count)
                && read_value(input, value.imitation_samples)
                && read_value(input, value.imitation_weight)
                && read_value(input, value.imitation_source_score);
        }

        bool read_legacy_metrics(std::ifstream& input, TrainingMetrics& value)
        {
            return read_value(input, value.update)
                && read_value(input, value.environment_steps)
                && read_value(input, value.total_updates)
                && read_value(input, value.total_environment_steps)
                && read_value(input, value.total_episodes)
                && read_value(input, value.total_valid_episodes)
                && read_value(input, value.total_invalid_episodes)
                && read_value(input, value.total_resets)
                && read_value(input, value.total_alternating_steps)
                && read_value(input, value.total_falls)
                && read_value(input, value.total_collisions)
                && read_value(input, value.total_powered_jumps)
                && read_value(input, value.total_landed_jumps)
                && read_value(input, value.total_landed_flips)
                && read_value(input, value.total_obstacles_passed)
                && read_value(input, value.total_distance)
                && read_value(input, value.total_training_seconds)
                && read_value(input, value.mean_reward)
                && read_value(input, value.mean_episode_distance)
                && read_value(input, value.mean_speed)
                && read_value(input, value.policy_loss)
                && read_value(input, value.value_loss)
                && read_value(input, value.entropy)
                && read_value(input, value.learning_rate)
                && read_value(input, value.evaluation_reward)
                && read_value(input, value.evaluation_distance)
                && read_value(input, value.evaluation_speed)
                && read_value(input, value.evaluation_score)
                && read_value(input, value.evaluation_survival)
                && read_value(input, value.evaluation_collisions)
                && read_value(input, value.evaluation_airborne_ratio)
                && read_value(input, value.evaluation_stride_events)
                && read_value(input, value.evaluation_duck_seconds)
                && read_value(input, value.evaluation_powered_jumps)
                && read_value(input, value.evaluation_jump_landings)
                && read_value(input, value.evaluation_spin_turns)
                && read_value(input, value.evaluation_spin_landings)
                && read_value(input, value.evaluation_obstacles_passed)
                && read_value(input, value.evaluation_stable_stance)
                && read_value(input, value.evaluation_longest_stance)
                && read_value(input, value.evaluation_duck_recoveries)
                && read_value(input, value.evaluation_max_joint_speed)
                && read_value(input, value.evaluation_hand_contacts)
                && read_value(input, value.evaluation_climb_transfers)
                && read_value(input, value.evaluation_climbs)
                && read_value(input, value.evaluation_descents)
                && read_value(input, value.evaluation_shots)
                && read_value(input, value.evaluation_target_hits)
                && read_value(input, value.evaluation_equipment_transitions)
                && read_value(input, value.evaluation_quality_key)
                && read_value(input, value.evaluation_rejection_mask)
                && read_value(input, value.evaluation_invalid_runs)
                && read_value(input, value.evaluation_invalid_reason)
                && read_value(input, value.evaluation_valid)
                && read_value(input, value.best_evaluation_distance)
                && read_value(input, value.best_evaluation_score)
                && read_value(input, value.best_quality_key)
                && read_value(input, value.best_update)
                && read_value(input, value.evaluation_count)
                && read_value(input, value.imitation_samples)
                && read_value(input, value.imitation_weight)
                && read_value(input, value.imitation_source_score);
        }

        bool read_v0727_metrics(std::ifstream& input, TrainingMetrics& value)
        {
            return read_value(input, value.update)
                && read_value(input, value.environment_steps)
                && read_value(input, value.total_updates)
                && read_value(input, value.total_environment_steps)
                && read_value(input, value.total_episodes)
                && read_value(input, value.total_valid_episodes)
                && read_value(input, value.total_invalid_episodes)
                && read_value(input, value.total_resets)
                && read_value(input, value.total_alternating_steps)
                && read_value(input, value.total_falls)
                && read_value(input, value.total_collisions)
                && read_value(input, value.total_powered_jumps)
                && read_value(input, value.total_landed_jumps)
                && read_value(input, value.total_landed_flips)
                && read_value(input, value.total_obstacles_passed)
                && read_value(input, value.total_distance)
                && read_value(input, value.total_training_seconds)
                && read_value(input, value.mean_reward)
                && read_value(input, value.mean_episode_distance)
                && read_value(input, value.mean_speed)
                && read_value(input, value.policy_loss)
                && read_value(input, value.value_loss)
                && read_value(input, value.entropy)
                && read_value(input, value.learning_rate)
                && read_value(input, value.evaluation_reward)
                && read_value(input, value.evaluation_distance)
                && read_value(input, value.evaluation_speed)
                && read_value(input, value.evaluation_score)
                && read_value(input, value.evaluation_survival)
                && read_value(input, value.evaluation_collisions)
                && read_value(input, value.evaluation_airborne_ratio)
                && read_value(input, value.evaluation_stride_events)
                && read_value(input, value.evaluation_duck_seconds)
                && read_value(input, value.evaluation_powered_jumps)
                && read_value(input, value.evaluation_jump_landings)
                && read_value(input, value.evaluation_spin_turns)
                && read_value(input, value.evaluation_spin_landings)
                && read_value(input, value.evaluation_obstacles_passed)
                && read_value(input, value.evaluation_stable_stance)
                && read_value(input, value.evaluation_longest_stance)
                && read_value(input, value.evaluation_duck_recoveries)
                && read_value(input, value.evaluation_max_joint_speed)
                && read_value(input, value.evaluation_quality_key)
                && read_value(input, value.evaluation_rejection_mask)
                && read_value(input, value.evaluation_invalid_runs)
                && read_value(input, value.evaluation_valid)
                && read_value(input, value.best_evaluation_distance)
                && read_value(input, value.best_evaluation_score)
                && read_value(input, value.best_quality_key)
                && read_value(input, value.best_update)
                && read_value(input, value.evaluation_count)
                && read_value(input, value.imitation_samples)
                && read_value(input, value.imitation_weight)
                && read_value(input, value.imitation_source_score);
        }
        bool migrate_v0727_parameters(std::span<const float> source,
            std::vector<float>& destination)
        {
            constexpr std::size_t hidden = PolicyNetwork::hidden_size;
            constexpr std::size_t legacy_w1 = 0u;
            constexpr std::size_t legacy_b1 = legacy_w1 + hidden * v0727_input_size;
            constexpr std::size_t legacy_w2 = legacy_b1 + hidden;
            constexpr std::size_t legacy_b2 = legacy_w2 + hidden * hidden;
            constexpr std::size_t legacy_actor_w = legacy_b2 + hidden;
            constexpr std::size_t legacy_actor_b = legacy_actor_w + v0727_output_size * hidden;
            constexpr std::size_t legacy_value_w = legacy_actor_b + v0727_output_size;
            constexpr std::size_t legacy_value_b = legacy_value_w + hidden;
            constexpr std::size_t legacy_log_std = legacy_value_b + 1u;

            constexpr std::size_t current_w1 = 0u;
            constexpr std::size_t current_b1 = current_w1 + hidden * PolicyNetwork::input_size;
            constexpr std::size_t current_w2 = current_b1 + hidden;
            constexpr std::size_t current_b2 = current_w2 + hidden * hidden;
            constexpr std::size_t current_actor_w = current_b2 + hidden;
            constexpr std::size_t current_actor_b = current_actor_w
                + PolicyNetwork::output_size * hidden;
            constexpr std::size_t current_value_w = current_actor_b + PolicyNetwork::output_size;
            constexpr std::size_t current_value_b = current_value_w + hidden;
            constexpr std::size_t current_log_std = current_value_b + 1u;
            constexpr std::size_t current_total = current_log_std + PolicyNetwork::output_size;
            static_assert(legacy_log_std + v0727_output_size == v0727_parameter_count);

            if (source.size() != v0727_parameter_count
                || destination.size() != current_total)
                return false;
            for (std::size_t row = 0; row < hidden; ++row)
            {
                std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(
                        legacy_w1 + row * v0727_input_size),
                    v0727_input_size,
                    destination.begin() + static_cast<std::ptrdiff_t>(
                        current_w1 + row * PolicyNetwork::input_size));
            }
            std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(legacy_b1), hidden,
                destination.begin() + static_cast<std::ptrdiff_t>(current_b1));
            std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(legacy_w2), hidden * hidden,
                destination.begin() + static_cast<std::ptrdiff_t>(current_w2));
            std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(legacy_b2), hidden,
                destination.begin() + static_cast<std::ptrdiff_t>(current_b2));
            std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(legacy_actor_w),
                v0727_output_size * hidden,
                destination.begin() + static_cast<std::ptrdiff_t>(current_actor_w));
            std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(legacy_actor_b),
                v0727_output_size,
                destination.begin() + static_cast<std::ptrdiff_t>(current_actor_b));
            std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(legacy_value_w), hidden,
                destination.begin() + static_cast<std::ptrdiff_t>(current_value_w));
            destination[current_value_b] = source[legacy_value_b];
            std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(legacy_log_std),
                v0727_output_size,
                destination.begin() + static_cast<std::ptrdiff_t>(current_log_std));
            return true;
        }
    }

    void PpoTrainer::seal_checkpoint(CheckpointData& data,
        CheckpointData::Origin origin) noexcept
    {
        data.origin_ = origin;
        data.artifact_seal_ = checkpoint_public_digest(
            data, static_cast<std::uint8_t>(origin));
    }

    bool PpoTrainer::checkpoint_seal_valid(const CheckpointData& data) noexcept
    {
        return data.origin_ != CheckpointData::Origin::none
            && data.artifact_seal_ != 0u
            && data.artifact_seal_ == checkpoint_public_digest(
                data, static_cast<std::uint8_t>(data.origin_));
    }

    PpoTrainer::CheckpointData PpoTrainer::checkpoint_data() const
    {
        CheckpointData data{};
        data.training_semantics = training_semantics_version;
        data.rig_signature = blueprint_.signature();
        data.parameters = policy_.parameters();
        data.first_moment = adam_.first_moment;
        data.second_moment = adam_.second_moment;
        data.best_parameters = best_parameters_;
        data.best_guidance_mode = best_policy_guidance_mode_;
        data.raw_ppo_receipt = best_raw_ppo_receipt_;
        data.parameters_digest = parameter_digest(data.parameters);
        data.best_parameters_digest = parameter_digest(data.best_parameters);
        data.reward_history = reward_history_;
        data.speed_history = speed_history_;
        data.optimizer_step = adam_.step;
        data.random_state = random_state_;
        data.lesson_update = lesson_update_;
        data.metrics = metrics_;
        data.stage = course_stage_;
        data.gait_task = gait_task_;
        data.equipment_directive = equipment_directive_;
        data.action_mask_bits = action_mask_bits(policy_.action_mask());
        data.difficulty = course_difficulty_;
        seal_checkpoint(data, CheckpointData::Origin::live_trainer);
        return data;
    }

    PpoTrainer::VerifiedCheckpoint PpoTrainer::verified_checkpoint() const
    {
        CheckpointData data = checkpoint_data();
        std::string error{};
        if (!checkpoint_payload_valid(data, true, error))
            return {};
        return VerifiedCheckpoint{ std::move(data) };
    }

    bool PpoTrainer::write_checkpoint_data(const CheckpointData& data,
        const std::filesystem::path& path, std::string& error,
        std::uint64_t* written_content_digest)
    {
        if (!checkpoint_seal_valid(data)
            || data.origin_ == CheckpointData::Origin::legacy_transfer)
        {
            error = "Checkpoint was not issued by a trainer or exact loader.";
            return false;
        }
        if (!checkpoint_payload_valid(data, true, error))
            return false;
        const std::uint64_t parameters_digest = data.parameters_digest;
        const std::uint64_t best_parameters_digest = data.best_parameters_digest;
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "Could not open checkpoint for writing: " + temporary.string();
            return false;
        }
        const auto stage = static_cast<std::uint8_t>(data.stage);
        const auto gait_task = static_cast<std::uint8_t>(data.gait_task);
        const auto equipment_directive = static_cast<std::uint8_t>(
            data.equipment_directive);
        const auto best_guidance = static_cast<std::uint8_t>(
            data.best_guidance_mode);
        output.write(checkpoint_magic.data(), static_cast<std::streamsize>(checkpoint_magic.size()));
        const bool ok = write_value(output, data.training_semantics)
            && write_value(output, data.rig_signature)
            && write_value(output, data.optimizer_step)
            && write_value(output, data.random_state)
            && write_value(output, stage)
            && write_value(output, gait_task)
            && write_value(output, equipment_directive)
            && write_value(output, data.action_mask_bits)
            && write_value(output, data.difficulty)
            && write_value(output, data.lesson_update)
            && write_metrics(output, data.metrics)
            && write_vector(output, data.parameters)
            && write_vector(output, data.first_moment)
            && write_vector(output, data.second_moment)
            && write_vector(output, data.best_parameters)
            && write_vector(output, data.reward_history)
            && write_vector(output, data.speed_history)
            && write_value(output, best_guidance)
            && write_value(output, parameters_digest)
            && write_value(output, best_parameters_digest)
            && write_raw_receipt(output, data.raw_ppo_receipt);
        output.close();
        if (!ok || !output)
        {
            error = "Failed while writing checkpoint: " + path.string();
            std::error_code cleanup_error{};
            std::filesystem::remove(temporary, cleanup_error);
            return false;
        }
        std::uint64_t whole_content_digest{};
        if (!checkpoint_prefix_digest(temporary, 0u,
                whole_content_digest))
        {
            error = "Could not checksum checkpoint: " + temporary.string();
            std::error_code cleanup_error{};
            std::filesystem::remove(temporary, cleanup_error);
            return false;
        }
        std::ofstream digest_output(temporary,
            std::ios::binary | std::ios::app);
        if (!digest_output
            || !write_value(digest_output, whole_content_digest))
        {
            error = "Could not append checkpoint checksum: "
                + temporary.string();
            digest_output.close();
            std::error_code cleanup_error{};
            std::filesystem::remove(temporary, cleanup_error);
            return false;
        }
        digest_output.close();
        if (!digest_output)
        {
            error = "Failed while finalizing checkpoint checksum: "
                + temporary.string();
            std::error_code cleanup_error{};
            std::filesystem::remove(temporary, cleanup_error);
            return false;
        }
        if (!persistence::atomic_replace_file(temporary, path, error))
        {
            std::error_code cleanup_error{};
            std::filesystem::remove(temporary, cleanup_error);
            return false;
        }
        if (written_content_digest)
            *written_content_digest = whole_content_digest;
        return true;
    }

    bool PpoTrainer::read_checkpoint_data(const std::filesystem::path& path,
        CheckpointData& data, std::string& error)
    {
        data = {};
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "Could not open checkpoint: " + path.string();
            return false;
        }
        std::array<char, 8> magic{};
        input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
        const bool current = magic == checkpoint_magic;
        const bool v0744 = magic == v0744_checkpoint_magic;
        const bool v0743 = magic == v0743_checkpoint_magic;
        std::uint8_t stage{};
        std::uint8_t gait_task{};
        std::uint8_t equipment_directive{};
        std::uint8_t best_guidance{};
        const bool v0742 = magic == v0742_checkpoint_magic;
        const bool v0741 = magic == v0741_checkpoint_magic;
        const bool v0740 = magic == v0740_checkpoint_magic;
        const bool v0739 = magic == v0739_checkpoint_magic;
        const bool v0738 = magic == v0738_checkpoint_magic;
        const bool v0737 = magic == v0737_checkpoint_magic;
        const bool v0736 = magic == v0736_checkpoint_magic;
        const bool v0735 = magic == v0735_checkpoint_magic;
        const bool v0734 = magic == v0734_checkpoint_magic;
        const bool v0733 = magic == v0733_checkpoint_magic;
        const bool v0732 = magic == v0732_checkpoint_magic;
        const bool v0731 = magic == v0731_checkpoint_magic;
        const bool v0730 = magic == v0730_checkpoint_magic;
        const bool v0727 = magic == v0727_checkpoint_magic;
        const bool legacy_magic = v0744 || v0743 || v0742 || v0741
            || v0740 || v0739 || v0738 || v0737 || v0736
            || v0735 || v0734 || v0733 || v0732 || v0731 || v0730
            || v0727;
        const bool legacy_layout = v0730 || v0727;
        if (!input || (!current && !legacy_magic)
            || !read_value(input, data.training_semantics)
            || !read_value(input, data.rig_signature)
            || !read_value(input, data.optimizer_step)
            || !read_value(input, data.random_state)
            || !read_value(input, stage)
            || (current && !read_value(input, gait_task))
            || (current && !read_value(input, equipment_directive))
            || (current && !read_value(input, data.action_mask_bits))
            || !read_value(input, data.difficulty)
            || (!legacy_layout && !read_value(input, data.lesson_update))
            || !(v0727 ? read_v0727_metrics(input, data.metrics)
                       : current ? read_metrics(input, data.metrics)
                                 : read_legacy_metrics(input, data.metrics))
            || !read_vector(input, data.parameters, 2'000'000)
            || !read_vector(input, data.first_moment, 2'000'000)
            || !read_vector(input, data.second_moment, 2'000'000)
            || !read_vector(input, data.best_parameters, 2'000'000)
            || !read_vector(input, data.reward_history, 10'000)
            || !read_vector(input, data.speed_history, 10'000)
            || (current && !read_value(input, best_guidance))
            || stage >= (v0727 ? 8u : (v0738 || v0737 || v0736) ? sim::course_stage_count
                : legacy_magic ? 11u : sim::course_stage_count)
            || (current && gait_task >= sim::gait_task_count)
            || (current && equipment_directive
                >= sim::equipment_directive_count)
            || (current && best_guidance
                > static_cast<std::uint8_t>(
                    sim::GuidanceMode::raw_policy_audit))
            || !std::isfinite(data.difficulty)
            || data.difficulty < 0.10f || data.difficulty > 1.0f
            || !finite_vector(data.parameters)
            || !finite_vector(data.first_moment)
            || !finite_vector(data.second_moment)
            || !finite_vector(data.best_parameters)
            || !finite_vector(data.reward_history)
            || !finite_vector(data.speed_history))
        {
            error = "Invalid or truncated Epoch2DWalkEngine checkpoint.";
            return false;
        }
        data.stage = static_cast<sim::CourseStage>(stage);
        data.gait_task = current
            ? static_cast<sim::GaitTask>(gait_task) : sim::GaitTask::walk;
        data.equipment_directive = current
            ? static_cast<sim::EquipmentDirective>(equipment_directive)
            : sim::EquipmentDirective::passive;
        if (!current)
            data.action_mask_bits = action_mask_bits(default_action_mask());
        data.best_guidance_mode = !current
            ? sim::GuidanceMode::assisted
            : static_cast<sim::GuidanceMode>(best_guidance);
        if (data.best_parameters.empty())
            data.best_guidance_mode = sim::GuidanceMode::assisted;
        if (legacy_layout)
            data.lesson_update = 0u;
        if (!current)
        {
            data.raw_ppo_receipt = {};
            data.parameters_digest = parameter_digest(data.parameters);
            data.best_parameters_digest =
                parameter_digest(data.best_parameters);
            char trailing{};
            input.read(&trailing, 1);
            if (input.gcount() != 0 || !input.eof())
            {
                error = "Legacy EPPO checkpoint contains trailing content.";
                return false;
            }
            seal_checkpoint(data, CheckpointData::Origin::legacy_transfer);
            error = "LEGACY EPPO44-OR-OLDER CHECKPOINT - UNPROVEN WEIGHT TRANSFER ONLY";
            return true;
        }

        RawPpoReceipt& receipt = data.raw_ppo_receipt;
        std::uint8_t environments_reset{};
        std::uint8_t actions_reset{};
        std::uint8_t episodes_reset{};
        std::uint8_t whole_cohort_raw{};
        if (!read_value(input, data.parameters_digest)
            || !read_value(input, data.best_parameters_digest)
            || !read_value(input, receipt.schema_)
            || !read_value(input, receipt.training_semantics_)
            || !read_value(input, receipt.rig_signature_)
            || !read_value(input, receipt.handoff_update_)
            || !read_value(input, receipt.rollout_lesson_update_)
            || !read_value(input, receipt.optimizer_update_)
            || !read_value(input, receipt.global_update_)
            || !read_value(input, receipt.optimizer_step_before_)
            || !read_value(input, receipt.optimizer_step_)
            || !read_value(input, receipt.expected_sample_count_)
            || !read_value(input, receipt.origin_counts_)
            || !read_value(input, receipt.self_imitation_updates_)
            || !read_value(input, receipt.best_anchor_updates_)
            || !read_value(input, receipt.elapsed_phase_samples_)
            || !read_value(input, receipt.forward_intent_samples_)
            || !read_value(input, receipt.reverse_intent_samples_)
            || !read_value(input, receipt.stop_intent_samples_)
            || !read_value(input, receipt.absolute_advantage_sum_)
            || !read_value(input, receipt.parameters_before_digest_)
            || !read_value(input, receipt.parameters_after_digest_)
            || !read_value(input, receipt.active_actor_before_digest_)
            || !read_value(input, receipt.active_actor_after_digest_)
            || !read_value(input, receipt.evaluated_parameters_digest_)
            || !read_value(input, receipt.retained_parameters_digest_)
            || !read_value(input, receipt.action_mask_bits_)
            || !read_value(input, receipt.observation_scrub_schema_)
            || !read_value(input, receipt.stage_)
            || !read_value(input, receipt.gait_task_)
            || !read_value(input, receipt.equipment_directive_)
            || !read_value(input, environments_reset)
            || !read_value(input, actions_reset)
            || !read_value(input, episodes_reset)
            || !read_value(input, whole_cohort_raw)
            || !read_value(input, receipt.provenance_digest_)
            || !read_value(input, data.whole_content_digest))
        {
            error = "Invalid or truncated EPPO51 checkpoint provenance.";
            return false;
        }
        if (environments_reset > 1u || actions_reset > 1u
            || episodes_reset > 1u || whole_cohort_raw > 1u)
        {
            error = "EPPO51 checkpoint contains a non-canonical provenance flag.";
            return false;
        }
        receipt.environments_reset_at_handoff_ = environments_reset != 0u;
        receipt.actions_reset_at_handoff_ = actions_reset != 0u;
        receipt.episodes_reset_at_handoff_ = episodes_reset != 0u;
        receipt.whole_cohort_raw_ = whole_cohort_raw != 0u;
        char trailing{};
        input.read(&trailing, 1);
        if (input.gcount() != 0 || !input.eof())
        {
            error = "EPPO51 checkpoint contains trailing content.";
            return false;
        }
        std::uint64_t calculated_content_digest{};
        if (!checkpoint_prefix_digest(path, sizeof(std::uint64_t),
                calculated_content_digest)
            || calculated_content_digest != data.whole_content_digest
            || data.parameters_digest != parameter_digest(data.parameters)
            || data.best_parameters_digest
                != parameter_digest(data.best_parameters))
        {
            error = "EPPO51 checkpoint checksum verification failed.";
            return false;
        }
        if (!checkpoint_payload_valid(data, true, error))
            return false;
        seal_checkpoint(data, CheckpointData::Origin::exact_storage);
        error.clear();
        return true;
    }

    bool PpoTrainer::read_verified_checkpoint(
        const std::filesystem::path& path, VerifiedCheckpoint& checkpoint,
        std::string& error)
    {
        CheckpointData data{};
        if (!read_checkpoint_data(path, data, error)
            || data.origin_ != CheckpointData::Origin::exact_storage
            || !checkpoint_seal_valid(data))
        {
            checkpoint = {};
            if (error.empty())
                error = "Checkpoint is not exact verified EPPO51 storage.";
            return false;
        }
        checkpoint = VerifiedCheckpoint{ std::move(data) };
        return true;
    }

    PpoTrainer::CheckpointData PpoTrainer::retarget_checkpoint_for_rig(
        CheckpointData data, std::uint64_t rig_signature) noexcept
    {
        if (!checkpoint_seal_valid(data))
            return {};
        data.rig_signature = rig_signature;
        data.best_parameters.clear();
        data.best_guidance_mode = sim::GuidanceMode::assisted;
        data.raw_ppo_receipt = {};
        data.parameters_digest = parameter_digest(data.parameters);
        data.best_parameters_digest = parameter_digest(data.best_parameters);
        data.whole_content_digest = 0u;
        data.metrics.evaluation_reward = 0.0f;
        data.metrics.evaluation_distance = 0.0f;
        data.metrics.evaluation_speed = 0.0f;
        data.metrics.evaluation_score = -std::numeric_limits<float>::infinity();
        data.metrics.evaluation_survival = 0.0f;
        data.metrics.evaluation_collisions = 0.0f;
        data.metrics.evaluation_airborne_ratio = 0.0f;
        data.metrics.evaluation_stride_events = 0.0f;
        data.metrics.evaluation_duck_seconds = 0.0f;
        data.metrics.evaluation_powered_jumps = 0.0f;
        data.metrics.evaluation_jump_landings = 0.0f;
        data.metrics.evaluation_spin_turns = 0.0f;
        data.metrics.evaluation_spin_landings = 0.0f;
        data.metrics.evaluation_obstacles_passed = 0.0f;
        data.metrics.evaluation_stable_stance = 0.0f;
        data.metrics.evaluation_longest_stance = 0.0f;
        data.metrics.evaluation_duck_recoveries = 0.0f;
        data.metrics.evaluation_max_joint_speed = 0.0f;
        data.metrics.evaluation_max_backward_brace_seconds = 0.0f;
        data.metrics.evaluation_hand_contacts = 0.0f;
        data.metrics.evaluation_climb_transfers = 0.0f;
        data.metrics.evaluation_climbs = 0.0f;
        data.metrics.evaluation_descents = 0.0f;
        data.metrics.evaluation_shots = 0.0f;
        data.metrics.evaluation_target_hits = 0.0f;
        data.metrics.evaluation_equipment_transitions = 0.0f;
        data.metrics.evaluation_quality_key = 0u;
        data.metrics.evaluation_rejection_mask = 0u;
        data.metrics.evaluation_invalid_runs = 0u;
        data.metrics.evaluation_invalid_reason = sim::InvalidMotion::none;
        data.metrics.evaluation_valid = false;
        data.metrics.best_evaluation_distance = -std::numeric_limits<float>::infinity();
        data.metrics.best_evaluation_score = -std::numeric_limits<float>::infinity();
        data.metrics.best_quality_key = 0u;
        data.metrics.best_update = 0u;
        seal_checkpoint(data, CheckpointData::Origin::live_trainer);
        return data;
    }

    bool PpoTrainer::apply_checkpoint_data(CheckpointData data, std::string& error,
        bool transfer_only)
    {
        if (!transfer_only && (!checkpoint_seal_valid(data)
                || data.origin_ == CheckpointData::Origin::legacy_transfer))
        {
            error = "UNPROVEN CHECKPOINT - RESUME BLOCKED; USE EXPLICIT WEIGHT TRANSFER";
            return false;
        }
        if (!transfer_only && data.training_semantics != training_semantics_version)
        {
            error = "INCOMPATIBLE TRAINING SEMANTICS - RESUME BLOCKED; USE EXPLICIT WEIGHT TRANSFER";
            return false;
        }
        bool migrated_v0727 = false;
        if (transfer_only && data.parameters.size() == v0727_parameter_count)
        {
            std::vector<float> migrated = policy_.parameters();
            if (!migrate_v0727_parameters(data.parameters, migrated))
            {
                error = "Invalid v0.7.27 checkpoint dimensions.";
                return false;
            }
            data.parameters = std::move(migrated);
            migrated_v0727 = true;
        }
        if (!transfer_only && !checkpoint_payload_valid(data, true, error))
            return false;
        const std::size_t expected = policy_.parameter_count();
        const bool optimizer_dimensions_valid = data.first_moment.size() == expected
            && data.second_moment.size() == expected
            && (data.best_parameters.empty() || data.best_parameters.size() == expected);
        if (data.parameters.size() != expected
            || (!transfer_only && !optimizer_dimensions_valid))
        {
            error = "Invalid or incompatible checkpoint dimensions.";
            return false;
        }
        if (!finite_vector(data.parameters)
            || (!transfer_only && (!finite_vector(data.first_moment)
                || !finite_vector(data.second_moment)
                || !finite_vector(data.best_parameters)
                || !finite_vector(data.reward_history)
                || !finite_vector(data.speed_history)
                || data.parameters_digest
                    != parameter_digest(data.parameters)
                || data.best_parameters_digest
                    != parameter_digest(data.best_parameters))))
        {
            error = "Checkpoint contains invalid values or parameter digests.";
            return false;
        }
        if (!transfer_only && data.rig_signature != blueprint_.signature())
        {
            error = std::format("RIG MISMATCH {:016X} != {:016X}.",
                data.rig_signature, blueprint_.signature());
            return false;
        }
        if (!transfer_only)
        {
            sim::Environment context{ blueprint_, 0x7515EA1u };
            context.set_course(data.stage, data.difficulty);
            context.set_gait_task(data.gait_task);
            context.set_equipment_directive(data.equipment_directive);
            if (!data.best_parameters.empty()
                && data.action_mask_bits
                != action_mask_bits(action_mask_for(context, data.stage)))
            {
                error = "Checkpoint action mask does not match its execution context.";
                return false;
            }
        }
        policy_.parameters() = std::move(data.parameters);
        preview_policy_.parameters() = policy_.parameters();
        if (transfer_only)
        {
            reset_training_state();
            controller_state_ = ControllerState::transferred;
            if (migrated_v0727)
                error = "V0.7.27 ANATOMY WEIGHTS TRANSFERRED - NEW MATERIAL/EQUIPMENT CHANNELS NEUTRAL; OPTIMIZER, BEST, AND MASTERY RESET";
            else
                error = data.training_semantics == training_semantics_version
                    ? "WEIGHTS TRANSFERRED - OPTIMIZER AND BEST STATE RESET"
                    : "LEGACY WEIGHTS TRANSFERRED - SEMANTICS, OPTIMIZER, BEST, AND MASTERY RESET";
            return true;
        }
        adam_.first_moment = std::move(data.first_moment);
        adam_.second_moment = std::move(data.second_moment);
        adam_.step = data.optimizer_step;
        random_state_ = data.random_state;
        lesson_update_ = data.lesson_update;
        gait_task_ = data.gait_task;
        equipment_directive_ = data.equipment_directive;
        metrics_ = data.metrics;
        best_parameters_ = std::move(data.best_parameters);
        best_policy_guidance_mode_ = best_parameters_.empty()
            ? sim::GuidanceMode::assisted : data.best_guidance_mode;
        best_raw_ppo_receipt_ = best_policy_guidance_mode_
                == sim::GuidanceMode::raw_policy_audit
            ? data.raw_ppo_receipt : RawPpoReceipt{};
        preview_policy_.parameters() = best_parameters_.empty()
            ? policy_.parameters() : best_parameters_;
        reward_history_ = std::move(data.reward_history);
        speed_history_ = std::move(data.speed_history);
        course_stage_ = data.stage;
        course_difficulty_ = data.difficulty;
        preview_guidance_mode_ = best_parameters_.empty()
            ? sim::GuidanceMode::assisted : best_policy_guidance_mode_;
        for (sim::Environment& environment : environments_)
        {
            environment.set_course(course_stage_, course_difficulty_);
            environment.set_gait_task(gait_task_);
            environment.set_equipment_directive(equipment_directive_);
        }
        preview_.set_course(course_stage_, course_difficulty_);
        preview_.set_gait_task(gait_task_);
        preview_.set_equipment_directive(equipment_directive_);
        transition_action_mask(action_mask_for(preview_, course_stage_));
        preview_.set_guidance_mode(preview_guidance_mode_);
        preview_.set_physical_guidance_authority(
            preview_guidance_mode_ == sim::GuidanceMode::assisted
            ? lesson_teacher_authority(lesson_update_, course_stage_,
                preview_.blueprint())
            : 0.0f);
        if (best_policy_guidance_mode_ == sim::GuidanceMode::raw_policy_audit)
            clear_self_imitation_prior();
        else
            refresh_self_imitation_prior();
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (auto& action : rollout_previous_actions_)
            action.fill(0.0f);
        const bool raw_human_walk = course_stage_ == sim::CourseStage::uneven
            && gait_task_ == sim::GaitTask::walk
            && blueprint_.human_casual_gait_plan()
            && lesson_update_
                >= foundational_walk_teacher_handoff_update(blueprint_);
        for (std::size_t index = 0; index < environments_.size(); ++index)
        {
            const sim::GuidanceMode restored_guidance = raw_human_walk
                ? sim::GuidanceMode::raw_policy_audit
                : sim::GuidanceMode::assisted;
            environments_[index].set_guidance_mode(restored_guidance);
            environments_[index].set_physical_guidance_authority(
                restored_guidance == sim::GuidanceMode::assisted
                ? lesson_teacher_authority(lesson_update_, course_stage_,
                    environments_[index].blueprint())
                : 0.0f);
            environments_[index].reset(0x1000u + index * 7919u);
        }
        raw_handoff_reset_complete_ = raw_human_walk;
        if (raw_human_walk)
            clear_foundational_teacher_prior();
        staged_raw_ppo_evidence_ = {};
        last_raw_ppo_receipt_ = best_raw_ppo_receipt_;
        preview_.reset(0xDEADBEEFu + metrics_.update);
        controller_state_ = ControllerState::resumed;
        error.clear();
        return true;
    }

    bool PpoTrainer::apply_verified_checkpoint(
        const VerifiedCheckpoint& checkpoint, std::string& error)
    {
        if (!checkpoint.valid())
        {
            error = "Verified checkpoint artifact is empty.";
            return false;
        }
        return apply_checkpoint_data(checkpoint.data(), error, false);
    }

    bool PpoTrainer::import_lifetime_ledger(const TrainingMetrics& lifetime,
        std::string& error) noexcept
    {
        if (!std::isfinite(lifetime.total_distance)
            || !std::isfinite(lifetime.total_training_seconds)
            || lifetime.total_distance < 0.0 || lifetime.total_training_seconds < 0.0)
        {
            error = "INVALID LEGACY LIFETIME LEDGER - NON-FINITE OR NEGATIVE TOTAL";
            return false;
        }

        metrics_.total_updates = std::max(metrics_.total_updates, lifetime.total_updates);
        metrics_.total_environment_steps = std::max(
            metrics_.total_environment_steps, lifetime.total_environment_steps);
        metrics_.total_episodes = std::max(metrics_.total_episodes, lifetime.total_episodes);
        metrics_.total_valid_episodes = std::max(
            metrics_.total_valid_episodes, lifetime.total_valid_episodes);
        metrics_.total_invalid_episodes = std::max(
            metrics_.total_invalid_episodes, lifetime.total_invalid_episodes);
        metrics_.total_resets = std::max(metrics_.total_resets, lifetime.total_resets);
        metrics_.total_alternating_steps = std::max(
            metrics_.total_alternating_steps, lifetime.total_alternating_steps);
        metrics_.total_falls = std::max(metrics_.total_falls, lifetime.total_falls);
        metrics_.total_collisions = std::max(
            metrics_.total_collisions, lifetime.total_collisions);
        metrics_.total_powered_jumps = std::max(
            metrics_.total_powered_jumps, lifetime.total_powered_jumps);
        metrics_.total_landed_jumps = std::max(
            metrics_.total_landed_jumps, lifetime.total_landed_jumps);
        metrics_.total_landed_flips = std::max(
            metrics_.total_landed_flips, lifetime.total_landed_flips);
        metrics_.total_obstacles_passed = std::max(
            metrics_.total_obstacles_passed, lifetime.total_obstacles_passed);
        metrics_.total_distance = std::max(metrics_.total_distance, lifetime.total_distance);
        metrics_.total_training_seconds = std::max(
            metrics_.total_training_seconds, lifetime.total_training_seconds);
        metrics_.evaluation_count = std::max(
            metrics_.evaluation_count, lifetime.evaluation_count);
        error = "LEGACY LIFETIME LEDGER IMPORTED - POLICY, OPTIMIZER, BEST, AND MASTERY START FRESH";
        return true;
    }

    bool PpoTrainer::save_checkpoint(const std::filesystem::path& path, std::string& error) const
    {
        return write_checkpoint_data(checkpoint_data(), path, error);
    }

    bool PpoTrainer::load_checkpoint(const std::filesystem::path& path, std::string& error,
        bool transfer_only)
    {
        if (!transfer_only)
        {
            VerifiedCheckpoint verified{};
            return read_verified_checkpoint(path, verified, error)
                && apply_verified_checkpoint(verified, error);
        }
        CheckpointData data{};
        return read_checkpoint_data(path, data, error)
            && apply_checkpoint_data(std::move(data), error, true);
    }
}
