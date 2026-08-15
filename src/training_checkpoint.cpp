#include "ppo.hpp"

#include <algorithm>

#include <array>
#include <cmath>
#include <fstream>
#include <format>
#include <type_traits>

namespace runner::rl
{
    namespace
    {
        constexpr std::array<char, 8> checkpoint_magic{ 'E', 'P', 'P', 'O', '3', '4', '\0', '\1' };
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

        template <typename T>
        bool read_value(std::ifstream& input, T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            input.read(reinterpret_cast<char*>(&value), sizeof(T));
            return static_cast<bool>(input);
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

    PpoTrainer::CheckpointData PpoTrainer::checkpoint_data() const
    {
        CheckpointData data{};
        data.training_semantics = training_semantics_version;
        data.rig_signature = blueprint_.signature();
        data.parameters = policy_.parameters();
        data.first_moment = adam_.first_moment;
        data.second_moment = adam_.second_moment;
        data.best_parameters = best_parameters_;
        data.reward_history = reward_history_;
        data.speed_history = speed_history_;
        data.optimizer_step = adam_.step;
        data.random_state = random_state_;
        data.lesson_update = lesson_update_;
        data.metrics = metrics_;
        data.stage = course_stage_;
        data.difficulty = course_difficulty_;
        return data;
    }

    bool PpoTrainer::write_checkpoint_data(const CheckpointData& data,
        const std::filesystem::path& path, std::string& error)
    {
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "Could not open checkpoint for writing: " + temporary.string();
            return false;
        }
        const auto stage = static_cast<std::uint8_t>(data.stage);
        output.write(checkpoint_magic.data(), static_cast<std::streamsize>(checkpoint_magic.size()));
        const bool ok = write_value(output, data.training_semantics)
            && write_value(output, data.rig_signature)
            && write_value(output, data.optimizer_step)
            && write_value(output, data.random_state)
            && write_value(output, stage)
            && write_value(output, data.difficulty)
            && write_value(output, data.lesson_update)
            && write_metrics(output, data.metrics)
            && write_vector(output, data.parameters)
            && write_vector(output, data.first_moment)
            && write_vector(output, data.second_moment)
            && write_vector(output, data.best_parameters)
            && write_vector(output, data.reward_history)
            && write_vector(output, data.speed_history);
        output.close();
        if (!ok || !output)
        {
            error = "Failed while writing checkpoint: " + path.string();
            return false;
        }
        std::error_code filesystem_error{};
        std::filesystem::remove(path, filesystem_error);
        filesystem_error.clear();
        std::filesystem::rename(temporary, path, filesystem_error);
        if (filesystem_error)
        {
            error = "Could not replace checkpoint atomically: " + filesystem_error.message();
            return false;
        }
        error.clear();
        return true;
    }

    bool PpoTrainer::read_checkpoint_data(const std::filesystem::path& path,
        CheckpointData& data, std::string& error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "Could not open checkpoint: " + path.string();
            return false;
        }
        std::array<char, 8> magic{};
        input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
        std::uint8_t stage{};
        const bool v0733 = magic == v0733_checkpoint_magic;
        const bool v0732 = magic == v0732_checkpoint_magic;
        const bool v0731 = magic == v0731_checkpoint_magic;
        const bool v0730 = magic == v0730_checkpoint_magic;
        const bool v0727 = magic == v0727_checkpoint_magic;
        const bool legacy_magic = v0733 || v0732 || v0731 || v0730 || v0727;
        const bool legacy_layout = v0730 || v0727;
        if (!input || (magic != checkpoint_magic && !legacy_magic)
            || !read_value(input, data.training_semantics)
            || !read_value(input, data.rig_signature)
            || !read_value(input, data.optimizer_step)
            || !read_value(input, data.random_state)
            || !read_value(input, stage)
            || !read_value(input, data.difficulty)
            || (!legacy_layout && !read_value(input, data.lesson_update))
            || !(v0727 ? read_v0727_metrics(input, data.metrics)
                       : read_metrics(input, data.metrics))
            || !read_vector(input, data.parameters, 2'000'000)
            || !read_vector(input, data.first_moment, 2'000'000)
            || !read_vector(input, data.second_moment, 2'000'000)
            || !read_vector(input, data.best_parameters, 2'000'000)
            || !read_vector(input, data.reward_history, 10'000)
            || !read_vector(input, data.speed_history, 10'000)
            || stage >= (v0727 ? 8u : sim::course_stage_count)
            || data.difficulty < 0.10f || data.difficulty > 1.0f)
        {
            error = "Invalid or truncated Runner checkpoint.";
            return false;
        }
        data.stage = static_cast<sim::CourseStage>(stage);
        if (legacy_layout)
            data.lesson_update = 0u;
        error.clear();
        return true;
    }

    PpoTrainer::CheckpointData PpoTrainer::retarget_checkpoint_for_rig(
        CheckpointData data, std::uint64_t rig_signature) noexcept
    {
        data.rig_signature = rig_signature;
        data.best_parameters.clear();
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
        return data;
    }

    bool PpoTrainer::apply_checkpoint_data(CheckpointData data, std::string& error,
        bool transfer_only)
    {
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
        if (!transfer_only && data.rig_signature != blueprint_.signature())
        {
            error = std::format("RIG MISMATCH {:016X} != {:016X}.",
                data.rig_signature, blueprint_.signature());
            return false;
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
        metrics_ = data.metrics;
        best_parameters_ = std::move(data.best_parameters);
        preview_policy_.parameters() = best_parameters_.empty()
            ? policy_.parameters() : best_parameters_;
        reward_history_ = std::move(data.reward_history);
        speed_history_ = std::move(data.speed_history);
        course_stage_ = data.stage;
        course_difficulty_ = data.difficulty;
        for (sim::Environment& environment : environments_)
            environment.set_course(course_stage_, course_difficulty_);
        preview_.set_course(course_stage_, course_difficulty_);
        refresh_self_imitation_prior();
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (auto& action : rollout_previous_actions_)
            action.fill(0.0f);
        for (std::size_t index = 0; index < environments_.size(); ++index)
            environments_[index].reset(0x1000u + index * 7919u);
        preview_.reset(0xDEADBEEFu + metrics_.update);
        controller_state_ = ControllerState::resumed;
        error.clear();
        return true;
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
        CheckpointData data{};
        return read_checkpoint_data(path, data, error)
            && apply_checkpoint_data(std::move(data), error, transfer_only);
    }
}
