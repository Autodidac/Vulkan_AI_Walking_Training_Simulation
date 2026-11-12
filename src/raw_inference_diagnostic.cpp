#include "raw_inference_diagnostic.hpp"

#include "ppo.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace runner::diagnostics
{
    namespace
    {
        struct CorrelationMoments
        {
            double x{};
            double y{};
            double xx{};
            double yy{};
            double xy{};
            std::uint64_t samples{};

            void add(double first, double second) noexcept
            {
                x += first;
                y += second;
                xx += first * first;
                yy += second * second;
                xy += first * second;
                ++samples;
            }

            void merge(const CorrelationMoments& other) noexcept
            {
                x += other.x;
                y += other.y;
                xx += other.xx;
                yy += other.yy;
                xy += other.xy;
                samples += other.samples;
            }

            [[nodiscard]] float correlation() const noexcept
            {
                if (samples < 2u)
                    return 0.0f;
                const double count = static_cast<double>(samples);
                const double covariance = xy - x * y / count;
                const double variance_x = xx - x * x / count;
                const double variance_y = yy - y * y / count;
                const double denominator = std::sqrt(std::max(0.0,
                    variance_x * variance_y));
                return denominator > 1.0e-12
                    ? static_cast<float>(covariance / denominator) : 0.0f;
            }
        };

        struct ReplayAccumulation
        {
            RawInferenceSeedEvidence evidence{};
            std::array<double, sim::anatomy_action_count> action_sum{};
            std::array<double, sim::anatomy_action_count> action_square_sum{};
            std::array<CorrelationMoments,
                sim::anatomy_action_count> lag_moments{};
            std::array<CorrelationMoments, 2> contralateral_moments{};
            std::array<double, sim::anatomy_action_count> prior_action{};
            std::uint32_t action_samples{};
        };

        constexpr std::array internal_authority_sources{
            sim::AuthoritySource::distance_constraints,
            sim::AuthoritySource::joint_motors,
            sim::AuthoritySource::articulated_toe,
            sim::AuthoritySource::passive_anatomy,
            sim::AuthoritySource::support_separation,
            sim::AuthoritySource::final_leg_constraints,
            sim::AuthoritySource::toe_rate_limit,
            sim::AuthoritySource::structure_projection,
            sim::AuthoritySource::equipment
        };

        [[nodiscard]] Vec2 mass_center(const sim::Environment& environment) noexcept
        {
            Vec2 center{};
            float total_mass{};
            for (const sim::Particle& particle : environment.particles())
            {
                const float mass = 1.0f
                    / std::max(particle.inverse_mass, 1.0e-6f);
                center += particle.position * mass;
                total_mass += mass;
            }
            return center / std::max(total_mass, 1.0e-6f);
        }

        [[nodiscard]] std::uint64_t append_fingerprint(
            std::uint64_t fingerprint, float value) noexcept
        {
            fingerprint ^= std::bit_cast<std::uint32_t>(value);
            fingerprint *= 1099511628211ULL;
            return fingerprint;
        }

        [[nodiscard]] std::uint64_t trajectory_fingerprint(
            const sim::Environment& environment) noexcept
        {
            std::uint64_t result = 1469598103934665603ULL;
            for (const sim::Particle& particle : environment.particles())
            {
                result = append_fingerprint(result, particle.position.x);
                result = append_fingerprint(result, particle.position.y);
                result = append_fingerprint(result, particle.previous.x);
                result = append_fingerprint(result, particle.previous.y);
            }
            result ^= environment.gait_cycles();
            result *= 1099511628211ULL;
            result ^= environment.alternating_steps();
            return result;
        }

        [[nodiscard]] std::uint64_t parameter_fingerprint(
            std::span<const float> parameters) noexcept
        {
            std::uint64_t result = 1469598103934665603ULL;
            for (const float parameter : parameters)
                result = append_fingerprint(result, parameter);
            result ^= parameters.size();
            result *= 1099511628211ULL;
            return result;
        }

        [[nodiscard]] std::uint32_t masked_action_mismatches(
            const sim::Environment& environment, sim::CourseStage stage,
            const std::array<float, sim::action_count>& raw_action,
            const std::array<float, sim::action_count>& effective_action) noexcept
        {
            const rl::ActionMask active = rl::action_mask_for(environment, stage);
            std::uint32_t mismatches{};
            for (std::size_t action = 0; action < sim::action_count; ++action)
            {
                const float expected = active[action]
                    ? clamp(raw_action[action], -1.0f, 1.0f) : 0.0f;
                mismatches += effective_action[action] == expected ? 0u : 1u;
            }
            return mismatches;
        }

        [[nodiscard]] ReplayAccumulation replay_network(
            const rl::PolicyNetwork& policy, std::uint64_t seed,
            bool collect_actions, float direction = 1.0f)
        {
            constexpr float dt = 1.0f / 60.0f;
            sim::Environment environment{
                sim::CreatureBlueprint::humanoid(), seed };
            environment.set_course(sim::CourseStage::uneven, 0.30f);
            environment.set_course_motion_enabled(false);
            environment.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
            environment.set_raw_authority_telemetry_enabled(true);
            if (!environment.set_locomotion_intent(direction, false))
                return {};

            ReplayAccumulation result{};
            result.evidence.seed = seed;
            result.evidence.requested_direction = direction;
            result.evidence.command_trace_digest = 1469598103934665603ULL;
            result.evidence.contact_trace_digest = 1469598103934665603ULL;
            const float infinity = std::numeric_limits<float>::infinity();
            result.evidence.raw_action_min.fill(infinity);
            result.evidence.raw_action_max.fill(-infinity);
            result.evidence.effective_action_min.fill(infinity);
            result.evidence.effective_action_max.fill(-infinity);
            result.evidence.actuator_action_min.fill(infinity);
            result.evidence.actuator_action_max.fill(-infinity);
            result.evidence.motor_action_min.fill(infinity);
            result.evidence.motor_action_max.fill(-infinity);
            result.evidence.stationary_course =
                !environment.course_motion_enabled()
                && environment.course_speed() == 0.0f
                && environment.course_progress() == 0.0f;
            Vec2 previous_center = mass_center(environment);
            float previous_center_velocity{};
            float previous_pelvis_x = environment.particles()[
                environment.blueprint().root_node].position.x;
            float previous_pelvis_velocity{};
            std::array<float, sim::anatomy_action_count> previous_actuator{};
            bool previous_left_support = environment.left_supported();
            bool previous_right_support = environment.right_supported();
            double pelvis_delta_sum{};
            double center_delta_sum{};
            std::uint32_t frames{};
            for (int frame = 0; frame < 1200; ++frame)
            {
                const auto raw_action = policy.deterministic_action(
                    rl::foundational_policy_observation(environment,
                        sim::CourseStage::uneven, sim::GaitTask::walk,
                        sim::GuidanceMode::raw_policy_audit));
                const auto effective_action = rl::effective_policy_action(
                    environment, raw_action, sim::CourseStage::uneven, 0.0f,
                    sim::GuidanceMode::raw_policy_audit);
                result.evidence.raw_action_mismatches +=
                    masked_action_mismatches(environment,
                        sim::CourseStage::uneven, raw_action, effective_action);
                for (std::size_t action = 0;
                    action < sim::anatomy_action_count; ++action)
                {
                    result.evidence.raw_action_min[action] = std::min(
                        result.evidence.raw_action_min[action], raw_action[action]);
                    result.evidence.raw_action_max[action] = std::max(
                        result.evidence.raw_action_max[action], raw_action[action]);
                    result.evidence.effective_action_min[action] = std::min(
                        result.evidence.effective_action_min[action],
                        effective_action[action]);
                    result.evidence.effective_action_max[action] = std::max(
                        result.evidence.effective_action_max[action],
                        effective_action[action]);
                    result.evidence.command_trace_digest = append_fingerprint(
                        result.evidence.command_trace_digest, raw_action[action]);
                    result.evidence.command_trace_digest = append_fingerprint(
                        result.evidence.command_trace_digest,
                        effective_action[action]);
                }
                if (collect_actions)
                {
                    for (std::size_t action = 0;
                        action < sim::anatomy_action_count; ++action)
                    {
                        const double value = raw_action[action];
                        result.action_sum[action] += value;
                        result.action_square_sum[action] += value * value;
                        if (result.action_samples > 0u)
                            result.lag_moments[action].add(
                                value, result.prior_action[action]);
                        result.prior_action[action] = value;
                    }
                    result.contralateral_moments[0].add(
                        raw_action[0], raw_action[2]);
                    result.contralateral_moments[1].add(
                        raw_action[1], raw_action[3]);
                    ++result.action_samples;
                }

                std::array<float, sim::anatomy_action_count>
                    expected_actuator{};
                const float control_ramp = sim::policy_control_ramp(
                    environment.elapsed_seconds());
                for (std::size_t action = 0;
                    action < sim::anatomy_action_count; ++action)
                {
                    const float desired = clamp(effective_action[action],
                            -1.0f, 1.0f)
                        * control_ramp;
                    expected_actuator[action] = previous_actuator[action]
                        + clamp(desired - previous_actuator[action],
                            -dt, dt);
                }

                const sim::StepResult step = environment.step(effective_action);
                result.evidence.stationary_course =
                    result.evidence.stationary_course
                    && !environment.course_motion_enabled()
                    && environment.course_speed() == 0.0f
                    && environment.course_progress() == 0.0f;
                const sim::RawAuthorityTelemetry& authority =
                    environment.raw_authority_telemetry();
                const auto applications = [&](sim::AuthoritySource source)
                {
                    return authority.sources[static_cast<std::size_t>(source)]
                        .applications;
                };
                result.evidence.helper_applications +=
                    applications(sim::AuthoritySource::posture_helper)
                    + applications(sim::AuthoritySource::manipulator_helper)
                    + applications(
                        sim::AuthoritySource::ledge_training_scaffold)
                    + applications(sim::AuthoritySource::structure_projection);
                result.evidence.root_drive_corrections +=
                    authority.root_drive_corrections;
                result.evidence.torso_drive_corrections +=
                    authority.torso_drive_corrections;
                result.evidence.course_motion_corrections +=
                    authority.course_motion_corrections;
                result.evidence.powered_toe_commands +=
                    authority.powered_toe_commands;
                result.evidence.action_support_release_requests +=
                    authority.action_support_release_requests;
                result.evidence.action_support_release_active_frames +=
                    authority.action_support_release_active_frames;
                result.evidence.friction_work += authority.friction_work_joules;
                result.evidence.friction_positive_work +=
                    authority.friction_positive_work_joules;
                result.evidence.toe_constraint_applications += applications(
                    sim::AuthoritySource::articulated_toe);
                result.evidence.toe_rate_limit_applications += applications(
                    sim::AuthoritySource::toe_rate_limit);
                result.evidence.passive_anatomy_applications += applications(
                    sim::AuthoritySource::passive_anatomy);
                const auto& integration = authority.sources[
                    static_cast<std::size_t>(sim::AuthoritySource::integration)];
                result.evidence.integration_positive_horizontal_energy +=
                    integration.positive_horizontal_specific_energy;
                for (const sim::AuthoritySource source : internal_authority_sources)
                {
                    const auto& telemetry = authority.sources[
                        static_cast<std::size_t>(source)];
                    result.evidence.internal_center_delta_peak = std::max(
                        result.evidence.internal_center_delta_peak,
                        telemetry.maximum_absolute_center_velocity_delta_x);
                    result.evidence.internal_positive_horizontal_energy +=
                        telemetry.positive_horizontal_specific_energy;
                }
                for (std::size_t action = 0;
                    action < sim::anatomy_action_count; ++action)
                {
                    if (authority.commanded_actions[action]
                        != effective_action[action])
                    {
                        ++result.evidence.command_telemetry_mismatches;
                        ++result.evidence.command_mismatches_by_joint[action];
                    }
                    if (std::abs(authority.actuator_actions[action]
                            - expected_actuator[action]) > 1.0e-7f)
                    {
                        ++result.evidence.actuator_transition_mismatches;
                        ++result.evidence.actuator_mismatches_by_joint[action];
                    }
                    const sim::MotorDiagnostic motor =
                        environment.motor_diagnostic(action);
                    if (!motor.available || !motor.enabled
                        || motor.applied_action
                            != authority.actuator_actions[action])
                    {
                        ++result.evidence.motor_application_mismatches;
                        ++result.evidence.motor_mismatches_by_joint[action];
                    }
                    result.evidence.actuator_action_min[action] = std::min(
                        result.evidence.actuator_action_min[action],
                        authority.actuator_actions[action]);
                    result.evidence.actuator_action_max[action] = std::max(
                        result.evidence.actuator_action_max[action],
                        authority.actuator_actions[action]);
                    result.evidence.motor_action_min[action] = std::min(
                        result.evidence.motor_action_min[action],
                        motor.applied_action);
                    result.evidence.motor_action_max[action] = std::max(
                        result.evidence.motor_action_max[action],
                        motor.applied_action);
                    result.evidence.command_trace_digest = append_fingerprint(
                        result.evidence.command_trace_digest,
                        authority.actuator_actions[action]);
                    result.evidence.command_trace_digest = append_fingerprint(
                        result.evidence.command_trace_digest,
                        motor.applied_action);
                    previous_actuator[action] =
                        authority.actuator_actions[action];
                }
                for (std::size_t side = 0; side < 2u; ++side)
                {
                    if (authority.support_traction_used[side]
                        > authority.support_traction_capacity[side] + 1.0e-5f)
                        ++result.evidence.traction_limit_violations;
                }

                const auto particles = environment.particles();
                const std::uint16_t root = environment.blueprint().root_node;
                const float pelvis_velocity = (particles[root].position.x
                    - previous_pelvis_x) / dt;
                const float pelvis_delta = std::abs(pelvis_velocity
                    - previous_pelvis_velocity);
                pelvis_delta_sum += pelvis_delta;
                result.evidence.pelvis_delta_peak = std::max(
                    result.evidence.pelvis_delta_peak, pelvis_delta);
                previous_pelvis_x = particles[root].position.x;
                previous_pelvis_velocity = pelvis_velocity;

                const Vec2 center = mass_center(environment);
                const float center_velocity = (center.x
                    - previous_center.x) / dt;
                const float center_delta = std::abs(center_velocity
                    - previous_center_velocity);
                center_delta_sum += center_delta;
                result.evidence.center_delta_peak = std::max(
                    result.evidence.center_delta_peak, center_delta);
                const float ground_impulse = authority.sources[
                    static_cast<std::size_t>(sim::AuthoritySource::ground_contact)]
                        .center_velocity_delta_x;
                const float course_impulse = authority.sources[
                    static_cast<std::size_t>(sim::AuthoritySource::course_collision)]
                        .center_velocity_delta_x;
                const bool contact_causal = std::abs(ground_impulse) > 1.0e-5f
                    || std::abs(course_impulse) > 1.0e-5f;
                const float directed_velocity = center_velocity * direction;
                const float previous_directed_velocity =
                    previous_center_velocity * direction;
                const bool directed_energy_increased = directed_velocity > 0.0f
                    && directed_velocity
                        > std::max(0.0f, previous_directed_velocity) + 0.025f;
                if (directed_energy_increased && !contact_causal)
                    ++result.evidence.unsupported_forward_impulses;
                previous_center = center;
                previous_center_velocity = center_velocity;

                result.evidence.torso_turn_rate_peak = std::max(
                    result.evidence.torso_turn_rate_peak,
                    std::abs(environment.observation()[
                        sim::torso_turn_rate_observation]) * 6.0f);
                result.evidence.stance_slip_peak = std::max(
                    result.evidence.stance_slip_peak,
                    environment.stance_slip_speed());
                const bool left = environment.left_supported();
                const bool right = environment.right_supported();
                if (left != previous_left_support
                    || right != previous_right_support)
                    ++result.evidence.contact_transitions;
                result.evidence.contact_trace_digest = append_fingerprint(
                    result.evidence.contact_trace_digest,
                    static_cast<float>((left ? 1u : 0u)
                        | (right ? 2u : 0u)));
                previous_left_support = left;
                previous_right_support = right;
                if (left != right)
                {
                    result.evidence.left_only_support_frames += left ? 1u : 0u;
                    result.evidence.right_only_support_frames += right ? 1u : 0u;
                }
                float support_x{};
                std::uint32_t support_count{};
                for (std::size_t node = 0; node < particles.size(); ++node)
                {
                    if (!environment.blueprint().is_support_seed(node)
                        || !particles[node].grounded)
                        continue;
                    support_x += particles[node].position.x;
                    ++support_count;
                }
                if (support_count > 0u)
                {
                    result.evidence.support_lead_peak = std::max(
                        result.evidence.support_lead_peak,
                        std::abs(center.x - support_x
                            / static_cast<float>(support_count)));
                }
                ++frames;
                if (step.terminated)
                    break;
            }
            result.evidence.distance = environment.distance_travelled();
            result.evidence.gait_cycles = environment.gait_cycles();
            result.evidence.alternating_steps = environment.alternating_steps();
            result.evidence.pelvis_delta_mean = static_cast<float>(
                pelvis_delta_sum / std::max<std::uint32_t>(frames, 1u));
            result.evidence.center_delta_mean = static_cast<float>(
                center_delta_sum / std::max<std::uint32_t>(frames, 1u));
            result.evidence.invalid_reason = environment.invalid_reason();
            result.evidence.body_integrity = environment.body_integrity_valid();
            result.evidence.trajectory_fingerprint =
                trajectory_fingerprint(environment);
            result.evidence.passed = environment.valid_motion()
                && result.evidence.body_integrity
                && result.evidence.invalid_reason == sim::InvalidMotion::none
                && result.evidence.distance * direction >= 10.0f
                && result.evidence.gait_cycles >= 12u
                && result.evidence.alternating_steps >= 12u
                && result.evidence.pelvis_delta_mean <= 0.18f
                && result.evidence.pelvis_delta_peak <= 1.0f
                && result.evidence.center_delta_mean <= 0.12f
                && result.evidence.center_delta_peak <= 0.75f
                && result.evidence.torso_turn_rate_peak <= 2.5f
                && result.evidence.support_lead_peak <= 0.55f
                && result.evidence.stance_slip_peak <= 1.5f
                && result.evidence.unsupported_forward_impulses == 0u
                && result.evidence.left_only_support_frames > 0u
                && result.evidence.right_only_support_frames > 0u
                && result.evidence.contact_transitions > 0u
                && result.evidence.raw_action_mismatches == 0u
                && result.evidence.helper_applications == 0u
                && result.evidence.root_drive_corrections == 0u
                && result.evidence.torso_drive_corrections == 0u
                && result.evidence.course_motion_corrections == 0u
                && result.evidence.powered_toe_commands == 0u
                && result.evidence.traction_limit_violations == 0u
                && result.evidence.friction_positive_work <= 1.0e-6f
                && result.evidence.internal_center_delta_peak <= 5.0e-5f
                && result.evidence.internal_positive_horizontal_energy
                    <= 1.0e-6f
                && result.evidence.integration_positive_horizontal_energy
                    <= 1.0e-6f
                && result.evidence.toe_constraint_applications == 0u
                && result.evidence.toe_rate_limit_applications == 0u
                && result.evidence.action_support_release_requests == 0u
                && result.evidence.action_support_release_active_frames == 0u
                && result.evidence.command_telemetry_mismatches == 0u
                && result.evidence.actuator_transition_mismatches == 0u
                && result.evidence.motor_application_mismatches == 0u
                && result.evidence.stationary_course;
            return result;
        }

        [[nodiscard]] RawStopSeedEvidence replay_stop_network(
            const rl::PolicyNetwork& policy, std::uint64_t seed)
        {
            constexpr float dt = 1.0f / 60.0f;
            sim::Environment environment{
                sim::CreatureBlueprint::humanoid(), seed };
            environment.set_course(sim::CourseStage::uneven, 0.30f);
            environment.set_course_motion_enabled(false);
            environment.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
            environment.set_raw_authority_telemetry_enabled(true);

            RawStopSeedEvidence evidence{};
            evidence.seed = seed;
            evidence.requested_direction = 0.0f;
            evidence.command_trace_digest = 1469598103934665603ULL;
            evidence.contact_trace_digest = 1469598103934665603ULL;
            evidence.finite_motion = true;
            evidence.stationary_course = !environment.course_motion_enabled()
                && environment.course_speed() == 0.0f
                && environment.course_progress() == 0.0f;
            if (!environment.set_locomotion_intent(0.0f, true))
                return evidence;
            evidence.stop_requested = environment.locomotion_stop_requested()
                && environment.locomotion_direction() == 0.0f;

            const Vec2 initial_center = mass_center(environment);
            Vec2 previous_center = initial_center;
            for (std::uint32_t frame = 0u;
                frame < raw_stop_replay_frames; ++frame)
            {
                const auto raw_action = policy.deterministic_action(
                    rl::foundational_policy_observation(environment,
                        sim::CourseStage::uneven, sim::GaitTask::walk,
                        sim::GuidanceMode::raw_policy_audit));
                const auto effective_action = rl::effective_policy_action(
                    environment, raw_action, sim::CourseStage::uneven, 0.0f,
                    sim::GuidanceMode::raw_policy_audit);
                evidence.raw_action_mismatches += masked_action_mismatches(
                    environment, sim::CourseStage::uneven,
                    raw_action, effective_action);
                for (std::size_t action = 0;
                    action < sim::action_count; ++action)
                {
                    evidence.command_trace_digest = append_fingerprint(
                        evidence.command_trace_digest, raw_action[action]);
                    evidence.command_trace_digest = append_fingerprint(
                        evidence.command_trace_digest, effective_action[action]);
                }

                const sim::StepResult step = environment.step(effective_action);
                ++evidence.completed_frames;
                evidence.stop_requested = evidence.stop_requested
                    && environment.locomotion_stop_requested()
                    && environment.locomotion_direction() == 0.0f;
                evidence.stationary_course = evidence.stationary_course
                    && !environment.course_motion_enabled()
                    && environment.course_speed() == 0.0f
                    && environment.course_progress() == 0.0f;

                const sim::RawAuthorityTelemetry& authority =
                    environment.raw_authority_telemetry();
                const auto applications = [&](sim::AuthoritySource source)
                {
                    return authority.sources[static_cast<std::size_t>(source)]
                        .applications;
                };
                evidence.optional_authority_applications +=
                    applications(sim::AuthoritySource::posture_helper)
                    + applications(sim::AuthoritySource::manipulator_helper)
                    + applications(
                        sim::AuthoritySource::ledge_training_scaffold)
                    + applications(sim::AuthoritySource::structure_projection)
                    + applications(sim::AuthoritySource::equipment)
                    + applications(sim::AuthoritySource::articulated_toe)
                    + applications(sim::AuthoritySource::toe_rate_limit)
                    + authority.root_drive_corrections
                    + authority.torso_drive_corrections
                    + authority.course_motion_corrections
                    + authority.powered_toe_commands
                    + authority.action_support_release_requests
                    + authority.action_support_release_active_frames;

                const Vec2 center = mass_center(environment);
                const float horizontal_speed = std::abs(
                    (center.x - previous_center.x) / dt);
                const float horizontal_drift = std::abs(
                    center.x - initial_center.x);
                evidence.finite_motion = evidence.finite_motion
                    && std::isfinite(center.x)
                    && std::isfinite(horizontal_speed)
                    && std::isfinite(horizontal_drift);
                if (evidence.finite_motion)
                {
                    evidence.maximum_horizontal_speed = std::max(
                        evidence.maximum_horizontal_speed, horizontal_speed);
                    evidence.maximum_horizontal_drift = std::max(
                        evidence.maximum_horizontal_drift, horizontal_drift);
                }
                previous_center = center;

                const bool non_foot_contact = environment.non_foot_grounded();
                evidence.non_foot_contact_frames += non_foot_contact ? 1u : 0u;
                evidence.contact_trace_digest = append_fingerprint(
                    evidence.contact_trace_digest,
                    static_cast<float>((environment.left_supported() ? 1u : 0u)
                        | (environment.right_supported() ? 2u : 0u)
                        | (non_foot_contact ? 4u : 0u)));
                if (step.terminated)
                    break;
            }

            evidence.gait_cycles = environment.gait_cycles();
            evidence.alternating_steps = environment.alternating_steps();
            evidence.invalid_reason = environment.invalid_reason();
            evidence.body_integrity = environment.body_integrity_valid();
            evidence.trajectory_fingerprint = trajectory_fingerprint(environment);
            return evidence;
        }

        struct ControlOutcome
        {
            float maximum_distance{};
            std::uint32_t maximum_cycles{};
            std::uint32_t passes{};
        };

        [[nodiscard]] ControlOutcome replay_control(float constant_action)
        {
            ControlOutcome outcome{};
            std::array<float, sim::action_count> action{};
            action.fill(constant_action);
            for (const std::uint64_t seed : raw_inference_replay_seeds)
            {
                sim::Environment environment{
                    sim::CreatureBlueprint::humanoid(), seed };
                environment.set_course(sim::CourseStage::uneven, 0.30f);
                environment.set_course_motion_enabled(false);
                environment.set_guidance_mode(
                    sim::GuidanceMode::raw_policy_audit);
                for (int frame = 0; frame < 1200; ++frame)
                {
                    if (environment.step(action).terminated)
                        break;
                }
                outcome.maximum_distance = std::max(outcome.maximum_distance,
                    environment.distance_travelled());
                outcome.maximum_cycles = std::max(outcome.maximum_cycles,
                    environment.gait_cycles());
                if (environment.valid_motion()
                    && environment.body_integrity_valid()
                    && environment.distance_travelled() >= 10.0f
                    && environment.gait_cycles() >= 12u)
                    ++outcome.passes;
            }
            return outcome;
        }
    }

    bool raw_stop_seed_qualifies(
        const RawStopSeedEvidence& evidence) noexcept
    {
        return evidence.requested_direction == 0.0f
            && evidence.stop_requested
            && evidence.finite_motion
            && evidence.maximum_horizontal_drift
                < raw_stop_maximum_horizontal_drift
            && evidence.maximum_horizontal_speed
                < raw_stop_maximum_horizontal_speed
            && evidence.completed_frames == raw_stop_replay_frames
            && evidence.gait_cycles == 0u
            && evidence.alternating_steps == 0u
            && evidence.non_foot_contact_frames == 0u
            && evidence.raw_action_mismatches == 0u
            && evidence.optional_authority_applications == 0u
            && evidence.invalid_reason == sim::InvalidMotion::none
            && evidence.body_integrity
            && evidence.stationary_course
            && evidence.deterministic_repeat;
    }

    RawCandidateEvaluationEvidence capture_raw_candidate_evaluation(
        std::uint64_t update, const rl::TrainingMetrics& metrics)
    {
        return RawCandidateEvaluationEvidence{
            .present = true,
            .update = update,
            .valid = metrics.evaluation_valid,
            .invalid_runs = metrics.evaluation_invalid_runs,
            .invalid_reason = metrics.evaluation_invalid_reason,
            .rejection_mask = metrics.evaluation_rejection_mask,
            .rejection_summary = rl::motion_rejection_summary(
                metrics.evaluation_rejection_mask),
            .distance = metrics.evaluation_distance,
            .stride_events = metrics.evaluation_stride_events,
            .survival = metrics.evaluation_survival,
            .quality_key = metrics.evaluation_quality_key
        };
    }

    RawInferenceReport run_raw_inference_diagnostic(std::uint64_t updates)
    {
        RawInferenceReport report{};
        report.training_seed = 0x751C01Du;
        report.requested_updates = std::max<std::uint64_t>(updates, 1u);
        const sim::CreatureBlueprint blueprint =
            sim::CreatureBlueprint::humanoid();
        report.handoff_update =
            rl::foundational_walk_teacher_handoff_update(blueprint);

        rl::PpoTrainer trainer{ blueprint, 8u, true, 8u };
        trainer.reset_policy(report.training_seed, true);
        trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
        std::uint64_t prior_evaluation_count =
            trainer.metrics().evaluation_count;
        for (std::uint64_t update = 0; update < report.requested_updates; ++update)
        {
            trainer.train_one_update();
            const rl::TrainingMetrics& metrics = trainer.metrics();
            if (raw_candidate_evaluation_observed(prior_evaluation_count,
                    metrics.evaluation_count, trainer.lesson_update(),
                    report.handoff_update))
            {
                RawCandidateEvaluationEvidence evidence =
                    capture_raw_candidate_evaluation(
                        trainer.lesson_update(), metrics);
                if (!report.first_raw_candidate.present)
                    report.first_raw_candidate = evidence;
                report.last_raw_candidate = std::move(evidence);
            }
            prior_evaluation_count = metrics.evaluation_count;
        }
        report.completed_updates = trainer.lesson_update();
        report.retained_policy = trainer.has_best_policy();
        report.retained_update = trainer.metrics().best_update;
        report.retained_guidance_mode = trainer.best_policy_guidance_mode();
        const rl::RawPpoReceipt& receipt = trainer.raw_ppo_receipt();
        report.raw_receipt_digest = receipt.provenance_digest();
        report.raw_receipt_optimizer_update = receipt.optimizer_update();
        report.raw_receipt_sample_count = receipt.expected_sample_count();
        report.retained_parameter_digest = rl::parameter_digest(
            trainer.best_policy_parameters());
        const rl::PpoTrainer::VerifiedCheckpoint live_artifact =
            trainer.verified_checkpoint();
        const rl::PpoTrainer::CheckpointData& live_checkpoint =
            live_artifact.data();
        const rl::ActionMask live_mask = [&live_checkpoint]() noexcept
        {
            rl::ActionMask mask{};
            for (std::size_t index = 0; index < mask.size(); ++index)
                mask[index] = (live_checkpoint.action_mask_bits
                    & (std::uint32_t{ 1u } << index)) != 0u;
            return mask;
        }();
        const rl::RawPpoReceiptContext live_context{
            .training_semantics = live_checkpoint.training_semantics,
            .rig_signature = live_checkpoint.rig_signature,
            .best_update = live_checkpoint.metrics.best_update,
            .checkpoint_optimizer_step = live_checkpoint.optimizer_step,
            .retained_parameters_digest = live_checkpoint.best_parameters_digest,
            .retained_active_actor_digest =
                rl::PolicyNetwork::active_actor_parameter_digest(
                    live_checkpoint.best_parameters, live_mask),
            .action_mask_bits = live_checkpoint.action_mask_bits,
            .stage = static_cast<std::uint8_t>(live_checkpoint.stage),
            .gait_task = static_cast<std::uint8_t>(live_checkpoint.gait_task),
            .equipment_directive = static_cast<std::uint8_t>(
                live_checkpoint.equipment_directive)
        };
        report.raw_receipt_verified = rl::raw_ppo_receipt_valid(
            receipt, live_context);
        report.post_handoff_update = report.raw_receipt_verified
            && receipt.optimizer_update() > report.handoff_update
            && receipt.rollout_lesson_update() >= report.handoff_update
            && report.completed_updates > report.handoff_update;

        std::vector<float> immutable_parameters{};
        if (report.retained_policy)
        {
            rl::PpoTrainer restored{ blueprint, 1u, false };
            std::string error{};
            const std::filesystem::path checkpoint_path =
                std::filesystem::temp_directory_path()
                / "epoch2dwalk-v0751-raw-inference.eppo";
            std::error_code filesystem_error{};
            std::filesystem::remove(checkpoint_path, filesystem_error);
            rl::PpoTrainer::VerifiedCheckpoint stored_artifact{};
            report.checkpoint_truthful = trainer.save_checkpoint(
                    checkpoint_path, error)
                && rl::PpoTrainer::read_verified_checkpoint(
                    checkpoint_path, stored_artifact, error)
                && restored.apply_verified_checkpoint(stored_artifact, error)
                && restored.has_best_policy()
                && restored.best_policy_guidance_mode()
                    == live_checkpoint.best_guidance_mode
                && restored.best_policy_parameters()
                    == trainer.best_policy_parameters()
                && restored.raw_ppo_receipt().provenance_digest()
                    == receipt.provenance_digest();
            std::filesystem::remove(checkpoint_path, filesystem_error);
            if (report.checkpoint_truthful)
                immutable_parameters = restored.best_policy_parameters();
        }
        report.retained_parameter_fingerprint = parameter_fingerprint(
            immutable_parameters);

        std::array<double, sim::anatomy_action_count> action_sum{};
        std::array<double, sim::anatomy_action_count> action_square_sum{};
        std::array<CorrelationMoments,
            sim::anatomy_action_count> lag_moments{};
        std::array<CorrelationMoments, 2> contralateral_moments{};
        std::uint64_t action_samples{};
        report.raw_action_identity = report.checkpoint_truthful;
        report.no_optional_authority = report.checkpoint_truthful;
        report.opposing_support_phase = report.checkpoint_truthful;
        report.reverse_direction_evidence = report.checkpoint_truthful;
        report.stop_behavior_evidence = report.checkpoint_truthful;
        if (report.checkpoint_truthful)
        {
            rl::PolicyNetwork policy{ 0x751A11u };
            policy.parameters() = immutable_parameters;
            rl::PolicyNetwork ablated_policy{ 0x751AB1Au };
            ablated_policy.parameters() = immutable_parameters;
            for (std::size_t action = 0;
                action < sim::anatomy_action_count; ++action)
                ablated_policy.neutralize_action_slot(action);
            for (std::size_t index = 0; index < report.seeds.size(); ++index)
            {
                ReplayAccumulation replay = replay_network(
                    policy, raw_inference_replay_seeds[index], true);
                const ReplayAccumulation repeated = replay_network(
                    policy, raw_inference_replay_seeds[index], false);
                replay.evidence.deterministic_repeat =
                    replay.evidence.trajectory_fingerprint
                        == repeated.evidence.trajectory_fingerprint
                    && replay.evidence.command_trace_digest
                        == repeated.evidence.command_trace_digest
                    && replay.evidence.contact_trace_digest
                        == repeated.evidence.contact_trace_digest
                    && replay.evidence.distance == repeated.evidence.distance
                    && replay.evidence.gait_cycles
                        == repeated.evidence.gait_cycles
                    && replay.evidence.invalid_reason
                        == repeated.evidence.invalid_reason;
                replay.evidence.passed = replay.evidence.passed
                    && replay.evidence.deterministic_repeat;
                report.raw_action_identity = report.raw_action_identity
                    && replay.evidence.raw_action_mismatches == 0u;
                report.no_optional_authority = report.no_optional_authority
                    && replay.evidence.helper_applications == 0u
                    && replay.evidence.root_drive_corrections == 0u
                    && replay.evidence.torso_drive_corrections == 0u
                    && replay.evidence.course_motion_corrections == 0u
                    && replay.evidence.powered_toe_commands == 0u
                    && replay.evidence.toe_constraint_applications == 0u
                    && replay.evidence.toe_rate_limit_applications == 0u
                    && replay.evidence.action_support_release_requests == 0u
                    && replay.evidence.action_support_release_active_frames == 0u
                    && replay.evidence.command_telemetry_mismatches == 0u
                    && replay.evidence.actuator_transition_mismatches == 0u
                    && replay.evidence.motor_application_mismatches == 0u
                    && replay.evidence.internal_center_delta_peak <= 5.0e-5f
                    && replay.evidence.internal_positive_horizontal_energy
                        <= 1.0e-6f
                    && replay.evidence.integration_positive_horizontal_energy
                        <= 1.0e-6f
                    && replay.evidence.friction_positive_work <= 1.0e-6f
                    && replay.evidence.stationary_course;
                report.opposing_support_phase = report.opposing_support_phase
                    && replay.evidence.left_only_support_frames > 0u
                    && replay.evidence.right_only_support_frames > 0u;
                for (std::size_t action = 0;
                    action < sim::anatomy_action_count; ++action)
                {
                    action_sum[action] += replay.action_sum[action];
                    action_square_sum[action] +=
                        replay.action_square_sum[action];
                    lag_moments[action].merge(replay.lag_moments[action]);
                }
                for (std::size_t pair = 0;
                    pair < contralateral_moments.size(); ++pair)
                {
                    contralateral_moments[pair].merge(
                        replay.contralateral_moments[pair]);
                }
                action_samples += replay.action_samples;
                report.seeds[index] = replay.evidence;

                ReplayAccumulation reverse = replay_network(
                    policy, raw_inference_replay_seeds[index], false, -1.0f);
                const ReplayAccumulation repeated_reverse = replay_network(
                    policy, raw_inference_replay_seeds[index], false, -1.0f);
                reverse.evidence.deterministic_repeat =
                    reverse.evidence.trajectory_fingerprint
                        == repeated_reverse.evidence.trajectory_fingerprint
                    && reverse.evidence.command_trace_digest
                        == repeated_reverse.evidence.command_trace_digest
                    && reverse.evidence.contact_trace_digest
                        == repeated_reverse.evidence.contact_trace_digest
                    && reverse.evidence.distance
                        == repeated_reverse.evidence.distance
                    && reverse.evidence.gait_cycles
                        == repeated_reverse.evidence.gait_cycles
                    && reverse.evidence.invalid_reason
                        == repeated_reverse.evidence.invalid_reason;
                reverse.evidence.passed = reverse.evidence.passed
                    && reverse.evidence.deterministic_repeat;
                report.reverse_direction_evidence =
                    report.reverse_direction_evidence
                    && reverse.evidence.requested_direction == -1.0f
                    && reverse.evidence.contact_transitions > 0u
                    && reverse.evidence.passed;
                report.reverse_seeds[index] = reverse.evidence;

                RawStopSeedEvidence stop = replay_stop_network(
                    policy, raw_inference_replay_seeds[index]);
                const RawStopSeedEvidence repeated_stop = replay_stop_network(
                    policy, raw_inference_replay_seeds[index]);
                stop.deterministic_repeat =
                    stop.requested_direction == repeated_stop.requested_direction
                    && stop.maximum_horizontal_drift
                        == repeated_stop.maximum_horizontal_drift
                    && stop.maximum_horizontal_speed
                        == repeated_stop.maximum_horizontal_speed
                    && stop.completed_frames == repeated_stop.completed_frames
                    && stop.gait_cycles == repeated_stop.gait_cycles
                    && stop.alternating_steps == repeated_stop.alternating_steps
                    && stop.non_foot_contact_frames
                        == repeated_stop.non_foot_contact_frames
                    && stop.raw_action_mismatches
                        == repeated_stop.raw_action_mismatches
                    && stop.optional_authority_applications
                        == repeated_stop.optional_authority_applications
                    && stop.invalid_reason == repeated_stop.invalid_reason
                    && stop.command_trace_digest
                        == repeated_stop.command_trace_digest
                    && stop.contact_trace_digest
                        == repeated_stop.contact_trace_digest
                    && stop.trajectory_fingerprint
                        == repeated_stop.trajectory_fingerprint
                    && stop.stop_requested == repeated_stop.stop_requested
                    && stop.finite_motion == repeated_stop.finite_motion
                    && stop.body_integrity == repeated_stop.body_integrity
                    && stop.stationary_course
                        == repeated_stop.stationary_course;
                stop.passed = raw_stop_seed_qualifies(stop);
                report.raw_action_identity = report.raw_action_identity
                    && stop.raw_action_mismatches == 0u;
                report.no_optional_authority = report.no_optional_authority
                    && stop.optional_authority_applications == 0u;
                report.stop_behavior_evidence =
                    report.stop_behavior_evidence && stop.passed;
                report.stop_seeds[index] = stop;

                const ReplayAccumulation ablated = replay_network(
                    ablated_policy, raw_inference_replay_seeds[index], false);
                report.weight_ablation_max_distance = std::max(
                    report.weight_ablation_max_distance,
                    ablated.evidence.distance);
                report.weight_ablation_max_cycles = std::max(
                    report.weight_ablation_max_cycles,
                    ablated.evidence.gait_cycles);
                if (ablated.evidence.passed)
                    ++report.weight_ablation_passes;
                report.weight_ablation_changes_trajectory =
                    report.weight_ablation_changes_trajectory
                    || ablated.evidence.trajectory_fingerprint
                        != replay.evidence.trajectory_fingerprint;
            }
            report.replay_parameter_fingerprint = parameter_fingerprint(
                policy.parameters());
            report.parameter_fingerprint_immutable =
                report.replay_parameter_fingerprint
                    == report.retained_parameter_fingerprint
                && policy.parameters() == immutable_parameters;
        }

        report.all_joint_outputs_vary = action_samples > 0u;
        for (std::size_t action = 0;
            action < sim::anatomy_action_count; ++action)
        {
            const double mean = action_samples > 0u
                ? action_sum[action] / static_cast<double>(action_samples)
                : 0.0;
            report.action_variance[action] = action_samples > 0u
                ? static_cast<float>(action_square_sum[action]
                    / static_cast<double>(action_samples) - mean * mean)
                : 0.0f;
            report.lag_one_correlation[action] =
                lag_moments[action].correlation();
            report.all_joint_outputs_vary = report.all_joint_outputs_vary
                && report.action_variance[action] >= 1.0e-5f;
        }
        report.contralateral_leg_correlation = 0.5f
            * (contralateral_moments[0].correlation()
                + contralateral_moments[1].correlation());
        report.opposing_action_phase = report.contralateral_leg_correlation
            < -1.0e-4f;

        const ControlOutcome zero = replay_control(0.0f);
        const ControlOutcome fixed = replay_control(0.25f);
        report.zero_control_max_distance = zero.maximum_distance;
        report.zero_control_max_cycles = zero.maximum_cycles;
        report.zero_control_passes = zero.passes;
        report.fixed_control_max_distance = fixed.maximum_distance;
        report.fixed_control_max_cycles = fixed.maximum_cycles;
        report.fixed_control_passes = fixed.passes;
        report.controls_fail = zero.passes == 0u && fixed.passes == 0u
            && report.weight_ablation_passes == 0u
            && report.weight_ablation_changes_trajectory;

        report.passed = report.retained_policy
            && report.retained_guidance_mode
                == sim::GuidanceMode::raw_policy_audit
            && report.post_handoff_update
            && report.raw_receipt_verified
            && report.checkpoint_truthful
            && report.parameter_fingerprint_immutable
            && report.raw_action_identity
            && report.all_joint_outputs_vary
            && report.opposing_action_phase
            && report.opposing_support_phase
            && report.reverse_direction_evidence
            && report.stop_behavior_evidence
            && report.no_optional_authority
            && report.controls_fail
            && std::ranges::all_of(report.seeds,
                [](const RawInferenceSeedEvidence& seed)
                {
                    return seed.passed;
                })
            && std::ranges::all_of(report.stop_seeds,
                [](const RawStopSeedEvidence& seed)
                {
                    return seed.passed;
                });
        return report;
    }

    void write_raw_inference_report(const RawInferenceReport& report,
        std::ostream& output)
    {
        const auto write_candidate = [&output](std::string_view label,
            const RawCandidateEvaluationEvidence& evidence)
        {
            output << label << " present=" << evidence.present;
            if (evidence.present)
            {
                output << " update=" << evidence.update
                    << " valid=" << evidence.valid
                    << " invalid_runs=" << evidence.invalid_runs
                    << " invalid="
                        << sim::invalid_motion_name(evidence.invalid_reason)
                    << " rejection_mask=0x" << std::hex
                        << evidence.rejection_mask << std::dec
                    << " rejection=\"" << evidence.rejection_summary << '"'
                    << " distance=" << evidence.distance
                    << " strides=" << evidence.stride_events
                    << " survival=" << evidence.survival
                    << " quality=0x" << std::hex << evidence.quality_key
                        << std::dec;
            }
            output << '\n';
        };
        output << std::fixed << std::setprecision(6)
            << "RAW RETAINED NETWORK: "
            << (report.passed ? "PASS" : "FAIL")
            << " training_seed=0x" << std::hex << report.training_seed
            << std::dec << " updates=" << report.completed_updates
            << " retained_update=" << report.retained_update
            << " handoff=" << report.handoff_update
            << " retained_mode="
            << (report.retained_guidance_mode
                    == sim::GuidanceMode::raw_policy_audit
                ? "RAW" : "ASSISTED")
             << " checkpoint_truthful=" << report.checkpoint_truthful
            << " parameters=0x" << std::hex
                << report.retained_parameter_fingerprint << "/0x"
                << report.replay_parameter_fingerprint << std::dec
            << " parameter_digest=0x" << std::hex
                << report.retained_parameter_digest
            << " receipt=0x" << report.raw_receipt_digest << std::dec
            << " receipt_update=" << report.raw_receipt_optimizer_update
            << " receipt_samples=" << report.raw_receipt_sample_count
            << " receipt_verified=" << report.raw_receipt_verified
            << " parameters_immutable="
                << report.parameter_fingerprint_immutable
            << " raw_effective_identity=" << report.raw_action_identity
            << " reverse_evidence=" << report.reverse_direction_evidence
            << " stop_evidence=" << report.stop_behavior_evidence
            << " helpers_zero=" << report.no_optional_authority << '\n';
        write_candidate("raw_candidate_first", report.first_raw_candidate);
        write_candidate("raw_candidate_last", report.last_raw_candidate);
        if (!report.retained_policy)
        {
            output << "replay_skipped_no_retained_policy=1\n";
            return;
        }
        if (!report.checkpoint_truthful)
        {
            output << "replay_skipped_unverified_retained_policy=1\n";
            return;
        }
        const auto write_seeds = [&output](std::string_view label,
            const auto& seeds)
        {
            for (const RawInferenceSeedEvidence& seed : seeds)
            {
                output << label << " seed=0x" << std::hex << seed.seed
                    << std::dec << " direction=" << seed.requested_direction
                    << " pass=" << seed.passed
                    << " deterministic=" << seed.deterministic_repeat
                    << " distance=" << seed.distance
                    << " cycles=" << seed.gait_cycles
                    << " alternating=" << seed.alternating_steps
                    << " pelvis_dv=" << seed.pelvis_delta_mean << '/'
                        << seed.pelvis_delta_peak
                    << " com_dv=" << seed.center_delta_mean << '/'
                        << seed.center_delta_peak
                    << " torso_rate=" << seed.torso_turn_rate_peak
                    << " support_lead=" << seed.support_lead_peak
                    << " slip=" << seed.stance_slip_peak
                    << " support_phase=" << seed.left_only_support_frames
                        << '/' << seed.right_only_support_frames
                    << " contact_transitions=" << seed.contact_transitions
                    << " contact_trace=0x" << std::hex
                        << seed.contact_trace_digest
                    << " command_trace=0x" << seed.command_trace_digest
                        << std::dec
                    << " unsupported=" << seed.unsupported_forward_impulses
                    << " internal_com_dv=" << seed.internal_center_delta_peak
                    << " internal_positive_energy="
                        << seed.internal_positive_horizontal_energy
                    << " integration_positive_energy="
                        << seed.integration_positive_horizontal_energy
                    << " helper_apps=" << seed.helper_applications
                    << " passive_apps=" << seed.passive_anatomy_applications
                    << " root_drive=" << seed.root_drive_corrections
                    << " torso_drive=" << seed.torso_drive_corrections
                    << " course_drive=" << seed.course_motion_corrections
                    << " toe_drive=" << seed.powered_toe_commands
                    << " toe_constraint=" << seed.toe_constraint_applications
                    << " toe_rate_limit=" << seed.toe_rate_limit_applications
                    << " support_release="
                        << seed.action_support_release_requests << '/'
                        << seed.action_support_release_active_frames
                    << " command_mismatch="
                        << seed.command_telemetry_mismatches
                    << " actuator_transition_mismatch="
                        << seed.actuator_transition_mismatches
                    << " motor_application_mismatch="
                        << seed.motor_application_mismatches
                    << " traction_violations=" << seed.traction_limit_violations
                    << " friction_work=" << seed.friction_work
                    << " friction_positive=" << seed.friction_positive_work
                    << " stationary_course=" << seed.stationary_course
                    << " invalid=" << sim::invalid_motion_name(seed.invalid_reason)
                    << '\n';
                for (std::size_t joint = 0;
                    joint < sim::anatomy_action_count; ++joint)
                {
                    output << label << " joint=" << joint
                        << " raw=" << seed.raw_action_min[joint] << '/'
                            << seed.raw_action_max[joint]
                        << " effective=" << seed.effective_action_min[joint]
                            << '/' << seed.effective_action_max[joint]
                        << " actuator=" << seed.actuator_action_min[joint]
                            << '/' << seed.actuator_action_max[joint]
                        << " motor=" << seed.motor_action_min[joint]
                            << '/' << seed.motor_action_max[joint]
                        << " mismatch="
                            << seed.command_mismatches_by_joint[joint] << '/'
                            << seed.actuator_mismatches_by_joint[joint] << '/'
                            << seed.motor_mismatches_by_joint[joint] << '\n';
                }
            }
        };
        write_seeds("forward", report.seeds);
        write_seeds("reverse", report.reverse_seeds);
        for (const RawStopSeedEvidence& seed : report.stop_seeds)
        {
            output << "stop seed=0x" << std::hex << seed.seed << std::dec
                << " direction=" << seed.requested_direction
                << " requested=" << seed.stop_requested
                << " pass=" << seed.passed
                << " deterministic=" << seed.deterministic_repeat
                << " frames=" << seed.completed_frames
                << " drift=" << seed.maximum_horizontal_drift << "/<"
                    << raw_stop_maximum_horizontal_drift
                << " speed=" << seed.maximum_horizontal_speed << "/<"
                    << raw_stop_maximum_horizontal_speed
                << " cycles=" << seed.gait_cycles
                << " alternating=" << seed.alternating_steps
                << " non_foot_contact=" << seed.non_foot_contact_frames
                << " raw_effective_mismatch=" << seed.raw_action_mismatches
                << " optional_authority="
                    << seed.optional_authority_applications
                << " stationary_course=" << seed.stationary_course
                << " finite=" << seed.finite_motion
                << " body_integrity=" << seed.body_integrity
                << " invalid=" << sim::invalid_motion_name(seed.invalid_reason)
                << " contact_trace=0x" << std::hex
                    << seed.contact_trace_digest
                << " command_trace=0x" << seed.command_trace_digest
                << " trajectory=0x" << seed.trajectory_fingerprint
                    << std::dec << '\n';
        }
        output << "joint_variance=";
        for (std::size_t index = 0; index < report.action_variance.size(); ++index)
            output << (index == 0u ? "" : ",") << report.action_variance[index];
        output << " lag1=";
        for (std::size_t index = 0;
            index < report.lag_one_correlation.size(); ++index)
            output << (index == 0u ? "" : ",")
                << report.lag_one_correlation[index];
        output << " contralateral=" << report.contralateral_leg_correlation
            << " controls_zero=" << report.zero_control_max_distance << '/'
                << report.zero_control_max_cycles << '/'
                << report.zero_control_passes
            << " controls_fixed=" << report.fixed_control_max_distance << '/'
                << report.fixed_control_max_cycles << '/'
                << report.fixed_control_passes
            << " weights_ablated=" << report.weight_ablation_max_distance << '/'
                << report.weight_ablation_max_cycles << '/'
                << report.weight_ablation_passes
            << " ablation_changed="
                << report.weight_ablation_changes_trajectory << '\n';
    }
}
