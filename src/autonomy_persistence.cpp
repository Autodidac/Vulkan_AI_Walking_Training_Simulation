#include "autonomy.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

namespace runner::rl
{
    namespace
    {
        bool write_autonomy_state(const std::filesystem::path& path,
            sim::CourseStage stage, float difficulty, std::uint64_t rig_generation,
            std::uint64_t accepted, std::uint64_t rejected, int rollback,
            std::string& error)
        {
            if (path.empty())
                return true;
            const std::filesystem::path temporary = path.string() + ".tmp";
            std::ofstream output(temporary, std::ios::trunc);
            if (!output)
            {
                error = "Could not open autonomy state for writing: " + temporary.string();
                return false;
            }
            output << "RUNAUTONOMY 16\n";
            output << static_cast<int>(stage) << ' ' << difficulty << ' ' << rig_generation << ' '
                << accepted << ' ' << rejected << ' ' << rollback << '\n';
            output.close();
            if (!output)
            {
                error = "Failed while writing autonomy state: " + temporary.string();
                return false;
            }
            std::error_code filesystem_error{};
            std::filesystem::remove(path, filesystem_error);
            filesystem_error.clear();
            std::filesystem::rename(temporary, path, filesystem_error);
            if (filesystem_error)
            {
                error = "Could not replace autonomy state atomically: " + filesystem_error.message();
                return false;
            }
            return true;
        }

        void read_autonomy_state(const std::filesystem::path& path,
            sim::CourseStage& stage, float& difficulty, std::uint64_t& rig_generation,
            std::uint64_t& accepted, std::uint64_t& rejected, int& rollback)
        {
            if (path.empty() || !std::filesystem::exists(path))
                return;
            std::ifstream input(path);
            std::string magic{};
            int version{};
            int stage_value{};
            input >> magic >> version >> stage_value >> difficulty >> rig_generation
                >> accepted >> rejected >> rollback;
            if (!input || magic != "RUNAUTONOMY" || version != 16
                || stage_value < 0 || stage_value >= static_cast<int>(sim::course_stage_count))
                return;
            stage = static_cast<sim::CourseStage>(stage_value);
            difficulty = clamp(difficulty, 0.10f, 1.0f);
        }
    }

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
        const std::span<const sim::Environment> environments = worker_.environments();
        const sim::Environment* representative = nullptr;
        int representative_priority = 0;
        std::uint64_t representative_quality = 0u;
        float representative_tiebreak = -std::numeric_limits<float>::infinity();
        for (const sim::Environment& environment : environments)
        {
            const StageMotionQualification qualification =
                stage_motion_qualification(stage_, environment);
            const bool stage_eligible = stage_display_sample_eligible(stage_, environment);
            const int display_priority = training_preview_priority(stage_, environment);
            if (display_priority == 0)
                continue;
            const std::uint64_t display_quality = qualification.valid
                ? qualification.quality_key
                : pack_quality(
                    static_cast<std::uint16_t>(stage_eligible ? 3u
                        : environment.body_integrity_valid() ? 2u : 1u),
                    static_cast<std::uint16_t>(std::min<std::uint32_t>(
                        environment.alternating_steps(), 65535u)),
                    quality_bucket(environment.crouch_walk_distance(), 100.0f),
                    quality_bucket(environment.elapsed_seconds(), 10.0f));
            const float tiebreak = environment.crouch_walk_distance() * 20.0f
                + environment.distance_travelled() * 10.0f
                + environment.elapsed_seconds();
            if (representative == nullptr
                || display_priority > representative_priority
                || (display_priority == representative_priority
                    && (display_quality > representative_quality
                        || (display_quality == representative_quality
                            && tiebreak > representative_tiebreak))))
            {
                representative = &environment;
                representative_priority = display_priority;
                representative_quality = display_quality;
                representative_tiebreak = tiebreak;
            }
        }
        if (representative != nullptr)
        {
            snapshot.training_preview = *representative;
            snapshot.has_training_preview = true;
        }
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
        const TrainingMetrics& stage_metrics = worker_.metrics();
        snapshot.status.stage_fresh_updates = stage_metrics.total_updates >= stage_entry_total_updates_
            ? stage_metrics.total_updates - stage_entry_total_updates_ : 0u;
        snapshot.status.stage_required_updates = stage_minimum_fresh_updates(stage_);
        snapshot.status.stage_fresh_episodes = stage_metrics.total_episodes >= stage_entry_total_episodes_
            ? stage_metrics.total_episodes - stage_entry_total_episodes_ : 0u;
        snapshot.status.stage_required_episodes = stage_minimum_fresh_episodes(stage_);
        snapshot.status.stage_fresh_evaluations = stage_metrics.evaluation_count >= stage_entry_evaluation_count_
            ? stage_metrics.evaluation_count - stage_entry_evaluation_count_ : 0u;
        snapshot.status.stage_required_evaluations = static_cast<std::uint64_t>(
            required_mastery_confirmations(stage_));
        snapshot.status.updates_per_second = worker_updates_per_second_;
        snapshot.status.speed_mode = updates_per_cycle_.load(std::memory_order_relaxed);
        snapshot.status.worker_busy = worker_busy_.load(std::memory_order_relaxed);
        snapshot.status.pipeline_stage = worker_pipeline_stage_;
        snapshot.status.pipeline_stage_mask = worker_pipeline_stage_mask_;
        snapshot.status.message = worker_message_;

        std::scoped_lock lock(snapshot_mutex_);
        snapshot.serial = published_.serial + 1u;
        published_ = std::move(snapshot);
    }

    void AutonomousTrainer::queue_checkpoint_save(std::filesystem::path path,
        PpoTrainer::CheckpointData data)
    {
        PersistenceJob job{};
        job.kind = PersistenceKind::save_checkpoint;
        job.checkpoint_path = std::move(path);
        job.checkpoint = std::move(data);
        {
            std::scoped_lock lock(persistence_mutex_);
            persistence_queue_.push_back(std::move(job));
        }
        persistence_cv_.notify_all();
    }

    void AutonomousTrainer::queue_autosave()
    {
        PersistenceJob job{};
        job.kind = PersistenceKind::save_autosave;
        job.checkpoint = worker_.checkpoint_data();
        job.blueprint = worker_.blueprint();
        job.stage = stage_;
        job.difficulty = difficulty_;
        job.rig_generation = rig_generation_;
        job.accepted_rig_changes = accepted_rig_changes_;
        job.rejected_rig_changes = rejected_rig_changes_;
        job.rollback_count = rollback_count_;
        {
            std::scoped_lock lock(persistence_mutex_);
            job.checkpoint_path = autosave_checkpoint_;
            job.rig_path = autosave_rig_;
            job.state_path = autosave_state_;
            std::erase_if(persistence_queue_, [](const PersistenceJob& queued)
            {
                return queued.kind == PersistenceKind::save_autosave;
            });
            persistence_queue_.push_back(std::move(job));
        }
        persistence_cv_.notify_all();
    }

    void AutonomousTrainer::queue_checkpoint_load(std::filesystem::path path, bool transfer_only)
    {
        PersistenceJob job{};
        job.kind = PersistenceKind::load_checkpoint;
        job.checkpoint_path = std::move(path);
        job.transfer_only = transfer_only;
        {
            std::scoped_lock lock(persistence_mutex_);
            persistence_queue_.push_back(std::move(job));
        }
        persistence_cv_.notify_all();
    }

    void AutonomousTrainer::queue_autosave_load()
    {
        PersistenceJob job{};
        job.kind = PersistenceKind::load_autosave;
        job.blueprint = live_blueprint_;
        {
            std::scoped_lock lock(persistence_mutex_);
            job.checkpoint_path = autosave_checkpoint_;
            job.rig_path = autosave_rig_;
            job.state_path = autosave_state_;
            persistence_queue_.push_back(std::move(job));
        }
        persistence_cv_.notify_all();
    }

    void AutonomousTrainer::consume_persistence_message()
    {
        std::scoped_lock lock(persistence_mutex_);
        if (consumed_persistence_message_serial_ == persistence_message_serial_)
            return;
        consumed_persistence_message_serial_ = persistence_message_serial_;
        worker_message_ = persistence_message_;
    }

    void AutonomousTrainer::persistence_main(std::stop_token stop_token)
    {
        while (!stop_token.stop_requested())
        {
            PersistenceJob job{};
            {
                std::unique_lock lock(persistence_mutex_);
                persistence_cv_.wait(lock, stop_token, [this]
                {
                    return !persistence_queue_.empty();
                });
                if (stop_token.stop_requested())
                    break;
                job = std::move(persistence_queue_.front());
                persistence_queue_.pop_front();
            }

            std::string message{};
            if (job.kind == PersistenceKind::save_checkpoint)
            {
                if (PpoTrainer::write_checkpoint_data(job.checkpoint, job.checkpoint_path, message))
                    message = "CHECKPOINT SAVED ASYNCHRONOUSLY";
            }
            else if (job.kind == PersistenceKind::save_autosave)
            {
                bool ok = true;
                if (!job.checkpoint_path.empty())
                    ok = PpoTrainer::write_checkpoint_data(job.checkpoint, job.checkpoint_path, message);
                if (ok && !job.rig_path.empty())
                    ok = job.blueprint.save(job.rig_path, message);
                if (ok)
                    ok = write_autonomy_state(job.state_path, job.stage, job.difficulty,
                        job.rig_generation, job.accepted_rig_changes,
                        job.rejected_rig_changes, job.rollback_count, message);
                if (ok)
                    message = "AUTOSAVE SNAPSHOT PUBLISHED ASYNCHRONOUSLY";
            }
            else if (job.kind == PersistenceKind::load_checkpoint)
            {
                auto data = std::make_shared<PpoTrainer::CheckpointData>();
                if (PpoTrainer::read_checkpoint_data(job.checkpoint_path, *data, message))
                {
                    PendingCommand command{};
                    command.type = CommandType::apply_checkpoint;
                    command.checkpoint = std::move(data);
                    command.transfer_only = job.transfer_only;
                    enqueue_command(std::move(command));
                    message = "CHECKPOINT READ ASYNCHRONOUSLY - APPLY QUEUED";
                }
            }
            else
            {
                if (!job.rig_path.empty() && std::filesystem::exists(job.rig_path))
                {
                    std::string rig_error{};
                    const sim::CreatureBlueprint loaded = sim::CreatureBlueprint::load(job.rig_path, rig_error);
                    if (rig_error.empty())
                        job.blueprint = loaded;
                    else
                        message = rig_error;
                }
                auto data = std::make_shared<PpoTrainer::CheckpointData>();
                if (message.empty()
                    && PpoTrainer::read_checkpoint_data(job.checkpoint_path, *data, message))
                {
                    read_autonomy_state(job.state_path, job.stage, job.difficulty,
                        job.rig_generation, job.accepted_rig_changes,
                        job.rejected_rig_changes, job.rollback_count);
                    PendingCommand command{};
                    command.type = CommandType::apply_autosave;
                    command.blueprint = std::move(job.blueprint);
                    command.checkpoint = std::move(data);
                    command.rig_generation = job.rig_generation;
                    command.accepted_rig_changes = job.accepted_rig_changes;
                    command.rejected_rig_changes = job.rejected_rig_changes;
                    command.rollback_count = job.rollback_count;
                    enqueue_command(std::move(command));
                    message = "AUTOSAVE READ ASYNCHRONOUSLY - APPLY QUEUED";
                }
            }

            {
                std::scoped_lock lock(persistence_mutex_);
                persistence_message_ = std::move(message);
                ++persistence_message_serial_;
            }
            wake_cv_.notify_all();
        }
    }
}
