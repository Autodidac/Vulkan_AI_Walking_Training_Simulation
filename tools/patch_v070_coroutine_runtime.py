from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


header = Path("src/autonomy.hpp")
text = header.read_text(encoding="utf-8")
text = replace_once(
    text,
    """        bool worker_busy{};
        std::string message{ "LEARNING TO BALANCE" };
""",
    """        bool worker_busy{};
        std::uint64_t pipeline_suspensions{};
        std::string pipeline_stage{ "IDLE" };
        std::string message{ "LEARNING TO BALANCE" };
""",
    "pipeline status",
)
text = replace_once(
    text,
    """        enum class RoutineStage : std::uint8_t
        {
            idle,
            commands,
            trained,
            published
        };
""",
    """        enum class RoutineStage : std::uint8_t
        {
            idle,
            commands,
            rollout_dispatched,
            advantages_ready,
            gradient_dispatched,
            gradient_reduced,
            optimized,
            evaluation_dispatched,
            evaluated,
            published
        };
""",
    "routine stages",
)
text = replace_once(
    text,
    """        [[nodiscard]] TrainingRoutine training_routine(std::stop_token stop_token);
        void worker_main(std::stop_token stop_token);
        void perform_training_update();
        void perform_post_update();
        void throttle_after_update() const;
""",
    """        [[nodiscard]] TrainingRoutine training_routine(std::stop_token stop_token);
        void worker_main(std::stop_token stop_token);
        void perform_post_update();
        void record_update_timing(std::chrono::steady_clock::time_point started);
        void throttle_after_update() const;
        [[nodiscard]] static std::string_view routine_stage_name(RoutineStage stage) noexcept;
""",
    "runtime helper declarations",
)
text = replace_once(
    text,
    """        std::atomic_bool worker_busy_{ false };
        std::atomic_int64_t last_update_nanoseconds_{};
        std::jthread worker_thread_{};
""",
    """        std::atomic_bool worker_busy_{ false };
        std::atomic_int64_t last_update_nanoseconds_{};
        std::atomic<RoutineStage> pipeline_stage_{ RoutineStage::idle };
        std::atomic_uint64_t pipeline_suspensions_{};
        std::jthread worker_thread_{};
""",
    "runtime stage atomics",
)
header.write_text(text, encoding="utf-8")

runtime = Path("src/autonomy_runtime.cpp")
text = runtime.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    void AutonomousTrainer::synchronize()
    {
        PublishedSnapshot snapshot{};
""",
    """    void AutonomousTrainer::synchronize()
    {
        cached_status_.pipeline_stage = std::string(routine_stage_name(
            pipeline_stage_.load(std::memory_order_relaxed)));
        cached_status_.pipeline_suspensions = pipeline_suspensions_.load(std::memory_order_relaxed);

        PublishedSnapshot snapshot{};
""",
    "live pipeline telemetry",
)
start = text.index("    AutonomousTrainer::TrainingRoutine AutonomousTrainer::training_routine(")
end = text.index("\n    void AutonomousTrainer::throttle_after_update() const", start)
replacement = r'''    std::string_view AutonomousTrainer::routine_stage_name(RoutineStage stage) noexcept
    {
        switch (stage)
        {
        case RoutineStage::idle: return "IDLE";
        case RoutineStage::commands: return "COMMANDS";
        case RoutineStage::rollout_dispatched: return "AWAITING ROLLOUTS";
        case RoutineStage::advantages_ready: return "ADVANTAGES READY";
        case RoutineStage::gradient_dispatched: return "AWAITING GRADIENTS";
        case RoutineStage::gradient_reduced: return "GRADIENT REDUCED";
        case RoutineStage::optimized: return "OPTIMIZED";
        case RoutineStage::evaluation_dispatched: return "AWAITING EVALUATION";
        case RoutineStage::evaluated: return "EVALUATED";
        case RoutineStage::published: return "PUBLISHED";
        }
        return "UNKNOWN";
    }

    AutonomousTrainer::TrainingRoutine AutonomousTrainer::training_routine(
        std::stop_token stop_token)
    {
        while (!stop_token.stop_requested())
        {
            apply_pending_commands();
            co_yield RoutineStage::commands;

            if (!consume_update_request())
            {
                co_yield RoutineStage::idle;
                continue;
            }

            worker_busy_.store(true, std::memory_order_relaxed);
            const auto started = std::chrono::steady_clock::now();
            {
                std::scoped_lock lock(worker_mutex_);
                worker_.set_cpu_mode(updates_per_cycle_.load(std::memory_order_relaxed));
                worker_.begin_update();
            }
            co_yield RoutineStage::rollout_dispatched;

            {
                std::scoped_lock lock(worker_mutex_);
                worker_.finish_rollout();
                worker_.compute_advantages();
                worker_.begin_policy_update();
            }
            co_yield RoutineStage::advantages_ready;

            for (;;)
            {
                bool complete{};
                {
                    std::scoped_lock lock(worker_mutex_);
                    complete = worker_.policy_update_complete();
                }
                if (complete)
                    break;

                co_yield RoutineStage::gradient_dispatched;
                {
                    std::scoped_lock lock(worker_mutex_);
                    worker_.finish_policy_batch();
                }
                co_yield RoutineStage::gradient_reduced;
            }

            bool evaluate{};
            {
                std::scoped_lock lock(worker_mutex_);
                worker_.finalize_update_metrics();
                evaluate = worker_.evaluation_due();
                if (evaluate)
                    worker_.begin_evaluation();
            }
            co_yield RoutineStage::optimized;

            if (evaluate)
            {
                co_yield RoutineStage::evaluation_dispatched;
                {
                    std::scoped_lock lock(worker_mutex_);
                    worker_.finish_evaluation();
                }
                co_yield RoutineStage::evaluated;
            }

            perform_post_update();
            record_update_timing(started);
            worker_busy_.store(false, std::memory_order_relaxed);
            co_yield RoutineStage::published;
        }
    }

    void AutonomousTrainer::worker_main(std::stop_token stop_token)
    {
        TrainingRoutine routine = training_routine(stop_token);
        RoutineStage stage = RoutineStage::idle;

        while (!stop_token.stop_requested())
        {
            const bool pipeline_in_flight = stage == RoutineStage::rollout_dispatched
                || stage == RoutineStage::advantages_ready
                || stage == RoutineStage::gradient_dispatched
                || stage == RoutineStage::gradient_reduced
                || stage == RoutineStage::optimized
                || stage == RoutineStage::evaluation_dispatched
                || stage == RoutineStage::evaluated;
            if (!pipeline_in_flight && !has_pending_work())
            {
                std::unique_lock lock(wake_mutex_);
                wake_cv_.wait_for(lock, std::chrono::milliseconds(8), [this, &stop_token]
                {
                    return stop_token.stop_requested() || has_pending_work();
                });
                if (stop_token.stop_requested())
                    break;
            }

            bool completed = true;
            if (stage == RoutineStage::rollout_dispatched)
                completed = worker_.wait_for_rollout(stop_token);
            else if (stage == RoutineStage::gradient_dispatched)
                completed = worker_.wait_for_policy_batch(stop_token);
            else if (stage == RoutineStage::evaluation_dispatched)
                completed = worker_.wait_for_evaluation(stop_token);

            if (!completed || stop_token.stop_requested())
                break;
            if (stage == RoutineStage::rollout_dispatched
                || stage == RoutineStage::gradient_dispatched
                || stage == RoutineStage::evaluation_dispatched)
            {
                pipeline_suspensions_.fetch_add(1u, std::memory_order_relaxed);
            }

            stage = routine.resume();
            pipeline_stage_.store(stage, std::memory_order_relaxed);
            if (stage == RoutineStage::published)
                throttle_after_update();
            else if (stage == RoutineStage::idle)
                std::this_thread::yield();
        }
        pipeline_stage_.store(RoutineStage::idle, std::memory_order_relaxed);
        worker_busy_.store(false, std::memory_order_relaxed);
    }

    void AutonomousTrainer::record_update_timing(
        std::chrono::steady_clock::time_point started)
    {
        const auto finished = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started);
        last_update_nanoseconds_.store(elapsed.count(), std::memory_order_relaxed);
        ++rate_window_updates_;

        const std::chrono::duration<double> rate_elapsed = finished - rate_window_started_;
        if (rate_elapsed.count() >= 1.0)
        {
            worker_updates_per_second_ = static_cast<double>(rate_window_updates_)
                / rate_elapsed.count();
            rate_window_updates_ = 0;
            rate_window_started_ = finished;
        }
    }

    void AutonomousTrainer::perform_post_update()
    {
        std::scoped_lock lock(worker_mutex_);
        manage_curriculum_locked();
        publish_locked();
    }
'''
text = text[:start] + replacement + text[end:]
runtime.write_text(text, encoding="utf-8")

persistence = Path("src/autonomy_persistence.cpp")
text = persistence.read_text(encoding="utf-8")
text = replace_once(
    text,
    """        snapshot.status.worker_busy = worker_busy_.load(std::memory_order_relaxed);
        snapshot.status.message = worker_message_;
""",
    """        snapshot.status.worker_busy = worker_busy_.load(std::memory_order_relaxed);
        snapshot.status.pipeline_suspensions = pipeline_suspensions_.load(std::memory_order_relaxed);
        snapshot.status.pipeline_stage = std::string(routine_stage_name(
            pipeline_stage_.load(std::memory_order_relaxed)));
        snapshot.status.message = worker_message_;
""",
    "published pipeline telemetry",
)
persistence.write_text(text, encoding="utf-8")

app = Path("src/app.cpp")
text = app.read_text(encoding="utf-8")
text = replace_once(
    text,
    """            add_text(canvas, cursor, autonomy.worker_busy ? "TRAINER BUSY" : "TRAINER IDLE", 1.04f,
                autonomy.worker_busy ? yellow : green);
            cursor.y += 23.0f;
""",
    """            add_text(canvas, cursor, autonomy.worker_busy ? "TRAINER BUSY" : "TRAINER IDLE", 1.04f,
                autonomy.worker_busy ? yellow : green);
            cursor.y += 23.0f;
            add_text(canvas, cursor, std::format("PIPELINE {}   SUSPENSIONS {}",
                autonomy.pipeline_stage, autonomy.pipeline_suspensions), 1.02f, muted);
            cursor.y += 23.0f;
""",
    "pipeline UI telemetry",
)
app.write_text(text, encoding="utf-8")

tests = Path("tests/core_tests.cpp")
text = tests.read_text(encoding="utf-8")
anchor = """    rl::PpoTrainer trainer{ humanoid, 16 };
"""
staged_tests = r'''    {
        rl::PpoTrainer monolithic{ humanoid, 16 };
        rl::PpoTrainer staged{ humanoid, 16 };
        monolithic.set_cpu_mode(2);
        staged.set_cpu_mode(2);
        monolithic.train_one_update();

        staged.begin_update();
        require(staged.wait_for_rollout({}), "staged rollout wait was cancelled");
        staged.finish_rollout();
        staged.compute_advantages();
        staged.begin_policy_update();
        std::size_t gradient_batches{};
        while (!staged.policy_update_complete())
        {
            require(staged.wait_for_policy_batch({}), "staged gradient wait was cancelled");
            staged.finish_policy_batch();
            ++gradient_batches;
            require(gradient_batches < 1024, "staged optimizer failed to terminate");
        }
        staged.finalize_update_metrics();
        if (staged.evaluation_due())
        {
            staged.begin_evaluation();
            require(staged.wait_for_evaluation({}), "staged evaluation wait was cancelled");
            staged.finish_evaluation();
        }

        require(staged.metrics().update == 1, "staged PPO update did not finalize");
        require(staged.optimizer_step() == monolithic.optimizer_step(),
            "staged PPO changed the number of Adam applications");
        require(staged.policy().parameters().size() == monolithic.policy().parameters().size(),
            "staged PPO changed policy dimensions");
        for (std::size_t index = 0; index < staged.policy().parameters().size(); ++index)
        {
            require(std::abs(staged.policy().parameters()[index]
                    - monolithic.policy().parameters()[index]) < 1.0e-5f,
                "staged PPO diverged from deterministic monolithic compatibility path");
        }
    }

    rl::PpoTrainer trainer{ humanoid, 16 };
'''
text = replace_once(text, anchor, staged_tests, "staged PPO tests")
text = replace_once(
    text,
    """        require(autonomous.rig_signature() == edited.signature(),
            "queued hip edit was not eventually published");
""",
    """        require(autonomous.rig_signature() == edited.signature(),
            "queued hip edit was not eventually published");
        require(autonomous.autonomy_status().pipeline_suspensions > 0,
            "coroutine pipeline never suspended for persistent worker completion");
        require(!autonomous.autonomy_status().pipeline_stage.empty(),
            "coroutine pipeline stage telemetry is missing");
""",
    "coroutine runtime tests",
)
tests.write_text(text, encoding="utf-8")
