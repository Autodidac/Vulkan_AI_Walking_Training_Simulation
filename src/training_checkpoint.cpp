#include "ppo.hpp"

#include <array>
#include <fstream>
#include <format>
#include <type_traits>

namespace epochrunner::rl
{
    namespace
    {
        constexpr std::array<char, 8> checkpoint_magic{ 'E', 'P', 'P', 'O', '2', '7', '\0', '\1' };

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
                && write_value(output, value.evaluation_invalid_runs)
                && write_value(output, value.evaluation_valid)
                && write_value(output, value.best_evaluation_distance)
                && write_value(output, value.best_evaluation_score)
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
                && read_value(input, value.evaluation_invalid_runs)
                && read_value(input, value.evaluation_valid)
                && read_value(input, value.best_evaluation_distance)
                && read_value(input, value.best_evaluation_score)
                && read_value(input, value.best_update)
                && read_value(input, value.evaluation_count)
                && read_value(input, value.imitation_samples)
                && read_value(input, value.imitation_weight)
                && read_value(input, value.imitation_source_score);
        }
    }

    PpoTrainer::CheckpointData PpoTrainer::checkpoint_data() const
    {
        CheckpointData data{};
        data.rig_signature = blueprint_.signature();
        data.parameters = policy_.parameters();
        data.first_moment = adam_.first_moment;
        data.second_moment = adam_.second_moment;
        data.best_parameters = best_parameters_;
        data.reward_history = reward_history_;
        data.speed_history = speed_history_;
        data.optimizer_step = adam_.step;
        data.random_state = random_state_;
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
        const bool ok = write_value(output, data.rig_signature)
            && write_value(output, data.optimizer_step)
            && write_value(output, data.random_state)
            && write_value(output, stage)
            && write_value(output, data.difficulty)
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
        if (!input || magic != checkpoint_magic
            || !read_value(input, data.rig_signature)
            || !read_value(input, data.optimizer_step)
            || !read_value(input, data.random_state)
            || !read_value(input, stage)
            || !read_value(input, data.difficulty)
            || !read_metrics(input, data.metrics)
            || !read_vector(input, data.parameters, 2'000'000)
            || !read_vector(input, data.first_moment, 2'000'000)
            || !read_vector(input, data.second_moment, 2'000'000)
            || !read_vector(input, data.best_parameters, 2'000'000)
            || !read_vector(input, data.reward_history, 10'000)
            || !read_vector(input, data.speed_history, 10'000)
            || stage >= sim::course_stage_count
            || data.difficulty < 0.10f || data.difficulty > 1.0f)
        {
            error = "Invalid or incompatible EpochRunner v0.7 checkpoint.";
            return false;
        }
        data.stage = static_cast<sim::CourseStage>(stage);
        error.clear();
        return true;
    }

    bool PpoTrainer::apply_checkpoint_data(CheckpointData data, std::string& error,
        bool transfer_only)
    {
        const std::size_t expected = policy_.parameter_count();
        if (data.parameters.size() != expected
            || data.first_moment.size() != expected
            || data.second_moment.size() != expected
            || (!data.best_parameters.empty() && data.best_parameters.size() != expected))
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
        if (transfer_only)
        {
            reset_training_state();
            controller_state_ = ControllerState::transferred;
            error = "WEIGHTS TRANSFERRED - OPTIMIZER AND BEST STATE RESET";
            return true;
        }
        adam_.first_moment = std::move(data.first_moment);
        adam_.second_moment = std::move(data.second_moment);
        adam_.step = data.optimizer_step;
        random_state_ = data.random_state;
        metrics_ = data.metrics;
        best_parameters_ = std::move(data.best_parameters);
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
