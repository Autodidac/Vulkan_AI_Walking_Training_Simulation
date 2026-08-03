#pragma once

#include "ppo.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace runner::rl
{
    inline constexpr int mastery_lock_confirmations = 8;
    inline constexpr int balance_mastery_lock_confirmations = 3;
    inline constexpr float standing_mastery_joint_speed_limit = 10.0f;

    [[nodiscard]] inline int required_mastery_confirmations(
        sim::CourseStage stage) noexcept
    {
        return stage == sim::CourseStage::balance
            ? balance_mastery_lock_confirmations : mastery_lock_confirmations;
    }

    [[nodiscard]] inline bool strict_balance_mastery(
        const TrainingMetrics& metrics) noexcept
    {
        return metrics.evaluation_valid
            && metrics.evaluation_invalid_runs == 0u
            && metrics.evaluation_longest_stance >= standing_mastery_seconds
            && metrics.evaluation_survival >= standing_mastery_seconds
            && metrics.evaluation_spin_turns <= standing_mastery_spin_limit
            && metrics.evaluation_max_joint_speed <= standing_mastery_joint_speed_limit;
    }

    struct AutonomyStatus
    {
        bool enabled{ false };
        sim::CourseStage stage{ sim::CourseStage::balance };
        float difficulty{ 0.25f };
        std::uint64_t rig_generation{};
        std::uint64_t accepted_rig_changes{};
        std::uint64_t rejected_rig_changes{};
        int mastery_streak{};
        int rollback_count{};
        std::size_t rollout_threads{ 1 };
        std::size_t environment_count{};
        std::size_t pending_commands{};
        double updates_per_second{};
        int speed_mode{ 1 };
        bool worker_busy{};
        std::string pipeline_stage{ "IDLE" };
        std::uint32_t pipeline_stage_mask{};
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
        [[nodiscard]] bool background_enabled() const noexcept { return enabled_.load(std::memory_order_relaxed); }
        void set_updates_per_cycle(int updates) noexcept;
        [[nodiscard]] int updates_per_cycle() const noexcept { return updates_per_cycle_.load(std::memory_order_relaxed); }
        void set_autosave_paths(std::filesystem::path checkpoint, std::filesystem::path rig,
            std::filesystem::path state);
        [[nodiscard]] bool load_autosave(std::string& message);

        void set_blueprint(const sim::CreatureBlueprint& blueprint, bool preserve_policy = false);
        void reset_policy(std::uint64_t seed = 0xC0FFEEu);
        void set_exploration(float standard_deviation) noexcept;
        void train_one_update() noexcept;
        void step_preview(float dt = 1.0f / 60.0f);
        void reset_preview(std::uint64_t seed = 0xDEADBEEFu) noexcept;
        [[nodiscard]] bool save_checkpoint(const std::filesystem::path& path, std::string& error);
        [[nodiscard]] bool load_checkpoint(const std::filesystem::path& path, std::string& error,
            bool transfer_only = false);
        [[nodiscard]] bool restore_best_policy() noexcept;

        [[nodiscard]] const sim::Environment& preview() const noexcept { return live_.preview(); }
        [[nodiscard]] const sim::Environment& training_preview() const noexcept
        {
            return cached_training_preview_;
        }
        [[nodiscard]] bool has_training_preview() const noexcept { return cached_has_training_preview_; }
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
        enum class CommandType : std::uint8_t
        {
            set_blueprint,
            reset_policy,
            set_exploration,
            restore_best,
            save_checkpoint,
            apply_checkpoint,
            apply_autosave
        };

        struct PendingCommand
        {
            CommandType type{ CommandType::set_blueprint };
            sim::CreatureBlueprint blueprint{};
            bool preserve_policy{};
            std::uint64_t seed{};
            float scalar{};
            std::filesystem::path path{};
            std::shared_ptr<PpoTrainer::CheckpointData> checkpoint{};
            bool transfer_only{};
            std::uint64_t rig_generation{};
            std::uint64_t accepted_rig_changes{};
            std::uint64_t rejected_rig_changes{};
            int rollback_count{};
        };

        struct PublishedSnapshot
        {
            sim::CreatureBlueprint blueprint{};
            std::vector<float> parameters{};
            TrainingMetrics metrics{};
            std::vector<float> reward_history{};
            std::vector<float> speed_history{};
            ControllerState controller_state{ ControllerState::fresh };
            AutonomyStatus status{};
            float exploration{ 0.14f };
            std::uint64_t optimizer_step{};
            bool has_best{};
            sim::Environment training_preview{};
            bool has_training_preview{};
            std::uint64_t serial{};
        };

        enum class RoutineStage : std::uint8_t
        {
            idle,
            commands,
            rollout,
            advantages,
            optimizer,
            evaluation,
            published,
            persistence
        };

        class TrainingRoutine
        {
        public:
            struct promise_type
            {
                RoutineStage stage{ RoutineStage::idle };

                [[nodiscard]] TrainingRoutine get_return_object() noexcept;
                [[nodiscard]] std::suspend_always initial_suspend() const noexcept { return {}; }
                [[nodiscard]] std::suspend_always final_suspend() const noexcept { return {}; }
                [[nodiscard]] std::suspend_always yield_value(RoutineStage value) noexcept
                {
                    stage = value;
                    return {};
                }
                void return_void() const noexcept {}
                void unhandled_exception() const noexcept;
            };

            explicit TrainingRoutine(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}
            TrainingRoutine(TrainingRoutine&& other) noexcept;
            TrainingRoutine& operator=(TrainingRoutine&& other) noexcept;
            ~TrainingRoutine();
            [[nodiscard]] RoutineStage resume();

        private:
            std::coroutine_handle<promise_type> handle_{};
        };

        void enqueue_command(PendingCommand command);
        [[nodiscard]] bool has_pending_work() const;
        [[nodiscard]] std::size_t pending_command_count() const;
        [[nodiscard]] bool consume_update_request() noexcept;
        void apply_pending_commands();
        void apply_command_locked(PendingCommand&& command);

        [[nodiscard]] TrainingRoutine training_routine(std::stop_token stop_token);
        void worker_main(std::stop_token stop_token);
        void consume_persistence_message();
        void throttle_after_update() const;

        void manage_curriculum_locked();
        void attempt_rig_evolution_locked();
        [[nodiscard]] float evaluate_rig_locked(const sim::CreatureBlueprint& candidate) const;
        [[nodiscard]] sim::CreatureBlueprint mutate_rig_locked() noexcept;
        void publish_locked();
        void queue_autosave();
        void queue_checkpoint_save(std::filesystem::path path, PpoTrainer::CheckpointData data);
        void queue_checkpoint_load(std::filesystem::path path, bool transfer_only);
        void queue_autosave_load();
        void persistence_main(std::stop_token stop_token);
        [[nodiscard]] bool stage_mastered_locked() const noexcept;
        void advance_stage_locked();

        mutable std::mutex snapshot_mutex_{};
        mutable std::mutex command_mutex_{};
        mutable std::mutex wake_mutex_{};
        std::condition_variable_any wake_cv_{};
        std::deque<PendingCommand> command_queue_{};

        enum class PersistenceKind : std::uint8_t
        {
            save_checkpoint,
            save_autosave,
            load_checkpoint,
            load_autosave
        };

        struct PersistenceJob
        {
            PersistenceKind kind{ PersistenceKind::save_checkpoint };
            std::filesystem::path checkpoint_path{};
            std::filesystem::path rig_path{};
            std::filesystem::path state_path{};
            PpoTrainer::CheckpointData checkpoint{};
            sim::CreatureBlueprint blueprint{};
            bool transfer_only{};
            sim::CourseStage stage{ sim::CourseStage::balance };
            float difficulty{ 0.25f };
            std::uint64_t rig_generation{};
            std::uint64_t accepted_rig_changes{};
            std::uint64_t rejected_rig_changes{};
            int rollback_count{};
        };

        mutable std::mutex persistence_mutex_{};
        std::condition_variable_any persistence_cv_{};
        std::deque<PersistenceJob> persistence_queue_{};
        std::string persistence_message_{};
        std::uint64_t persistence_message_serial_{};
        std::uint64_t consumed_persistence_message_serial_{};

        PpoTrainer worker_;
        PpoTrainer live_;
        sim::CreatureBlueprint live_blueprint_{};
        sim::Environment cached_training_preview_{};
        bool cached_has_training_preview_{};
        PublishedSnapshot published_{};
        std::uint64_t applied_serial_{};
        TrainingMetrics cached_metrics_{};
        std::vector<float> cached_reward_history_{};
        std::vector<float> cached_speed_history_{};
        ControllerState cached_controller_state_{ ControllerState::fresh };
        AutonomyStatus cached_status_{};
        float cached_exploration_{ 0.14f };
        std::uint64_t cached_optimizer_step_{};
        bool cached_has_best_{};

        std::filesystem::path autosave_checkpoint_{ "runner-autosave.eppo" };
        std::filesystem::path autosave_rig_{ "runner-evolved.rig" };
        std::filesystem::path autosave_state_{ "runner-autonomy.state" };
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
        std::string worker_pipeline_stage_{ "IDLE" };
        std::uint32_t worker_pipeline_stage_mask_{};

        std::chrono::steady_clock::time_point rate_window_started_{ std::chrono::steady_clock::now() };
        std::uint64_t rate_window_updates_{};
        double worker_updates_per_second_{};

        std::atomic_bool enabled_{ false };
        std::atomic_int updates_per_cycle_{ 1 };
        std::atomic_uint32_t requested_updates_{};
        std::atomic_bool worker_busy_{ false };
        std::atomic_int64_t last_update_nanoseconds_{};
        std::jthread worker_thread_{};
        std::jthread persistence_thread_{};
    };
}
