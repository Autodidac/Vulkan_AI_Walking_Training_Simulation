#pragma once

#include "ppo.hpp"

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace epochrunner::rl
{
    struct AutonomyStatus
    {
        bool enabled{ true };
        sim::CourseStage stage{ sim::CourseStage::balance };
        float difficulty{ 0.25f };
        std::uint64_t rig_generation{};
        std::uint64_t accepted_rig_changes{};
        std::uint64_t rejected_rig_changes{};
        int mastery_streak{};
        int rollback_count{};
        std::size_t rollout_threads{ 1 };
        std::size_t environment_count{};
        std::string message{ "LEARNING TO BALANCE" };
    };

    class AutonomousTrainer
    {
    public:
        explicit AutonomousTrainer(const sim::CreatureBlueprint& blueprint, std::size_t environment_count = 64);
        ~AutonomousTrainer();

        AutonomousTrainer(const AutonomousTrainer&) = delete;
        AutonomousTrainer& operator=(const AutonomousTrainer&) = delete;

        void synchronize();
        void set_background_enabled(bool enabled) noexcept;
        [[nodiscard]] bool background_enabled() const noexcept { return enabled_.load(); }
        void set_updates_per_cycle(int updates) noexcept;
        [[nodiscard]] int updates_per_cycle() const noexcept { return updates_per_cycle_.load(); }

        void set_autosave_paths(std::filesystem::path checkpoint, std::filesystem::path rig,
            std::filesystem::path state);
        [[nodiscard]] bool load_autosave(std::string& message);

        void set_blueprint(const sim::CreatureBlueprint& blueprint, bool preserve_policy = false);
        void reset_policy(std::uint64_t seed = 0xC0FFEEu);
        void set_exploration(float standard_deviation) noexcept;
        void train_one_update() noexcept;
        void step_preview(float dt = 1.0f / 60.0f);
        void reset_preview(std::uint64_t seed = 0xDEADBEEFu) noexcept;

        [[nodiscard]] bool save_checkpoint(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] bool load_checkpoint(const std::filesystem::path& path, std::string& error,
            bool transfer_only = false);
        [[nodiscard]] bool restore_best_policy() noexcept;

        [[nodiscard]] const sim::Environment& preview() const noexcept { return live_.preview(); }
        [[nodiscard]] const sim::CreatureBlueprint& blueprint() const noexcept { return live_blueprint_; }
        [[nodiscard]] const TrainingMetrics& metrics() const noexcept { return cached_metrics_; }
        [[nodiscard]] const std::vector<float>& reward_history() const noexcept { return cached_reward_history_; }
        [[nodiscard]] const std::vector<float>& speed_history() const noexcept { return cached_speed_history_; }
        [[nodiscard]] std::span<const sim::Environment> environments() const noexcept { return live_.environments(); }
        [[nodiscard]] std::size_t environment_count() const noexcept { return cached_status_.environment_count; }
        [[nodiscard]] std::string_view controller_state_name() const noexcept;
        [[nodiscard]] std::uint64_t rig_signature() const noexcept { return live_blueprint_.signature(); }
        [[nodiscard]] bool has_best_policy() const noexcept { return cached_has_best_; }
        [[nodiscard]] std::uint64_t optimizer_step() const noexcept { return cached_optimizer_step_; }
        [[nodiscard]] float exploration() const noexcept { return cached_exploration_; }
        [[nodiscard]] const AutonomyStatus& autonomy_status() const noexcept { return cached_status_; }

    private:
        struct PublishedSnapshot
        {
            sim::CreatureBlueprint blueprint{};
            std::vector<float> parameters{};
            TrainingMetrics metrics{};
            std::vector<float> reward_history{};
            std::vector<float> speed_history{};
            ControllerState controller_state{ ControllerState::fresh };
            AutonomyStatus status{};
            float exploration{ 0.18f };
            std::uint64_t optimizer_step{};
            bool has_best{};
            std::uint64_t serial{};
        };

        class TrainingRoutine
        {
        public:
            struct promise_type
            {
                [[nodiscard]] TrainingRoutine get_return_object() noexcept;
                [[nodiscard]] std::suspend_always initial_suspend() const noexcept { return {}; }
                [[nodiscard]] std::suspend_always final_suspend() const noexcept { return {}; }
                [[nodiscard]] std::suspend_always yield_value(int) const noexcept { return {}; }
                void return_void() const noexcept {}
                void unhandled_exception() const noexcept;
            };

            explicit TrainingRoutine(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}
            TrainingRoutine(TrainingRoutine&& other) noexcept;
            TrainingRoutine& operator=(TrainingRoutine&& other) noexcept;
            ~TrainingRoutine();

            [[nodiscard]] bool resume();

        private:
            std::coroutine_handle<promise_type> handle_{};
        };

        [[nodiscard]] TrainingRoutine training_routine(std::stop_token stop_token);
        void worker_main(std::stop_token stop_token);
        void run_training_cycle();
        void manage_curriculum_locked();
        void attempt_rig_evolution_locked();
        [[nodiscard]] float evaluate_rig_locked(const sim::CreatureBlueprint& candidate) const;
        [[nodiscard]] sim::CreatureBlueprint mutate_rig_locked() noexcept;
        void publish_locked();
        void autosave_locked();
        void write_state_locked() const;
        void read_state_locked();
        [[nodiscard]] bool stage_mastered_locked() const noexcept;
        void advance_stage_locked();

        mutable std::mutex worker_mutex_{};
        mutable std::mutex snapshot_mutex_{};
        PpoTrainer worker_;
        PpoTrainer live_;
        sim::CreatureBlueprint live_blueprint_{};
        PublishedSnapshot published_{};
        std::uint64_t applied_serial_{};
        TrainingMetrics cached_metrics_{};
        std::vector<float> cached_reward_history_{};
        std::vector<float> cached_speed_history_{};
        ControllerState cached_controller_state_{ ControllerState::fresh };
        AutonomyStatus cached_status_{};
        float cached_exploration_{ 0.18f };
        std::uint64_t cached_optimizer_step_{};
        bool cached_has_best_{};

        std::filesystem::path autosave_checkpoint_{ "epochrunner-autosave.eppo" };
        std::filesystem::path autosave_rig_{ "epochrunner-evolved.epochrig" };
        std::filesystem::path autosave_state_{ "epochrunner-autonomy.state" };

        sim::CourseStage stage_{ sim::CourseStage::balance };
        float difficulty_{ 0.25f };
        std::uint64_t rig_generation_{};
        std::uint64_t accepted_rig_changes_{};
        std::uint64_t rejected_rig_changes_{};
        std::uint64_t last_evaluation_count_{};
        std::uint64_t last_saved_best_update_{};
        int mastery_streak_{};
        int degradation_streak_{};
        int rollback_count_{};
        std::string worker_message_{ "LEARNING TO BALANCE" };

        std::atomic_bool enabled_{ true };
        std::atomic_int updates_per_cycle_{ 2 };
        std::atomic_uint32_t requested_updates_{};
        std::jthread worker_thread_{};
    };
}
