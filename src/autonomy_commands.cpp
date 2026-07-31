#include "autonomy.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace epochrunner::rl
{
    void AutonomousTrainer::set_autosave_paths(std::filesystem::path checkpoint, std::filesystem::path rig,
        std::filesystem::path state)
    {
        std::scoped_lock lock(worker_mutex_);
        autosave_checkpoint_ = std::move(checkpoint);
        autosave_rig_ = std::move(rig);
        autosave_state_ = std::move(state);
    }

    bool AutonomousTrainer::load_autosave(std::string& message)
    {
        std::scoped_lock lock(worker_mutex_);
        bool loaded_anything = false;
        read_state_locked();
        if (std::filesystem::exists(autosave_rig_))
        {
            std::string rig_error{};
            const sim::CreatureBlueprint loaded = sim::CreatureBlueprint::load(autosave_rig_, rig_error);
            if (rig_error.empty())
            {
                worker_.set_blueprint(loaded, false);
                loaded_anything = true;
            }
            else
            {
                message = rig_error;
            }
        }
        worker_.set_course(stage_, difficulty_, false);
        if (std::filesystem::exists(autosave_checkpoint_))
        {
            std::string checkpoint_error{};
            if (worker_.load_checkpoint(autosave_checkpoint_, checkpoint_error, false))
            {
                stage_ = worker_.course_stage();
                difficulty_ = worker_.course_difficulty();
                loaded_anything = true;
                worker_message_ = "AUTOSAVE RESUMED - TRAINING CONTINUES IN BACKGROUND";
            }
            else
            {
                message = checkpoint_error;
            }
        }
        publish_locked();
        if (loaded_anything)
            message = worker_message_;
        else if (message.empty())
            message = "NO V0.4 AUTOSAVE FOUND - STARTING WITH BALANCE TRAINING";
        return loaded_anything;
    }

    bool AutonomousTrainer::save_checkpoint(const std::filesystem::path& path, std::string& error) const
    {
        std::scoped_lock lock(worker_mutex_);
        return worker_.save_checkpoint(path, error);
    }

    bool AutonomousTrainer::load_checkpoint(const std::filesystem::path& path, std::string& error,
        bool transfer_only)
    {
        std::scoped_lock lock(worker_mutex_);
        const bool loaded = worker_.load_checkpoint(path, error, transfer_only);
        if (loaded)
        {
            stage_ = worker_.course_stage();
            difficulty_ = worker_.course_difficulty();
            worker_message_ = transfer_only
                ? "CONTROLLER TRANSFERRED - AUTOPILOT RECALIBRATING"
                : "CHECKPOINT RESUMED - AUTOPILOT CONTINUING";
            publish_locked();
        }
        return loaded;
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
        {
            std::scoped_lock lock(worker_mutex_);
            for (PendingCommand& command : pending)
                apply_command_locked(std::move(command));
            worker_busy_.store(false, std::memory_order_relaxed);
            publish_locked();
        }
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
                : "RIG UPDATED WITHOUT BLOCKING THE UI - FRESH BALANCE LESSON STARTED";
            break;

        case CommandType::reset_policy:
            worker_.reset_policy(command.seed);
            worker_.set_course(stage_, difficulty_, false);
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            last_evaluation_count_ = 0;
            last_saved_best_update_ = 0;
            worker_message_ = "CONTROLLER RESET - AUTOPILOT RESTARTED CURRENT LESSON";
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
        }
    }
}
