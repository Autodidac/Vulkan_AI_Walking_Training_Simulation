#include <Epoch2DWalkEngine/Engine.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
    namespace integration = runner::integration;
    namespace rl = runner::rl;
    namespace sim = runner::sim;

    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Epoch2DWalkEngine v0.7.51 Runner-example lake challenge test failed: "
                  << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    struct LakeEvidence
    {
        sim::CreatureSpecies species{ sim::CreatureSpecies::human };
        std::uint64_t seed{};
        integration::JointControlSource action_source{
            integration::JointControlSource::expert_joint_actions };
        sim::GuidanceMode guidance{ sim::GuidanceMode::raw_policy_audit };
        bool raw_authority_enabled{};
        std::size_t bone_count{};
        std::array<float, sim::anatomy_action_count> requested_actions{};
        std::array<float, sim::anatomy_action_count> expected_applied_actions{};
        std::array<float, sim::anatomy_action_count> applied_actions{};
        sim::RawAuthorityTelemetry authority{};
        float root_x{};
        std::uint32_t root_cell_x{};
        std::uint32_t root_cell_y{};
        std::uint32_t root_cell_material{};
        float shore_x{};
        float elapsed_seconds{};
        sim::GaitTask gait_task{ sim::GaitTask::walk };
        std::uint32_t gait_cycles{};
        sim::TerrainRegion region{ sim::TerrainRegion::firm };
        std::uint32_t challenge_id{};
        float challenge_progress{};
        sim::TrialTerminalCause terminal_cause{
            sim::TrialTerminalCause::none };
        sim::InvalidMotion invalid_reason{ sim::InvalidMotion::none };
        float traction_used{};
        float traction_capacity{};
        float slip_speed{};
        float slip_distance{};
        float water_depth{};
        float immersion{};
        float zero_progress_seconds{};
        float micro_motion_seconds{};
        float hazard_stall_seconds{};
        sim::WaterTraversalPhase water_phase{
            sim::WaterTraversalPhase::dry };
        bool recovering{};
        std::uint32_t recovery_events{};
        std::uint32_t recovery_successes{};
        bool motion_valid{};
        bool root_cell_valid{};
    };

    [[nodiscard]] LakeEvidence lake_evidence(const sim::Environment& environment,
        const sim::StepResult& step, std::uint64_t seed,
        std::uint64_t fixed_tick, float shore_x,
        const std::array<float, sim::action_count>& requested_actions,
        const std::array<float, sim::anatomy_action_count>& expected_actions)
    {
        const sim::RawAuthorityTelemetry& raw =
            environment.raw_authority_telemetry();
        for (std::size_t side = 0u;
            side < raw.support_traction_capacity.size(); ++side)
        {
            const float capacity = raw.support_traction_capacity[side];
            const float used = raw.support_traction_used[side];
            const float tolerance = 1.0e-5f
                * std::max({ 1.0f, std::abs(capacity), std::abs(used) });
            require(std::isfinite(capacity) && capacity >= 0.0f
                    && std::isfinite(used) && used >= 0.0f
                    && used <= capacity + tolerance,
                "Raw support manifold retained traction above its finite per-step capacity");
        }
        const integration::CanonicalSnapshot state = integration::snapshot(
            environment, seed, fixed_tick);
        require(state.valid && integration::canonical_snapshot_valid(state),
            "lake evidence could not be represented by canonical snapshot schema 5");
        LakeEvidence value{};
        value.species = state.species;
        value.seed = seed;
        value.guidance = environment.guidance_mode();
        value.raw_authority_enabled = state.raw_authority_telemetry_enabled;
        value.bone_count = state.bone_count;
        for (std::size_t index = 0u; index < value.requested_actions.size();
            ++index)
        {
            value.requested_actions[index] = std::clamp(
                requested_actions[index], -1.0f, 1.0f);
            value.expected_applied_actions[index] = expected_actions[index];
            value.applied_actions[index] = state.applied_motor_actions[index];
        }
        value.authority = state.raw_authority;
        value.root_x = state.root_position.x;
        value.root_cell_x = state.root_cell.global_x;
        value.root_cell_y = state.root_cell.global_y;
        value.root_cell_material = state.root_cell.cell.material;
        value.shore_x = shore_x;
        value.elapsed_seconds = environment.elapsed_seconds();
        value.gait_task = state.gait_task;
        value.gait_cycles = environment.gait_cycles();
        value.region = state.terrain_region;
        value.challenge_id = state.stable_challenge_id;
        value.challenge_progress = state.challenge_progress;
        value.terminal_cause = step.terminal_cause;
        value.invalid_reason = step.invalid_reason;
        value.traction_used = state.used_traction;
        value.traction_capacity = state.available_traction;
        value.slip_speed = state.stance_slip_speed;
        value.slip_distance = state.stance_slip_distance;
        value.water_depth = state.water_depth;
        value.immersion = state.water_submersion;
        value.zero_progress_seconds = state.zero_progress_seconds;
        value.micro_motion_seconds = state.micro_motion_seconds;
        value.hazard_stall_seconds = state.hazard_stall_seconds;
        value.water_phase = state.water_phase;
        value.recovering = environment.recovering();
        value.recovery_events = environment.recovery_events();
        value.recovery_successes = environment.recovery_successes();
        value.motion_valid = state.motion_valid;
        value.root_cell_valid = state.root_cell.valid;
        return value;
    }

    [[nodiscard]] constexpr std::size_t authority_index(
        sim::AuthoritySource source) noexcept
    {
        return static_cast<std::size_t>(source);
    }

    [[nodiscard]] std::array<float, sim::anatomy_action_count>
        expected_raw_actuator_actions(const sim::Environment& environment,
            const std::array<float, sim::action_count>& requested,
            const std::array<float, sim::anatomy_action_count>& previous) noexcept
    {
        constexpr float dt = 1.0f / 60.0f;
        constexpr float human_raw_slew_per_second = 1.0f;
        const bool physical_moving_human =
            environment.blueprint().human_casual_gait_plan()
            && environment.course_stage() != sim::CourseStage::balance
            && environment.course_stage() != sim::CourseStage::duck_press;
        const float ramp = sim::policy_control_ramp(
            environment.elapsed_seconds() + (physical_moving_human
                ? sim::policy_control_ramp_start_seconds : 0.0f));
        std::array<float, sim::anatomy_action_count> result{};
        for (std::size_t index = 0u;
            index < environment.blueprint().active_motor_count; ++index)
        {
            const float desired = std::clamp(requested[index], -1.0f, 1.0f)
                * ramp;
            if (environment.blueprint().human_casual_gait_plan())
            {
                const float maximum_delta = human_raw_slew_per_second * dt;
                result[index] = previous[index] + std::clamp(
                    desired - previous[index], -maximum_delta, maximum_delta);
            }
            else
            {
                result[index] = sim::conditioned_single_support_motor_action(
                    environment.blueprint(), index, previous[index], desired, dt);
            }
        }
        return result;
    }

    [[nodiscard]] bool quiet_authority_source(
        const sim::AuthoritySourceTelemetry& source) noexcept
    {
        return source.center_velocity_delta_x == 0.0f
            && source.center_velocity_delta_y == 0.0f
            && source.maximum_absolute_center_velocity_delta_x == 0.0f
            && source.maximum_absolute_center_velocity_delta_y == 0.0f
            && source.positive_horizontal_specific_energy == 0.0f
            && source.center_position_delta_x == 0.0f
            && source.center_position_delta_y == 0.0f
            && source.center_history_delta_x == 0.0f
            && source.center_history_delta_y == 0.0f
            && source.root_position_delta_x == 0.0f
            && source.root_position_delta_y == 0.0f
            && source.torso_position_delta_x == 0.0f
            && source.torso_position_delta_y == 0.0f
            && source.applications == 0u;
    }

    [[nodiscard]] bool same_authority_source(
        const sim::AuthoritySourceTelemetry& left,
        const sim::AuthoritySourceTelemetry& right) noexcept
    {
        return left.center_velocity_delta_x == right.center_velocity_delta_x
            && left.center_velocity_delta_y == right.center_velocity_delta_y
            && left.maximum_absolute_center_velocity_delta_x
                == right.maximum_absolute_center_velocity_delta_x
            && left.maximum_absolute_center_velocity_delta_y
                == right.maximum_absolute_center_velocity_delta_y
            && left.positive_horizontal_specific_energy
                == right.positive_horizontal_specific_energy
            && left.center_position_delta_x == right.center_position_delta_x
            && left.center_position_delta_y == right.center_position_delta_y
            && left.center_history_delta_x == right.center_history_delta_x
            && left.center_history_delta_y == right.center_history_delta_y
            && left.root_position_delta_x == right.root_position_delta_x
            && left.root_position_delta_y == right.root_position_delta_y
            && left.torso_position_delta_x == right.torso_position_delta_x
            && left.torso_position_delta_y == right.torso_position_delta_y
            && left.applications == right.applications;
    }

    [[nodiscard]] bool same_authority(
        const sim::RawAuthorityTelemetry& left,
        const sim::RawAuthorityTelemetry& right) noexcept
    {
        if (left.commanded_actions != right.commanded_actions
            || left.actuator_actions != right.actuator_actions
            || left.total_center_velocity_delta_x
                != right.total_center_velocity_delta_x
            || left.total_center_velocity_delta_y
                != right.total_center_velocity_delta_y
            || left.maximum_bone_error_ratio
                != right.maximum_bone_error_ratio
            || left.maximum_bone_error_index != right.maximum_bone_error_index
            || left.support_contact_corrections
                != right.support_contact_corrections
            || left.non_support_contact_corrections
                != right.non_support_contact_corrections
            || left.support_traction_capacity != right.support_traction_capacity
            || left.support_traction_used != right.support_traction_used
            || left.support_normal_impulse != right.support_normal_impulse
            || left.support_tangential_impulse
                != right.support_tangential_impulse
            || left.friction_work_joules != right.friction_work_joules
            || left.friction_positive_work_joules
                != right.friction_positive_work_joules
            || left.powered_toe_commands != right.powered_toe_commands
            || left.action_support_release_requests
                != right.action_support_release_requests
            || left.action_support_release_active_frames
                != right.action_support_release_active_frames
            || left.root_drive_corrections != right.root_drive_corrections
            || left.torso_drive_corrections != right.torso_drive_corrections
            || left.course_motion_corrections != right.course_motion_corrections)
            return false;
        for (std::size_t index = 0u; index < left.sources.size(); ++index)
            if (!same_authority_source(left.sources[index], right.sources[index]))
                return false;
        return true;
    }

    [[nodiscard]] bool valid_evidence(const LakeEvidence& value) noexcept
    {
        const bool finite = std::isfinite(value.root_x)
            && std::isfinite(value.shore_x)
            && std::isfinite(value.elapsed_seconds)
            && std::isfinite(value.challenge_progress)
            && std::isfinite(value.traction_used)
            && std::isfinite(value.traction_capacity)
            && std::isfinite(value.slip_speed)
            && std::isfinite(value.slip_distance)
            && std::isfinite(value.water_depth)
            && std::isfinite(value.immersion)
            && std::isfinite(value.zero_progress_seconds)
            && std::isfinite(value.micro_motion_seconds)
            && std::isfinite(value.hazard_stall_seconds);
        const float traction_tolerance = 1.0e-5f * std::max({ 1.0f,
            std::abs(value.traction_used), std::abs(value.traction_capacity) });
        bool action_identity = true;
        for (std::size_t index = 0u; index < value.requested_actions.size();
            ++index)
        {
            action_identity = action_identity
                && std::isfinite(value.requested_actions[index])
                && value.requested_actions[index] >= -1.0f
                && value.requested_actions[index] <= 1.0f
                && value.requested_actions[index]
                    == value.authority.commanded_actions[index]
                && std::isfinite(value.expected_applied_actions[index])
                && value.expected_applied_actions[index] >= -1.0f
                && value.expected_applied_actions[index] <= 1.0f
                && value.expected_applied_actions[index]
                    == value.authority.actuator_actions[index]
                && value.expected_applied_actions[index]
                    == value.applied_actions[index];
        }
        const sim::AuthoritySourceTelemetry& posture = value.authority.sources[
            authority_index(sim::AuthoritySource::posture_helper)];
        const sim::AuthoritySourceTelemetry& manipulator = value.authority.sources[
            authority_index(sim::AuthoritySource::manipulator_helper)];
        return finite && action_identity && value.seed != 0u
            && value.action_source
                == integration::JointControlSource::expert_joint_actions
            && value.guidance == sim::GuidanceMode::raw_policy_audit
            && value.raw_authority_enabled
            && integration::valid_raw_authority_telemetry(value.authority,
                true, value.bone_count)
            && value.authority.root_drive_corrections == 0u
            && value.authority.torso_drive_corrections == 0u
            && value.authority.course_motion_corrections == 0u
            && value.authority.powered_toe_commands == 0u
            && quiet_authority_source(posture)
            && quiet_authority_source(manipulator)
            && value.root_cell_valid
            && value.root_cell_x
                < sim::DeformableTerrain::training_world_dimensions.width
            && value.root_cell_y
                < sim::DeformableTerrain::training_world_dimensions.height
            && value.root_cell_material < sandhybrid::material_count
            && value.shore_x > 0.0f && value.elapsed_seconds >= 0.0f
            && value.elapsed_seconds <= 120.0f + 1.0f / 30.0f
            && static_cast<unsigned>(value.species)
                <= static_cast<unsigned>(sim::CreatureSpecies::hexapod)
            && static_cast<unsigned>(value.gait_task)
                <= static_cast<unsigned>(sim::GaitTask::run)
            && static_cast<unsigned>(value.region)
                <= static_cast<unsigned>(sim::TerrainRegion::hole)
            && static_cast<unsigned>(value.terminal_cause)
                <= static_cast<unsigned>(sim::TrialTerminalCause::time_limit)
            && static_cast<unsigned>(value.invalid_reason)
                <= static_cast<unsigned>(sim::InvalidMotion::structural_compression)
            && static_cast<unsigned>(value.water_phase)
                <= static_cast<unsigned>(sim::WaterTraversalPhase::shore_exit)
            && value.challenge_id != 0u
            && value.challenge_progress >= 0.0f
            && value.challenge_progress <= 1.0f
            && value.traction_used >= 0.0f
            && value.traction_capacity >= 0.0f
            && value.traction_used
                <= value.traction_capacity + traction_tolerance
            && value.slip_speed >= 0.0f && value.slip_distance >= 0.0f
            && value.water_depth >= 0.0f
            && value.immersion >= 0.0f && value.immersion <= 1.0f
            && value.zero_progress_seconds >= 0.0f
            && value.micro_motion_seconds >= 0.0f
            && value.hazard_stall_seconds >= 0.0f
            && value.recovery_successes <= value.recovery_events;
    }

    [[nodiscard]] bool same_evidence(const LakeEvidence& left,
        const LakeEvidence& right) noexcept
    {
        return left.species == right.species && left.seed == right.seed
            && left.action_source == right.action_source
            && left.guidance == right.guidance
            && left.raw_authority_enabled == right.raw_authority_enabled
            && left.bone_count == right.bone_count
            && left.requested_actions == right.requested_actions
            && left.expected_applied_actions
                == right.expected_applied_actions
            && left.applied_actions == right.applied_actions
            && same_authority(left.authority, right.authority)
            && left.root_x == right.root_x
            && left.root_cell_x == right.root_cell_x
            && left.root_cell_y == right.root_cell_y
            && left.root_cell_material == right.root_cell_material
            && left.shore_x == right.shore_x
            && left.elapsed_seconds == right.elapsed_seconds
            && left.gait_task == right.gait_task
            && left.gait_cycles == right.gait_cycles
            && left.region == right.region
            && left.challenge_id == right.challenge_id
            && left.challenge_progress == right.challenge_progress
            && left.terminal_cause == right.terminal_cause
            && left.invalid_reason == right.invalid_reason
            && left.traction_used == right.traction_used
            && left.traction_capacity == right.traction_capacity
            && left.slip_speed == right.slip_speed
            && left.slip_distance == right.slip_distance
            && left.water_depth == right.water_depth
            && left.immersion == right.immersion
            && left.zero_progress_seconds == right.zero_progress_seconds
            && left.micro_motion_seconds == right.micro_motion_seconds
            && left.hazard_stall_seconds == right.hazard_stall_seconds
            && left.water_phase == right.water_phase
            && left.recovering == right.recovering
            && left.recovery_events == right.recovery_events
            && left.recovery_successes == right.recovery_successes
            && left.motion_valid == right.motion_valid
            && left.root_cell_valid == right.root_cell_valid;
    }

    [[nodiscard]] std::string describe(const LakeEvidence& value)
    {
        std::ostringstream out;
        out << "species=" << sim::creature_species_name(value.species)
            << " seed=" << value.seed
            << " action_source=EXPERT RAW TEACHER JOINT ACTIONS"
            << " guidance=" << sim::guidance_mode_name(value.guidance)
            << " raw_authority=" << value.raw_authority_enabled
            << " helpers=" << value.authority.root_drive_corrections << '/'
                << value.authority.torso_drive_corrections << '/'
                << value.authority.course_motion_corrections
            << " posture=" << value.authority.sources[authority_index(
                sim::AuthoritySource::posture_helper)].applications
            << " manipulator=" << value.authority.sources[authority_index(
                sim::AuthoritySource::manipulator_helper)].applications
            << " powered_toe=" << value.authority.powered_toe_commands
            << " passive_toe=" << value.authority.sources[authority_index(
                sim::AuthoritySource::articulated_toe)].applications
            << " x=" << value.root_x
            << " cell=" << value.root_cell_x << ',' << value.root_cell_y
            << " material=" << value.root_cell_material
            << " shore=" << value.shore_x
            << " elapsed=" << value.elapsed_seconds
            << " gait=" << sim::gait_task_name(value.gait_task)
            << " cycles=" << value.gait_cycles
            << " region=" << sim::terrain_region_name(value.region)
            << " challenge=" << value.challenge_id
            << " progress=" << value.challenge_progress
            << " terminal=" << sim::trial_terminal_cause_name(
                value.terminal_cause, value.invalid_reason)
            << " invalid=" << sim::invalid_motion_name(value.invalid_reason)
            << " traction=" << value.traction_used << '/'
                << value.traction_capacity
            << " slip=" << value.slip_speed << '/' << value.slip_distance
            << " water=" << value.water_depth
            << " immersion=" << value.immersion
            << " stall=" << value.zero_progress_seconds << '/'
                << value.micro_motion_seconds << '/'
                << value.hazard_stall_seconds
            << " phase=" << sim::water_traversal_phase_name(value.water_phase)
            << " recovery=" << (value.recovering ? "ACTIVE" : "IDLE")
            << " recovery_events=" << value.recovery_events
            << " recovery_successes=" << value.recovery_successes;
        out << " actions=";
        for (std::size_t index = 0u; index < value.requested_actions.size();
            ++index)
        {
            if (index != 0u)
                out << ',';
            out << index << ':' << value.requested_actions[index] << '/'
                << value.expected_applied_actions[index] << '/'
                << value.applied_actions[index];
        }
        return out.str();
    }

    [[nodiscard]] float shallow_water_end(const sim::Environment& environment)
    {
        float result{};
        for (const sim::TerrainChallenge& challenge :
            environment.terrain().terrain_challenges())
        {
            if (challenge.region == sim::TerrainRegion::shallow_water)
                result = std::max(result, challenge.end);
        }
        require(result > 0.0f,
            "authored course did not expose a shallow-water shore");
        return result;
    }

    void configure_lake_trial(sim::Environment& environment)
    {
        environment.set_course(sim::CourseStage::uneven, 0.30f);
        environment.set_course_motion_enabled(false);
        environment.set_trial_time_limit(120.0f);
        environment.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
        environment.set_raw_authority_telemetry_enabled(true);
    }

    void test_four_species_cross_lake_repeated_deterministically()
    {
        const std::array rigs{
            sim::CreatureBlueprint::humanoid(),
            sim::CreatureBlueprint::chicken(),
            sim::CreatureBlueprint::crawler4(),
            sim::CreatureBlueprint::hexapod()
        };
        constexpr std::array<std::uint64_t, 2> seeds{ 0x7501u, 0x7502u };

        for (const sim::CreatureBlueprint& rig : rigs)
        {
            for (const std::uint64_t seed : seeds)
            {
                sim::Environment first{ rig, seed };
                sim::Environment repeated{ rig, seed };
                configure_lake_trial(first);
                configure_lake_trial(repeated);
                const float shore = shallow_water_end(first);
                require(shore == shallow_water_end(repeated),
                    "paired course construction changed the lake shore");

                std::optional<LakeEvidence> first_stall;
                std::optional<LakeEvidence> repeated_first_stall;
                LakeEvidence final_first{};
                LakeEvidence final_repeated{};
                bool previously_stalled{};
                bool repeated_previously_stalled{};
                bool crossed{};
                bool repeated_crossed{};
                std::array<float, sim::anatomy_action_count>
                    previous_applied{};
                std::array<float, sim::anatomy_action_count>
                    repeated_previous_applied{};

                for (std::uint64_t frame = 1u; frame <= 7200u; ++frame)
                {
                    const auto first_action =
                        rl::raw_walking_teacher_action(first);
                    const auto repeated_action =
                        rl::raw_walking_teacher_action(repeated);
                    require(first_action == repeated_action,
                        "paired lake policy action diverged for one seed/species");
                    const auto first_expected = expected_raw_actuator_actions(
                        first, first_action, previous_applied);
                    const auto repeated_expected = expected_raw_actuator_actions(
                        repeated, repeated_action, repeated_previous_applied);
                    require(first_expected == repeated_expected,
                        "paired Raw actuator conditioning diverged");
                    const sim::StepResult first_step = first.step(first_action);
                    const sim::StepResult repeated_step =
                        repeated.step(repeated_action);
                    final_first = lake_evidence(first, first_step, seed,
                        frame, shore, first_action, first_expected);
                    final_repeated = lake_evidence(repeated, repeated_step,
                        seed, frame, shore, repeated_action,
                        repeated_expected);
                    previous_applied = final_first.applied_actions;
                    repeated_previous_applied = final_repeated.applied_actions;
                    if (!valid_evidence(final_first)
                        || !valid_evidence(final_repeated))
                    {
                        std::cerr << "lake invalid evidence A: "
                                  << describe(final_first) << '\n'
                                  << "lake invalid evidence B: "
                                  << describe(final_repeated) << '\n';
                    }
                    require(valid_evidence(final_first)
                            && valid_evidence(final_repeated),
                        "lake trial emitted invalid or unbounded evidence");
                    if (!same_evidence(final_first, final_repeated))
                    {
                        std::cerr << "lake divergent evidence A: "
                                  << describe(final_first) << '\n'
                                  << "lake divergent evidence B: "
                                  << describe(final_repeated) << '\n';
                    }
                    require(same_evidence(final_first, final_repeated),
                        "paired lake evidence diverged for one seed/species");

                    const bool stalled = integration::trial_stalled(
                        first_step.invalid_reason,
                        final_first.zero_progress_seconds,
                        final_first.micro_motion_seconds,
                        final_first.hazard_stall_seconds);
                    const bool repeated_stalled = integration::trial_stalled(
                        repeated_step.invalid_reason,
                        final_repeated.zero_progress_seconds,
                        final_repeated.micro_motion_seconds,
                        final_repeated.hazard_stall_seconds);
                    if (stalled && !previously_stalled)
                        first_stall = final_first;
                    if (repeated_stalled && !repeated_previously_stalled)
                        repeated_first_stall = final_repeated;
                    previously_stalled = stalled;
                    repeated_previously_stalled = repeated_stalled;

                    crossed = final_first.root_x > shore + 0.5f;
                    repeated_crossed = final_repeated.root_x > shore + 0.5f;
                    require(crossed == repeated_crossed,
                        "paired lake crossing frame diverged");
                    if (crossed || first_step.terminated
                        || repeated_step.terminated)
                        break;
                }

                require(first_stall.has_value()
                        == repeated_first_stall.has_value(),
                    "paired first-stall presence diverged");
                if (first_stall && repeated_first_stall)
                {
                    require(valid_evidence(*first_stall)
                            && same_evidence(*first_stall,
                                *repeated_first_stall),
                        "paired first-stall evidence was invalid or divergent");
                }
                if (!crossed || !repeated_crossed || !final_first.motion_valid)
                {
                    std::cerr << "lake final: " << describe(final_first) << '\n';
                    if (first_stall)
                        std::cerr << "lake first stall: "
                                  << describe(*first_stall) << '\n';
                }
                require(crossed && repeated_crossed
                        && final_first.motion_valid
                        && final_repeated.motion_valid,
                    "a production morphology failed the repeated authored-lake gate");
            }
        }
    }

    void test_zero_action_records_bounded_first_stall()
    {
        constexpr std::uint64_t seed = 0x7510u;
        sim::Environment environment{ sim::CreatureBlueprint::crawler4(), seed };
        configure_lake_trial(environment);
        const float shore = shallow_water_end(environment);
        const std::array<float, sim::action_count> no_actions{};
        std::array<float, sim::anatomy_action_count> previous_applied{};
        std::optional<LakeEvidence> first_stall;
        LakeEvidence final{};

        for (std::uint64_t frame = 1u; frame <= 600u; ++frame)
        {
            const auto expected = expected_raw_actuator_actions(environment,
                no_actions, previous_applied);
            const sim::StepResult step = environment.step(no_actions);
            final = lake_evidence(environment, step, seed, frame, shore,
                no_actions, expected);
            previous_applied = final.applied_actions;
            require(valid_evidence(final),
                "zero-action probe emitted invalid diagnostic evidence");
            if (integration::trial_stalled(step.invalid_reason,
                    environment.zero_progress_seconds(),
                    environment.micro_motion_seconds(),
                    environment.hazard_stall_seconds()))
            {
                first_stall = final;
                break;
            }
            if (step.terminated)
                break;
        }

        require(final.root_x <= shore + 0.5f,
            "zero-action Raw probe was silently advanced across the lake");
        if (!first_stall)
            std::cerr << "zero-action terminal: " << describe(final) << '\n';
        require(first_stall.has_value() && valid_evidence(*first_stall),
            "zero-action Raw probe did not expose a bounded first stall");
        const std::string diagnostic = describe(*first_stall);
        for (const std::string_view field : { "species=", "seed=",
            " action_source=", " guidance=", " raw_authority=", " helpers=",
            " posture=", " manipulator=", " powered_toe=", " passive_toe=",
            " x=",
            " cell=", " material=", " shore=", " elapsed=", " gait=",
            " cycles=", " region=", " challenge=", " progress=",
            " terminal=", " invalid=", " traction=", " slip=", " water=",
            " immersion=", " stall=", " phase=", " recovery=", " actions=" })
        {
            require(diagnostic.find(field) != std::string::npos,
                "first-stall diagnostic omitted a required field");
        }
    }
}

int main()
{
    static_assert(integration::api_version == 7u);
    static_assert(integration::canonical_snapshot_schema_version == 5u);
    test_four_species_cross_lake_repeated_deterministically();
    test_zero_action_records_bounded_first_stall();
    std::cout << "Epoch2DWalkEngine v0.7.51 Runner-example repeated-seed lake challenge tests passed\n";
    return EXIT_SUCCESS;
}
