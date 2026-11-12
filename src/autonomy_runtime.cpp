#include "autonomy.hpp"
#include "integration.hpp"
#include "preview_sync.hpp"
#include "training_budget.hpp"

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
        worker_.autopilot_unaided_stand_ = true;
        set_autosave_paths(autosave_checkpoint_, autosave_rig_, autosave_state_);
        director_state_.graph = integration::compatible_task_graph(
            worker_.preview(), director::default_training_graph());
        director_state_.current = director::select(director_state_.graph,
            director_state_.evidence, director_state_.profile);
        worker_.set_course(stage_, difficulty_, false);
        live_.set_course(stage_, difficulty_, false);
        // The large preview is a real locomotion test, not a conveyor-belt
        // visualization. Training workers may still use moving-course pressure.
        live_.set_preview_course_motion_enabled(false);
        worker_.rollout_sample_sink_ = [this](const sim::Environment& environment)
        {
            sample_training_rollout(environment);
        };
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
        synchronize_snapshot(std::nullopt);
    }

    void AutonomousTrainer::sample_training_rollout(const sim::Environment& environment)
    {
        // Rollout lane zero alone calls this; generations never overlap.
        const auto now = std::chrono::steady_clock::now();
        if (now - last_rollout_sample_ < std::chrono::milliseconds{ 125 })
            return;
        last_rollout_sample_ = now;
        sim::Environment sample = environment;
        std::scoped_lock lock(snapshot_mutex_);
        pending_rollout_sample_ = std::move(sample);
    }

    void AutonomousTrainer::start_cached_replay(std::uint64_t seed)
    {
        live_.set_course(cached_status_.stage, cached_status_.difficulty, false);
        live_.set_gait_task(cached_status_.gait_task, false);
        live_.set_equipment_directive(cached_status_.equipment_directive);
        live_.policy().parameters() = cached_replay_parameters_;
        replay_uses_retained_controller_ = cached_has_best_;
        replay_controller_guidance_mode_ = cached_controller_guidance_mode_;
        replay_controller_update_ = cached_has_best_
            ? cached_metrics_.best_update : cached_metrics_.update;
        live_.reset_preview(seed);
    }

    void AutonomousTrainer::synchronize_snapshot(
        std::optional<std::uint64_t> replay_seed)
    {
        std::optional<PublishedSnapshot> next{};
        std::optional<sim::Environment> rollout_sample{};
        {
            std::scoped_lock lock(snapshot_mutex_);
            if (published_.serial != applied_serial_)
                next.emplace(published_);
            rollout_sample.swap(pending_rollout_sample_);
        }
        const auto apply_rollout = [&]
        {
            if (!rollout_sample)
                return;
            cached_training_preview_ = std::move(*rollout_sample);
            cached_has_training_preview_ = true;
            cached_preview_is_rollout_sample_ = true;
        };
        if (!next)
        {
            apply_rollout();
            if (replay_seed && !live_morphology_preview_active_)
                start_cached_replay(*replay_seed);
            else if (replay_seed)
                live_.reset_preview(*replay_seed);
            return;
        }

        auto& snapshot = *next;
        const bool preview_committed = live_morphology_preview_active_
            && snapshot.blueprint.signature() == live_blueprint_.signature();
        const bool hold_preview = live_morphology_preview_active_
            && !preview_committed;
        if (preview_committed)
            live_morphology_preview_active_ = false;
        const bool rig_changed = !hold_preview
            && (preview_committed
                || snapshot.blueprint.signature() != live_blueprint_.signature());
        const preview_sync::Decision decision = preview_sync::decide(
            rig_changed, applied_serial_ == 0, hold_preview, replay_seed.has_value());
        if (decision.replace_blueprint)
        {
            live_blueprint_ = snapshot.blueprint;
            live_.set_blueprint(live_blueprint_, false);
        }
        cached_replay_parameters_ = std::move(snapshot.parameters);
        cached_metrics_ = snapshot.metrics;
        cached_reward_history_ = std::move(snapshot.reward_history);
        cached_speed_history_ = std::move(snapshot.speed_history);
        cached_controller_state_ = snapshot.controller_state;
        cached_status_ = std::move(snapshot.status);
        cached_exploration_ = snapshot.exploration;
        cached_optimizer_step_ = snapshot.optimizer_step;
        cached_has_best_ = snapshot.has_best;
        cached_controller_guidance_mode_ = snapshot.controller_guidance_mode;
        cached_has_training_preview_ = snapshot.training_preview.has_value();
        cached_preview_is_rollout_sample_ = false;
        if (snapshot.training_preview)
            cached_training_preview_ = std::move(*snapshot.training_preview);
        apply_rollout();
        applied_serial_ = snapshot.serial;
        if (decision.start_replay)
            start_cached_replay(replay_seed.value_or(0xDEADBEEFu
                + cached_metrics_.update + cached_metrics_.best_update));
        else if (replay_seed)
            live_.reset_preview(*replay_seed);
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

    bool AutonomousTrainer::preview_blueprint(
        const sim::CreatureBlueprint& blueprint)
    {
        if (!blueprint.valid())
            return false;
        live_blueprint_ = blueprint;
        live_.set_blueprint(live_blueprint_, true);
        live_.set_preview_course_motion_enabled(false);
        live_morphology_preview_active_ = true;
        return true;
    }

    void AutonomousTrainer::cancel_blueprint_preview()
    {
        PublishedSnapshot snapshot{};
        {
            std::scoped_lock lock(snapshot_mutex_);
            snapshot = published_;
        }
        live_blueprint_ = snapshot.blueprint;
        live_.set_blueprint(live_blueprint_, false);
        live_.set_course(snapshot.status.stage, snapshot.status.difficulty, false);
        live_.set_gait_task(snapshot.status.gait_task, false);
        live_.set_equipment_directive(snapshot.status.equipment_directive);
        live_.policy().parameters() = snapshot.parameters;
        replay_uses_retained_controller_ = snapshot.has_best;
        replay_controller_guidance_mode_ = snapshot.controller_guidance_mode;
        replay_controller_update_ = snapshot.has_best
            ? snapshot.metrics.best_update : snapshot.metrics.update;
        live_.set_preview_course_motion_enabled(false);
        live_morphology_preview_active_ = false;
        live_.reset_preview();
    }

    void AutonomousTrainer::set_rig_optimization_mode(RigOptimizationMode mode) noexcept
    {
        PendingCommand command{};
        command.type = CommandType::set_optimization_mode;
        command.optimization_mode = mode == RigOptimizationMode::morphology_evolve
            ? RigOptimizationMode::morphology_evolve
            : RigOptimizationMode::control_optimize;
        enqueue_command(std::move(command));
    }

    void AutonomousTrainer::set_gait_task(sim::GaitTask task) noexcept
    {
        PendingCommand command{};
        command.type = CommandType::set_gait_task;
        command.gait_task = task;
        enqueue_command(std::move(command));
    }

    void AutonomousTrainer::select_director_task(
        std::uint16_t task_index) noexcept
    {
        PendingCommand command{};
        command.type = CommandType::set_director_task;
        command.director_task_index = task_index;
        enqueue_command(std::move(command));
    }

    void AutonomousTrainer::set_director_profile(
        director::Profile profile) noexcept
    {
        PendingCommand command{};
        command.type = CommandType::set_director_profile;
        command.director_profile = profile;
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

    void AutonomousTrainer::reset_preview(std::uint64_t seed)
    {
        synchronize_snapshot(seed);
    }

    void AutonomousTrainer::configure_preview_equipment(
        sim::WeaponClass weapon, float target_distance)
    {
        live_.configure_preview_equipment(weapon, target_distance);
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
        if (pending_command_count() != 0u)
            return true;
        std::scoped_lock lock(persistence_mutex_);
        return consumed_persistence_message_serial_ != persistence_message_serial_;
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
            if (consume_persistence_message())
                publish_locked();
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
            worker_.set_cpu_mode(updates_per_cycle_.load(std::memory_order_relaxed));

            const bool confirming_stand = mastery_streak_ > 0
                && worker_.verified_unaided_stand();
            if (worker_.trains_unaided_stand() && !confirming_stand)
                mastery_streak_ = 0;
            const bool confirming_retained = !worker_.trains_unaided_stand()
                && should_confirm_retained_mastery(
                stage_, mastery_streak_, worker_.has_best_policy());
            if (confirming_stand)
            {
                worker_pipeline_stage_ = "EVALUATION / CURRICULUM";
                worker_pipeline_stage_mask_ |= 1u << 4u;
                worker_.evaluate_policy(worker_.metrics().evaluation_count);
                manage_curriculum_locked();
                co_yield RoutineStage::evaluation;
            }
            else if (confirming_retained)
            {
                // Freeze optimization and test the exact retained controller.
                // This prevents a stage-safe policy from drifting between the
                // independent confirmations that own the final 20 percent.
                worker_pipeline_stage_ = "RETAINED MASTERY TEST";
                worker_pipeline_stage_mask_ |= 1u << 4u;
                if (worker_.evaluate_retained_policy())
                    manage_curriculum_locked();
                else
                    mastery_streak_ = 0;
                co_yield RoutineStage::evaluation;
            }
            else
            {
                worker_pipeline_stage_ = "ROLLOUT COLLECTION";
                worker_pipeline_stage_mask_ |= 1u << 1u;
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
            }

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
                // Bounded fallback covers notifications racing the atomic setters.
                // Persistence completion is work too, even while training is paused.
                wake_cv_.wait_for(lock, stop_token, std::chrono::milliseconds(250), [this]
                {
                    return has_pending_work();
                });
                if (stop_token.stop_requested())
                    break;
            }

            stage = routine.resume();
            if (stage == RoutineStage::persistence)
                throttle_after_update(stop_token);
            else if (stage == RoutineStage::idle)
                std::this_thread::yield();
        }
    }

    void AutonomousTrainer::throttle_after_update(std::stop_token stop_token)
    {
        const int mode = updates_per_cycle_.load(std::memory_order_relaxed);
        const std::int64_t update_nanoseconds = last_update_nanoseconds_.load(std::memory_order_relaxed);
        const auto delay = std::chrono::nanoseconds(
            training_budget::idle_nanoseconds(mode, update_nanoseconds));
        const auto deadline = std::chrono::steady_clock::now() + delay;
        std::unique_lock lock(wake_mutex_);
        while (!stop_token.stop_requested() && std::chrono::steady_clock::now() < deadline)
        {
            if (wake_cv_.wait_until(lock, stop_token,
                std::min(deadline, std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(250)), [this, mode]
                {
                    return !enabled_.load(std::memory_order_relaxed)
                        || updates_per_cycle_.load(std::memory_order_relaxed) != mode
                        || pending_command_count() != 0u;
                }))
                break;
        }
    }
}
