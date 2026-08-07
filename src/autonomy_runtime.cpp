#include "autonomy.hpp"
#include "preview_sync.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <thread>
#include <utility>

namespace runner::rl
{
    AutonomousTrainer::TrainingRoutine AutonomousTrainer::TrainingRoutine::promise_type::get_return_object() noexcept
    {
        return TrainingRoutine{ std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    void AutonomousTrainer::TrainingRoutine::promise_type::unhandled_exception() const noexcept
    {
        std::terminate();
    }

    AutonomousTrainer::TrainingRoutine::TrainingRoutine(TrainingRoutine&& other) noexcept
        : handle_(std::exchange(other.handle_, {}))
    {
    }

    AutonomousTrainer::TrainingRoutine& AutonomousTrainer::TrainingRoutine::operator=(TrainingRoutine&& other) noexcept
    {
        if (this == &other)
            return *this;
        if (handle_)
            handle_.destroy();
        handle_ = std::exchange(other.handle_, {});
        return *this;
    }

    AutonomousTrainer::TrainingRoutine::~TrainingRoutine()
    {
        if (handle_)
            handle_.destroy();
    }

    AutonomousTrainer::RoutineStage AutonomousTrainer::TrainingRoutine::resume()
    {
        if (!handle_ || handle_.done())
            return RoutineStage::idle;
        handle_.resume();
        return handle_.done() ? RoutineStage::idle : handle_.promise().stage;
    }

    AutonomousTrainer::AutonomousTrainer(const sim::CreatureBlueprint& blueprint, std::size_t environment_count)
        : worker_(blueprint, environment_count), live_(blueprint, 8, false), live_blueprint_(blueprint)
    {
        worker_.set_course(stage_, difficulty_, false);
        live_.set_course(stage_, difficulty_, false);
        publish_locked();
        synchronize();
        persistence_thread_ = std::jthread([this](std::stop_token stop_token)
        {
            persistence_main(stop_token);
        });
        worker_thread_ = std::jthread([this](std::stop_token stop_token)
        {
            worker_main(stop_token);
        });
    }

    AutonomousTrainer::~AutonomousTrainer()
    {
        if (worker_thread_.joinable())
        {
            worker_thread_.request_stop();
            wake_cv_.notify_all();
        }
        if (persistence_thread_.joinable())
        {
            persistence_thread_.request_stop();
            persistence_cv_.notify_all();
        }
    }

    void AutonomousTrainer::synchronize()
    {
        PublishedSnapshot snapshot{};
        {
            std::scoped_lock lock(snapshot_mutex_);
            if (published_.serial == applied_serial_)
                return;
            snapshot = published_;
        }

        const bool rig_changed = snapshot.blueprint.signature() != live_blueprint_.signature();
        const bool best_changed = snapshot.has_best
            && snapshot.metrics.best_update != cached_metrics_.best_update;
        const bool course_changed = snapshot.status.stage != cached_status_.stage
            || std::abs(snapshot.status.difficulty - cached_status_.difficulty) > 1.0e-5f;

        const preview_sync::Decision decision = preview_sync::decide(
            rig_changed, course_changed, best_changed);
        if (decision.replace_blueprint)
        {
            live_blueprint_ = snapshot.blueprint;
            live_.set_blueprint(live_blueprint_, false);
        }
        if (decision.replace_course)
            live_.set_course(snapshot.status.stage, snapshot.status.difficulty, false);
        if (decision.adopt_controller)
            live_.policy().parameters() = snapshot.parameters;
        if (decision.reset_episode)
            live_.reset_preview(0xDEADBEEFu
                + snapshot.metrics.update + snapshot.metrics.best_update);

        cached_metrics_ = snapshot.metrics;
        cached_reward_history_ = std::move(snapshot.reward_history);
        cached_speed_history_ = std::move(snapshot.speed_history);
        cached_controller_state_ = snapshot.controller_state;
        cached_status_ = std::move(snapshot.status);
        cached_exploration_ = snapshot.exploration;
        cached_optimizer_step_ = snapshot.optimizer_step;
        cached_has_best_ = snapshot.has_best;
        cached_training_preview_ = std::move(snapshot.training_preview);
        cached_has_training_preview_ = snapshot.has_training_preview;
        applied_serial_ = snapshot.serial;
    }

    void AutonomousTrainer::set_background_enabled(bool enabled) noexcept
    {
        enabled_.store(enabled, std::memory_order_relaxed);
        if (!enabled)
            requested_updates_.store(0u, std::memory_order_relaxed);
        wake_cv_.notify_all();
    }

    void AutonomousTrainer::set_updates_per_cycle(int updates) noexcept
    {
        const int mode = updates <= 1 ? 1 : (updates <= 2 ? 2 : 4);
        updates_per_cycle_.store(mode, std::memory_order_relaxed);
        wake_cv_.notify_all();
    }

    void AutonomousTrainer::set_blueprint(const sim::CreatureBlueprint& blueprint, bool preserve_policy)
    {
        PendingCommand command{};
        command.type = CommandType::set_blueprint;
        command.blueprint = blueprint;
        command.preserve_policy = preserve_policy;
        enqueue_command(std::move(command));
    }

    void AutonomousTrainer::reset_policy(std::uint64_t seed)
    {
        PendingCommand command{};
        command.type = CommandType::reset_policy;
        command.seed = seed;
        enqueue_command(std::move(command));
    }

    void AutonomousTrainer::set_exploration(float standard_deviation) noexcept
    {
        PendingCommand command{};
        command.type = CommandType::set_exploration;
        command.scalar = standard_deviation;
        enqueue_command(std::move(command));
    }

    void AutonomousTrainer::train_one_update() noexcept
    {
        const std::uint32_t previous = requested_updates_.fetch_add(1u, std::memory_order_relaxed);
        if (previous > 1024u)
            requested_updates_.store(1024u, std::memory_order_relaxed);
        wake_cv_.notify_all();
    }

    void AutonomousTrainer::step_preview(float dt)
    {
        live_.step_preview(dt);
    }

    void AutonomousTrainer::reset_preview(std::uint64_t seed) noexcept
    {
        live_.reset_preview(seed);
    }

    bool AutonomousTrainer::restore_best_policy() noexcept
    {
        if (!cached_has_best_)
            return false;
        PendingCommand command{};
        command.type = CommandType::restore_best;
        enqueue_command(std::move(command));
        return true;
    }

    std::string_view AutonomousTrainer::controller_state_name() const noexcept
    {
        switch (cached_controller_state_)
        {
        case ControllerState::fresh: return "FRESH";
        case ControllerState::training: return "TRAINING";
        case ControllerState::resumed: return "RESUMED";
        case ControllerState::transferred: return "TRANSFERRED";
        }
        return "UNKNOWN";
    }

    bool AutonomousTrainer::has_pending_work() const
    {
        if (enabled_.load(std::memory_order_relaxed)
            || requested_updates_.load(std::memory_order_relaxed) != 0u)
        {
            return true;
        }
        return pending_command_count() != 0u;
    }

    bool AutonomousTrainer::consume_update_request() noexcept
    {
        if (enabled_.load(std::memory_order_relaxed))
        {
            requested_updates_.store(0u, std::memory_order_relaxed);
            return true;
        }

        std::uint32_t pending = requested_updates_.load(std::memory_order_relaxed);
        while (pending != 0u)
        {
            if (requested_updates_.compare_exchange_weak(
                pending, pending - 1u, std::memory_order_relaxed, std::memory_order_relaxed))
            {
                return true;
            }
        }
        return false;
    }

    AutonomousTrainer::TrainingRoutine AutonomousTrainer::training_routine(std::stop_token stop_token)
    {
        while (!stop_token.stop_requested())
        {
            consume_persistence_message();
            worker_pipeline_stage_ = "COMMANDS";
            worker_pipeline_stage_mask_ |= 1u << 0u;
            apply_pending_commands();
            co_yield RoutineStage::commands;

            if (!consume_update_request())
            {
                worker_pipeline_stage_ = "IDLE";
                co_yield RoutineStage::idle;
                continue;
            }

            worker_busy_.store(true, std::memory_order_relaxed);
            const auto started = std::chrono::steady_clock::now();

            worker_pipeline_stage_ = "ROLLOUT COLLECTION";
            worker_pipeline_stage_mask_ |= 1u << 1u;
            worker_.set_cpu_mode(updates_per_cycle_.load(std::memory_order_relaxed));
            worker_.begin_staged_update();
            co_yield RoutineStage::rollout;

            worker_pipeline_stage_ = "ADVANTAGE COMPUTATION";
            worker_pipeline_stage_mask_ |= 1u << 2u;
            worker_.compute_staged_advantages();
            co_yield RoutineStage::advantages;

            worker_pipeline_stage_ = "PARALLEL GRADIENT / OPTIMIZER";
            worker_pipeline_stage_mask_ |= 1u << 3u;
            worker_.optimize_staged_update();
            co_yield RoutineStage::optimizer;

            worker_pipeline_stage_ = "EVALUATION / CURRICULUM";
            worker_pipeline_stage_mask_ |= 1u << 4u;
            worker_.finish_staged_update();
            manage_curriculum_locked();
            co_yield RoutineStage::evaluation;

            const auto finished = std::chrono::steady_clock::now();
            worker_busy_.store(false, std::memory_order_relaxed);
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started);
            last_update_nanoseconds_.store(elapsed.count(), std::memory_order_relaxed);
            ++rate_window_updates_;
            const std::chrono::duration<double> rate_elapsed = finished - rate_window_started_;
            if (rate_elapsed.count() >= 1.0)
            {
                worker_updates_per_second_ = static_cast<double>(rate_window_updates_) / rate_elapsed.count();
                rate_window_updates_ = 0;
                rate_window_started_ = finished;
            }

            worker_pipeline_stage_ = "IMMUTABLE PUBLICATION";
            worker_pipeline_stage_mask_ |= 1u << 5u;
            publish_locked();
            co_yield RoutineStage::published;

            worker_pipeline_stage_ = "ASYNC PERSISTENCE";
            worker_pipeline_stage_mask_ |= 1u << 6u;
            co_yield RoutineStage::persistence;
        }
    }

    void AutonomousTrainer::worker_main(std::stop_token stop_token)
    {
        TrainingRoutine routine = training_routine(stop_token);
        RoutineStage stage = RoutineStage::idle;

        while (!stop_token.stop_requested())
        {
            const bool pipeline_must_finish = stage == RoutineStage::rollout
                || stage == RoutineStage::advantages
                || stage == RoutineStage::optimizer
                || stage == RoutineStage::evaluation
                || stage == RoutineStage::published;
            if (!pipeline_must_finish && !has_pending_work())
            {
                std::unique_lock lock(wake_mutex_);
                wake_cv_.wait_for(lock, std::chrono::milliseconds(8), [this, &stop_token]
                {
                    return stop_token.stop_requested() || has_pending_work();
                });
                if (stop_token.stop_requested())
                    break;
            }

            stage = routine.resume();
            if (stage == RoutineStage::persistence)
                throttle_after_update();
            else if (stage == RoutineStage::idle)
                std::this_thread::yield();
        }
    }

    void AutonomousTrainer::throttle_after_update() const
    {
        const int mode = updates_per_cycle_.load(std::memory_order_relaxed);
        const std::int64_t update_nanoseconds = last_update_nanoseconds_.load(std::memory_order_relaxed);
        if (update_nanoseconds <= 0)
        {
            std::this_thread::yield();
            return;
        }

        if (mode == 1)
        {
            const auto delay = std::chrono::nanoseconds(
                std::min<std::int64_t>(update_nanoseconds, 80'000'000LL));
            std::this_thread::sleep_for(delay);
        }
        else if (mode == 2)
        {
            const auto delay = std::chrono::nanoseconds(
                std::min<std::int64_t>(update_nanoseconds / 4, 20'000'000LL));
            if (delay.count() > 0)
                std::this_thread::sleep_for(delay);
        }
        else
        {
            std::this_thread::yield();
        }
    }
}
