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
        bool exists = false;
        {
            std::scoped_lock lock(persistence_mutex_);
            exists = std::filesystem::exists(autosave_checkpoint_)
                || std::filesystem::exists(autosave_rig_)
                || std::filesystem::exists(autosave_state_);
        }
        if (!exists)
        {
            message = "NO V0.7.6 AUTOSAVE FOUND - STARTING WITH STAND TRAINING";
            return false;
        }
        queue_autosave_load();
        message = "AUTOSAVE LOAD QUEUED - TRAINER REMAINS RESPONSIVE";
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
                difficulty_ = 0.25f;
                rig_generation_ = 0;
                accepted_rig_changes_ = 0;
                rejected_rig_changes_ = 0;
                rollback_count_ = 0;
            }
            worker_.set_blueprint(command.blueprint, command.preserve_policy);
            worker_.set_course(stage_, difficulty_, false);
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            last_evaluation_count_ = 0;
            last_saved_best_update_ = 0;
            worker_message_ = command.preserve_policy
                ? "RIG UPDATED WITHOUT BLOCKING THE UI - CONTROLLER RECALIBRATING"
                : "RIG UPDATED WITHOUT BLOCKING THE UI - FRESH STAND LESSON STARTED";
            break;

        case CommandType::reset_policy:
            worker_.reset_policy(command.seed);
            worker_.set_course(stage_, difficulty_, false);
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            last_evaluation_count_ = 0;
            last_saved_best_update_ = 0;
            worker_message_ = "CONTROLLER RESET - CURRENT SKILL RESTARTED";
            break;

        case CommandType::set_exploration:
            worker_.set_exploration(command.scalar);
            worker_message_ = std::format("EXPLORATION SET TO {:.3f}", command.scalar);
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
                std::string error{};
                if (worker_.apply_checkpoint_data(std::move(*command.checkpoint), error, false))
                {
                    stage_ = worker_.course_stage();
                    difficulty_ = worker_.course_difficulty();
                    rig_generation_ = command.rig_generation;
                    accepted_rig_changes_ = command.accepted_rig_changes;
                    rejected_rig_changes_ = command.rejected_rig_changes;
                    rollback_count_ = command.rollback_count;
                    worker_message_ = "V0.7.6 AUTOSAVE RESUMED ASYNCHRONOUSLY";
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
