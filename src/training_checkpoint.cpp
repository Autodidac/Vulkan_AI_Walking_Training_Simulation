#include "ppo.hpp"

#include <array>
#include <fstream>
#include <format>
#include <limits>
#include <type_traits>

namespace epochrunner::rl
{
    namespace
    {
        constexpr std::array<char, 8> checkpoint_magic{ 'E', 'P', 'P', 'O', '2', '4', '\0', '\1' };
        constexpr std::array<char, 8> legacy_magic{ 'E', 'P', 'P', 'O', '2', '3', '\0', '\1' };

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
            if (values.empty())
                return true;
            output.write(reinterpret_cast<const char*>(values.data()),
                static_cast<std::streamsize>(values.size() * sizeof(float)));
            return static_cast<bool>(output);
        }

        bool read_vector(std::ifstream& input, std::vector<float>& values, std::size_t count)
        {
            values.resize(count);
            if (values.empty())
                return true;
            input.read(reinterpret_cast<char*>(values.data()),
                static_cast<std::streamsize>(values.size() * sizeof(float)));
            return static_cast<bool>(input);
        }
    }

    bool PpoTrainer::save_checkpoint(const std::filesystem::path& path, std::string& error) const
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "Could not open checkpoint for writing: " + path.string();
            return false;
        }

        const std::uint64_t signature = blueprint_.signature();
        const std::uint64_t parameter_count = policy_.parameters().size();
        const std::uint64_t reward_count = reward_history_.size();
        const std::uint64_t speed_count = speed_history_.size();
        const std::uint64_t best_count = best_parameters_.size();
        output.write(checkpoint_magic.data(), static_cast<std::streamsize>(checkpoint_magic.size()));
        bool ok = write_value(output, signature) && write_value(output, parameter_count)
            && write_value(output, reward_count) && write_value(output, speed_count)
            && write_value(output, best_count) && write_value(output, adam_.step)
            && write_value(output, random_state_) && write_value(output, metrics_.update)
            && write_value(output, metrics_.environment_steps) && write_value(output, metrics_.best_update)
            && write_value(output, metrics_.evaluation_count)
            && write_value(output, metrics_.mean_reward) && write_value(output, metrics_.mean_episode_distance)
            && write_value(output, metrics_.mean_speed) && write_value(output, metrics_.policy_loss)
            && write_value(output, metrics_.value_loss) && write_value(output, metrics_.entropy)
            && write_value(output, metrics_.learning_rate) && write_value(output, metrics_.evaluation_reward)
            && write_value(output, metrics_.evaluation_distance) && write_value(output, metrics_.evaluation_speed)
            && write_value(output, metrics_.best_evaluation_distance)
            && write_vector(output, policy_.parameters()) && write_vector(output, adam_.first_moment)
            && write_vector(output, adam_.second_moment) && write_vector(output, best_parameters_)
            && write_vector(output, reward_history_) && write_vector(output, speed_history_);
        if (!ok)
        {
            error = "Failed while writing checkpoint: " + path.string();
            return false;
        }
        error.clear();
        return true;
    }

    bool PpoTrainer::load_checkpoint(const std::filesystem::path& path, std::string& error, bool transfer_only)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "Could not open checkpoint: " + path.string();
            return false;
        }
        std::array<char, 8> magic{};
        input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
        if (!input)
        {
            error = "Truncated checkpoint header.";
            return false;
        }
        if (magic == legacy_magic)
        {
            input.close();
            if (!transfer_only)
            {
                error = "Legacy EPPO23 stores weights only. Use TRANSFER AI; exact resume is impossible.";
                return false;
            }
            if (!policy_.load(path, error))
                return false;
            reset_training_state();
            controller_state_ = ControllerState::transferred;
            error = "LEGACY WEIGHTS TRANSFERRED - OPTIMIZER AND PROGRESS RESET";
            return true;
        }
        if (magic != checkpoint_magic)
        {
            error = "Invalid EpochRunner checkpoint.";
            return false;
        }

        std::uint64_t signature{}, parameter_count{}, reward_count{}, speed_count{}, best_count{};
        std::uint64_t adam_step{}, random_state{}, update{}, environment_steps{}, best_update{}, evaluation_count{};
        TrainingMetrics loaded{};
        bool ok = read_value(input, signature) && read_value(input, parameter_count)
            && read_value(input, reward_count) && read_value(input, speed_count)
            && read_value(input, best_count) && read_value(input, adam_step)
            && read_value(input, random_state) && read_value(input, update)
            && read_value(input, environment_steps) && read_value(input, best_update)
            && read_value(input, evaluation_count)
            && read_value(input, loaded.mean_reward) && read_value(input, loaded.mean_episode_distance)
            && read_value(input, loaded.mean_speed) && read_value(input, loaded.policy_loss)
            && read_value(input, loaded.value_loss) && read_value(input, loaded.entropy)
            && read_value(input, loaded.learning_rate) && read_value(input, loaded.evaluation_reward)
            && read_value(input, loaded.evaluation_distance) && read_value(input, loaded.evaluation_speed)
            && read_value(input, loaded.best_evaluation_distance);
        if (!ok || parameter_count != policy_.parameter_count() || reward_count > 10000
            || speed_count > 10000 || (best_count != 0 && best_count != parameter_count))
        {
            error = "Invalid or incompatible checkpoint dimensions.";
            return false;
        }

        std::vector<float> parameters, first, second, best, rewards, speeds;
        ok = read_vector(input, parameters, static_cast<std::size_t>(parameter_count))
            && read_vector(input, first, static_cast<std::size_t>(parameter_count))
            && read_vector(input, second, static_cast<std::size_t>(parameter_count))
            && read_vector(input, best, static_cast<std::size_t>(best_count))
            && read_vector(input, rewards, static_cast<std::size_t>(reward_count))
            && read_vector(input, speeds, static_cast<std::size_t>(speed_count));
        if (!ok)
        {
            error = "Truncated checkpoint data.";
            return false;
        }

        if (!transfer_only && signature != blueprint_.signature())
        {
            error = std::format("RIG MISMATCH {:016X} != {:016X}. USE TRANSFER AI ONLY IF INTENTIONAL.",
                signature, blueprint_.signature());
            return false;
        }

        policy_.parameters() = std::move(parameters);
        if (transfer_only)
        {
            reset_training_state();
            controller_state_ = ControllerState::transferred;
            error = "WEIGHTS TRANSFERRED - OPTIMIZER, METRICS, AND BEST SNAPSHOT RESET";
            return true;
        }

        adam_.first_moment = std::move(first);
        adam_.second_moment = std::move(second);
        adam_.step = adam_step;
        random_state_ = random_state;
        loaded.update = update;
        loaded.environment_steps = environment_steps;
        loaded.best_update = best_update;
        loaded.evaluation_count = evaluation_count;
        metrics_ = loaded;
        best_parameters_ = std::move(best);
        reward_history_ = std::move(rewards);
        speed_history_ = std::move(speeds);
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (std::size_t index = 0; index < environments_.size(); ++index)
            environments_[index].reset(0x1000u + index * 7919u);
        preview_.reset(0xDEADBEEFu + metrics_.update);
        controller_state_ = ControllerState::resumed;
        error.clear();
        return true;
    }
}
