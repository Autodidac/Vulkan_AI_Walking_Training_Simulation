#pragma once

#include "ai_director.hpp"
#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace runner::integration
{
    inline constexpr std::uint32_t api_version = 3u;

    enum class ControlMode : std::uint8_t
    {
        autonomous,
        player_guided,
        training
    };

    enum class DiagnosticKind : std::uint8_t
    {
        trial_started,
        task_selected,
        task_rejected,
        trial_retried,
        challenge_entered,
        contact_changed,
        traction_limited,
        water_phase_changed,
        equipment_state_changed,
        target_missed,
        target_hit,
        trial_stalled,
        trial_terminated
    };

    struct TaskCommand
    {
        std::uint32_t stable_id{};
        director::TaskKind task{ director::TaskKind::stand };
        director::ChallengeKind challenge{ director::ChallengeKind::flat };
        bool supported{};

        [[nodiscard]] constexpr bool selected() const noexcept
        {
            return stable_id != 0u;
        }
    };

    [[nodiscard]] constexpr TaskCommand task_command(
        const director::TaskGraph& graph,
        const director::Decision& decision) noexcept
    {
        if (!graph.valid() || !decision.selected()
            || decision.task_index >= graph.count)
            return {};
        const director::TaskNode& node = graph.nodes[decision.task_index];
        if (node.stable_id != decision.stable_id || !node.enabled)
            return {};
        return { node.stable_id, node.task, node.challenge,
            director::task_supported(node.task) };
    }

    [[nodiscard]] constexpr director::Profile profile_for(
        ControlMode mode) noexcept
    {
        switch (mode)
        {
        case ControlMode::autonomous: return director::Profile::autonomous_npc;
        case ControlMode::player_guided: return director::Profile::player_guided;
        case ControlMode::training: return director::Profile::curriculum;
        }
        return director::Profile::curriculum;
    }

    struct DesiredTaskState
    {
        sim::CourseStage stage{ sim::CourseStage::balance };
        sim::GaitTask gait{ sim::GaitTask::walk };
        sim::EquipmentDirective equipment{ sim::EquipmentDirective::passive };
        bool equipment_task{};
        bool valid{};
    };

    [[nodiscard]] constexpr DesiredTaskState desired_task_state(
        director::TaskKind task) noexcept
    {
        using enum director::TaskKind;
        switch (task)
        {
        case stand:
            return { sim::CourseStage::balance, sim::GaitTask::walk,
                sim::EquipmentDirective::passive, false, true };
        case walk:
        case wade:
        case swim:
        case shore_exit:
            return { sim::CourseStage::uneven, sim::GaitTask::walk,
                sim::EquipmentDirective::passive, false, true };
        case safe_carry_walk:
            return { sim::CourseStage::uneven, sim::GaitTask::walk,
                sim::EquipmentDirective::safe_carry_walk, true, true };
        case low_ready_walk:
            return { sim::CourseStage::uneven, sim::GaitTask::walk,
                sim::EquipmentDirective::low_ready_walk, true, true };
        case speed_walk:
            return { sim::CourseStage::uneven, sim::GaitTask::speed_walk,
                sim::EquipmentDirective::passive, false, true };
        case walk_run_transition:
            return { sim::CourseStage::uneven,
                sim::GaitTask::walk_run_transition,
                sim::EquipmentDirective::passive, false, true };
        case run:
            return { sim::CourseStage::uneven, sim::GaitTask::run,
                sim::EquipmentDirective::passive, false, true };
        case crouch_walk:
            return { sim::CourseStage::crouch_walk,
                sim::GaitTask::walk,
                sim::EquipmentDirective::passive, false, true };
        case stop_and_plant:
            return { sim::CourseStage::equipment_targets, sim::GaitTask::walk,
                sim::EquipmentDirective::stop_and_plant, true, true };
        case gun_stance:
            return { sim::CourseStage::equipment_targets, sim::GaitTask::walk,
                sim::EquipmentDirective::gun_stance, true, true };
        case acquire_and_aim:
            return { sim::CourseStage::equipment_targets, sim::GaitTask::walk,
                sim::EquipmentDirective::acquire_and_aim, true, true };
        case fire_and_correct:
            return { sim::CourseStage::equipment_targets, sim::GaitTask::walk,
                sim::EquipmentDirective::fire_and_correct, true, true };
        case break_contact:
            return { sim::CourseStage::combat_course, sim::GaitTask::walk,
                sim::EquipmentDirective::break_contact, true, true };
        case recover:
            return { sim::CourseStage::duck_press, sim::GaitTask::walk,
                sim::EquipmentDirective::passive, false, true };
        case construction:
        case tool_traversal:
            return {};
        }
        return {};
    }

    struct TaskApplication
    {
        std::uint32_t stable_task_id{};
        bool selected{};
        bool applied{};
        bool rejected{};
    };

    [[nodiscard]] inline TaskApplication apply_task(
        sim::Environment& environment, const TaskCommand& command,
        float difficulty = 0.30f)
    {
        TaskApplication result{};
        result.stable_task_id = command.stable_id;
        result.selected = command.selected();
        if (!result.selected)
            return result;
        const DesiredTaskState desired = desired_task_state(command.task);
        if (!command.supported || !desired.valid
            || (desired.equipment_task && !environment.equipment_capable()))
        {
            result.rejected = true;
            return result;
        }
        const bool stage_changed = environment.course_stage() != desired.stage;
        const bool gait_changed = environment.gait_task() != desired.gait;
        const bool directive_changed =
            environment.equipment_directive() != desired.equipment;
        if (stage_changed)
            environment.set_course(desired.stage, difficulty);
        if (gait_changed)
            environment.set_gait_task(desired.gait);
        const bool equipment_changed = desired.equipment_task
            && environment.weapon_class() != sim::WeaponClass::carbine;
        if (equipment_changed)
            environment.configure_equipment(sim::WeaponClass::carbine, 8.0f);
        if (directive_changed)
            environment.set_equipment_directive(desired.equipment);
        result.applied = stage_changed || gait_changed || equipment_changed
            || directive_changed;
        return result;
    }

    struct ControlRequest
    {
        std::array<float, sim::action_count> actions{};
        TaskCommand selected_task{};
        ControlMode mode{ ControlMode::autonomous };
        bool retry_trial{};
    };

    struct RuntimeSnapshot
    {
        std::uint64_t trial_id{};
        std::uint64_t fixed_tick{};
        Vec2 root_position{};
        Vec2 root_velocity{};
        float uprightness{};
        float forward_speed{};
        float water_depth{};
        float water_submersion{};
        float stance_slip_speed{};
        float stance_slip_distance{};
        float available_traction{};
        float used_traction{};
        float challenge_progress{};
        std::uint32_t stable_challenge_id{};
        std::uint32_t shots_fired{};
        std::uint32_t target_hits{};
        std::uint32_t shots_missed{};
        sim::TerrainRegion terrain_region{ sim::TerrainRegion::firm };
        sim::WaterTraversalPhase water_phase{ sim::WaterTraversalPhase::dry };
        sim::WeaponClass weapon{ sim::WeaponClass::none };
        sim::EquipmentState equipment{ sim::EquipmentState::unarmed };
        sim::EquipmentDirective equipment_directive{ sim::EquipmentDirective::passive };
        bool left_supported{};
        bool right_supported{};
        bool valid{};
        bool motion_valid{};
    };

    struct FixedStepResult
    {
        RuntimeSnapshot snapshot{};
        float reward{};
        bool terminal{};
        sim::InvalidMotion invalid_reason{ sim::InvalidMotion::none };
        sim::TrialTerminalCause terminal_cause{ sim::TrialTerminalCause::none };
        std::uint32_t stable_task_id{};
        bool task_applied{};
        bool task_rejected{};
        bool retry_applied{};
    };

    struct DiagnosticEvent
    {
        std::uint64_t trial_id{};
        std::uint64_t fixed_tick{};
        DiagnosticKind kind{ DiagnosticKind::trial_started };
        std::uint32_t stable_task_id{};
        std::uint32_t stable_challenge_id{};
        float primary_value{};
        float secondary_value{};
    };

    struct DiagnosticEvents
    {
        std::array<DiagnosticEvent, 16u> values{};
        std::size_t count{};

        void push(DiagnosticEvent event) noexcept
        {
            if (count < values.size())
                values[count++] = event;
        }

        [[nodiscard]] std::span<const DiagnosticEvent> span() const noexcept
        {
            return { values.data(), count };
        }
    };

    [[nodiscard]] inline RuntimeSnapshot snapshot(const sim::Environment& environment,
        std::uint64_t trial_id, std::uint64_t fixed_tick,
        float fixed_dt = 1.0f / 60.0f) noexcept
    {
        RuntimeSnapshot value{};
        value.trial_id = trial_id;
        value.fixed_tick = fixed_tick;
        const auto& particles = environment.particles();
        const auto root = environment.blueprint().root_node;
        if (root < particles.size())
        {
            value.root_position = particles[root].position;
            const float safe_dt = std::isfinite(fixed_dt) && fixed_dt > 1.0e-6f
                ? fixed_dt : 1.0f / 60.0f;
            value.root_velocity = (particles[root].position - particles[root].previous)
                / safe_dt;
            value.valid = std::isfinite(value.root_position.x)
                && std::isfinite(value.root_position.y)
                && std::isfinite(value.root_velocity.x)
                && std::isfinite(value.root_velocity.y);
        }
        value.uprightness = environment.uprightness();
        value.forward_speed = environment.forward_speed();
        value.water_depth = environment.water_depth();
        value.water_submersion = environment.water_submersion();
        value.stance_slip_speed = environment.stance_slip_speed();
        value.stance_slip_distance = environment.stance_slip_distance();
        value.available_traction = environment.available_traction();
        value.used_traction = environment.used_traction();
        value.terrain_region = environment.terrain_region_at(value.root_position.x);
        const float sample_x = sim::terrain_sample_x(value.root_position.x,
            environment.course_progress());
        const sim::TerrainChallenge challenge = environment.terrain().challenge_at(sample_x);
        value.stable_challenge_id = challenge.stable_id;
        value.challenge_progress = environment.terrain().challenge_progress_at(sample_x);
        value.water_phase = environment.water_traversal_phase();
        value.weapon = environment.weapon_class();
        value.equipment = environment.equipment_state();
        value.equipment_directive = environment.equipment_directive();
        value.shots_fired = environment.shots_fired();
        value.target_hits = environment.target_hits();
        value.shots_missed = environment.shots_missed();
        value.left_supported = environment.left_supported();
        value.right_supported = environment.right_supported();
        value.motion_valid = environment.valid_motion();
        return value;
    }

    [[nodiscard]] inline DiagnosticEvents diagnostic_events(
        const RuntimeSnapshot* previous, const FixedStepResult& step,
        std::uint32_t stable_task_id = 0u) noexcept
    {
        DiagnosticEvents events{};
        const RuntimeSnapshot& current = step.snapshot;
        const std::uint32_t effective_task_id = stable_task_id != 0u
            ? stable_task_id : step.stable_task_id;
        const auto emit = [&](DiagnosticKind kind, float primary = 0.0f,
            float secondary = 0.0f) noexcept
        {
            events.push({ current.trial_id, current.fixed_tick, kind,
                effective_task_id, current.stable_challenge_id, primary, secondary });
        };
        if (step.task_rejected)
            emit(DiagnosticKind::task_rejected);
        else if (step.task_applied)
            emit(DiagnosticKind::task_selected);
        if (step.retry_applied)
            emit(DiagnosticKind::trial_retried);
        if (previous == nullptr || previous->trial_id != current.trial_id)
            emit(DiagnosticKind::trial_started);
        if (previous == nullptr
            || previous->stable_challenge_id != current.stable_challenge_id)
            emit(DiagnosticKind::challenge_entered,
                current.challenge_progress, static_cast<float>(current.terrain_region));
        if (previous == nullptr || previous->left_supported != current.left_supported
            || previous->right_supported != current.right_supported)
            emit(DiagnosticKind::contact_changed,
                current.left_supported ? 1.0f : 0.0f,
                current.right_supported ? 1.0f : 0.0f);
        if (current.available_traction > 1.0e-6f
            && current.used_traction >= current.available_traction * 0.95f
            && current.stance_slip_speed > 0.02f)
            emit(DiagnosticKind::traction_limited,
                current.used_traction, current.available_traction);
        if (previous == nullptr || previous->water_phase != current.water_phase)
            emit(DiagnosticKind::water_phase_changed,
                static_cast<float>(current.water_phase), current.water_depth);
        if (previous == nullptr || previous->equipment != current.equipment)
            emit(DiagnosticKind::equipment_state_changed,
                static_cast<float>(current.equipment));
        if (previous != nullptr && current.shots_missed > previous->shots_missed)
            emit(DiagnosticKind::target_missed,
                static_cast<float>(current.shots_missed));
        if (previous != nullptr && current.target_hits > previous->target_hits)
            emit(DiagnosticKind::target_hit,
                static_cast<float>(current.target_hits));
        if (step.terminal)
            emit(DiagnosticKind::trial_terminated,
                static_cast<float>(step.invalid_reason));
        return events;
    }

    [[nodiscard]] inline FixedStepResult fixed_step(sim::Environment& environment,
        const ControlRequest& request, std::uint64_t trial_id,
        std::uint64_t fixed_tick, float fixed_dt = 1.0f / 60.0f)
    {
        const float safe_dt = std::isfinite(fixed_dt)
            ? std::clamp(fixed_dt, 1.0f / 240.0f, 1.0f / 20.0f)
            : 1.0f / 60.0f;
        const TaskApplication task = apply_task(environment, request.selected_task);
        const bool retry_applied = request.retry_trial;
        if (retry_applied)
            environment.reset();
        const sim::StepResult result = environment.step(request.actions, safe_dt);
        FixedStepResult output{};
        output.snapshot = snapshot(environment, trial_id, fixed_tick, safe_dt);
        output.reward = result.reward;
        output.terminal = result.terminated;
        output.invalid_reason = result.invalid_reason;
        output.terminal_cause = result.terminal_cause;
        output.stable_task_id = task.stable_task_id;
        output.task_applied = task.applied;
        output.task_rejected = task.rejected;
        output.retry_applied = retry_applied;
        return output;
    }
}
