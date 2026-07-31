#include "ppo.hpp"

#include <array>
#include <fstream>
#include <format>
#include <type_traits>

namespace epochrunner::rl
{
    namespace
    {
        constexpr std::array<char, 8> checkpoint_magic{ 'E', 'P', 'P', 'O', '2', '6', '\0', '\1' };
        constexpr std::array<char, 8> previous_magic{ 'E', 'P', 'P', 'O', '2', '5', '\0', '\1' };
        constexpr std::array<char, 8> previous_v24_magic{ 'E', 'P', 'P', 'O', '2', '4', '\0', '\1' };
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
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "Could not open checkpoint for writing: " + temporary.string();
            return false;
        }

        const std::uint64_t signature = blueprint_.signature();
        const std::uint64_t parameter_count = policy_.parameters().size();
        const std::uint64_t reward_count = reward_history_.size();
        const std::uint64_t speed_count = speed_history_.size();
        const std::uint64_t best_count = best_parameters_.size();
        const auto stage = static_cast<std::uint8_t>(course_stage_);
        const std::uint8_t evaluation_valid = metrics_.evaluation_valid ? 1u : 0u;

        output.write(checkpoint_magic.data(), static_cast<std::streamsize>(checkpoint_magic.size()));
        bool ok = write_value(output, signature) && write_value(output, parameter_count)
            && write_value(output, reward_count) && write_value(output, speed_count)
            && write_value(output, best_count) && write_value(output, adam_.step)
            && write_value(output, random_state_) && write_value(output, metrics_.update)
            && write_value(output, metrics_.environment_steps) && write_value(output, metrics_.best_update)
            && write_value(output, metrics_.evaluation_count)
            && write_value(output, stage) && write_value(output, course_difficulty_)
            && write_value(output, metrics_.mean_reward) && write_value(output, metrics_.mean_episode_distance)
            && write_value(output, metrics_.mean_speed) && write_value(output, metrics_.policy_loss)
            && write_value(output, metrics_.value_loss) && write_value(output, metrics_.entropy)
            && write_value(output, metrics_.learning_rate) && write_value(output, metrics_.evaluation_reward)
            && write_value(output, metrics_.evaluation_distance) && write_value(output, metrics_.evaluation_speed)
            && write_value(output, metrics_.evaluation_score) && write_value(output, metrics_.evaluation_survival)
            && write_value(output, metrics_.evaluation_collisions) && write_value(output, metrics_.evaluation_airborne_ratio)
            && write_value(output, metrics_.evaluation_stride_events) && write_value(output, metrics_.evaluation_invalid_runs)
            && write_value(output, evaluation_valid)
            && write_value(output, metrics_.best_evaluation_distance)
            && write_value(output, metrics_.best_evaluation_score)
            && write_vector(output, policy_.parameters()) && write_vector(output, adam_.first_moment)
            && write_vector(output, adam_.second_moment) && write_vector(output, best_parameters_)
            && write_vector(output, reward_history_) && write_vector(output, speed_history_);
        if (!ok)
        {
            error = "Failed while writing checkpoint: " + path.string();
            return false;
        }
        output.close();
        if (!output)
        {
            error = "Failed while finalizing checkpoint: " + temporary.string();
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
        if (magic == legacy_magic || magic == previous_magic || magic == previous_v24_magic)
        {
            error = transfer_only
                ? "OLDER CONTROLLER/CHECKPOINT IS QUARANTINED. V0.4.1 REQUIRES CLEAN GAIT VALIDATION AND WILL NOT IMPORT HOP/FLY BEST STATE."
                : "OLDER EPPO23/24/25 CHECKPOINT CANNOT RESUME: V0.4.1 FIXES THE GAIT AND MICRO-MOTION GATES.";
            return false;
        }
        if (magic != checkpoint_magic)
        {
            error = "Invalid EpochRunner checkpoint.";
            return false;
        }

        std::uint64_t signature{}, parameter_count{}, reward_count{}, speed_count{}, best_count{};
        std::uint64_t adam_step{}, random_state{}, update{}, environment_steps{}, best_update{}, evaluation_count{};
        std::uint8_t stage_value{}, evaluation_valid{};
        float difficulty{};
        TrainingMetrics loaded{};
        bool ok = read_value(input, signature) && read_value(input, parameter_count)
            && read_value(input, reward_count) && read_value(input, speed_count)
            && read_value(input, best_count) && read_value(input, adam_step)
            && read_value(input, random_state) && read_value(input, update)
            && read_value(input, environment_steps) && read_value(input, best_update)
            && read_value(input, evaluation_count)
            && read_value(input, stage_value) && read_value(input, difficulty)
            && read_value(input, loaded.mean_reward) && read_value(input, loaded.mean_episode_distance)
            && read_value(input, loaded.mean_speed) && read_value(input, loaded.policy_loss)
            && read_value(input, loaded.value_loss) && read_value(input, loaded.entropy)
            && read_value(input, loaded.learning_rate) && read_value(input, loaded.evaluation_reward)
            && read_value(input, loaded.evaluation_distance) && read_value(input, loaded.evaluation_speed)
            && read_value(input, loaded.evaluation_score) && read_value(input, loaded.evaluation_survival)
            && read_value(input, loaded.evaluation_collisions) && read_value(input, loaded.evaluation_airborne_ratio)
            && read_value(input, loaded.evaluation_stride_events) && read_value(input, loaded.evaluation_invalid_runs)
            && read_value(input, evaluation_valid)
            && read_value(input, loaded.best_evaluation_distance)
            && read_value(input, loaded.best_evaluation_score);
        if (!ok || parameter_count != policy_.parameter_count() || reward_count > 10000
            || speed_count > 10000 || (best_count != 0 && best_count != parameter_count)
            || stage_value >= sim::course_stage_count || difficulty < 0.10f || difficulty > 1.0f)
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
            error = std::format("RIG MISMATCH {:016X} != {:016X}. AUTOPILOT WILL NOT SILENTLY RESUME ANOTHER BODY.",
                signature, blueprint_.signature());
            return false;
        }

        policy_.parameters() = std::move(parameters);
        if (transfer_only)
        {
            reset_training_state();
            controller_state_ = ControllerState::transferred;
            error = "WEIGHTS TRANSFERRED - OPTIMIZER, CURRICULUM METRICS, AND BEST SNAPSHOT RESET";
            return true;
        }

        course_stage_ = static_cast<sim::CourseStage>(stage_value);
        course_difficulty_ = difficulty;
        for (sim::Environment& environment : environments_)
            environment.set_course(course_stage_, course_difficulty_);
        preview_.set_course(course_stage_, course_difficulty_);
        adam_.first_moment = std::move(first);
        adam_.second_moment = std::move(second);
        adam_.step = adam_step;
        random_state_ = random_state;
        loaded.update = update;
        loaded.environment_steps = environment_steps;
        loaded.best_update = best_update;
        loaded.evaluation_count = evaluation_count;
        loaded.evaluation_valid = evaluation_valid != 0;
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
