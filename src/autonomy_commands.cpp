#include "autonomy.hpp"
#include "integration.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace runner::rl
{
    void AutonomousTrainer::set_autosave_paths(std::filesystem::path checkpoint,
        std::filesystem::path rig, std::filesystem::path state)
    {
        // Do not treat the legacy Assisted Stand ledger as unaided evidence
        // or write back to the user's existing Human autosave.
        const auto isolate_human_autosave = [](std::filesystem::path& path)
        {
            std::string name = path.filename().string();
            constexpr std::string_view old_prefix{ "epoch2dwalk-v0751-human-" };
            if (name.starts_with(old_prefix))
            {
                name.replace(0u, old_prefix.size(),
                    "epoch2dwalk-v0751-raw-stand-human-");
                path.replace_filename(name);
            }
        };
        isolate_human_autosave(checkpoint);
        isolate_human_autosave(rig);
        isolate_human_autosave(state);
        std::scoped_lock lock(persistence_mutex_);
        autosave_checkpoint_ = std::move(checkpoint);
        autosave_rig_ = std::move(rig);
        autosave_state_ = std::move(state);
    }

    bool AutonomousTrainer::load_autosave(std::string& message,
        bool start_training_after_load)
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
                constexpr std::string_view current_prefix{
                    "epoch2dwalk-v0751-" };
                constexpr std::string_view prior_prefix{ "runner-v0750-" };
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
                    legacy_lifetime_version = "V0.7.50";
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
            message = "NO V0.7.51 AUTOSAVE FOUND - STARTING WITH STAND TRAINING";
            return false;
        }
        queue_autosave_load(std::move(checkpoint), std::move(rig), std::move(state),
            start_training_after_load);
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
        const auto activate_director_task = [&](director::Evidence evidence,
            director::Decision selected, director::Profile profile) -> bool
        {
            const integration::TaskCommand task_command =
                integration::task_command(director_state_.graph,
                    evidence, selected, profile);
            if (!selected.selected() || !task_command.selected()
                || selected.task_index >= director_state_.graph.count)
            {
                worker_message_ = "DIRECTOR TASK REJECTED - PREREQUISITES NOT PROVEN";
                return false;
            }
            const director::TaskNode& node =
                director_state_.graph.nodes[selected.task_index];
            const integration::DesiredTaskState desired =
                integration::desired_task_state(node.task, node.challenge);
            if (!node.enabled || !director::task_supported(node.task)
                || !desired.valid)
            {
                worker_message_ = std::format("DIRECTOR TASK UNAVAILABLE - {}",
                    director::task_name(node.task));
                return false;
            }
            if (desired.equipment_task && !worker_.preview().equipment_capable())
            {
                worker_message_ = "DIRECTOR GUN TASK REJECTED - RIG HAS NO PHYSICAL MOUNT";
                return false;
            }

            // Selection is a transaction: authenticate and resolve the exact
            // physical challenge against an isolated candidate before any
            // user-visible trainer, graph, or policy state is changed.
            sim::Environment candidate = worker_.preview();
            const integration::TaskApplication preflight =
                integration::apply_task(candidate, task_command,
                    director_state_.graph, evidence, profile);
            if (preflight.rejected || !preflight.challenge_present)
            {
                worker_message_ = "DIRECTOR TASK REJECTED - PHYSICAL CHALLENGE ABSENT";
                return false;
            }

            stage_ = desired.stage;
            gait_task_ = desired.gait;
            difficulty_ = clamp(node.difficulty, 0.10f, 1.0f);
            worker_.set_course(stage_, difficulty_, false);
            worker_.set_gait_task(gait_task_, false);
            worker_.set_equipment_directive(desired.equipment);
            director_state_.profile = profile;
            director_state_.evidence = evidence;
            director_state_.current = selected;
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            last_evaluation_count_ = worker_.metrics().evaluation_count;
            last_saved_best_update_ = 0u;
            stage_entry_total_updates_ = worker_.metrics().total_updates;
            stage_entry_total_episodes_ = worker_.metrics().total_episodes;
            stage_entry_evaluation_count_ = worker_.metrics().evaluation_count;
            stage_entry_baseline_initialized_ = true;
            worker_message_ = std::format(
                "DIRECTOR TASK SELECTED - {} / {} [CHALLENGE {:08X}]",
                director::task_name(node.task),
                director::challenge_name(node.challenge),
                preflight.stable_challenge_id);
            queue_autosave();
            return true;
        };

        switch (command.type)
        {
        case CommandType::set_blueprint:
        {
            const bool requalify_human = command.blueprint.human_casual_gait_plan()
                && worker_.rig_signature() != command.blueprint.signature();
            if (!command.preserve_policy || requalify_human)
            {
                stage_ = sim::CourseStage::balance;
                gait_task_ = sim::GaitTask::walk;
                difficulty_ = 0.25f;
                rig_generation_ = 0;
                accepted_rig_changes_ = 0;
                rejected_rig_changes_ = 0;
                rollback_count_ = 0;
                optimization_mode_ = RigOptimizationMode::control_optimize;
                director_state_ = director::make_state(director_state_.profile);
            }
            worker_.set_blueprint(command.blueprint, command.preserve_policy);
            director_state_.graph = integration::compatible_task_graph(
                worker_.preview(), director::default_training_graph());
            const auto clear_incompatible_reference = [&](std::uint16_t& index)
            {
                if (index >= director_state_.graph.count
                    || !director_state_.graph.nodes[index].enabled)
                    index = director::invalid_task_index;
            };
            clear_incompatible_reference(director_state_.evidence.first_stall);
            clear_incompatible_reference(director_state_.evidence.requested_goal);
            director_state_.current = director::select(director_state_.graph,
                director_state_.evidence, director_state_.profile);
            if (director_state_.current.selected())
            {
                const director::TaskNode& task = director_state_.graph.nodes[
                    director_state_.current.task_index];
                const integration::DesiredTaskState desired =
                    integration::desired_task_state(task.task, task.challenge);
                if (desired.valid)
                {
                    stage_ = desired.stage;
                    gait_task_ = desired.gait;
                    difficulty_ = clamp(task.difficulty, 0.10f, 1.0f);
                }
            }
            worker_.set_course(stage_, difficulty_, false);
            worker_.set_gait_task(gait_task_, false);
            if (director_state_.current.selected())
            {
                const director::TaskNode& task = director_state_.graph.nodes[
                    director_state_.current.task_index];
                worker_.set_equipment_directive(
                    integration::desired_task_state(task.task, task.challenge).equipment);
            }
            else
                worker_.set_equipment_directive(sim::EquipmentDirective::passive);
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
        }

        case CommandType::reset_policy:
            if (worker_.blueprint().human_casual_gait_plan())
            {
                stage_ = sim::CourseStage::balance;
                difficulty_ = 0.25f;
                gait_task_ = sim::GaitTask::walk;
                director_state_ = director::make_state(director_state_.profile);
                director_state_.graph = integration::compatible_task_graph(
                    worker_.preview(), director::default_training_graph());
                director_state_.current = director::select(director_state_.graph,
                    director_state_.evidence, director_state_.profile);
                worker_.set_gait_task(gait_task_, false);
                worker_.set_equipment_directive(sim::EquipmentDirective::passive);
            }
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
        {
            const std::uint16_t task_index = command.gait_task == sim::GaitTask::walk
                ? 1u : command.gait_task == sim::GaitTask::speed_walk
                ? 7u : command.gait_task == sim::GaitTask::walk_run_transition
                ? 8u : 9u;
            director::Evidence evidence = director_state_.evidence;
            evidence.requested_goal = task_index;
            const director::Decision selected = director::select(
                director_state_.graph, evidence, director_state_.profile);
            if (!selected.selected() || selected.task_index != task_index)
                worker_message_ = "GAIT TASK REJECTED - DIRECTOR PREREQUISITES NOT PROVEN";
            else
                (void)activate_director_task(evidence, selected,
                    director_state_.profile);
            break;
        }

        case CommandType::set_director_task:
        {
            const std::uint16_t index = command.director_task_index;
            if (index >= director_state_.graph.count)
            {
                worker_message_ = "DIRECTOR TASK REJECTED - INVALID STABLE INDEX";
                break;
            }
            director::Evidence requested_evidence = director_state_.evidence;
            requested_evidence.requested_goal = index;
            const director::Decision selected = director::select(
                director_state_.graph, requested_evidence,
                director_state_.profile);
            if (!selected.selected() || selected.task_index != index)
            {
                worker_message_ = "DIRECTOR TASK REJECTED - PREREQUISITES NOT PROVEN";
                break;
            }
            (void)activate_director_task(requested_evidence, selected,
                director_state_.profile);
            break;
        }

        case CommandType::set_director_profile:
        {
            if (command.director_profile != director::Profile::curriculum
                && command.director_profile != director::Profile::autonomous_npc
                && command.director_profile != director::Profile::player_guided)
            {
                worker_message_ = "DIRECTOR PROFILE REJECTED - INVALID AUTHORITY";
                break;
            }
            director::Evidence evidence = director_state_.evidence;
            const director::Decision selected = director::select(
                director_state_.graph, evidence, command.director_profile);
            if (!selected.selected())
            {
                director_state_.profile = command.director_profile;
                director_state_.current = selected;
                worker_message_ = selected.reason == director::SelectionReason::graph_complete
                    ? "DIRECTOR PROFILE CHANGED - TASK GRAPH COMPLETE"
                    : "DIRECTOR PROFILE REJECTED - NO ELIGIBLE PHYSICAL TASK";
                queue_autosave();
                break;
            }
            (void)activate_director_task(evidence, selected,
                command.director_profile);
            break;
        }

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
                if (worker_.blueprint().human_casual_gait_plan()
                    && (stage_ != sim::CourseStage::balance
                        || command.checkpoint->stage != sim::CourseStage::balance))
                {
                    worker_message_ = "HUMAN CHECKPOINT REJECTED - AUTOPILOT REQUIRES ITS VERIFIED STAND LINEAGE";
                    break;
                }
                std::string error{};
                if (worker_.apply_checkpoint_data(std::move(*command.checkpoint), error,
                    command.transfer_only))
                {
                    stage_ = worker_.course_stage();
                    difficulty_ = worker_.course_difficulty();
                    worker_message_ = command.transfer_only
                        ? "CONTROLLER TRANSFERRED - CURRENT SKILL RECALIBRATING"
                        : "CHECKPOINT LOADED - TRAINING ENABLE STATE UNCHANGED";
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
                const bool unverified_human_import = command.blueprint.human_casual_gait_plan()
                    && !command.autosave_state_verified;
                if (unverified_human_import)
                    error = "HISTORICAL CONTROLLER HAS NO VERIFIED UNAIDED STAND";
                if (!unverified_human_import
                    && worker_.apply_checkpoint_data(std::move(*command.checkpoint), error, false))
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
                    if (command.director_state.valid())
                    {
                        director_state_ = command.director_state;
                        if (director_state_.current.selected())
                        {
                            const director::TaskNode& node =
                                director_state_.graph.nodes[
                                    director_state_.current.task_index];
                            const integration::DesiredTaskState desired =
                                integration::desired_task_state(
                                    node.task, node.challenge);
                            if (desired.valid)
                                worker_.set_equipment_directive(
                                    desired.equipment);
                        }
                    }
                    worker_message_ = std::format("V0.7.51 AUTOSAVE RESUMED - {}",
                        rig_optimization_mode_name(optimization_mode_));
                    if (command.start_training_after_load)
                        set_background_enabled(true);
                }
                else if (worker_.import_lifetime_ledger(lifetime, error))
                {
                    // An unchanged rig signature does not reset set_blueprint().
                    // Drop incompatible weights/optimizer even on a same-rig load,
                    // preserving only the historical totals just imported.
                    worker_.reset_policy(0xC0FFEEu, false);
                    enabled_.store(false, std::memory_order_relaxed);
                    requested_updates_.store(0u, std::memory_order_relaxed);
                    stage_ = sim::CourseStage::balance;
                    gait_task_ = sim::GaitTask::walk;
                    difficulty_ = 0.25f;
                    director_state_ = director::make_state(director::Profile::curriculum);
                    director_state_.graph = integration::compatible_task_graph(
                        worker_.preview(), director::default_training_graph());
                    director_state_.current = director::select(director_state_.graph,
                        director_state_.evidence, director_state_.profile);
                    worker_.set_course(stage_, difficulty_, false);
                    worker_.set_gait_task(gait_task_, false);
                    worker_.set_equipment_directive(sim::EquipmentDirective::passive);
                    mastery_streak_ = 0;
                    degradation_streak_ = 0;
                    last_evaluation_count_ = worker_.metrics().evaluation_count;
                    last_saved_best_update_ = 0;
                    stage_entry_total_updates_ = worker_.metrics().total_updates;
                    stage_entry_total_episodes_ = worker_.metrics().total_episodes;
                    stage_entry_evaluation_count_ = worker_.metrics().evaluation_count;
                    stage_entry_baseline_initialized_ = true;
                    if (command.start_training_after_load)
                        set_background_enabled(true);
                    worker_message_ = error + (command.start_training_after_load
                        ? " - STAND RETRAINING STARTED" : " - STAND RETRAINING PAUSED");
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
