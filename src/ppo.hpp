#pragma once

#include "simulation.hpp"

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace epochrunner::rl
{
    struct TrainingMetrics
    {
        std::uint64_t update{};
        std::uint64_t environment_steps{};
        float mean_reward{};
        float mean_episode_distance{};
        float mean_speed{};
        float policy_loss{};
        float value_loss{};
        float entropy{};
        float learning_rate{ 3.0e-4f };

        float evaluation_reward{};
        float evaluation_distance{};
        float evaluation_speed{};
        float evaluation_score{ -std::numeric_limits<float>::infinity() };
        float evaluation_survival{};
        float evaluation_collisions{};
        float evaluation_airborne_ratio{};
        float evaluation_stride_events{};
        std::uint32_t evaluation_invalid_runs{};
        bool evaluation_valid{};

        float best_evaluation_distance{ -std::numeric_limits<float>::infinity() };
        float best_evaluation_score{ -std::numeric_limits<float>::infinity() };
        std::uint64_t best_update{};
        std::uint64_t evaluation_count{};
    };

    enum class ControllerState : std::uint8_t
    {
        fresh,
        training,
        resumed,
        transferred
    };

    class PolicyNetwork
    {
    public:
        static constexpr std::size_t hidden_size = 64;
        static constexpr std::size_t input_size = sim::observation_count;
        static constexpr std::size_t output_size = sim::action_count;

        struct Evaluation
        {
            std::array<float, output_size> mean{};
            float value{};
        };

        PolicyNetwork();
        explicit PolicyNetwork(std::uint64_t seed);

        [[nodiscard]] Evaluation evaluate(std::span<const float, input_size> observation) const noexcept;
        [[nodiscard]] std::array<float, output_size> deterministic_action(
            std::span<const float, input_size> observation) const noexcept;
        [[nodiscard]] std::size_t parameter_count() const noexcept { return parameters_.size(); }
        [[nodiscard]] const std::vector<float>& parameters() const noexcept { return parameters_; }
        [[nodiscard]] std::vector<float>& parameters() noexcept { return parameters_; }

        void zero_gradients() noexcept;
        void accumulate_gradient(
            std::span<const float, input_size> observation,
            std::span<const float, output_size> action,
            float old_log_probability,
            float advantage,
            float target_value,
            float clip_range,
            float value_coefficient,
            float entropy_coefficient,
            float& policy_loss,
            float& value_loss,
            float& entropy) noexcept;

        [[nodiscard]] const std::vector<float>& gradients() const noexcept { return gradients_; }
        [[nodiscard]] std::vector<float>& gradients() noexcept { return gradients_; }
        [[nodiscard]] std::array<float, output_size> standard_deviation() const noexcept;
        void set_exploration(float standard_deviation) noexcept;
        [[nodiscard]] float mean_exploration() const noexcept;
        [[nodiscard]] float log_probability(
            std::span<const float, output_size> action,
            const Evaluation& evaluation) const noexcept;

        [[nodiscard]] bool save(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] bool load(const std::filesystem::path& path, std::string& error);

    private:
        struct Layout
        {
            std::size_t w1{};
            std::size_t b1{};
            std::size_t w2{};
            std::size_t b2{};
            std::size_t actor_w{};
            std::size_t actor_b{};
            std::size_t value_w{};
            std::size_t value_b{};
            std::size_t log_std{};
            std::size_t total{};
        };

        [[nodiscard]] static consteval Layout make_layout() noexcept
        {
            Layout result{};
            result.w1 = 0;
            result.b1 = result.w1 + hidden_size * input_size;
            result.w2 = result.b1 + hidden_size;
            result.b2 = result.w2 + hidden_size * hidden_size;
            result.actor_w = result.b2 + hidden_size;
            result.actor_b = result.actor_w + output_size * hidden_size;
            result.value_w = result.actor_b + output_size;
            result.value_b = result.value_w + hidden_size;
            result.log_std = result.value_b + 1;
            result.total = result.log_std + output_size;
            return result;
        }
        [[nodiscard]] float random_normal() noexcept;

        static const Layout layout_;
        std::vector<float> parameters_{};
        std::vector<float> gradients_{};
        std::uint64_t random_state_{ 1 };
    };

    class PpoTrainer
    {
    public:
        explicit PpoTrainer(const sim::CreatureBlueprint& blueprint,
            std::size_t environment_count = 64,
            bool enable_rollout_workers = true);
        ~PpoTrainer();

        PpoTrainer(const PpoTrainer&) = delete;
        PpoTrainer& operator=(const PpoTrainer&) = delete;

        void set_blueprint(const sim::CreatureBlueprint& blueprint, bool preserve_policy = false);
        void set_course(sim::CourseStage stage, float difficulty, bool preserve_best = true);
        void reset_policy(std::uint64_t seed = 0xC0FFEEu);
        void set_exploration(float standard_deviation) noexcept;
        void set_cpu_mode(int mode) noexcept;
        [[nodiscard]] int cpu_mode() const noexcept { return cpu_mode_; }
        [[nodiscard]] bool save_checkpoint(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] bool load_checkpoint(const std::filesystem::path& path, std::string& error,
            bool transfer_only = false);
        [[nodiscard]] bool restore_best_policy() noexcept;
        void train_one_update();
        void step_preview(float dt = 1.0f / 60.0f);
        void reset_preview(std::uint64_t seed = 0xDEADBEEFu) noexcept;

        [[nodiscard]] const PolicyNetwork& policy() const noexcept { return policy_; }
        [[nodiscard]] PolicyNetwork& policy() noexcept { return policy_; }
        [[nodiscard]] const sim::Environment& preview() const noexcept { return preview_; }
        [[nodiscard]] const sim::CreatureBlueprint& blueprint() const noexcept { return blueprint_; }
        [[nodiscard]] const TrainingMetrics& metrics() const noexcept { return metrics_; }
        [[nodiscard]] const std::vector<float>& reward_history() const noexcept { return reward_history_; }
        [[nodiscard]] const std::vector<float>& speed_history() const noexcept { return speed_history_; }
        [[nodiscard]] std::size_t environment_count() const noexcept { return environments_.size(); }
        [[nodiscard]] std::span<const sim::Environment> environments() const noexcept { return environments_; }
        [[nodiscard]] ControllerState controller_state() const noexcept { return controller_state_; }
        [[nodiscard]] std::string_view controller_state_name() const noexcept;
        [[nodiscard]] std::uint64_t rig_signature() const noexcept { return blueprint_.signature(); }
        [[nodiscard]] bool has_best_policy() const noexcept { return !best_parameters_.empty(); }
        [[nodiscard]] const std::vector<float>& best_policy_parameters() const noexcept
        {
            return best_parameters_;
        }
        [[nodiscard]] std::uint64_t optimizer_step() const noexcept { return adam_.step; }
        [[nodiscard]] float exploration() const noexcept { return policy_.mean_exploration(); }
        [[nodiscard]] std::size_t rollout_worker_count() const noexcept { return active_worker_count_; }
        [[nodiscard]] std::size_t maximum_worker_count() const noexcept { return rollout_worker_count_; }
        [[nodiscard]] sim::CourseStage course_stage() const noexcept { return course_stage_; }
        [[nodiscard]] float course_difficulty() const noexcept { return course_difficulty_; }

    private:
        struct Transition
        {
            std::array<float, sim::observation_count> observation{};
            std::array<float, sim::action_count> action{};
            float log_probability{};
            float value{};
            float reward{};
            float advantage{};
            float return_value{};
            bool terminal{};
        };

        struct AdamState
        {
            std::vector<float> first_moment{};
            std::vector<float> second_moment{};
            std::uint64_t step{};
        };

        struct RolloutTotals
        {
            float accumulated_speed{};
            float completed_reward{};
            float completed_distance{};
            std::size_t completed_episodes{};
        };

        struct ParallelState;

        [[nodiscard]] float random_uniform() noexcept;
        [[nodiscard]] float random_normal() noexcept;
        [[nodiscard]] std::array<float, sim::action_count> sample_action(
            const PolicyNetwork::Evaluation& evaluation,
            std::uint64_t& random_state,
            float& log_probability) const noexcept;
        void update_policy();
        void evaluate_policy();
        void reset_training_state(bool clear_best = true) noexcept;
        void apply_adam(float learning_rate, float gradient_scale);
        void append_history(std::vector<float>& history, float value);
        [[nodiscard]] RolloutTotals collect_rollout_partition(std::size_t worker_index,
            std::size_t worker_count, std::uint64_t update_seed);
        void rollout_worker_main(std::size_t worker_index, std::stop_token stop_token);

        void initialize_parallel_workers();
        void shutdown_parallel_workers() noexcept;
        void parallel_accumulate_batch(
            const std::vector<std::size_t>& indices,
            std::size_t begin,
            std::size_t end,
            float clip_range,
            float value_coefficient,
            float entropy_coefficient,
            float& policy_loss,
            float& value_loss,
            float& entropy);
        void parallel_evaluate_policy();

        static constexpr std::size_t rollout_horizon = 128;

        sim::CreatureBlueprint blueprint_{};
        std::vector<sim::Environment> environments_{};
        sim::Environment preview_{};
        PolicyNetwork policy_{};
        AdamState adam_{};
        std::vector<Transition> rollout_{};
        std::vector<float> episode_rewards_{};
        std::vector<float> episode_distances_{};
        std::vector<float> reward_history_{};
        std::vector<float> speed_history_{};
        std::vector<float> best_parameters_{};
        TrainingMetrics metrics_{};
        ControllerState controller_state_{ ControllerState::fresh };
        sim::CourseStage course_stage_{ sim::CourseStage::balance };
        float course_difficulty_{ 0.25f };
        int cpu_mode_{ 4 };
        std::size_t active_worker_count_{ 1 };
        std::size_t rollout_worker_count_{ 1 };
        std::size_t rollout_active_worker_count_{ 1 };
        std::vector<RolloutTotals> rollout_worker_totals_{};
        std::mutex rollout_mutex_{};
        std::condition_variable_any rollout_start_cv_{};
        std::condition_variable rollout_done_cv_{};
        std::uint64_t rollout_generation_{};
        std::uint64_t rollout_update_seed_{};
        std::size_t rollout_completed_{};
        std::uint64_t random_state_{ 0x12345678ABCDEFu };
        std::vector<std::jthread> rollout_workers_{};
        std::shared_ptr<ParallelState> parallel_{};
    };
}
