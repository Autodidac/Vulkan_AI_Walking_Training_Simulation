#pragma once

#include "ai_director.hpp"
#include "ppo.hpp"
#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace runner::integration
{
    inline constexpr std::uint32_t api_version = 7u;
    inline constexpr std::uint32_t action_layout_schema_version = 2u;

    enum class ControlMode : std::uint8_t
    {
        autonomous,
        player_guided,
        training
    };

    // The public fixed-step boundary never infers either of these authorities
    // from Environment defaults. An unspecified value is rejected before a
    // task, reset, or physics step can mutate caller-owned state.
    enum class JointControlSource : std::uint8_t
    {
        unspecified,
        retained_policy,
        expert_joint_actions
    };

    enum class GuidanceRequest : std::uint8_t
    {
        unspecified,
        raw_policy,
        assisted
    };

    enum class ControlRejection : std::uint8_t
    {
        none,
        unspecified_joint_control,
        unspecified_guidance,
        malformed_director_state,
        missing_player_intent,
        invalid_player_intent,
        unsupported_player_intent,
        player_guidance_not_raw,
        retained_player_guidance_not_raw,
        mixed_joint_authority,
        invalid_expert_action,
        inactive_expert_action,
        retained_policy_required,
        retained_policy_mismatch,
        task_rejected,
        nonfinite_policy_action
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
        std::uint16_t task_index{ director::invalid_task_index };
        director::TaskKind task{ director::TaskKind::stand };
        director::ChallengeKind challenge{ director::ChallengeKind::flat };
        director::Profile profile{ director::Profile::curriculum };

        [[nodiscard]] constexpr bool selected() const noexcept
        {
            return stable_id != 0u
                && task_index != director::invalid_task_index;
        }
    };

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

    enum class ChallengeAuthority : std::uint8_t
    {
        none,
        terrain_interval,
        terrain_boundary,
        course_feature,
        equipment_target
    };

    struct DesiredTaskState
    {
        director::TaskKind task{ director::TaskKind::stand };
        director::ChallengeKind challenge{ director::ChallengeKind::flat };
        sim::CourseStage stage{ sim::CourseStage::balance };
        sim::GaitTask gait{ sim::GaitTask::walk };
        sim::EquipmentDirective equipment{ sim::EquipmentDirective::passive };
        ChallengeAuthority authority{ ChallengeAuthority::none };
        sim::TerrainRegion terrain_region{ sim::TerrainRegion::firm };
        sim::CourseFeatureKind feature{ sim::CourseFeatureKind::rock };
        sim::WaterTraversalPhase water_phase{ sim::WaterTraversalPhase::dry };
        bool equipment_task{};
        bool valid{};
    };

    [[nodiscard]] constexpr DesiredTaskState desired_task_state(
        director::TaskKind task, director::ChallengeKind challenge) noexcept
    {
        using enum director::TaskKind;
        switch (task)
        {
        case stand:
            if (challenge == director::ChallengeKind::flat)
                return { task, challenge, sim::CourseStage::balance,
                    sim::GaitTask::walk, sim::EquipmentDirective::passive,
                    ChallengeAuthority::terrain_interval,
                    sim::TerrainRegion::firm, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, false, true };
            return {};
        case walk:
            if (challenge == director::ChallengeKind::flat)
                return { task, challenge, sim::CourseStage::uneven,
                    sim::GaitTask::walk, sim::EquipmentDirective::passive,
                    ChallengeAuthority::terrain_interval,
                    sim::TerrainRegion::firm, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, false, true };
            if (challenge == director::ChallengeKind::loose_cells)
                return { task, challenge, sim::CourseStage::uneven,
                    sim::GaitTask::walk, sim::EquipmentDirective::passive,
                    ChallengeAuthority::terrain_interval,
                    sim::TerrainRegion::dry_sand, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, false, true };
            if (challenge == director::ChallengeKind::waterlogged_cells)
                return { task, challenge, sim::CourseStage::uneven,
                    sim::GaitTask::walk, sim::EquipmentDirective::passive,
                    ChallengeAuthority::terrain_interval,
                    sim::TerrainRegion::waterlogged, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::approach, false, true };
            return {};
        case wade:
            if (challenge == director::ChallengeKind::shallow_water)
                return { task, challenge, sim::CourseStage::uneven,
                    sim::GaitTask::walk, sim::EquipmentDirective::passive,
                    ChallengeAuthority::terrain_interval,
                    sim::TerrainRegion::shallow_water, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::wading, false, true };
            return {};
        case swim:
            if (challenge == director::ChallengeKind::shallow_water)
                return { task, challenge, sim::CourseStage::uneven,
                    sim::GaitTask::walk, sim::EquipmentDirective::passive,
                    ChallengeAuthority::terrain_interval,
                    sim::TerrainRegion::shallow_water, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::swimming, false, true };
            return {};
        case shore_exit:
            if (challenge == director::ChallengeKind::shore)
                return { task, challenge, sim::CourseStage::uneven,
                    sim::GaitTask::walk, sim::EquipmentDirective::passive,
                    ChallengeAuthority::terrain_boundary,
                    sim::TerrainRegion::shallow_water, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::shore_exit, false, true };
            return {};
        case safe_carry_walk:
            if (challenge == director::ChallengeKind::flat)
                return { task, challenge, sim::CourseStage::uneven,
                    sim::GaitTask::walk,
                    sim::EquipmentDirective::safe_carry_walk,
                    ChallengeAuthority::terrain_interval,
                    sim::TerrainRegion::firm, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, true, true };
            return {};
        case low_ready_walk:
            if (challenge == director::ChallengeKind::flat)
                return { task, challenge, sim::CourseStage::uneven,
                    sim::GaitTask::walk,
                    sim::EquipmentDirective::low_ready_walk,
                    ChallengeAuthority::terrain_interval,
                    sim::TerrainRegion::firm, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, true, true };
            return {};
        case speed_walk:
            if (challenge == director::ChallengeKind::ramp)
                return { task, challenge, sim::CourseStage::uneven,
                    sim::GaitTask::speed_walk,
                    sim::EquipmentDirective::passive,
                    ChallengeAuthority::terrain_interval,
                    sim::TerrainRegion::firm, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, false, true };
            return {};
        case walk_run_transition:
            if (challenge == director::ChallengeKind::step)
                return { task, challenge, sim::CourseStage::uneven,
                    sim::GaitTask::walk_run_transition,
                    sim::EquipmentDirective::passive,
                    ChallengeAuthority::terrain_interval,
                    sim::TerrainRegion::firm, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, false, true };
            return {};
        case run:
            if (challenge == director::ChallengeKind::gap)
                return { task, challenge, sim::CourseStage::uneven,
                    sim::GaitTask::run, sim::EquipmentDirective::passive,
                    ChallengeAuthority::terrain_interval,
                    sim::TerrainRegion::hole, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, false, true };
            return {};
        case crouch_walk:
            if (challenge == director::ChallengeKind::low_clearance)
                return { task, challenge, sim::CourseStage::crouch_walk,
                    sim::GaitTask::walk, sim::EquipmentDirective::passive,
                    ChallengeAuthority::course_feature,
                    sim::TerrainRegion::firm,
                    sim::CourseFeatureKind::overhead_bar,
                    sim::WaterTraversalPhase::dry, false, true };
            return {};
        case stop_and_plant:
            if (challenge == director::ChallengeKind::aim_range)
                return { task, challenge, sim::CourseStage::equipment_targets,
                    sim::GaitTask::walk,
                    sim::EquipmentDirective::stop_and_plant,
                    ChallengeAuthority::equipment_target,
                    sim::TerrainRegion::firm, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, true, true };
            return {};
        case gun_stance:
            if (challenge == director::ChallengeKind::aim_range)
                return { task, challenge, sim::CourseStage::equipment_targets,
                    sim::GaitTask::walk, sim::EquipmentDirective::gun_stance,
                    ChallengeAuthority::equipment_target,
                    sim::TerrainRegion::firm, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, true, true };
            return {};
        case acquire_and_aim:
            if (challenge == director::ChallengeKind::aim_range)
                return { task, challenge, sim::CourseStage::equipment_targets,
                    sim::GaitTask::walk,
                    sim::EquipmentDirective::acquire_and_aim,
                    ChallengeAuthority::equipment_target,
                    sim::TerrainRegion::firm, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, true, true };
            return {};
        case fire_and_correct:
            if (challenge == director::ChallengeKind::aim_range)
                return { task, challenge, sim::CourseStage::equipment_targets,
                    sim::GaitTask::walk,
                    sim::EquipmentDirective::fire_and_correct,
                    ChallengeAuthority::equipment_target,
                    sim::TerrainRegion::firm, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, true, true };
            return {};
        case break_contact:
            if (challenge == director::ChallengeKind::aim_range)
                return { task, challenge, sim::CourseStage::combat_course,
                    sim::GaitTask::walk,
                    sim::EquipmentDirective::break_contact,
                    ChallengeAuthority::equipment_target,
                    sim::TerrainRegion::firm, sim::CourseFeatureKind::rock,
                    sim::WaterTraversalPhase::dry, true, true };
            return {};
        case recover:
        case construction:
        case tool_traversal:
            return {};
        }
        return {};
    }

    [[nodiscard]] constexpr bool task_node_has_physical_binding(
        const director::TaskNode& node) noexcept
    {
        return node.enabled && director::task_supported(node.task)
            && desired_task_state(node.task, node.challenge).valid;
    }

    [[nodiscard]] inline director::TaskGraph compatible_task_graph(
        const sim::Environment& environment,
        const director::TaskGraph& source) noexcept
    {
        if (!source.valid())
            return {};
        director::TaskGraph compatible = source;
        for (std::size_t index = 0; index < compatible.count; ++index)
        {
            director::TaskNode& node = compatible.nodes[index];
            const DesiredTaskState desired = desired_task_state(
                node.task, node.challenge);
            if (!task_node_has_physical_binding(node) || !desired.valid
                || (desired.equipment_task && !environment.equipment_capable()))
                node.enabled = false;
        }
        // A task whose prerequisite is inapplicable to this body is also
        // inapplicable. Repeat to a fixed point so unsupported Human-only
        // equipment chains cannot trap Dog/Chicken/Hexapod Directors.
        for (std::size_t pass = 0; pass < compatible.count; ++pass)
        {
            bool changed{};
            for (std::size_t index = 0; index < compatible.count; ++index)
            {
                director::TaskNode& node = compatible.nodes[index];
                if (!node.enabled)
                    continue;
                for (std::size_t prerequisite = 0;
                    prerequisite < node.prerequisite_count; ++prerequisite)
                {
                    if (!compatible.nodes[node.prerequisites[prerequisite]].enabled)
                    {
                        node.enabled = false;
                        changed = true;
                        break;
                    }
                }
            }
            if (!changed)
                break;
        }
        return compatible;
    }

    [[nodiscard]] inline std::optional<director::Evidence>
    evidence_for_compatible_graph(const director::TaskGraph& source,
        const director::TaskGraph& compatible,
        const director::Evidence& evidence) noexcept
    {
        if (!source.valid() || !compatible.valid()
            || source.count != compatible.count
            || !director::evidence_valid(source, evidence))
            return std::nullopt;
        for (std::size_t index = 0; index < source.count; ++index)
        {
            if (source.nodes[index].stable_id != compatible.nodes[index].stable_id
                || source.nodes[index].stable_challenge_id
                    != compatible.nodes[index].stable_challenge_id
                || source.nodes[index].task != compatible.nodes[index].task
                || source.nodes[index].challenge
                    != compatible.nodes[index].challenge
                || source.nodes[index].prerequisites
                    != compatible.nodes[index].prerequisites
                || source.nodes[index].prerequisite_count
                    != compatible.nodes[index].prerequisite_count
                || source.nodes[index].minimum_evidence
                    != compatible.nodes[index].minimum_evidence
                || source.nodes[index].difficulty
                    != compatible.nodes[index].difficulty
                || (compatible.nodes[index].enabled
                    && !source.nodes[index].enabled))
                return std::nullopt;
        }
        director::Evidence result = evidence;
        if (result.first_stall < compatible.count
            && !compatible.nodes[result.first_stall].enabled)
            result.first_stall = director::invalid_task_index;
        if (result.requested_goal < compatible.count
            && !compatible.nodes[result.requested_goal].enabled)
            result.requested_goal = director::invalid_task_index;
        if (!director::evidence_valid(compatible, result))
            return std::nullopt;
        return result;
    }

    [[nodiscard]] inline director::Decision select_for_environment(
        const sim::Environment& environment,
        const director::TaskGraph& source, const director::Evidence& evidence,
        director::Profile profile) noexcept
    {
        const director::TaskGraph compatible = compatible_task_graph(
            environment, source);
        const std::optional<director::Evidence> compatible_evidence =
            evidence_for_compatible_graph(source, compatible, evidence);
        return compatible_evidence.has_value()
            ? director::select(compatible, *compatible_evidence, profile)
            : director::Decision{};
    }

    [[nodiscard]] constexpr bool task_command_valid(
        const director::TaskGraph& graph, const director::Evidence& evidence,
        const TaskCommand& command, director::Profile profile) noexcept
    {
        if (!graph.valid() || !command.selected() || command.profile != profile
            || command.task_index >= graph.count)
            return false;
        const director::TaskNode& node = graph.nodes[command.task_index];
        if (node.stable_id != command.stable_id || node.task != command.task
            || node.challenge != command.challenge
            || !task_node_has_physical_binding(node)
            || !director::prerequisites_met(graph, evidence, command.task_index))
            return false;
        const director::Decision selected = director::select(graph, evidence, profile);
        return selected.selected() && selected.task_index == command.task_index
            && selected.stable_id == command.stable_id;
    }

    [[nodiscard]] constexpr TaskCommand task_command(
        const director::TaskGraph& graph, const director::Evidence& evidence,
        const director::Decision& decision, director::Profile profile) noexcept
    {
        if (!graph.valid() || !decision.selected()
            || decision.task_index >= graph.count)
            return {};
        const director::TaskNode& node = graph.nodes[decision.task_index];
        const TaskCommand command{ node.stable_id, decision.task_index,
            node.task, node.challenge, profile };
        return task_command_valid(graph, evidence, command, profile)
            ? command : TaskCommand{};
    }

    struct ChallengeResolution
    {
        std::uint32_t stable_id{};
        ChallengeAuthority authority{ ChallengeAuthority::none };
        director::ChallengeKind challenge{ director::ChallengeKind::flat };
        sim::TerrainRegion terrain_region{ sim::TerrainRegion::firm };
        sim::CourseFeatureKind feature{ sim::CourseFeatureKind::rock };
        sim::WaterTraversalPhase expected_water_phase{
            sim::WaterTraversalPhase::dry };
        float course_x{};
        bool present{};
    };

    [[nodiscard]] constexpr std::uint32_t physical_challenge_stable_id(
        director::ChallengeKind challenge) noexcept
    {
        return 0xE2D10000u
            + static_cast<std::uint32_t>(challenge) + 1u;
    }

    [[nodiscard]] inline ChallengeResolution resolve_challenge(
        const sim::Environment& environment,
        const DesiredTaskState& desired) noexcept
    {
        ChallengeResolution resolution{};
        resolution.authority = desired.authority;
        resolution.challenge = desired.challenge;
        resolution.terrain_region = desired.terrain_region;
        resolution.feature = desired.feature;
        resolution.expected_water_phase = desired.water_phase;
        if (!desired.valid)
            return resolution;
        if (desired.authority == ChallengeAuthority::terrain_interval
            || desired.authority == ChallengeAuthority::terrain_boundary)
        {
            const auto challenge = environment.terrain().challenge_for(
                desired.challenge);
            const bool material_region_required =
                desired.challenge == director::ChallengeKind::flat
                || desired.challenge == director::ChallengeKind::loose_cells
                || desired.challenge
                    == director::ChallengeKind::waterlogged_cells
                || desired.challenge == director::ChallengeKind::shallow_water;
            if (!challenge || challenge->kind != desired.challenge
                || (material_region_required
                    && challenge->region != desired.terrain_region))
                return resolution;
            resolution.stable_id = challenge->stable_id;
            resolution.terrain_region = challenge->region;
            resolution.course_x = challenge->center();
            resolution.present = true;
            return resolution;
        }
        if (desired.authority == ChallengeAuthority::course_feature)
        {
            for (const sim::CourseFeature& feature : environment.course_features())
            {
                if (feature.kind != desired.feature)
                    continue;
                resolution.stable_id = physical_challenge_stable_id(
                    desired.challenge);
                resolution.course_x = feature.center.x;
                resolution.present = true;
                return resolution;
            }
            return resolution;
        }
        if (desired.authority == ChallengeAuthority::equipment_target)
        {
            const sim::EquipmentTarget& target = environment.equipment_target();
            if (!target.active)
                return resolution;
            resolution.stable_id = physical_challenge_stable_id(
                desired.challenge);
            resolution.course_x = target.position.x;
            resolution.present = true;
        }
        return resolution;
    }

    struct TaskApplication
    {
        std::uint32_t stable_task_id{};
        std::uint32_t stable_challenge_id{};
        ChallengeAuthority challenge_authority{ ChallengeAuthority::none };
        float challenge_course_x{};
        bool challenge_present{};
        bool selected{};
        bool applied{};
        bool rejected{};
    };

    [[nodiscard]] inline TaskApplication apply_task_uncommitted(
        sim::Environment& environment, const TaskCommand& command,
        const director::TaskGraph& graph, const director::Evidence& evidence,
        director::Profile profile)
    {
        TaskApplication result{};
        result.stable_task_id = command.stable_id;
        result.selected = command.selected();
        if (!result.selected)
            return result;
        if (!task_command_valid(graph, evidence, command, profile))
        {
            result.rejected = true;
            return result;
        }
        const director::TaskNode& node = graph.nodes[command.task_index];
        const DesiredTaskState desired = desired_task_state(
            node.task, node.challenge);
        if (!desired.valid
            || (desired.equipment_task && !environment.equipment_capable()))
        {
            result.rejected = true;
            return result;
        }
        const bool stage_changed = environment.course_stage() != desired.stage;
        const bool difficulty_changed =
            environment.course_difficulty() != node.difficulty;
        const bool gait_changed = environment.gait_task() != desired.gait;
        const bool directive_changed =
            environment.equipment_directive() != desired.equipment;
        if (stage_changed || difficulty_changed)
            environment.set_course(desired.stage, node.difficulty);
        if (gait_changed)
            environment.set_gait_task(desired.gait);
        const bool equipment_changed = desired.equipment_task
            && environment.weapon_class() != sim::WeaponClass::carbine;
        if (equipment_changed)
            environment.configure_equipment(sim::WeaponClass::carbine, 8.0f);
        if (directive_changed)
            environment.set_equipment_directive(desired.equipment);
        const ChallengeResolution resolved = resolve_challenge(
            environment, desired);
        result.stable_challenge_id = resolved.stable_id;
        result.challenge_authority = resolved.authority;
        result.challenge_course_x = resolved.course_x;
        result.challenge_present = resolved.present;
        if (!resolved.present)
        {
            result.rejected = true;
            return result;
        }
        result.applied = stage_changed || difficulty_changed || gait_changed
            || equipment_changed || directive_changed;
        return result;
    }

    [[nodiscard]] inline bool task_requires_staging(
        const sim::Environment& environment, const TaskCommand& command,
        const director::TaskGraph& graph) noexcept
    {
        if (!command.selected())
            return false;
        if (command.task_index >= graph.count)
            return true;
        const director::TaskNode& node = graph.nodes[command.task_index];
        const DesiredTaskState desired = desired_task_state(
            node.task, node.challenge);
        return !desired.valid
            || environment.course_stage() != desired.stage
            || environment.course_difficulty() != node.difficulty
            || environment.gait_task() != desired.gait
            || environment.equipment_directive() != desired.equipment
            || (desired.equipment_task
                && environment.weapon_class() != sim::WeaponClass::carbine);
    }

    [[nodiscard]] inline TaskApplication apply_task(
        sim::Environment& environment, const TaskCommand& command,
        const director::TaskGraph& graph, const director::Evidence& evidence,
        director::Profile profile)
    {
        if (!command.selected())
            return {};
        if (!task_requires_staging(environment, command, graph))
            return apply_task_uncommitted(environment, command, graph,
                evidence, profile);
        // Resolve the complete requested state on a candidate first. A graph
        // command is not applied merely because its metadata is authentic: the
        // referenced physical terrain, feature, or target must exist as well.
        sim::Environment candidate = environment;
        const TaskApplication result = apply_task_uncommitted(candidate,
            command, graph, evidence, profile);
        if (!result.rejected)
            environment = std::move(candidate);
        return result;
    }

    inline constexpr std::uint32_t retained_policy_provenance_schema_version = 3u;

    struct RetainedPolicyProvenance
    {
        std::uint32_t schema{ retained_policy_provenance_schema_version };
        std::uint32_t integration_api{ api_version };
        std::uint32_t action_layout_schema{ action_layout_schema_version };
        std::uint32_t training_semantics{};
        std::uint32_t policy_input_count{ rl::PolicyNetwork::input_size };
        std::uint32_t policy_output_count{ rl::PolicyNetwork::output_size };
        std::uint64_t rig_signature{};
        std::uint64_t optimizer_step{};
        std::uint64_t lesson_update{};
        std::uint64_t best_update{};
        std::uint64_t best_quality_key{};
        std::uint64_t retained_parameters_digest{};
        std::uint64_t raw_receipt_digest{};
        std::uint64_t raw_receipt_handoff_update{};
        std::uint64_t raw_receipt_optimizer_update{};
        std::uint64_t raw_receipt_sample_count{};
        float best_evaluation_score{};
        float best_evaluation_distance{};
        sim::GuidanceMode retained_guidance{ sim::GuidanceMode::assisted };
        bool retained_best_parameters{};
        bool quality_evidence{};
        bool checkpoint_verified{};
        bool raw_receipt_verified{};
        bool valid{};
    };

    class RetainedPolicyController
    {
    public:
        [[nodiscard]] bool load_retained_best(
            const rl::PpoTrainer::VerifiedCheckpoint& artifact,
            std::string& error)
        {
            if (!artifact.valid())
            {
                error = "Retained policy requires an issued verified checkpoint artifact.";
                return false;
            }
            const rl::PpoTrainer::CheckpointData& checkpoint = artifact.data();
            rl::PolicyNetwork candidate{};
            const std::uint64_t retained_digest = rl::parameter_digest(
                checkpoint.best_parameters);
            if (checkpoint.training_semantics
                    != rl::training_semantics_version
                || checkpoint.rig_signature == 0u
                || static_cast<unsigned>(checkpoint.best_guidance_mode)
                    > static_cast<unsigned>(sim::GuidanceMode::raw_policy_audit)
                || checkpoint.best_parameters.size()
                    != candidate.parameter_count()
                || std::ranges::any_of(checkpoint.best_parameters,
                    [](float value) noexcept
                    {
                        return !std::isfinite(value)
                            || std::abs(value) > 1.0e6f;
                    })
                || checkpoint.best_parameters_digest != retained_digest)
            {
                error = "Retained policy provenance or parameters are invalid.";
                return false;
            }
            const bool raw_guidance = checkpoint.best_guidance_mode
                == sim::GuidanceMode::raw_policy_audit;
            const rl::ActionMask retained_mask = [&checkpoint]() noexcept
            {
                rl::ActionMask mask{};
                for (std::size_t index = 0; index < mask.size(); ++index)
                    mask[index] = (checkpoint.action_mask_bits
                        & (std::uint32_t{ 1u } << index)) != 0u;
                return mask;
            }();
            const rl::RawPpoReceiptContext receipt_context{
                .training_semantics = checkpoint.training_semantics,
                .rig_signature = checkpoint.rig_signature,
                .best_update = checkpoint.metrics.best_update,
                .checkpoint_optimizer_step = checkpoint.optimizer_step,
                .retained_parameters_digest = retained_digest,
                .retained_active_actor_digest =
                    rl::PolicyNetwork::active_actor_parameter_digest(
                        checkpoint.best_parameters, retained_mask),
                .action_mask_bits = checkpoint.action_mask_bits,
                .stage = static_cast<std::uint8_t>(checkpoint.stage),
                .gait_task = static_cast<std::uint8_t>(checkpoint.gait_task),
                .equipment_directive = static_cast<std::uint8_t>(
                    checkpoint.equipment_directive)
            };
            if (raw_guidance
                && !rl::raw_ppo_receipt_valid(
                    checkpoint.raw_ppo_receipt, receipt_context))
            {
                error = "Raw retained policy has no verified PPO receipt.";
                return false;
            }
            candidate.parameters() = checkpoint.best_parameters;
            policy_ = std::move(candidate);
            const bool finite_quality =
                std::isfinite(checkpoint.metrics.best_evaluation_score)
                && std::isfinite(checkpoint.metrics.best_evaluation_distance);
            RetainedPolicyProvenance provenance{};
            provenance.training_semantics = checkpoint.training_semantics;
            provenance.rig_signature = checkpoint.rig_signature;
            provenance.optimizer_step = checkpoint.optimizer_step;
            provenance.lesson_update = checkpoint.lesson_update;
            provenance.best_update = checkpoint.metrics.best_update;
            provenance.best_quality_key = checkpoint.metrics.best_quality_key;
            provenance.retained_parameters_digest = retained_digest;
            provenance.raw_receipt_digest = raw_guidance
                ? checkpoint.raw_ppo_receipt.provenance_digest() : 0u;
            provenance.raw_receipt_optimizer_update = raw_guidance
                ? checkpoint.raw_ppo_receipt.optimizer_update() : 0u;
            provenance.raw_receipt_handoff_update = raw_guidance
                ? checkpoint.raw_ppo_receipt.handoff_update() : 0u;
            provenance.raw_receipt_sample_count = raw_guidance
                ? checkpoint.raw_ppo_receipt.expected_sample_count() : 0u;
            provenance.best_evaluation_score = finite_quality
                ? checkpoint.metrics.best_evaluation_score : 0.0f;
            provenance.best_evaluation_distance = finite_quality
                ? checkpoint.metrics.best_evaluation_distance : 0.0f;
            provenance.retained_guidance = checkpoint.best_guidance_mode;
            provenance.retained_best_parameters = true;
            provenance.quality_evidence = finite_quality
                && checkpoint.metrics.best_update > 0u
                && checkpoint.metrics.best_quality_key != 0u;
            provenance.checkpoint_verified = true;
            provenance.raw_receipt_verified = raw_guidance;
            provenance.valid = provenance.quality_evidence;
            provenance_ = provenance;
            raw_ppo_receipt_ = raw_guidance
                ? checkpoint.raw_ppo_receipt : rl::RawPpoReceipt{};
            error.clear();
            return true;
        }

        [[nodiscard]] const RetainedPolicyProvenance& provenance() const noexcept
        {
            return provenance_;
        }

        [[nodiscard]] bool valid_for(const sim::Environment& environment,
            sim::GuidanceMode guidance, sim::CourseStage stage,
            sim::GaitTask gait_task,
            sim::EquipmentDirective equipment_directive) const noexcept
        {
            return provenance_.valid
                && provenance_.schema
                    == retained_policy_provenance_schema_version
                && provenance_.integration_api == api_version
                && provenance_.action_layout_schema
                    == action_layout_schema_version
                && provenance_.policy_input_count == rl::PolicyNetwork::input_size
                && provenance_.policy_output_count == rl::PolicyNetwork::output_size
                && provenance_.retained_best_parameters
                && provenance_.checkpoint_verified
                && provenance_.training_semantics == rl::training_semantics_version
                && provenance_.rig_signature == environment.blueprint().signature()
                && provenance_.retained_guidance == guidance
                && provenance_.retained_parameters_digest
                    == rl::parameter_digest(policy_.parameters())
                && (guidance != sim::GuidanceMode::raw_policy_audit
                    || (provenance_.raw_receipt_verified
                        && provenance_.raw_receipt_digest
                            == raw_ppo_receipt_.provenance_digest()
                        && provenance_.raw_receipt_optimizer_update
                            == raw_ppo_receipt_.optimizer_update()
                        && provenance_.raw_receipt_handoff_update
                            == raw_ppo_receipt_.handoff_update()
                        && provenance_.raw_receipt_sample_count
                            == raw_ppo_receipt_.expected_sample_count()
                        && raw_ppo_receipt_.present()
                        && raw_ppo_receipt_.stage()
                            == static_cast<std::uint8_t>(stage)
                        && raw_ppo_receipt_.gait_task()
                            == static_cast<std::uint8_t>(gait_task)
                        && raw_ppo_receipt_.equipment_directive()
                            == static_cast<std::uint8_t>(equipment_directive)
                        && raw_ppo_receipt_.action_mask_bits()
                            == rl::action_mask_bits(rl::action_mask_for(
                                environment, stage))));
        }

        [[nodiscard]] bool valid_for(const sim::Environment& environment,
            sim::GuidanceMode guidance) const noexcept
        {
            return valid_for(environment, guidance, environment.course_stage(),
                environment.gait_task(), environment.equipment_directive());
        }

        [[nodiscard]] std::array<float, sim::action_count> action(
            const sim::Environment& environment) const noexcept
        {
            const auto observation = rl::foundational_policy_observation(
                environment, environment.course_stage(),
                environment.gait_task(), provenance_.retained_guidance);
            return policy_.deterministic_action(observation,
                rl::action_mask_for(environment, environment.course_stage()));
        }

    private:
        rl::PolicyNetwork policy_{};
        RetainedPolicyProvenance provenance_{};
        rl::RawPpoReceipt raw_ppo_receipt_{};
    };

    inline constexpr std::uint32_t player_intent_schema_version = 1u;

    struct PlayerIntent
    {
        std::uint32_t schema{ player_intent_schema_version };
        std::uint32_t stable_task_id{};
        std::uint16_t task_index{ director::invalid_task_index };
        director::TaskKind task{ director::TaskKind::stand };
        // -1 is reverse, 0 is planted/stopped, and +1 is forward. This changes
        // planner/observation intent only; the retained joint policy must still
        // generate every physical displacement through contacts and motors.
        float travel_direction{};
        bool stop{};
        bool interact{};
        bool use_tool{};
        bool fire{};

        [[nodiscard]] bool valid() const noexcept
        {
            return schema == player_intent_schema_version
                && stable_task_id != 0u
                && task_index != director::invalid_task_index
                && static_cast<unsigned>(task)
                    <= static_cast<unsigned>(director::TaskKind::tool_traversal)
                && std::isfinite(travel_direction)
                && travel_direction >= -1.0f && travel_direction <= 1.0f;
        }
    };

    struct ControlRequest
    {
        // Used only when joint_control is explicitly expert_joint_actions.
        // Player-guided control selects a Director task as high-level intent;
        // a RetainedPolicyController remains the sole joint-action authority.
        std::array<float, sim::action_count> actions{};
        director::TaskGraph task_graph{ director::default_training_graph() };
        director::Evidence evidence{};
        TaskCommand selected_task{};
        PlayerIntent player_intent{};
        ControlMode mode{ ControlMode::autonomous };
        JointControlSource joint_control{ JointControlSource::unspecified };
        GuidanceRequest guidance{ GuidanceRequest::unspecified };
        bool retry_trial{};
    };

    struct ControlAuthorityReport
    {
        JointControlSource joint_control{ JointControlSource::unspecified };
        GuidanceRequest requested_guidance{ GuidanceRequest::unspecified };
        sim::GuidanceMode applied_guidance{ sim::GuidanceMode::assisted };
        RetainedPolicyProvenance retained_policy{};
        float lesson_teacher{};
        float topology_support{};
        float topology_body{};
        float safety_support{};
        float safety_body{};
        float posture_guide{};
        float swing_clearance{};
        float physical_contact{};
        float mandatory_joint_cluster{};
        bool optional_guidance_active{};
        bool request_accepted{};
    };

    enum class ActionRole : std::uint8_t
    {
        anatomy_motor,
        equipment_state,
        equipment_aim,
        equipment_trigger
    };

    [[nodiscard]] inline constexpr std::array<ActionRole, sim::action_count>
    canonical_action_roles() noexcept
    {
        std::array<ActionRole, sim::action_count> result{};
        for (std::size_t index = 0; index < sim::anatomy_action_count; ++index)
            result[index] = ActionRole::anatomy_motor;
        result[sim::equipment_state_action] = ActionRole::equipment_state;
        result[sim::equipment_aim_action] = ActionRole::equipment_aim;
        result[sim::equipment_trigger_action] = ActionRole::equipment_trigger;
        return result;
    }

    struct ActionLayout
    {
        std::uint32_t schema{ action_layout_schema_version };
        std::uint32_t observation_count{ sim::observation_count };
        std::uint32_t action_count{ sim::action_count };
        std::uint32_t anatomy_action_count{ sim::anatomy_action_count };
        std::uint32_t equipment_action_count{ sim::equipment_action_count };
        std::array<ActionRole, sim::action_count> roles =
            canonical_action_roles();
        rl::ActionMask active{};
    };

    [[nodiscard]] inline ActionLayout action_layout_for(
        const sim::Environment& environment) noexcept
    {
        ActionLayout result{};
        result.active = rl::action_mask_for(environment,
            environment.course_stage());
        return result;
    }

    [[nodiscard]] inline constexpr bool valid_action_layout(
        const ActionLayout& value) noexcept
    {
        return value.schema == action_layout_schema_version
            && value.observation_count == sim::observation_count
            && value.action_count == sim::action_count
            && value.anatomy_action_count == sim::anatomy_action_count
            && value.equipment_action_count == sim::equipment_action_count
            && value.roles == canonical_action_roles();
    }

    inline constexpr std::uint16_t invalid_snapshot_node_index =
        std::numeric_limits<std::uint16_t>::max();

    struct PoseParticleSnapshot
    {
        Vec2 position{};
        Vec2 velocity{};
        float inverse_mass{};
        float radius{};
        bool grounded{};
    };

    struct MotorSnapshot
    {
        std::uint16_t action_slot{ invalid_snapshot_node_index };
        std::uint16_t reference_node{ invalid_snapshot_node_index };
        std::uint16_t pivot_node{ invalid_snapshot_node_index };
        std::uint16_t driven_node{ invalid_snapshot_node_index };
        float current_angle{};
        float angular_velocity{};
        float applied_action{};
        float target_angle{};
        bool available{};
        bool enabled{};
        bool support_branch{};
    };

    struct CanonicalCellSample
    {
        sandhybrid::SceneCell cell{};
        std::uint32_t global_x{};
        std::uint32_t global_y{};
        bool valid{};
    };

    inline constexpr std::uint32_t canonical_snapshot_schema_version = 5u;
    inline constexpr std::size_t maximum_snapshot_particles = 128u;

    struct CanonicalSnapshot
    {
        std::uint32_t schema{ canonical_snapshot_schema_version };
        std::uint32_t integration_api{ api_version };
        std::uint32_t training_semantics{ rl::training_semantics_version };
        std::uint64_t rig_signature{};
        std::uint64_t trial_id{};
        std::uint64_t fixed_tick{};
        sim::CreatureSpecies species{ sim::CreatureSpecies::human };
        sim::CourseStage course_stage{ sim::CourseStage::balance };
        sim::GaitTask gait_task{ sim::GaitTask::walk };
        float locomotion_intent_direction{ 1.0f };
        bool locomotion_stop_requested{};
        ControlAuthorityReport control_authority{};
        ActionLayout action_layout{};
        std::array<float, sim::action_count> requested_actions{};
        std::array<float, sim::anatomy_action_count> applied_motor_actions{};
        std::uint16_t root_node_index{ invalid_snapshot_node_index };
        std::uint16_t torso_node_index{ invalid_snapshot_node_index };
        std::uint16_t left_contact_node_index{ invalid_snapshot_node_index };
        std::uint16_t right_contact_node_index{ invalid_snapshot_node_index };
        PoseParticleSnapshot root_pose{};
        PoseParticleSnapshot torso_pose{};
        PoseParticleSnapshot left_contact_pose{};
        PoseParticleSnapshot right_contact_pose{};
        Vec2 root_position{};
        Vec2 root_velocity{};
        Vec2 center_of_mass_position{};
        Vec2 center_of_mass_velocity{};
        float total_particle_mass{};
        std::array<PoseParticleSnapshot, maximum_snapshot_particles> pose{};
        std::size_t pose_count{};
        std::array<MotorSnapshot, sim::anatomy_action_count> motors{};
        std::size_t motor_count{};
        std::size_t bone_count{};
        CanonicalCellSample root_cell{};
        CanonicalCellSample left_contact_cell{};
        CanonicalCellSample right_contact_cell{};
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
        std::uint32_t rounds_remaining{};
        std::uint32_t magazine_capacity{};
        float weapon_plant_seconds{};
        float reload_remaining{};
        float zero_progress_seconds{};
        float micro_motion_seconds{};
        float hazard_stall_seconds{};
        sim::EquipmentFireBlock fire_block{ sim::EquipmentFireBlock::unarmed };
        bool target_visible{};
        director::ChallengeKind challenge_kind{ director::ChallengeKind::flat };
        sim::TerrainRegion terrain_region{ sim::TerrainRegion::firm };
        sim::WaterTraversalPhase water_phase{ sim::WaterTraversalPhase::dry };
        sim::WeaponClass weapon{ sim::WeaponClass::none };
        sim::EquipmentState equipment{ sim::EquipmentState::unarmed };
        sim::EquipmentDirective equipment_directive{ sim::EquipmentDirective::passive };
        bool equipment_capable{};
        sim::FootContactPhase left_contact_phase{ sim::FootContactPhase::airborne };
        sim::FootContactPhase right_contact_phase{ sim::FootContactPhase::airborne };
        sim::RawAuthorityTelemetry raw_authority{};
        bool raw_authority_telemetry_enabled{};
        bool left_supported{};
        bool right_supported{};
        bool valid{};
        bool motion_valid{};
    };

    using RuntimeSnapshot = CanonicalSnapshot;

    struct FixedStepResult
    {
        CanonicalSnapshot snapshot{};
        ControlAuthorityReport authority{};
        std::array<float, sim::action_count> requested_actions{};
        std::array<float, sim::anatomy_action_count> applied_motor_actions{};
        float reward{};
        bool terminal{};
        sim::InvalidMotion invalid_reason{ sim::InvalidMotion::none };
        sim::TrialTerminalCause terminal_cause{ sim::TrialTerminalCause::none };
        std::uint32_t stable_task_id{};
        std::uint32_t selected_challenge_id{};
        ChallengeAuthority selected_challenge_authority{
            ChallengeAuthority::none };
        float selected_challenge_course_x{};
        bool selected_challenge_present{};
        bool task_applied{};
        bool task_rejected{};
        bool retry_applied{};
        bool control_applied{};
        ControlRejection control_rejection{ ControlRejection::none };
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

    [[nodiscard]] inline CanonicalCellSample canonical_cell_sample(
        const sim::Environment& environment, Vec2 world_position) noexcept
    {
        CanonicalCellSample sample{};
        if (!std::isfinite(world_position.x) || !std::isfinite(world_position.y))
            return sample;
        using Terrain = sim::DeformableTerrain;
        const float course_x = sim::terrain_sample_x(world_position.x,
            environment.course_progress());
        float wrapped = std::fmod(course_x, Terrain::period);
        if (wrapped < 0.0f)
            wrapped += Terrain::period;
        const auto signed_column = static_cast<std::ptrdiff_t>(
            std::floor(wrapped / Terrain::fine_cell_spacing));
        const std::size_t column = Terrain::wrap_column(signed_column);
        const float row_value = std::floor((world_position.y - Terrain::world_bottom)
            / Terrain::fine_cell_spacing);
        const std::size_t row = static_cast<std::size_t>(std::clamp(row_value,
            0.0f, static_cast<float>(Terrain::vertical_cell_count - 1u)));
        sample.cell = environment.terrain().fine_cell(column, row).canonical_cell();
        sample.global_x = Terrain::global_cell_x(course_x);
        sample.global_y = Terrain::global_cell_y(world_position.y);
        sample.valid = true;
        return sample;
    }

    [[nodiscard]] inline bool finite_vector(Vec2 value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    [[nodiscard]] constexpr bool valid_canonical_cell_sample(
        const CanonicalCellSample& value) noexcept
    {
        return value.valid && value.cell.material < sandhybrid::material_count;
    }

    [[nodiscard]] inline bool valid_pose_particle(
        const PoseParticleSnapshot& value) noexcept
    {
        return finite_vector(value.position) && finite_vector(value.velocity)
            && std::isfinite(value.inverse_mass) && value.inverse_mass >= 0.0f
            && std::isfinite(value.radius) && value.radius >= 0.0f;
    }

    [[nodiscard]] inline bool empty_pose_particle(
        const PoseParticleSnapshot& value) noexcept
    {
        return value.position.x == 0.0f && value.position.y == 0.0f
            && value.velocity.x == 0.0f && value.velocity.y == 0.0f
            && value.inverse_mass == 0.0f && value.radius == 0.0f
            && !value.grounded;
    }

    [[nodiscard]] inline bool same_pose_particle(
        const PoseParticleSnapshot& left,
        const PoseParticleSnapshot& right) noexcept
    {
        return left.position.x == right.position.x
            && left.position.y == right.position.y
            && left.velocity.x == right.velocity.x
            && left.velocity.y == right.velocity.y
            && left.inverse_mass == right.inverse_mass
            && left.radius == right.radius && left.grounded == right.grounded;
    }

    [[nodiscard]] inline bool valid_motor_snapshot(
        const MotorSnapshot& value, std::size_t expected_action_slot,
        std::size_t pose_count) noexcept
    {
        return value.action_slot == expected_action_slot
            && value.reference_node < pose_count
            && value.pivot_node < pose_count
            && value.driven_node < pose_count
            && value.reference_node != value.pivot_node
            && value.pivot_node != value.driven_node
            && value.reference_node != value.driven_node
            && value.available
            && std::isfinite(value.current_angle)
            && std::isfinite(value.angular_velocity)
            && std::isfinite(value.applied_action)
            && value.applied_action >= -1.0f
            && value.applied_action <= 1.0f
            && std::isfinite(value.target_angle);
    }

    [[nodiscard]] inline bool empty_motor_snapshot(
        const MotorSnapshot& value) noexcept
    {
        return value.action_slot == invalid_snapshot_node_index
            && value.reference_node == invalid_snapshot_node_index
            && value.pivot_node == invalid_snapshot_node_index
            && value.driven_node == invalid_snapshot_node_index
            && value.current_angle == 0.0f && value.angular_velocity == 0.0f
            && value.applied_action == 0.0f && value.target_angle == 0.0f
            && !value.available && !value.enabled && !value.support_branch;
    }

    [[nodiscard]] inline bool valid_control_authority_report(
        const ControlAuthorityReport& value) noexcept
    {
        const RetainedPolicyProvenance& retained = value.retained_policy;
        const bool basic_valid = static_cast<unsigned>(value.joint_control)
                <= static_cast<unsigned>(JointControlSource::expert_joint_actions)
            && static_cast<unsigned>(value.requested_guidance)
                <= static_cast<unsigned>(GuidanceRequest::assisted)
            && static_cast<unsigned>(value.applied_guidance)
                <= static_cast<unsigned>(sim::GuidanceMode::raw_policy_audit)
            && static_cast<unsigned>(retained.retained_guidance)
                <= static_cast<unsigned>(sim::GuidanceMode::raw_policy_audit)
            && std::isfinite(retained.best_evaluation_score)
            && std::isfinite(retained.best_evaluation_distance)
            && std::isfinite(value.lesson_teacher)
            && std::isfinite(value.topology_support)
            && std::isfinite(value.topology_body)
            && std::isfinite(value.safety_support)
            && std::isfinite(value.safety_body)
            && std::isfinite(value.posture_guide)
            && std::isfinite(value.swing_clearance)
            && std::isfinite(value.physical_contact)
            && std::isfinite(value.mandatory_joint_cluster)
            && (!retained.valid
                || (retained.schema == retained_policy_provenance_schema_version
                    && retained.integration_api == api_version
                    && retained.action_layout_schema
                        == action_layout_schema_version
                    && retained.training_semantics
                        == rl::training_semantics_version
                    && retained.policy_input_count
                        == rl::PolicyNetwork::input_size
                    && retained.policy_output_count
                        == rl::PolicyNetwork::output_size
                    && retained.rig_signature != 0u
                    && retained.retained_best_parameters
                    && retained.checkpoint_verified
                    && retained.retained_parameters_digest != 0u
                    && (retained.retained_guidance
                            != sim::GuidanceMode::raw_policy_audit
                        || (retained.raw_receipt_verified
                            && retained.raw_receipt_digest != 0u
                            && retained.raw_receipt_optimizer_update
                                > retained.raw_receipt_handoff_update
                            && retained.raw_receipt_sample_count > 0u))));
        if (!basic_valid || !value.request_accepted)
            return basic_valid;
        if (value.joint_control == JointControlSource::unspecified
            || value.requested_guidance == GuidanceRequest::unspecified)
            return false;
        const sim::GuidanceMode expected_guidance =
            value.requested_guidance == GuidanceRequest::raw_policy
            ? sim::GuidanceMode::raw_policy_audit : sim::GuidanceMode::assisted;
        if (value.applied_guidance != expected_guidance)
            return false;
        if (value.joint_control == JointControlSource::retained_policy)
            return retained.valid
                && retained.retained_guidance == value.applied_guidance;
        return !retained.valid;
    }

    [[nodiscard]] inline bool empty_raw_authority_telemetry(
        const sim::RawAuthorityTelemetry& value) noexcept
    {
        for (const sim::AuthoritySourceTelemetry& source : value.sources)
        {
            if (source.center_velocity_delta_x != 0.0f
                || source.center_velocity_delta_y != 0.0f
                || source.maximum_absolute_center_velocity_delta_x != 0.0f
                || source.maximum_absolute_center_velocity_delta_y != 0.0f
                || source.positive_horizontal_specific_energy != 0.0f
                || source.center_position_delta_x != 0.0f
                || source.center_position_delta_y != 0.0f
                || source.center_history_delta_x != 0.0f
                || source.center_history_delta_y != 0.0f
                || source.root_position_delta_x != 0.0f
                || source.root_position_delta_y != 0.0f
                || source.torso_position_delta_x != 0.0f
                || source.torso_position_delta_y != 0.0f
                || source.applications != 0u)
                return false;
        }
        for (const float action : value.commanded_actions)
            if (action != 0.0f)
                return false;
        for (const float action : value.actuator_actions)
            if (action != 0.0f)
                return false;
        for (std::size_t side = 0; side < value.support_traction_capacity.size();
            ++side)
        {
            if (value.support_traction_capacity[side] != 0.0f
                || value.support_traction_used[side] != 0.0f
                || value.support_normal_impulse[side] != 0.0f
                || value.support_tangential_impulse[side] != 0.0f)
                return false;
        }
        return value.total_center_velocity_delta_x == 0.0f
            && value.total_center_velocity_delta_y == 0.0f
            && value.maximum_bone_error_ratio == 0.0f
            && value.maximum_bone_error_index
                == std::numeric_limits<std::size_t>::max()
            && value.support_contact_corrections == 0u
            && value.non_support_contact_corrections == 0u
            && value.friction_work_joules == 0.0f
            && value.friction_positive_work_joules == 0.0f
            && value.powered_toe_commands == 0u
            && value.action_support_release_requests == 0u
            && value.action_support_release_active_frames == 0u
            && value.root_drive_corrections == 0u
            && value.torso_drive_corrections == 0u
            && value.course_motion_corrections == 0u;
    }

    [[nodiscard]] inline bool valid_raw_authority_telemetry(
        const sim::RawAuthorityTelemetry& value, bool enabled,
        std::size_t bone_count) noexcept
    {
        if (!enabled)
            return empty_raw_authority_telemetry(value);
        bool finite = std::isfinite(value.total_center_velocity_delta_x)
            && std::isfinite(value.total_center_velocity_delta_y)
            && std::isfinite(value.maximum_bone_error_ratio)
            && value.maximum_bone_error_ratio >= 0.0f
            && std::isfinite(value.friction_work_joules)
            && std::isfinite(value.friction_positive_work_joules)
            && value.friction_positive_work_joules >= 0.0f;
        for (const sim::AuthoritySourceTelemetry& source : value.sources)
        {
            finite = finite && std::isfinite(source.center_velocity_delta_x)
                && std::isfinite(source.center_velocity_delta_y)
                && std::isfinite(
                    source.maximum_absolute_center_velocity_delta_x)
                && source.maximum_absolute_center_velocity_delta_x >= 0.0f
                && std::isfinite(
                    source.maximum_absolute_center_velocity_delta_y)
                && source.maximum_absolute_center_velocity_delta_y >= 0.0f
                && std::isfinite(source.positive_horizontal_specific_energy)
                && source.positive_horizontal_specific_energy >= 0.0f
                && std::isfinite(source.center_position_delta_x)
                && std::isfinite(source.center_position_delta_y)
                && std::isfinite(source.center_history_delta_x)
                && std::isfinite(source.center_history_delta_y)
                && std::isfinite(source.root_position_delta_x)
                && std::isfinite(source.root_position_delta_y)
                && std::isfinite(source.torso_position_delta_x)
                && std::isfinite(source.torso_position_delta_y);
        }
        for (const float action : value.commanded_actions)
            finite = finite && std::isfinite(action)
                && action >= -1.0f && action <= 1.0f;
        for (const float action : value.actuator_actions)
            finite = finite && std::isfinite(action)
                && action >= -1.0f && action <= 1.0f;
        for (std::size_t side = 0; side < value.support_traction_capacity.size();
            ++side)
        {
            const float capacity = value.support_traction_capacity[side];
            const float used = value.support_traction_used[side];
            const float capacity_tolerance = 1.0e-5f
                * std::max({ 1.0f, std::abs(capacity), std::abs(used) });
            finite = finite
                && std::isfinite(capacity) && capacity >= 0.0f
                && std::isfinite(used) && used >= 0.0f
                && used <= capacity + capacity_tolerance
                && std::isfinite(value.support_normal_impulse[side])
                && value.support_normal_impulse[side] >= 0.0f
                && std::isfinite(value.support_tangential_impulse[side]);
        }
        const float work_tolerance = 1.0e-5f * std::max({ 1.0f,
            std::abs(value.friction_work_joules),
            std::abs(value.friction_positive_work_joules) });
        finite = finite && value.friction_positive_work_joules
            + work_tolerance >= std::max(0.0f, value.friction_work_joules);
        const std::size_t no_bone = std::numeric_limits<std::size_t>::max();
        const bool bone_index_valid = value.maximum_bone_error_ratio == 0.0f
            ? value.maximum_bone_error_index == no_bone
            : value.maximum_bone_error_index < bone_count;
        return finite && bone_index_valid;
    }

    [[nodiscard]] inline bool canonical_snapshot_valid(
        const CanonicalSnapshot& value) noexcept
    {
        if (!value.valid || value.schema != canonical_snapshot_schema_version
            || value.integration_api != api_version
            || value.training_semantics != rl::training_semantics_version
            || !valid_action_layout(value.action_layout)
            || value.rig_signature == 0u || value.pose_count == 0u
            || value.pose_count > value.pose.size()
            || value.motor_count > value.motors.size()
            || value.bone_count == 0u
            || value.root_node_index >= value.pose_count
            || value.torso_node_index >= value.pose_count
            || value.left_contact_node_index >= value.pose_count
            || value.right_contact_node_index >= value.pose_count
            || static_cast<unsigned>(value.species)
                > static_cast<unsigned>(sim::CreatureSpecies::hexapod)
            || static_cast<unsigned>(value.course_stage)
                > static_cast<unsigned>(sim::CourseStage::shuttle)
            || static_cast<unsigned>(value.gait_task)
                > static_cast<unsigned>(sim::GaitTask::run)
            || static_cast<unsigned>(value.left_contact_phase)
                > static_cast<unsigned>(sim::FootContactPhase::toe_off)
            || static_cast<unsigned>(value.right_contact_phase)
                > static_cast<unsigned>(sim::FootContactPhase::toe_off)
            || static_cast<unsigned>(value.water_phase)
                > static_cast<unsigned>(sim::WaterTraversalPhase::shore_exit)
            || static_cast<unsigned>(value.terrain_region)
                > static_cast<unsigned>(sim::TerrainRegion::hole)
            || static_cast<unsigned>(value.challenge_kind)
                > static_cast<unsigned>(director::ChallengeKind::construction_anchor)
            || static_cast<unsigned>(value.weapon)
                > static_cast<unsigned>(sim::WeaponClass::launcher)
            || static_cast<unsigned>(value.equipment)
                > static_cast<unsigned>(sim::EquipmentState::reloading)
            || static_cast<unsigned>(value.equipment_directive)
                > static_cast<unsigned>(sim::EquipmentDirective::break_contact)
            || static_cast<unsigned>(value.fire_block)
                > static_cast<unsigned>(sim::EquipmentFireBlock::hazardous_footing)
            || !std::isfinite(value.locomotion_intent_direction)
            || value.locomotion_intent_direction < -1.0f
            || value.locomotion_intent_direction > 1.0f
            || (value.locomotion_stop_requested
                && std::abs(value.locomotion_intent_direction) > 1.0e-6f)
            || !finite_vector(value.root_position)
            || !finite_vector(value.root_velocity)
            || !finite_vector(value.center_of_mass_position)
            || !finite_vector(value.center_of_mass_velocity)
            || !std::isfinite(value.total_particle_mass)
            || value.total_particle_mass <= 0.0f
            || !valid_control_authority_report(value.control_authority)
            || !valid_raw_authority_telemetry(value.raw_authority,
                value.raw_authority_telemetry_enabled, value.bone_count)
            || !valid_canonical_cell_sample(value.root_cell)
            || !valid_canonical_cell_sample(value.left_contact_cell)
            || !valid_canonical_cell_sample(value.right_contact_cell))
            return false;

        for (std::size_t index = 0; index < value.requested_actions.size();
            ++index)
        {
            const float action = value.requested_actions[index];
            if (!std::isfinite(action) || action < -1.0f || action > 1.0f
                || (!value.action_layout.active[index] && action != 0.0f))
                return false;
        }
        for (std::size_t index = 0; index < value.applied_motor_actions.size();
            ++index)
        {
            const float action = value.applied_motor_actions[index];
            if (!std::isfinite(action) || action < -1.0f || action > 1.0f
                || (!value.action_layout.active[index] && action != 0.0f))
                return false;
        }
        for (std::size_t index = 0; index < value.pose_count; ++index)
            if (!valid_pose_particle(value.pose[index]))
                return false;
        for (std::size_t index = value.pose_count; index < value.pose.size(); ++index)
            if (!empty_pose_particle(value.pose[index]))
                return false;
        for (std::size_t index = 0; index < value.motor_count; ++index)
        {
            if (!valid_motor_snapshot(value.motors[index], index,
                    value.pose_count)
                || value.action_layout.active[index]
                    != value.motors[index].enabled
                || value.applied_motor_actions[index]
                    != value.motors[index].applied_action)
                return false;
        }
        for (std::size_t index = value.motor_count; index < value.motors.size();
            ++index)
            if (!empty_motor_snapshot(value.motors[index])
                || value.action_layout.active[index]
                || value.applied_motor_actions[index] != 0.0f)
                return false;

        if (value.raw_authority_telemetry_enabled)
        {
            for (std::size_t index = 0;
                index < value.applied_motor_actions.size(); ++index)
            {
                if (value.raw_authority.actuator_actions[index]
                    != value.applied_motor_actions[index])
                    return false;
                if (value.control_authority.request_accepted
                    && value.raw_authority.commanded_actions[index]
                        != value.requested_actions[index])
                    return false;
            }
        }

        const bool equipment_actions_active = value.equipment_capable
            && (value.course_stage == sim::CourseStage::equipment_targets
                || value.course_stage == sim::CourseStage::combat_course);
        for (std::size_t index = sim::anatomy_action_count;
            index < sim::action_count; ++index)
            if (value.action_layout.active[index] != equipment_actions_active)
                return false;

        if (!same_pose_particle(value.root_pose,
                value.pose[value.root_node_index])
            || !same_pose_particle(value.torso_pose,
                value.pose[value.torso_node_index])
            || !same_pose_particle(value.left_contact_pose,
                value.pose[value.left_contact_node_index])
            || !same_pose_particle(value.right_contact_pose,
                value.pose[value.right_contact_node_index])
            || value.root_position.x != value.root_pose.position.x
            || value.root_position.y != value.root_pose.position.y
            || value.root_velocity.x != value.root_pose.velocity.x
            || value.root_velocity.y != value.root_pose.velocity.y)
            return false;

        double dynamic_mass = 0.0;
        double weighted_position_x = 0.0;
        double weighted_position_y = 0.0;
        double weighted_velocity_x = 0.0;
        double weighted_velocity_y = 0.0;
        for (std::size_t index = 0; index < value.pose_count; ++index)
        {
            const PoseParticleSnapshot& pose = value.pose[index];
            const double mass = 1.0 / static_cast<double>(
                std::max(pose.inverse_mass, 1.0e-5f));
            if (!std::isfinite(mass))
                return false;
            dynamic_mass += mass;
            weighted_position_x += mass * static_cast<double>(pose.position.x);
            weighted_position_y += mass * static_cast<double>(pose.position.y);
            weighted_velocity_x += mass * static_cast<double>(pose.velocity.x);
            weighted_velocity_y += mass * static_cast<double>(pose.velocity.y);
        }
        if (!(dynamic_mass > 0.0) || !std::isfinite(dynamic_mass))
            return false;
        const auto close = [](float actual, double expected) noexcept
        {
            const double tolerance = 1.0e-4
                * std::max(1.0, std::abs(expected));
            return std::abs(static_cast<double>(actual) - expected) <= tolerance;
        };
        return close(value.total_particle_mass, dynamic_mass)
            && close(value.center_of_mass_position.x,
                weighted_position_x / dynamic_mass)
            && close(value.center_of_mass_position.y,
                weighted_position_y / dynamic_mass)
            && close(value.center_of_mass_velocity.x,
                weighted_velocity_x / dynamic_mass)
            && close(value.center_of_mass_velocity.y,
                weighted_velocity_y / dynamic_mass)
            && std::isfinite(value.uprightness)
            && std::isfinite(value.forward_speed)
            && std::isfinite(value.water_depth)
            && std::isfinite(value.water_submersion)
            && std::isfinite(value.stance_slip_speed)
            && std::isfinite(value.stance_slip_distance)
            && std::isfinite(value.available_traction)
            && std::isfinite(value.used_traction)
            && std::isfinite(value.challenge_progress)
            && std::isfinite(value.weapon_plant_seconds)
            && std::isfinite(value.reload_remaining)
            && std::isfinite(value.zero_progress_seconds)
            && std::isfinite(value.micro_motion_seconds)
            && std::isfinite(value.hazard_stall_seconds);
    }

    [[nodiscard]] inline CanonicalSnapshot snapshot(const sim::Environment& environment,
        std::uint64_t trial_id, std::uint64_t fixed_tick,
        float fixed_dt = 1.0f / 60.0f) noexcept
    {
        CanonicalSnapshot value{};
        value.rig_signature = environment.blueprint().signature();
        value.trial_id = trial_id;
        value.fixed_tick = fixed_tick;
        value.species = environment.blueprint().presentation_species();
        value.course_stage = environment.course_stage();
        value.gait_task = environment.gait_task();
        value.locomotion_intent_direction =
            environment.locomotion_intent_direction();
        value.locomotion_stop_requested =
            environment.locomotion_stop_requested();
        const auto& particles = environment.particles();
        const auto root = environment.blueprint().root_node;
        const float safe_dt = std::isfinite(fixed_dt) && fixed_dt > 1.0e-6f
            ? fixed_dt : 1.0f / 60.0f;
        value.pose_count = std::min(particles.size(), value.pose.size());
        value.valid = particles.size() <= value.pose.size()
            && environment.blueprint().valid();
        double dynamic_mass = 0.0;
        double weighted_position_x = 0.0;
        double weighted_position_y = 0.0;
        double weighted_velocity_x = 0.0;
        double weighted_velocity_y = 0.0;
        for (std::size_t index = 0; index < value.pose_count; ++index)
        {
            const sim::Particle& particle = particles[index];
            PoseParticleSnapshot& pose = value.pose[index];
            pose.position = particle.position;
            pose.velocity = (particle.position - particle.previous) / safe_dt;
            pose.inverse_mass = particle.inverse_mass;
            pose.radius = particle.radius;
            pose.grounded = particle.grounded;
            value.valid = value.valid && std::isfinite(pose.position.x)
                && std::isfinite(pose.position.y)
                && std::isfinite(pose.velocity.x)
                && std::isfinite(pose.velocity.y)
                && std::isfinite(pose.inverse_mass) && pose.inverse_mass >= 0.0f
                && std::isfinite(pose.radius) && pose.radius >= 0.0f;
            const double mass = 1.0 / static_cast<double>(
                std::max(pose.inverse_mass, 1.0e-5f));
            if (!std::isfinite(mass))
                value.valid = false;
            else
            {
                dynamic_mass += mass;
                weighted_position_x += mass
                    * static_cast<double>(pose.position.x);
                weighted_position_y += mass
                    * static_cast<double>(pose.position.y);
                weighted_velocity_x += mass
                    * static_cast<double>(pose.velocity.x);
                weighted_velocity_y += mass
                    * static_cast<double>(pose.velocity.y);
            }
        }
        if (dynamic_mass > 0.0 && std::isfinite(dynamic_mass))
        {
            value.total_particle_mass = static_cast<float>(dynamic_mass);
            value.center_of_mass_position = {
                static_cast<float>(weighted_position_x / dynamic_mass),
                static_cast<float>(weighted_position_y / dynamic_mass) };
            value.center_of_mass_velocity = {
                static_cast<float>(weighted_velocity_x / dynamic_mass),
                static_cast<float>(weighted_velocity_y / dynamic_mass) };
        }
        else
            value.valid = false;
        value.motor_count = std::min(environment.blueprint().active_motor_count,
            value.motors.size());
        value.bone_count = environment.blueprint().bones.size();
        value.action_layout = action_layout_for(environment);
        for (std::size_t index = 0; index < value.motor_count; ++index)
        {
            const sim::MotorDiagnostic diagnostic = environment.motor_diagnostic(index);
            value.motors[index] = {
                static_cast<std::uint16_t>(diagnostic.action_slot),
                diagnostic.parent, diagnostic.pivot, diagnostic.driven,
                diagnostic.current_angle,
                diagnostic.angular_velocity, diagnostic.applied_action,
                diagnostic.target_angle, diagnostic.available,
                diagnostic.enabled, diagnostic.support_branch };
            value.applied_motor_actions[index] = diagnostic.applied_action;
            value.valid = value.valid && std::isfinite(diagnostic.current_angle)
                && std::isfinite(diagnostic.angular_velocity)
                && std::isfinite(diagnostic.applied_action)
                && std::isfinite(diagnostic.target_angle);
        }
        value.root_node_index = root;
        value.torso_node_index = environment.blueprint().torso_node;
        value.left_contact_node_index = environment.blueprint().left_contact_node;
        value.right_contact_node_index = environment.blueprint().right_contact_node;
        if (root < value.pose_count)
        {
            value.root_pose = value.pose[root];
            value.root_position = value.root_pose.position;
            value.root_velocity = value.root_pose.velocity;
            value.valid = value.valid && std::isfinite(value.root_position.x)
                && std::isfinite(value.root_position.y)
                && std::isfinite(value.root_velocity.x)
                && std::isfinite(value.root_velocity.y);
            value.root_cell = canonical_cell_sample(environment,
                value.root_position);
        }
        else
            value.valid = false;
        const auto torso = environment.blueprint().torso_node;
        if (torso < value.pose_count)
            value.torso_pose = value.pose[torso];
        else
            value.valid = false;
        const auto left = environment.blueprint().left_contact_node;
        if (left < value.pose_count)
        {
            value.left_contact_pose = value.pose[left];
            value.left_contact_cell = canonical_cell_sample(environment,
                particles[left].position);
        }
        else
            value.valid = false;
        const auto right = environment.blueprint().right_contact_node;
        if (right < value.pose_count)
        {
            value.right_contact_pose = value.pose[right];
            value.right_contact_cell = canonical_cell_sample(environment,
                particles[right].position);
        }
        else
            value.valid = false;
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
        value.challenge_kind = challenge.kind;
        value.challenge_progress = environment.terrain().challenge_progress_at(sample_x);
        value.water_phase = environment.water_traversal_phase();
        value.weapon = environment.weapon_class();
        value.equipment = environment.equipment_state();
        value.equipment_directive = environment.equipment_directive();
        value.equipment_capable = environment.equipment_capable();
        value.shots_fired = environment.shots_fired();
        value.target_hits = environment.target_hits();
        value.shots_missed = environment.shots_missed();
        value.rounds_remaining = environment.equipment_rounds_remaining();
        value.magazine_capacity = environment.equipment_magazine_capacity();
        value.weapon_plant_seconds = environment.equipment_plant_seconds();
        value.reload_remaining = environment.equipment_reload_remaining();
        value.zero_progress_seconds = environment.zero_progress_seconds();
        value.micro_motion_seconds = environment.micro_motion_seconds();
        value.hazard_stall_seconds = environment.hazard_stall_seconds();
        value.fire_block = environment.equipment_fire_block();
        value.target_visible = environment.equipment_target_visible();
        value.left_contact_phase = environment.left_foot_phase();
        value.right_contact_phase = environment.right_foot_phase();
        value.raw_authority_telemetry_enabled =
            environment.raw_authority_telemetry_enabled();
        value.raw_authority = value.raw_authority_telemetry_enabled
            ? environment.raw_authority_telemetry()
            : sim::RawAuthorityTelemetry{};
        value.left_supported = environment.left_supported();
        value.right_supported = environment.right_supported();
        value.motion_valid = environment.valid_motion();
        value.valid = value.valid && canonical_snapshot_valid(value);
        return value;
    }

    [[nodiscard]] inline bool trial_stalled(sim::InvalidMotion reason,
        float zero_progress_seconds, float micro_motion_seconds,
        float hazard_stall_seconds) noexcept
    {
        const bool finite = std::isfinite(zero_progress_seconds)
            && std::isfinite(micro_motion_seconds)
            && std::isfinite(hazard_stall_seconds);
        return reason == sim::InvalidMotion::zero_progress
            || reason == sim::InvalidMotion::micro_motion
            || reason == sim::InvalidMotion::hazard_quiver
            || reason == sim::InvalidMotion::buried_no_escape
            || (finite && (zero_progress_seconds
                    >= sim::zero_progress_reset_seconds
                || micro_motion_seconds >= 3.0f
                || hazard_stall_seconds >= 1.35f));
    }

    [[nodiscard]] inline bool trial_stalled(
        const CanonicalSnapshot& value,
        sim::InvalidMotion reason = sim::InvalidMotion::none) noexcept
    {
        return trial_stalled(reason, value.zero_progress_seconds,
            value.micro_motion_seconds, value.hazard_stall_seconds);
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
            float secondary = 0.0f,
            std::uint32_t challenge_id = 0u) noexcept
        {
            events.push({ current.trial_id, current.fixed_tick, kind,
                effective_task_id,
                challenge_id != 0u ? challenge_id : current.stable_challenge_id,
                primary, secondary });
        };
        if (step.task_rejected)
            emit(DiagnosticKind::task_rejected);
        else if (step.task_applied)
            emit(DiagnosticKind::task_selected,
                step.selected_challenge_course_x,
                static_cast<float>(step.selected_challenge_authority),
                step.selected_challenge_id);
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
        if (trial_stalled(current, step.invalid_reason)
            && (previous == nullptr || !trial_stalled(*previous)))
            emit(DiagnosticKind::trial_stalled,
                current.zero_progress_seconds,
                std::max(current.micro_motion_seconds,
                    current.hazard_stall_seconds));
        if (step.terminal)
            emit(DiagnosticKind::trial_terminated,
                static_cast<float>(step.invalid_reason));
        return events;
    }

    [[nodiscard]] constexpr bool guidance_mode_for(
        GuidanceRequest request, sim::GuidanceMode& result) noexcept
    {
        if (request == GuidanceRequest::raw_policy)
        {
            result = sim::GuidanceMode::raw_policy_audit;
            return true;
        }
        if (request == GuidanceRequest::assisted)
        {
            result = sim::GuidanceMode::assisted;
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool stationary_player_task(
        director::TaskKind task) noexcept
    {
        return task == director::TaskKind::stand
            || task == director::TaskKind::stop_and_plant
            || task == director::TaskKind::gun_stance
            || task == director::TaskKind::acquire_and_aim
            || task == director::TaskKind::fire_and_correct;
    }

    [[nodiscard]] inline PlayerIntent player_intent_for(
        const TaskCommand& command) noexcept
    {
        if (!command.selected())
            return {};
        const bool stationary = stationary_player_task(command.task);
        return { player_intent_schema_version, command.stable_id,
            command.task_index, command.task, stationary ? 0.0f : 1.0f,
            stationary, false, false,
            command.task == director::TaskKind::fire_and_correct };
    }

    [[nodiscard]] inline ControlRejection validate_player_intent(
        const ControlRequest& request) noexcept
    {
        const PlayerIntent& intent = request.player_intent;
        if (!intent.valid())
            return intent.stable_task_id == 0u
                ? ControlRejection::missing_player_intent
                : ControlRejection::invalid_player_intent;
        if (intent.stable_task_id != request.selected_task.stable_id
            || intent.task_index != request.selected_task.task_index
            || intent.task != request.selected_task.task)
            return ControlRejection::invalid_player_intent;
        if (intent.interact || intent.use_tool)
            return ControlRejection::unsupported_player_intent;
        const bool stationary = stationary_player_task(intent.task);
        // Stop is an independent momentary player intent. A moving task may be
        // paused without selecting a different Director node, but a task whose
        // physical contract is planted may never request travel.
        if ((stationary && !intent.stop)
            || (intent.stop && std::abs(intent.travel_direction) > 1.0e-6f)
            || (!intent.stop && std::abs(intent.travel_direction) <= 1.0e-6f)
            || (intent.fire
                != (intent.task == director::TaskKind::fire_and_correct)))
            return ControlRejection::invalid_player_intent;
        return ControlRejection::none;
    }

    [[nodiscard]] inline ControlAuthorityReport control_authority_report(
        const sim::Environment& environment, const ControlRequest& request,
        const RetainedPolicyController* controller, bool accepted) noexcept
    {
        const rl::GuidanceAuthorityReport source =
            rl::guidance_authority_report(environment,
                environment.course_stage(),
                environment.course_stage() == sim::CourseStage::crouch_walk
                    ? environment.effective_physical_guidance_authority()
                    : 0.0f);
        ControlAuthorityReport result{};
        result.joint_control = request.joint_control;
        result.requested_guidance = request.guidance;
        result.applied_guidance = source.mode;
        if (controller != nullptr)
            result.retained_policy = controller->provenance();
        result.lesson_teacher = source.lesson_teacher;
        result.topology_support = source.topology_support;
        result.topology_body = source.topology_body;
        result.safety_support = source.safety_support;
        result.safety_body = source.safety_body;
        result.posture_guide = source.posture_guide;
        result.swing_clearance = source.swing_clearance;
        result.physical_contact = source.physical_contact;
        result.mandatory_joint_cluster = source.mandatory_joint_cluster;
        result.optional_guidance_active = source.optional_guidance_active();
        result.request_accepted = accepted;
        return result;
    }

    [[nodiscard]] inline ControlRejection validate_control_request(
        const sim::Environment& environment, const ControlRequest& request,
        const RetainedPolicyController* controller,
        sim::GuidanceMode& guidance) noexcept
    {
        if (static_cast<unsigned>(request.mode)
                > static_cast<unsigned>(ControlMode::training)
            || !request.task_graph.valid()
            || !director::evidence_valid(request.task_graph, request.evidence))
            return ControlRejection::malformed_director_state;
        if (!guidance_mode_for(request.guidance, guidance))
            return ControlRejection::unspecified_guidance;
        if (request.joint_control == JointControlSource::unspecified
            || static_cast<unsigned>(request.joint_control)
                > static_cast<unsigned>(JointControlSource::expert_joint_actions))
            return ControlRejection::unspecified_joint_control;
        if (std::ranges::any_of(request.actions,
                [](float value) noexcept
                {
                    return !std::isfinite(value)
                        || value < -1.0f || value > 1.0f;
                }))
            return ControlRejection::invalid_expert_action;
        const bool request_carries_expert_actions = std::ranges::any_of(
            request.actions, [](float value) noexcept { return value != 0.0f; });
        if ((request.joint_control == JointControlSource::expert_joint_actions
                && controller != nullptr)
            || (request.joint_control == JointControlSource::retained_policy
                && request_carries_expert_actions))
            return ControlRejection::mixed_joint_authority;
        if (request.mode == ControlMode::player_guided
            && !request.selected_task.selected())
            return ControlRejection::missing_player_intent;
        if (request.mode == ControlMode::player_guided)
        {
            const ControlRejection intent = validate_player_intent(request);
            if (intent != ControlRejection::none)
                return intent;
        }
        if (request.mode == ControlMode::player_guided
            && request.joint_control != JointControlSource::retained_policy)
            return ControlRejection::retained_policy_required;
        if (request.mode == ControlMode::player_guided
            && request.guidance != GuidanceRequest::raw_policy)
            return ControlRejection::player_guidance_not_raw;
        if (request.mode == ControlMode::player_guided
            && controller != nullptr
            && controller->provenance().valid
            && controller->provenance().retained_guidance
                != sim::GuidanceMode::raw_policy_audit)
            return ControlRejection::retained_player_guidance_not_raw;
        if (request.selected_task.selected()
            && !task_command_valid(request.task_graph, request.evidence,
                request.selected_task, profile_for(request.mode)))
            return ControlRejection::task_rejected;
        if (request.joint_control == JointControlSource::retained_policy)
        {
            if (controller == nullptr)
                return ControlRejection::retained_policy_required;
            sim::CourseStage requested_stage = environment.course_stage();
            sim::GaitTask requested_gait = environment.gait_task();
            sim::EquipmentDirective requested_equipment =
                environment.equipment_directive();
            if (request.selected_task.selected())
            {
                const director::TaskNode& node = request.task_graph.nodes[
                    request.selected_task.task_index];
                const DesiredTaskState desired = desired_task_state(
                    node.task, node.challenge);
                requested_stage = desired.stage;
                requested_gait = desired.gait;
                requested_equipment = desired.equipment;
            }
            if (!controller->valid_for(environment, guidance, requested_stage,
                    requested_gait, requested_equipment))
                return ControlRejection::retained_policy_mismatch;
            return ControlRejection::none;
        }
        sim::CourseStage requested_stage = environment.course_stage();
        if (request.selected_task.selected())
        {
            const director::TaskNode& node = request.task_graph.nodes[
                request.selected_task.task_index];
            requested_stage = desired_task_state(node.task, node.challenge).stage;
        }
        const rl::ActionMask action_mask = rl::action_mask_for(environment,
            requested_stage);
        for (std::size_t index = 0; index < request.actions.size(); ++index)
            if (!action_mask[index] && request.actions[index] != 0.0f)
                return ControlRejection::inactive_expert_action;
        return ControlRejection::none;
    }

    inline void finish_control_snapshot(FixedStepResult& result,
        const sim::Environment& environment, const ControlRequest& request,
        const RetainedPolicyController* controller, std::uint64_t trial_id,
        std::uint64_t fixed_tick, float fixed_dt) noexcept
    {
        result.snapshot = snapshot(environment, trial_id, fixed_tick, fixed_dt);
        result.authority = control_authority_report(environment, request,
            controller, result.control_applied);
        result.snapshot.control_authority = result.authority;
        result.snapshot.requested_actions = result.requested_actions;
        if (!result.control_applied)
            result.applied_motor_actions =
                result.snapshot.applied_motor_actions;
        result.snapshot.applied_motor_actions = result.applied_motor_actions;
        result.snapshot.valid = canonical_snapshot_valid(result.snapshot);
    }

    [[nodiscard]] inline FixedStepResult fixed_step_impl(
        sim::Environment& environment, const ControlRequest& request,
        const RetainedPolicyController* controller, std::uint64_t trial_id,
        std::uint64_t fixed_tick, float fixed_dt)
    {
        const float safe_dt = std::isfinite(fixed_dt)
            ? std::clamp(fixed_dt, 1.0f / 240.0f, 1.0f / 20.0f)
            : 1.0f / 60.0f;
        FixedStepResult output{};
        sim::GuidanceMode guidance{};
        output.control_rejection = validate_control_request(environment,
            request, controller, guidance);
        if (output.control_rejection != ControlRejection::none)
        {
            output.task_rejected = output.control_rejection
                == ControlRejection::task_rejected;
            finish_control_snapshot(output, environment, request, controller,
                trial_id, fixed_tick, safe_dt);
            return output;
        }

        // Player stop/direction is session state in Environment. Resolve it for
        // every accepted host mode so a later autonomous/training request can
        // never inherit a stale player stop. This is planner intent only; the
        // Environment setter does not move a particle or inject a force.
        const float desired_locomotion_direction =
            request.mode == ControlMode::player_guided
            ? request.player_intent.travel_direction : 1.0f;
        const bool desired_locomotion_stop =
            request.mode == ControlMode::player_guided
            && request.player_intent.stop;
        const bool intent_change = environment.locomotion_intent_direction()
                != desired_locomotion_direction
            || environment.locomotion_stop_requested()
                != desired_locomotion_stop;
        const float desired_physical_guidance_authority =
            guidance == sim::GuidanceMode::assisted ? 1.0f : 0.0f;
        const bool stage_on_candidate = request.retry_trial
            || environment.guidance_mode() != guidance || intent_change
            || environment.physical_guidance_authority()
                != desired_physical_guidance_authority
            || task_requires_staging(environment, request.selected_task,
                request.task_graph);
        // A steady-state fixed step has no speculative state and therefore no
        // large world copy. Any request that can change task/retry/intent/
        // guidance is staged with the physical step and committed as one unit.
        std::optional<sim::Environment> candidate{};
        if (stage_on_candidate)
            candidate.emplace(environment);
        sim::Environment& controlled = candidate.has_value()
            ? *candidate : environment;
        const TaskApplication task = apply_task_uncommitted(controlled,
            request.selected_task, request.task_graph, request.evidence,
            profile_for(request.mode));
        output.stable_task_id = task.stable_task_id;
        output.selected_challenge_id = task.stable_challenge_id;
        output.selected_challenge_authority = task.challenge_authority;
        output.selected_challenge_course_x = task.challenge_course_x;
        output.selected_challenge_present = task.challenge_present;
        output.task_rejected = task.rejected;
        if (task.rejected)
        {
            output.control_rejection = ControlRejection::task_rejected;
            finish_control_snapshot(output, environment, request, controller,
                trial_id, fixed_tick, safe_dt);
            return output;
        }

        if (request.retry_trial)
            controlled.reset();
        if (!controlled.set_locomotion_intent(desired_locomotion_direction,
                desired_locomotion_stop))
        {
            output.control_rejection = ControlRejection::invalid_player_intent;
            finish_control_snapshot(output, environment, request, controller,
                trial_id, fixed_tick, safe_dt);
            return output;
        }
        controlled.set_guidance_mode(guidance);
        controlled.set_physical_guidance_authority(
            desired_physical_guidance_authority);
        if (request.joint_control == JointControlSource::retained_policy)
            output.requested_actions = controller->action(controlled);
        else
            output.requested_actions = request.actions;
        if (std::ranges::any_of(output.requested_actions,
                [](float value) noexcept { return !std::isfinite(value); }))
        {
            output.control_rejection = ControlRejection::nonfinite_policy_action;
            finish_control_snapshot(output, environment, request, controller,
                trial_id, fixed_tick, safe_dt);
            return output;
        }

        const sim::StepResult result = controlled.step(
            output.requested_actions, safe_dt);
        if (candidate.has_value())
            environment = std::move(*candidate);
        output.task_applied = task.applied;
        output.retry_applied = request.retry_trial;
        output.reward = result.reward;
        output.terminal = result.terminated;
        output.invalid_reason = result.invalid_reason;
        output.terminal_cause = result.terminal_cause;
        output.control_applied = true;
        for (std::size_t index = 0; index < output.applied_motor_actions.size();
            ++index)
            output.applied_motor_actions[index] =
                environment.motor_diagnostic(index).applied_action;
        finish_control_snapshot(output, environment, request, controller,
            trial_id, fixed_tick, safe_dt);
        return output;
    }

    [[nodiscard]] inline FixedStepResult fixed_step(sim::Environment& environment,
        const ControlRequest& request, std::uint64_t trial_id,
        std::uint64_t fixed_tick, float fixed_dt = 1.0f / 60.0f)
    {
        return fixed_step_impl(environment, request, nullptr, trial_id,
            fixed_tick, fixed_dt);
    }

    [[nodiscard]] inline FixedStepResult fixed_step(sim::Environment& environment,
        const ControlRequest& request,
        const RetainedPolicyController& controller, std::uint64_t trial_id,
        std::uint64_t fixed_tick, float fixed_dt = 1.0f / 60.0f)
    {
        return fixed_step_impl(environment, request, &controller, trial_id,
            fixed_tick, fixed_dt);
    }
}
