#include "autonomy.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace runner::rl
{
    void AutonomousTrainer::set_autosave_paths(std::filesystem::path checkpoint,
        std::filesystem::path rig, std::filesystem::path state)
    {
        std::scoped_lock lock(persistence_mutex_);
        autosave_checkpoint_ = std::move(checkpoint);
        autosave_rig_ = std::move(rig);
        autosave_state_ = std::move(state);
    }

    bool AutonomousTrainer::load_autosave(std::string& message)
    {
        std::filesystem::path checkpoint{};
        std::filesystem::path rig{};
        std::filesystem::path state{};
        std::string legacy_lifetime_version{};
        {
            std::scoped_lock lock(persistence_mutex_);
            checkpoint = autosave_checkpoint_;
            rig = autosave_rig_;
            state = autosave_state_;
            if (!std::filesystem::exists(checkpoint))
            {
                std::filesystem::path previous_candidate = checkpoint;
                std::string previous_name = previous_candidate.filename().string();
                constexpr std::string_view current_prefix{ "runner-v0749-" };
                constexpr std::string_view prior_prefix{ "runner-v0748-" };
                if (previous_name.starts_with(current_prefix))
                {
                    previous_name.replace(0u, current_prefix.size(), prior_prefix);
                    previous_candidate.replace_filename(previous_name);
                }
                if (std::filesystem::exists(previous_candidate))
                {
                    checkpoint = std::move(previous_candidate);
                    rig.clear();
                    state.clear();
                    legacy_lifetime_version = "V0.7.48";
                }
            }
            if (!std::filesystem::exists(checkpoint))
            {
                const std::array legacy_candidates{
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0741-natural-gait-autosave.eppo",
                        std::string{ "V0.7.41" } },
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0740-physical-facing-autosave.eppo",
                        std::string{ "V0.7.40" } },
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0739-static-cells-autosave.eppo",
                        std::string{ "V0.7.39" } },
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0738-topology-autosave.eppo",
                        std::string{ "V0.7.38" } },
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0737-hybrid-autosave.eppo",
                        std::string{ "V0.7.37" } },
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0736-authored-autosave.eppo",
                        std::string{ "V0.7.36" } },
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0735-posture-autosave.eppo",
                        std::string{ "V0.7.35" } },
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0734-curriculum-autosave.eppo",
                        std::string{ "V0.7.34" } },
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0733-granular-autosave.eppo",
                        std::string{ "V0.7.33" } },
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0732-shuttle-autosave.eppo",
                        std::string{ "V0.7.32" } },
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0731-active-autosave.eppo",
                        std::string{ "V0.7.31" } },
                    std::pair{ checkpoint.parent_path()
                        / "runner-v0730-walk-autosave.eppo",
                        std::string{ "V0.7.30" } }
                };
                for (const auto& [candidate, version] : legacy_candidates)
                {
                    if (!std::filesystem::exists(candidate))
                        continue;
                    checkpoint = candidate;
                    rig.clear();
                    state.clear();
                    legacy_lifetime_version = version;
                    break;
                }
            }
        }
        if (!std::filesystem::exists(checkpoint))
        {
            message = "NO V0.7.49 AUTOSAVE FOUND - STARTING WITH STAND TRAINING";
            return false;
        }
        queue_autosave_load(std::move(checkpoint), std::move(rig), std::move(state));
        message = !legacy_lifetime_version.empty()
            ? std::format("{} LIFETIME LEDGER IMPORT QUEUED - CURRENT TRAINING STARTS FRESH",
                legacy_lifetime_version)
            : "AUTOSAVE LOAD QUEUED - TRAINER REMAINS RESPONSIVE";
        return true;
    }

    bool AutonomousTrainer::save_checkpoint(const std::filesystem::path& path, std::string& error)
    {
        if (path.empty())
        {
            error = "Checkpoint path is empty.";
            return false;
        }
        PendingCommand command{};
        command.type = CommandType::save_checkpoint;
        command.path = path;
        enqueue_command(std::move(command));
        error = "CHECKPOINT SAVE QUEUED";
        return true;
    }

    bool AutonomousTrainer::load_checkpoint(const std::filesystem::path& path, std::string& error,
        bool transfer_only)
    {
        if (path.empty() || !std::filesystem::exists(path))
        {
            error = "Could not open checkpoint: " + path.string();
            return false;
        }
        queue_checkpoint_load(path, transfer_only);
        error = "CHECKPOINT LOAD QUEUED";
        return true;
    }

    void AutonomousTrainer::enqueue_command(PendingCommand command)
    {
        {
            std::scoped_lock lock(command_mutex_);
            std::erase_if(command_queue_, [&command](const PendingCommand& queued)
            {
                return queued.type == command.type;
            });
            command_queue_.push_back(std::move(command));
        }
        wake_cv_.notify_all();
    }

    std::size_t AutonomousTrainer::pending_command_count() const
    {
        std::scoped_lock lock(command_mutex_);
        return command_queue_.size();
    }

    void AutonomousTrainer::apply_pending_commands()
    {
        std::deque<PendingCommand> pending{};
        {
            std::scoped_lock lock(command_mutex_);
            pending.swap(command_queue_);
        }
        if (pending.empty())
            return;

        worker_busy_.store(true, std::memory_order_relaxed);
        for (PendingCommand& command : pending)
            apply_command_locked(std::move(command));
        worker_busy_.store(false, std::memory_order_relaxed);
        publish_locked();
    }

    void AutonomousTrainer::apply_command_locked(PendingCommand&& command)
    {
        switch (command.type)
        {
        case CommandType::set_blueprint:
            if (!command.preserve_policy)
            {
                stage_ = sim::CourseStage::balance;
                gait_task_ = sim::GaitTask::walk;
                difficulty_ = 0.25f;
                rig_generation_ = 0;
                accepted_rig_changes_ = 0;
                rejected_rig_changes_ = 0;
                rollback_count_ = 0;
                optimization_mode_ = RigOptimizationMode::control_optimize;
            }
            worker_.set_blueprint(command.blueprint, command.preserve_policy);
            worker_.set_course(stage_, difficulty_, false);
            worker_.set_gait_task(gait_task_, false);
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            last_evaluation_count_ = worker_.metrics().evaluation_count;
            last_saved_best_update_ = 0;
            stage_entry_total_updates_ = worker_.metrics().total_updates;
            stage_entry_total_episodes_ = worker_.metrics().total_episodes;
            stage_entry_evaluation_count_ = worker_.metrics().evaluation_count;
            stage_entry_baseline_initialized_ = true;
            worker_message_ = command.preserve_policy
                ? "RIG UPDATED WITHOUT BLOCKING THE UI - CONTROLLER RECALIBRATING"
                : "RIG UPDATED WITHOUT BLOCKING THE UI - FRESH STAND LESSON STARTED";
            break;

        case CommandType::reset_policy:
            worker_.reset_policy(command.seed);
            worker_.set_course(stage_, difficulty_, false);
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            last_evaluation_count_ = worker_.metrics().evaluation_count;
            last_saved_best_update_ = 0;
            stage_entry_total_updates_ = worker_.metrics().total_updates;
            stage_entry_total_episodes_ = worker_.metrics().total_episodes;
            stage_entry_evaluation_count_ = worker_.metrics().evaluation_count;
            stage_entry_baseline_initialized_ = true;
            worker_message_ = "CONTROLLER RESET - CURRENT SKILL RESTARTED";
            break;

        case CommandType::set_exploration:
            worker_.set_exploration(command.scalar);
            worker_message_ = std::format("EXPLORATION SET TO {:.3f}", command.scalar);
            break;

        case CommandType::set_optimization_mode:
            optimization_mode_ = command.optimization_mode
                == RigOptimizationMode::morphology_evolve
                ? RigOptimizationMode::morphology_evolve
                : RigOptimizationMode::control_optimize;
            worker_message_ = std::format("{} MODE SELECTED - NEXT HELD-OUT RIG CANDIDATE USES THIS CONTRACT",
                rig_optimization_mode_name(optimization_mode_));
            queue_autosave();
            break;

        case CommandType::set_gait_task:
            gait_task_ = command.gait_task;
            if (stage_ != sim::CourseStage::uneven)
            {
                stage_ = sim::CourseStage::uneven;
                difficulty_ = 0.30f;
                worker_.set_course(stage_, difficulty_, false);
            }
            worker_.set_gait_task(gait_task_, false);
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            last_evaluation_count_ = worker_.metrics().evaluation_count;
            last_saved_best_update_ = 0u;
            stage_entry_total_updates_ = worker_.metrics().total_updates;
            stage_entry_total_episodes_ = worker_.metrics().total_episodes;
            stage_entry_evaluation_count_ = worker_.metrics().evaluation_count;
            stage_entry_baseline_initialized_ = true;
            worker_message_ = std::format("GAIT TASK SELECTED - {}",
                sim::gait_task_name(gait_task_));
            queue_autosave();
            break;

        case CommandType::restore_best:
            if (worker_.restore_best_policy())
            {
                ++rollback_count_;
                worker_message_ = "BEST VERIFIED CONTROLLER RESTORED";
            }
            break;

        case CommandType::save_checkpoint:
            queue_checkpoint_save(std::move(command.path), worker_.checkpoint_data());
            worker_message_ = "IMMUTABLE CHECKPOINT SNAPSHOT QUEUED FOR ASYNC WRITE";
            break;

        case CommandType::apply_checkpoint:
            if (command.checkpoint)
            {
                std::string error{};
                if (worker_.apply_checkpoint_data(std::move(*command.checkpoint), error,
                    command.transfer_only))
                {
                    stage_ = worker_.course_stage();
                    difficulty_ = worker_.course_difficulty();
                    worker_message_ = command.transfer_only
                        ? "CONTROLLER TRANSFERRED - CURRENT SKILL RECALIBRATING"
                        : "CHECKPOINT RESUMED - BACKGROUND TRAINING CONTINUES";
                }
                else
                {
                    worker_message_ = error;
                }
            }
            break;

        case CommandType::apply_autosave:
            if (command.checkpoint)
            {
                worker_.set_blueprint(command.blueprint, false);
                const TrainingMetrics lifetime = command.checkpoint->metrics;
                std::string error{};
                if (worker_.apply_checkpoint_data(std::move(*command.checkpoint), error, false))
                {
                    stage_ = worker_.course_stage();
                    gait_task_ = command.gait_task;
                    worker_.set_gait_task(gait_task_, true);
                    difficulty_ = worker_.course_difficulty();
                    rig_generation_ = command.rig_generation;
                    accepted_rig_changes_ = command.accepted_rig_changes;
                    rejected_rig_changes_ = command.rejected_rig_changes;
                    rollback_count_ = command.rollback_count;
                    optimization_mode_ = command.optimization_mode;
                    worker_message_ = std::format("V0.7.49 AUTOSAVE RESUMED - {}",
                        rig_optimization_mode_name(optimization_mode_));
                }
                else if (worker_.import_lifetime_ledger(lifetime, error))
                {
                    stage_ = sim::CourseStage::balance;
                    difficulty_ = 0.25f;
                    worker_.set_course(stage_, difficulty_, false);
                    worker_message_ = error;
                }
                else
                {
                    worker_message_ = error;
                }
            }
            break;
        }
    }
}
