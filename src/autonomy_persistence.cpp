#include "autonomy.hpp"

#include <fstream>
#include <utility>

namespace epochrunner::rl
{
    void AutonomousTrainer::publish_locked()
    {
        PublishedSnapshot snapshot{};
        snapshot.blueprint = worker_.blueprint();
        snapshot.parameters = worker_.has_best_policy()
            ? worker_.best_policy_parameters()
            : worker_.policy().parameters();
        snapshot.metrics = worker_.metrics();
        snapshot.reward_history = worker_.reward_history();
        snapshot.speed_history = worker_.speed_history();
        snapshot.controller_state = worker_.controller_state();
        snapshot.exploration = worker_.exploration();
        snapshot.optimizer_step = worker_.optimizer_step();
        snapshot.has_best = worker_.has_best_policy();
        snapshot.status.enabled = enabled_.load(std::memory_order_relaxed);
        snapshot.status.stage = stage_;
        snapshot.status.difficulty = difficulty_;
        snapshot.status.rig_generation = rig_generation_;
        snapshot.status.accepted_rig_changes = accepted_rig_changes_;
        snapshot.status.rejected_rig_changes = rejected_rig_changes_;
        snapshot.status.mastery_streak = mastery_streak_;
        snapshot.status.rollback_count = rollback_count_;
        snapshot.status.rollout_threads = worker_.rollout_worker_count();
        snapshot.status.environment_count = worker_.environment_count();
        snapshot.status.pending_commands = pending_command_count();
        snapshot.status.updates_per_second = worker_updates_per_second_;
        snapshot.status.speed_mode = updates_per_cycle_.load(std::memory_order_relaxed);
        snapshot.status.worker_busy = worker_busy_.load(std::memory_order_relaxed);
        snapshot.status.pipeline_suspensions = pipeline_suspensions_.load(std::memory_order_relaxed);
        snapshot.status.pipeline_stage = std::string(routine_stage_name(
            pipeline_stage_.load(std::memory_order_relaxed)));
        snapshot.status.message = worker_message_;

        std::scoped_lock lock(snapshot_mutex_);
        snapshot.serial = published_.serial + 1u;
        published_ = std::move(snapshot);
    }

    void AutonomousTrainer::autosave_locked()
    {
        std::string error{};
        if (!autosave_checkpoint_.empty() && !worker_.save_checkpoint(autosave_checkpoint_, error))
        {
            worker_message_ = error;
            return;
        }
        if (!autosave_rig_.empty() && !worker_.blueprint().save(autosave_rig_, error))
        {
            worker_message_ = error;
            return;
        }
        write_state_locked();
    }

    void AutonomousTrainer::write_state_locked() const
    {
        if (autosave_state_.empty())
            return;
        const std::filesystem::path temporary = autosave_state_.string() + ".tmp";
        std::ofstream output(temporary, std::ios::trunc);
        if (!output)
            return;
        output << "EPOCHAUTONOMY 2\n";
        output << static_cast<int>(stage_) << ' ' << difficulty_ << ' ' << rig_generation_ << ' '
            << accepted_rig_changes_ << ' ' << rejected_rig_changes_ << ' ' << rollback_count_ << '\n';
        output.close();
        if (!output)
            return;
        std::error_code filesystem_error{};
        std::filesystem::remove(autosave_state_, filesystem_error);
        filesystem_error.clear();
        std::filesystem::rename(temporary, autosave_state_, filesystem_error);
    }

    void AutonomousTrainer::read_state_locked()
    {
        if (autosave_state_.empty() || !std::filesystem::exists(autosave_state_))
            return;
        std::ifstream input(autosave_state_);
        std::string magic{};
        int version{};
        int stage{};
        input >> magic >> version >> stage >> difficulty_ >> rig_generation_
            >> accepted_rig_changes_ >> rejected_rig_changes_ >> rollback_count_;
        if (!input || magic != "EPOCHAUTONOMY" || version != 2 || stage < 0
            || stage >= static_cast<int>(sim::course_stage_count))
        {
            stage_ = sim::CourseStage::balance;
            difficulty_ = 0.25f;
            rig_generation_ = 0;
            accepted_rig_changes_ = 0;
            rejected_rig_changes_ = 0;
            rollback_count_ = 0;
            return;
        }
        stage_ = static_cast<sim::CourseStage>(stage);
        difficulty_ = clamp(difficulty_, 0.10f, 1.0f);
    }
}
