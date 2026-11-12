#include "ppo.hpp"
#include "raw_inference_diagnostic.hpp"
#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static std::array<float, 3> walk_purchase_on_soil(Environment& environment,
            float x, float toe_gap, float receiving_impulse,
            float trailing_heel_gap = 0.0f, float trailing_impulse = 1.0f,
            bool receiving_heel_loaded = true) noexcept
        {
            for (auto& particle : environment.particles_)
                particle.grounded = false;
            float highest_skin_relative = -1.0f;
            for (std::size_t slot = 0; slot < human_support_contact_nodes.size(); ++slot)
            {
                Particle& particle = environment.particles_[human_support_contact_nodes[slot]];
                particle.position.x = x + static_cast<float>(slot) * 0.07f;
                const float surface = environment.ground_height_at(particle.position.x);
                const float offset = ground_contact_offset(true, particle.radius);
                const float sink = (1.0f - environment.terrain_firmness_at(
                    particle.position.x)) * 0.055f;
                const float lift = slot == 2u ? toe_gap : slot == 3u ? trailing_heel_gap : 0.0f;
                const bool lifted = lift > 0.0f;
                particle.position.y = surface + offset - sink + lift;
                particle.previous = particle.position;
                particle.grounded = !lifted;
                if (slot < 3u)
                    highest_skin_relative = std::max(highest_skin_relative,
                        particle.position.y - surface - offset);
            }
            environment.human_gait_coordinator_.active = true;
            environment.human_gait_coordinator_.outgoing_side = -1;
            environment.human_gait_coordinator_.stage = trailing_heel_gap > 0.0f
                ? HumanGaitCoordinatorStage::landing : HumanGaitCoordinatorStage::purchase;
            environment.human_gait_coordinator_.phase = pi * 1.80f;
            environment.human_gait_coordinator_.event_frames = 0u;
            environment.support_cluster_normal_impulse_ = { receiving_impulse, trailing_impulse };
            environment.support_normal_constraint_lambdas_[human_left_heel_node] =
                receiving_heel_loaded ? receiving_impulse * 0.5f / 60.0f : 0.0f;
            environment.elapsed_seconds_ = 2.0f;
            const auto measured = environment.observation();
            std::array<float, action_count> zero{};
            for (int frame = 0; frame < 2; ++frame)
                environment.update_human_gait_coordinator(zero, 1.0f / 60.0f);
            return { highest_skin_relative,
                measured[left_sole_maximum_clearance_observation] * 0.25f,
                static_cast<float>(environment.human_gait_coordinator_.outgoing_side) };
        }
        struct SolePatchProbe
        {
            float displacement{};
            float speed{};
            float capacity{};
            float used{};
            float positive_work{};
        };

        static SolePatchProbe sole_patch(float displacement, float speed,
            float normal_impulse, bool grounded, float reserved = 0.0f) noexcept
        {
            constexpr float dt = 1.0f / 60.0f;
            Environment environment{ CreatureBlueprint::humanoid(), 0x7517u };
            environment.set_course(reserved == 0.0f ? CourseStage::balance
                : CourseStage::uneven, 0.25f);
            environment.set_guidance_mode(GuidanceMode::raw_policy_audit);
            for (const auto node : human_support_contact_nodes)
            {
                environment.particles_[node].grounded = false;
                environment.support_contact_latch_[node] = 0u;
            }
            constexpr auto heel = human_left_heel_node;
            Particle& particle = environment.particles_[heel];
            const float initial_x = particle.position.x;
            particle.position.x += displacement;
            particle.previous.x = particle.position.x - speed * dt;
            particle.grounded = grounded;
            environment.support_contact_latch_[heel] = grounded ? 1u : 0u;
            environment.support_contact_anchor_x_[heel] = initial_x;
            environment.support_normal_constraint_lambdas_[heel] = normal_impulse * dt;
            environment.support_contact_tangent_impulses_[heel] = reserved;
            static_cast<void>(environment.solve_raw_support_tangent_manifold(dt));
            return { particle.position.x - initial_x,
                (particle.position.x - particle.previous.x) / dt,
                environment.support_traction_capacity_[heel],
                environment.support_traction_used_[heel],
                environment.support_friction_positive_work_joules_ };
        }

        static void prepare_torso_balance_probe(Environment& environment,
            float angle, float angular_speed, int support_side) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.30f);
            environment.set_course_motion_enabled(false);
            environment.set_guidance_mode(GuidanceMode::raw_policy_audit);
            environment.set_raw_authority_telemetry_enabled(true);
            const std::uint16_t root = environment.blueprint_.root_node;
            const std::uint16_t torso = environment.blueprint_.torso_node;
            const Vec2 pivot = environment.particles_[root].position;
            constexpr float dt = 1.0f / 60.0f;
            std::array<bool, 128> upper_body{};
            std::array<std::uint16_t, 128> stack{};
            std::size_t stack_size{};
            upper_body[root] = true;
            upper_body[torso] = true;
            stack[stack_size++] = torso;
            while (stack_size > 0u)
            {
                const std::uint16_t node = stack[--stack_size];
                for (const DistanceConstraint& bone : environment.blueprint_.bones)
                {
                    const std::uint16_t next = bone.a == node ? bone.b
                        : bone.b == node ? bone.a
                        : std::numeric_limits<std::uint16_t>::max();
                    if (next >= environment.particles_.size()
                        || upper_body[next])
                        continue;
                    upper_body[next] = true;
                    stack[stack_size++] = next;
                }
            }
            upper_body[root] = false;
            for (std::size_t index = 0;
                index < environment.particles_.size(); ++index)
            {
                if (!upper_body[index])
                    continue;
                Particle& particle = environment.particles_[index];
                const Vec2 segment = particle.position - pivot;
                particle.position = pivot + rotate(segment, angle);
                particle.previous = pivot
                    + rotate(segment, angle - angular_speed * dt);
            }
            environment.elapsed_seconds_ = 2.0f;
            environment.previous_torso_angle_ = environment.torso_roll_angle()
                - angular_speed * dt;
            environment.torso_turn_speed_ = angular_speed;
            for (std::size_t index = 0;
                index < environment.particles_.size(); ++index)
            {
                if (!environment.blueprint_.is_support_seed(index))
                    continue;
                const bool left = environment.contact_cluster_contains(
                    environment.blueprint_.left_contact_node, index);
                const bool right = environment.contact_cluster_contains(
                    environment.blueprint_.right_contact_node, index);
                const bool supported = support_side == 2
                    || (support_side < 0 && left)
                    || (support_side > 0 && right);
                environment.particles_[index].grounded = supported;
                environment.support_contact_latch_[index] = supported ? 1u : 0u;
                environment.support_contact_anchor_x_[index] =
                    environment.particles_[index].position.x;
            }
        }

        static void set_elapsed_seconds(Environment& environment,
            float elapsed_seconds) noexcept
        {
            environment.elapsed_seconds_ = elapsed_seconds;
        }

        static void prepare_joint_probe(Environment& environment,
            bool airborne) noexcept
        {
            constexpr float airborne_lift = 0.80f;
            for (Particle& particle : environment.particles_)
            {
                if (airborne)
                    particle.position.y += airborne_lift;
                particle.previous = particle.position;
                if (airborne)
                    particle.grounded = false;
            }
            if (airborne)
            {
                std::fill(environment.support_contact_latch_.begin(),
                    environment.support_contact_latch_.end(), std::uint8_t{ 0 });
                environment.support_cluster_contact_age_seconds_.fill(0.0f);
            }
            environment.elapsed_seconds_ = 2.0f;
            environment.previous_applied_actions_.fill(0.0f);
            environment.angular_velocities_.fill(0.0f);
            for (std::size_t index = 0;
                index < environment.blueprint_.active_motor_count; ++index)
            {
                environment.previous_angles_[index] = environment.joint_angle(
                    environment.blueprint_.motors[index]);
            }
            environment.previous_pelvis_ = environment.particles_[
                environment.blueprint_.root_node].position;
            environment.previous_root_for_path_ = environment.previous_pelvis_;
            environment.previous_torso_angle_ = environment.torso_roll_angle();
            environment.torso_turn_speed_ = 0.0f;
            environment.raw_authority_telemetry_ = {};
        }

        static void prepare_coherent_human_spawn(Environment& environment) noexcept
        {
            const float x_shift = environment.particles_[
                    environment.blueprint_.root_node].position.x
                - environment.blueprint_.nodes[environment.blueprint_.root_node].x;
            float y_shift = -std::numeric_limits<float>::infinity();
            for (std::size_t index = 0;
                index < environment.blueprint_.nodes.size(); ++index)
            {
                if (!environment.blueprint_.is_support_seed(index))
                    continue;
                const float x = environment.blueprint_.nodes[index].x + x_shift;
                const float target = environment.ground_height_at(x)
                    + ground_contact_offset(true,
                        environment.blueprint_.radii[index]);
                y_shift = std::max(y_shift,
                    target - environment.blueprint_.nodes[index].y);
            }
            if (!std::isfinite(y_shift))
                return;
            for (std::size_t index = 0;
                index < environment.particles_.size(); ++index)
            {
                Particle& particle = environment.particles_[index];
                particle.position = environment.blueprint_.nodes[index]
                    + Vec2{ x_shift, y_shift };
                particle.previous = particle.position;
                particle.grounded = false;
            }
            std::fill(environment.support_contact_latch_.begin(),
                environment.support_contact_latch_.end(), std::uint8_t{ 0 });
            environment.support_cluster_contact_age_seconds_.fill(0.0f);
            environment.previous_applied_actions_.fill(0.0f);
            environment.angular_velocities_.fill(0.0f);
            environment.previous_pelvis_ = environment.particles_[
                environment.blueprint_.root_node].position;
            environment.previous_root_for_path_ = environment.previous_pelvis_;
            environment.previous_torso_angle_ = environment.torso_roll_angle();
            environment.torso_turn_speed_ = 0.0f;
            environment.raw_authority_telemetry_ = {};
        }

        static void set_raw_support_tangential_impulses_enabled(
            Environment& environment, bool enabled) noexcept
        {
            environment.raw_support_tangential_impulses_enabled_ = enabled;
        }

        static void set_passive_toe_deviation(Environment& environment,
            bool left, float deviation) noexcept
        {
            MotorConstraint motor{};
            if (!environment.articulated_toe_motor(left, motor))
                return;
            Particle& parent = environment.particles_[motor.a];
            Particle& pivot = environment.particles_[motor.pivot];
            Particle& driven = environment.particles_[motor.c];
            const Vec2 parent_direction = normalized(
                parent.position - pivot.position, { -1.0f, 0.0f });
            const float driven_length = length(
                driven.position - pivot.position);
            driven.position = pivot.position + rotate(parent_direction,
                motor.neutral_angle + deviation) * driven_length;
            driven.previous = driven.position;
        }

        [[nodiscard]] static float passive_toe_deviation(
            const Environment& environment, bool left) noexcept
        {
            MotorConstraint motor{};
            if (!environment.articulated_toe_motor(left, motor))
                return 0.0f;
            return wrap_angle(environment.joint_angle(motor)
                * environment.facing_direction() - motor.neutral_angle);
        }

        static void solve_passive_toe_stop(Environment& environment,
            bool left) noexcept
        {
            environment.solve_passive_articulated_toe_stop(left);
        }

        struct PassiveToeDampingEvidence
        {
            float relative_rate_before{};
            float relative_rate_after{};
            float kinetic_energy_before{};
            float kinetic_energy_after{};
            Vec2 momentum_before{};
            Vec2 momentum_after{};
            float maximum_position_delta{};
        };

        static PassiveToeDampingEvidence passive_toe_damping(
            Environment& environment, bool left, float signed_rate) noexcept
        {
            constexpr float dt = 1.0f / 60.0f;
            MotorConstraint motor{};
            if (!environment.articulated_toe_motor(left, motor))
                return {};
            set_passive_toe_deviation(environment, left, 0.0f);
            Particle& parent = environment.particles_[motor.a];
            Particle& pivot = environment.particles_[motor.pivot];
            Particle& driven = environment.particles_[motor.c];
            const Vec2 parent_arm = parent.position - pivot.position;
            const Vec2 driven_arm = driven.position - pivot.position;
            const float facing = environment.facing_direction();
            const Vec2 parent_gradient = -perpendicular(parent_arm)
                / length_squared(parent_arm) * facing;
            const Vec2 driven_gradient = perpendicular(driven_arm)
                / length_squared(driven_arm) * facing;
            const Vec2 pivot_gradient = -(parent_gradient + driven_gradient);
            const float inverse_inertia =
                parent.inverse_mass * length_squared(parent_gradient)
                + pivot.inverse_mass * length_squared(pivot_gradient)
                + driven.inverse_mass * length_squared(driven_gradient);
            const float scale = signed_rate / inverse_inertia;
            parent.previous = parent.position
                - parent_gradient * (parent.inverse_mass * scale * dt);
            pivot.previous = pivot.position
                - pivot_gradient * (pivot.inverse_mass * scale * dt);
            driven.previous = driven.position
                - driven_gradient * (driven.inverse_mass * scale * dt);
            const std::array<Vec2, 3> positions_before{
                parent.position, pivot.position, driven.position };
            const auto sample = [&](float& rate, float& energy,
                                    Vec2& momentum) noexcept
            {
                rate = 0.0f;
                energy = 0.0f;
                momentum = {};
                const std::array<const Particle*, 3> particles{
                    &parent, &pivot, &driven };
                const std::array<Vec2, 3> gradients{
                    parent_gradient, pivot_gradient, driven_gradient };
                for (std::size_t index = 0; index < particles.size(); ++index)
                {
                    const Particle& particle = *particles[index];
                    const Vec2 velocity = (particle.position
                        - particle.previous) / dt;
                    const float mass = 1.0f / particle.inverse_mass;
                    rate += dot(gradients[index], velocity);
                    energy += 0.5f * mass * length_squared(velocity);
                    momentum += velocity * mass;
                }
            };
            PassiveToeDampingEvidence result{};
            sample(result.relative_rate_before, result.kinetic_energy_before,
                result.momentum_before);
            environment.damp_passive_articulated_toe_velocity(left, dt);
            sample(result.relative_rate_after, result.kinetic_energy_after,
                result.momentum_after);
            const std::array<Vec2, 3> positions_after{
                parent.position, pivot.position, driven.position };
            for (std::size_t index = 0; index < positions_before.size(); ++index)
            {
                result.maximum_position_delta = std::max(
                    result.maximum_position_delta,
                    length(positions_after[index] - positions_before[index]));
            }
            return result;
        }

        static void set_motor_half_range(Environment& environment,
            std::size_t motor_index, float half_range) noexcept
        {
            if (motor_index >= environment.blueprint_.active_motor_count)
                return;
            MotorConstraint& motor = environment.blueprint_.motors[motor_index];
            motor.minimum_angle = motor.neutral_angle - half_range;
            motor.maximum_angle = motor.neutral_angle + half_range;
        }

        static void solve_powered_motor_probe(Environment& environment,
            std::size_t motor_index, float action,
            int solver_iterations) noexcept
        {
            if (motor_index >= environment.blueprint_.active_motor_count)
                return;
            environment.raw_authority_telemetry_ = {};
            float lambda{};
            float positive_work{};
            for (int iteration = 0; iteration < solver_iterations; ++iteration)
            {
                environment.solve_powered_motor_xpbd(
                    environment.blueprint_.motors[motor_index], action, action,
                    1.0f / 60.0f, lambda, positive_work, motor_index);
            }
        }

        [[nodiscard]] static bool place_node_near_ledge(
            Environment& environment, std::uint16_t node,
            Vec2 anchor_offset) noexcept
        {
            environment.set_course(CourseStage::climb_descent, 0.30f);
            if (node >= environment.particles_.size())
                return false;
            const auto ledge = std::ranges::find_if(
                environment.course_features_, [](const CourseFeature& feature)
                {
                    return feature.kind == CourseFeatureKind::ledge;
                });
            if (ledge == environment.course_features_.end())
                return false;
            Particle& particle = environment.particles_[node];
            const float left = ledge->center.x - ledge->half_extent.x;
            const float top = ledge->center.y + ledge->half_extent.y;
            const Vec2 anchor{
                left - particle.radius * 0.20f,
                top + particle.radius * 0.18f };
            particle.position = anchor + anchor_offset;
            particle.previous = particle.position + Vec2{ 0.037f, -0.019f };
            particle.grounded = false;
            environment.ledge_grasp_nodes_.fill(
                std::numeric_limits<std::uint16_t>::max());
            environment.ledge_grasp_anchors_.fill({});
            environment.hand_ledge_contacts_ = 0u;
            environment.collided_this_step_ = false;
            environment.raw_authority_telemetry_ = {};
            return true;
        }

        static void apply_ledge_training_scaffold_probe(
            Environment& environment) noexcept
        {
            constexpr float dt = 1.0f / 60.0f;
            const Environment::AuthoritySnapshot before =
                environment.authority_snapshot(dt);
            environment.apply_ledge_training_scaffold();
            environment.record_authority_delta(
                AuthoritySource::ledge_training_scaffold, before, dt);
        }

        [[nodiscard]] static bool active_ledge_grasp(
            const Environment& environment) noexcept
        {
            return std::ranges::any_of(environment.ledge_grasp_nodes_,
                [](std::uint16_t node)
                {
                    return node != std::numeric_limits<std::uint16_t>::max();
                });
        }

        [[nodiscard]] static bool course_collision_latched(
            const Environment& environment) noexcept
        {
            return environment.collided_this_step_;
        }

    };
}

namespace
{
    using runner::Vec2;

    void require(bool condition, std::string_view message)
    {
        if (!condition)
            throw std::runtime_error(std::string{ message });
    }

    [[nodiscard]] Vec2 mass_center(
        const runner::sim::Environment& environment) noexcept
    {
        Vec2 center{};
        float mass_sum{};
        for (const runner::sim::Particle& particle : environment.particles())
        {
            const float mass = 1.0f
                / std::max(particle.inverse_mass, 1.0e-6f);
            center += particle.position * mass;
            mass_sum += mass;
        }
        return center / std::max(mass_sum, 1.0e-6f);
    }


    struct RampTrial
    {
        std::uint32_t first_grounded_support_nodes{};
        float center_velocity{};
        float pelvis_velocity{};
        float ground_delta_before_ramp{};
        float ground_delta_after_ramp{};
        std::array<float, 2> tangential_impulse{};
        std::array<float, 2> normal_impulse{};
        float normal_impulse_moment_x{};
        float startup_normal_impulse{};
        float startup_normal_impulse_moment_x{};
        float maximum_absolute_com_cop_lead{};
        std::array<float, 2> maximum_clearance{};
        std::array<float, 2> maximum_toe_angle_deviation{};
        std::array<int, 2> first_toe_range_violation_frame{ -1, -1 };
        std::array<int, 2> first_unload_frame{ -1, -1 };
        std::array<int, 2> first_replant_frame{ -1, -1 };
        std::uint32_t both_unsupported_frames{};
        std::uint32_t gait_cycles{};
        std::uint32_t alternating_steps{};
        std::uint32_t loaded_handoffs{};
        int first_loaded_handoff_frame{ -1 };
        float first_handoff_heel_impulse{};
        float distance{};
        std::uint32_t command_mismatches{};
        std::uint32_t actuator_mismatches{};
        std::uint32_t motor_mismatches{};
        std::uint32_t powered_toe_commands{};
        std::uint32_t root_or_helper_corrections{};
        float friction_work{};
        float friction_positive_work{};
        runner::sim::InvalidMotion invalid_reason{
            runner::sim::InvalidMotion::none };
    };

    [[nodiscard]] RampTrial run_ramp_trial(std::uint64_t seed,
        float direction, bool teacher, bool torso_balance,
        runner::sim::CourseStage course = runner::sim::CourseStage::uneven,
        bool detailed_contact_trace = false, float ankle_balance_gain = 1.0f,
        bool direct_stance_ankle = false, bool coherent_spawn = false,
        const runner::sim::CreatureBlueprint* blueprint_override = nullptr,
        bool disable_raw_tangential_impulses = false,
        int trial_frames = 240, bool transfer_trace = false, std::ostream* capture = nullptr)
    {
        using namespace runner;
        const sim::CreatureBlueprint blueprint = blueprint_override != nullptr
            ? *blueprint_override : sim::CreatureBlueprint::humanoid();
        sim::Environment environment{ blueprint, seed };
        environment.set_course(course, 0.30f);
        environment.set_course_motion_enabled(false);
        environment.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
        environment.set_raw_authority_telemetry_enabled(true);
        require(environment.set_locomotion_intent(direction, false),
            "finite locomotion intent was rejected");
        if (coherent_spawn)
            sim::EnvironmentTestAccess::prepare_coherent_human_spawn(environment);
        if (capture != nullptr)
            for (const auto& bone : blueprint.bones)
                *capture << "#bone," << bone.a << ',' << bone.b << '\n';
        RampTrial trial{};
        if (disable_raw_tangential_impulses)
        {
            sim::EnvironmentTestAccess::set_raw_support_tangential_impulses_enabled(
                environment, false);
        }

        constexpr float dt = 1.0f / 60.0f;
        const Vec2 initial_center = mass_center(environment);
        const float initial_pelvis = environment.particles()[
            environment.blueprint().root_node].position.x;
        std::array<float, sim::anatomy_action_count> previous_actuator{};
        std::array<bool, 2> was_supported{ true, true };
        constexpr auto source_count = static_cast<std::size_t>(sim::AuthoritySource::count);
        std::array<float, source_count> pre_touchdown_sources{};
        float pre_touchdown_net{};
        for (int frame = 0; frame < trial_frames; ++frame)
        {
            std::array<float, sim::action_count> action{};
            if (teacher)
            {
                action = rl::raw_walking_teacher_action(environment);
                if (!torso_balance || ankle_balance_gain != 1.0f)
                {
                    const auto correction =
                        rl::raw_human_torso_joint_balance_correction(environment);
                    for (std::size_t index = 0;
                        index < sim::anatomy_action_count; ++index)
                    {
                        const float retained = torso_balance
                            ? clamp(ankle_balance_gain, 0.0f, 1.0f) : 0.0f;
                        action[index] = clamp(action[index]
                                - correction[index] * (1.0f - retained),
                            -1.0f, 1.0f);
                    }
                }
                if (direct_stance_ankle)
                {
                    auto parameters = rl::anatomy_scaled_foundational_gait(
                        environment.blueprint(), environment.gait_task(),
                        environment.elapsed_seconds());
                    parameters.direction = direction;
                    parameters.cadence_hz = 0.64f;
                    parameters.step_length = 0.68f;
                    parameters.swing_lift = 0.14f;
                    parameters.transition_flex = 0.0f;
                    parameters.smooth_contact_transitions = true;
                    const auto targets = rl::biped_gait_teacher_action(
                        environment, parameters);
                    float phase = std::fmod(rl::locomotion_gait_seconds(environment)
                            * 2.0f * pi * parameters.cadence_hz
                            + parameters.phase_offset,
                        2.0f * pi);
                    if (phase < 0.0f)
                        phase += 2.0f * pi;
                    const std::size_t stance_ankle = phase < pi
                        ? sim::human_left_ankle_action
                        : sim::human_right_ankle_action;
                    const auto observation = environment.observation();
                    action[stance_ankle] = clamp(targets[stance_ankle]
                            - 0.010f * observation[
                                sim::joint_velocity_observation_index(
                                    stance_ankle)],
                        -1.0f, 1.0f);
                }
            }
            std::array<float, sim::anatomy_action_count> expected_actuator{};
            const float ramp = sim::policy_control_ramp(
                environment.elapsed_seconds()
                    + sim::policy_control_ramp_start_seconds);
            for (std::size_t index = 0;
                index < sim::anatomy_action_count; ++index)
            {
                const float desired = clamp(action[index], -1.0f, 1.0f) * ramp;
                expected_actuator[index] = previous_actuator[index]
                    + clamp(desired - previous_actuator[index],
                        -dt, dt);
            }
            const auto before_gait = environment.human_gait_coordinator();
            const sim::StepResult step = environment.step(action);
            const auto after_gait = environment.human_gait_coordinator();
            const sim::RawAuthorityTelemetry& authority =
                environment.raw_authority_telemetry();
            if (before_gait.active && after_gait.active
                && before_gait.outgoing_side != after_gait.outgoing_side)
            {
                ++trial.loaded_handoffs;
                if (trial.first_loaded_handoff_frame < 0)
                {
                    trial.first_loaded_handoff_frame = frame;
                    trial.first_handoff_heel_impulse = authority.support_contact_normal_impulse[
                        before_gait.outgoing_side < 0 ? 0u : 3u];
                }
            }
            if (capture != nullptr)
            {
                const auto pose = environment.particles();
                *capture << frame << ',' << environment.elapsed_seconds() << ','
                    << trial.loaded_handoffs << ',' << after_gait.outgoing_side << ','
                    << pose[sim::human_left_heel_node].grounded << ','
                    << pose[sim::human_right_heel_node].grounded << ','
                    << authority.support_normal_impulse[0] << ',' << authority.support_normal_impulse[1]
                    << ',' << authority.support_contact_normal_impulse[0] << ','
                    << environment.non_foot_grounded() << ','
                    << environment.ground_height_at(pose[blueprint.root_node].position.x);
                for (const auto& particle : pose)
                    *capture << ',' << particle.position.x << ',' << particle.position.y;
                *capture << '\n';
            }
            trial.powered_toe_commands += authority.powered_toe_commands;
            trial.friction_work += authority.friction_work_joules;
            trial.friction_positive_work +=
                authority.friction_positive_work_joules;
            trial.root_or_helper_corrections += authority.root_drive_corrections
                + authority.torso_drive_corrections
                + authority.course_motion_corrections
                + authority.sources[static_cast<std::size_t>(
                    sim::AuthoritySource::ledge_training_scaffold)].applications;
            const float ground_delta = authority.sources[static_cast<std::size_t>(
                sim::AuthoritySource::ground_contact)].center_velocity_delta_x;
            if (frame <= 20)
                trial.ground_delta_before_ramp += ground_delta;
            else
                trial.ground_delta_after_ramp += ground_delta;
            for (std::size_t side = 0; side < 2u; ++side)
            {
                trial.tangential_impulse[side] +=
                    authority.support_tangential_impulse[side];
                trial.normal_impulse[side] +=
                    authority.support_normal_impulse[side];
                trial.normal_impulse_moment_x +=
                    authority.support_normal_impulse_moment_x[side];
                if (frame <= 20)
                {
                    trial.startup_normal_impulse +=
                        authority.support_normal_impulse[side];
                    trial.startup_normal_impulse_moment_x +=
                        authority.support_normal_impulse_moment_x[side];
                }
            }
            const float frame_normal_impulse =
                authority.support_normal_impulse[0]
                + authority.support_normal_impulse[1];
            if (frame_normal_impulse > 1.0e-6f)
            {
                const float frame_cop = (
                    authority.support_normal_impulse_moment_x[0]
                    + authority.support_normal_impulse_moment_x[1])
                    / frame_normal_impulse;
                trial.maximum_absolute_com_cop_lead = std::max(
                    trial.maximum_absolute_com_cop_lead,
                    std::abs(mass_center(environment).x - frame_cop));
            }
            const std::array<bool, 2> supported{
                environment.left_supported(), environment.right_supported() };
            const auto particles = environment.particles();
            const auto cluster_clearance = [&](bool left)
            {
                const std::array<std::uint16_t, 3> nodes = left
                    ? std::array<std::uint16_t, 3>{
                        sim::human_left_heel_node, sim::human_left_ball_node,
                        sim::human_left_toe_node }
                    : std::array<std::uint16_t, 3>{
                        sim::human_right_heel_node, sim::human_right_ball_node,
                        sim::human_right_toe_node };
                float maximum{};
                for (const std::uint16_t node : nodes)
                {
                    maximum = std::max(maximum, particles[node].position.y
                        - sim::ground_contact_offset(true,
                            particles[node].radius)
                        - environment.ground_height_at(
                            particles[node].position.x));
                }
                return maximum;
            };
            for (std::size_t side = 0; side < 2u; ++side)
            {
                trial.maximum_clearance[side] = std::max(
                    trial.maximum_clearance[side],
                    cluster_clearance(side == 0u));
                if (was_supported[side] && !supported[side]
                    && trial.first_unload_frame[side] < 0)
                    trial.first_unload_frame[side] = frame;
                if (!was_supported[side] && supported[side]
                    && trial.first_unload_frame[side] >= 0
                    && trial.first_replant_frame[side] < 0)
                    trial.first_replant_frame[side] = frame;
                was_supported[side] = supported[side];
            }
            if (transfer_trace)
            {
                const std::size_t first_swing = direction > 0.0f ? 0u : 1u;
                const int unload = trial.first_unload_frame[first_swing];
                const int replant = trial.first_replant_frame[first_swing];
                if (unload >= 0 && frame > unload && (replant < 0 || frame <= replant))
                {
                    pre_touchdown_net += authority.total_center_velocity_delta_x;
                    for (std::size_t source = 0; source < source_count; ++source)
                        pre_touchdown_sources[source] += authority.sources[source].center_velocity_delta_x;
                }
                if (replant == frame)
                {
                    std::cerr << "first transfer window " << unload << ".." << replant
                        << " com-dvx=" << pre_touchdown_net;
                    for (std::size_t source = 0; source < source_count; ++source)
                    {
                        const float value = pre_touchdown_sources[source];
                        if (std::abs(value) > 1.0e-5f)
                            std::cerr << ' ' << sim::authority_source_name(
                                static_cast<sim::AuthoritySource>(source)) << '=' << value;
                    }
                    std::cerr << '\n';
                }
            }
            const auto toe_deviation = [&](bool left)
            {
                const std::uint16_t heel = left
                    ? sim::human_left_heel_node : sim::human_right_heel_node;
                const std::uint16_t ball = left
                    ? sim::human_left_ball_node : sim::human_right_ball_node;
                const std::uint16_t toe = left
                    ? sim::human_left_toe_node : sim::human_right_toe_node;
                const float current = signed_angle(
                    particles[heel].position - particles[ball].position,
                    particles[toe].position - particles[ball].position);
                const float neutral = signed_angle(
                    blueprint.nodes[heel] - blueprint.nodes[ball],
                    blueprint.nodes[toe] - blueprint.nodes[ball]);
                return wrap_angle(current - neutral);
            };
            for (std::size_t side = 0; side < 2u; ++side)
            {
                const float deviation = toe_deviation(side == 0u);
                trial.maximum_toe_angle_deviation[side] = std::max(
                    trial.maximum_toe_angle_deviation[side],
                    std::abs(deviation));
                const bool outside = deviation < -42.0f * pi / 180.0f
                    || deviation > 36.0f * pi / 180.0f;
                if (outside
                    && trial.first_toe_range_violation_frame[side] < 0)
                    trial.first_toe_range_violation_frame[side] = frame;
            }
            if (!supported[0] && !supported[1])
                ++trial.both_unsupported_frames;
            if (frame == 0)
            {
                for (std::size_t index = 0;
                    index < environment.particles().size(); ++index)
                {
                    if (environment.blueprint().is_support_seed(index)
                        && environment.particles()[index].grounded)
                        ++trial.first_grounded_support_nodes;
                }
            }
            for (std::size_t index = 0;
                index < sim::anatomy_action_count; ++index)
            {
                trial.command_mismatches += authority.commanded_actions[index]
                        == action[index] ? 0u : 1u;
                trial.actuator_mismatches += std::abs(
                    authority.actuator_actions[index]
                        - expected_actuator[index]) <= 1.0e-7f ? 0u : 1u;
                const sim::MotorDiagnostic motor =
                    environment.motor_diagnostic(index);
                trial.motor_mismatches += motor.available && motor.enabled
                        && motor.applied_action == authority.actuator_actions[index]
                    ? 0u : 1u;
                previous_actuator[index] = authority.actuator_actions[index];
            }
            if (transfer_trace && (frame % 6 == 0
                    || (frame >= 145 && frame <= 164)
                    || trial.first_unload_frame[0] == frame
                    || trial.first_unload_frame[1] == frame
                    || trial.first_replant_frame[0] == frame
                    || trial.first_replant_frame[1] == frame))
            {
                const auto& gait = environment.human_gait_coordinator();
                const auto support = environment.human_support_feedback();
                const auto observation = environment.observation();
                const auto source_dx = [&authority](sim::AuthoritySource source)
                {
                    return authority.sources[static_cast<std::size_t>(source)]
                        .center_velocity_delta_x;
                };
                std::cerr << "gait " << frame << " phase " << gait.phase << " stage "
                    << static_cast<int>(gait.stage) << " out " << gait.outgoing_side
                    << " x " << particles[environment.blueprint().root_node].position.x
                    << " dx " << environment.forward_speed()
                    << " feet " << particles[sim::human_left_ankle_node].position.x << ','
                    << particles[sim::human_right_ankle_node].position.x
                    << " left sole " << particles[sim::human_left_heel_node].position.x << ','
                    << particles[sim::human_left_ball_node].position.x << ','
                    << particles[sim::human_left_toe_node].position.x
                    << " right sole " << particles[sim::human_right_heel_node].position.x << ','
                    << particles[sim::human_right_ball_node].position.x << ','
                    << particles[sim::human_right_toe_node].position.x
                    << " right friction " << authority.support_traction_used[1] << '/'
                    << authority.support_traction_capacity[1]
                    << " support " << environment.left_supported() << ',' << environment.right_supported()
                    << " load " << support.total_normal_impulse << ',' << support.load_asymmetry
                    << " normal-left/right " << authority.support_normal_impulse[0]
                    << ',' << authority.support_normal_impulse[1]
                    << " sole-max-clearance " << 0.25f * observation[
                        sim::left_sole_maximum_clearance_observation]
                    << ',' << 0.25f * observation[
                        sim::right_sole_maximum_clearance_observation]
                    << " heel-ball-toe "
                    << particles[sim::human_left_heel_node].grounded
                    << particles[sim::human_left_ball_node].grounded
                    << particles[sim::human_left_toe_node].grounded << ','
                    << particles[sim::human_right_heel_node].grounded
                    << particles[sim::human_right_ball_node].grounded
                    << particles[sim::human_right_toe_node].grounded
                    << " com/cop/capture "
                    << support.center_of_mass.x << ',' << support.aggregate_cop_x
                    << ',' << support.capture_point_x
                    << " capture-valid " << support.valid
                    << " com-vx " << support.center_of_mass_velocity.x
                    << " dv-total/ground/joint/distance "
                    << authority.total_center_velocity_delta_x << ','
                    << source_dx(sim::AuthoritySource::ground_contact) << ','
                    << source_dx(sim::AuthoritySource::joint_motors) << ','
                    << source_dx(sim::AuthoritySource::distance_constraints)
                    << " non-foot " << environment.non_foot_grounded()
                    << " A " << action[0] << ',' << action[1] << ',' << action[2] << ',' << action[3]
                    << ',' << action[8] << ',' << action[9]
                    << " actual " << authority.actuator_actions[0] << ',' << authority.actuator_actions[1]
                    << ',' << authority.actuator_actions[2] << ',' << authority.actuator_actions[3]
                    << ',' << authority.actuator_actions[8] << ',' << authority.actuator_actions[9] << '\n';
            }
            if (detailed_contact_trace
                && (!teacher || (frame >= 80 && frame <= 110)))
            {
                const auto print_contact = [&](std::uint16_t node,
                    std::size_t slot)
                {
                    const sim::Particle& particle = particles[node];
                    const float ground = environment.ground_height_at(
                        particle.position.x);
                    const float bottom = particle.position.y
                        - sim::ground_contact_offset(true, particle.radius);
                    std::cerr << node << ':' << particle.position.x << ','
                        << particle.position.y << ',' << ground << ','
                        << bottom << ',' << bottom - ground << ','
                        << particle.grounded << ','
                        << authority.support_contact_normal_impulse[slot] << ','
                        << authority.support_contact_tangential_impulse[slot];
                };
                const sim::CreatureBlueprint& rig = environment.blueprint();
                const Vec2 right_shank = particles[5].position
                    - particles[sim::human_right_ankle_node].position;
                const Vec2 right_rear_foot =
                    particles[sim::human_right_ball_node].position
                    - particles[sim::human_right_ankle_node].position;
                const Vec2 right_sole =
                    particles[sim::human_right_ball_node].position
                    - particles[sim::human_right_heel_node].position;
                const Vec2 right_toe =
                    particles[sim::human_right_toe_node].position
                    - particles[sim::human_right_ball_node].position;
                const Vec2 left_sole =
                    particles[sim::human_left_ball_node].position
                    - particles[sim::human_left_heel_node].position;
                const Vec2 left_toe =
                    particles[sim::human_left_toe_node].position
                    - particles[sim::human_left_ball_node].position;
                const float right_expected_heel_x = particles[
                        sim::human_right_ankle_node].position.x
                    + rig.nodes[sim::human_right_heel_node].x
                    - rig.nodes[sim::human_right_ankle_node].x;
                const float right_expected_ball_x = particles[
                        sim::human_right_ankle_node].position.x
                    + rig.nodes[sim::human_right_ball_node].x
                    - rig.nodes[sim::human_right_ankle_node].x;
                const auto relative_length_error = [&](std::uint16_t a,
                    std::uint16_t b)
                {
                    const float rest = length(rig.nodes[b] - rig.nodes[a]);
                    return rest > 1.0e-6f
                        ? (length(particles[b].position
                                - particles[a].position) - rest) / rest
                        : 0.0f;
                };
                const auto diagnostic = [&](std::size_t motor)
                {
                    const sim::MotorDiagnostic value =
                        environment.motor_diagnostic(motor);
                    std::cerr << value.current_angle << '/' << value.target_angle;
                };
                std::cerr << "raw-contact mode="
                    << (direct_stance_ankle ? "direct-stance" : "baseline")
                    << " course=" << static_cast<int>(course)
                    << " frame=" << frame
                    << " phase=" << rl::locomotion_gait_seconds(environment)
                        * 2.0f * pi
                        * rl::anatomy_scaled_foundational_gait(rig).cadence_hz
                        + rl::anatomy_scaled_foundational_gait(rig).phase_offset
                    << " right_ankle_angle="
                        << signed_angle(right_shank, right_rear_foot)
                    << " right_sole_angle=" << std::atan2(
                        right_sole.y, right_sole.x)
                    << " left_sole_angle=" << std::atan2(
                        left_sole.y, left_sole.x)
                    << " expected_ground=" << right_expected_heel_x << ':'
                        << environment.ground_height_at(right_expected_heel_x)
                        << ',' << right_expected_ball_x << ':'
                        << environment.ground_height_at(right_expected_ball_x)
                    << " right_toe_angle=" << signed_angle(-1.0f * right_sole,
                        right_toe)
                    << " left_toe_angle=" << signed_angle(-1.0f * left_sole,
                        left_toe)
                    << " ankle_vy="
                        << (particles[sim::human_left_ankle_node].position.y
                            - particles[sim::human_left_ankle_node].previous.y)
                            / dt << ','
                        << (particles[sim::human_right_ankle_node].position.y
                            - particles[sim::human_right_ankle_node].previous.y)
                            / dt
                    << " ankle_action="
                        << authority.actuator_actions[
                            sim::human_left_ankle_action] << ','
                    << authority.actuator_actions[
                            sim::human_right_ankle_action]
                    << " motor=";
                diagnostic(0u);
                std::cerr << ',';
                diagnostic(1u);
                std::cerr << ',';
                diagnostic(2u);
                std::cerr << ',';
                diagnostic(3u);
                std::cerr << ',';
                diagnostic(sim::human_left_ankle_action);
                std::cerr << ',';
                diagnostic(sim::human_right_ankle_action);
                const auto print_authority_source = [&](sim::AuthoritySource source)
                {
                    const sim::AuthoritySourceTelemetry& value =
                        authority.sources[static_cast<std::size_t>(source)];
                    std::cerr << sim::authority_source_name(source) << ':'
                        << value.applications
                        << ",com_pos=" << value.center_position_delta_x << ','
                            << value.center_position_delta_y
                        << ",com_hist=" << value.center_history_delta_x << ','
                            << value.center_history_delta_y
                        << ",com_dv=" << value.center_velocity_delta_x << ','
                            << value.center_velocity_delta_y
                        << ",root=" << value.root_position_delta_x << ','
                            << value.root_position_delta_y
                        << ",torso=" << value.torso_position_delta_x << ','
                            << value.torso_position_delta_y;
                };
                std::cerr << " foot_error="
                    << relative_length_error(sim::human_right_ankle_node,
                        sim::human_right_heel_node) << ','
                    << relative_length_error(sim::human_right_ankle_node,
                        sim::human_right_ball_node) << ','
                    << relative_length_error(sim::human_right_heel_node,
                        sim::human_right_ball_node) << ','
                    << relative_length_error(sim::human_right_ball_node,
                        sim::human_right_toe_node)
                    << " authority=";
                print_authority_source(sim::AuthoritySource::joint_motors);
                std::cerr << '/';
                print_authority_source(
                    sim::AuthoritySource::distance_constraints);
                std::cerr << '/';
                print_authority_source(
                    sim::AuthoritySource::final_leg_constraints);
                std::cerr << '/';
                print_authority_source(sim::AuthoritySource::ground_contact);
                std::cerr << " right=";
                print_contact(sim::human_right_heel_node, 3u);
                std::cerr << '/';
                print_contact(sim::human_right_ball_node, 4u);
                std::cerr << '/';
                print_contact(sim::human_right_toe_node, 5u);
                std::cerr << '\n';
            }
            if (step.terminated)
                break;
        }
        trial.center_velocity = (mass_center(environment).x - initial_center.x)
            / std::max(environment.elapsed_seconds(), dt);
        trial.pelvis_velocity = (environment.particles()[
                environment.blueprint().root_node].position.x - initial_pelvis)
            / std::max(environment.elapsed_seconds(), dt);
        trial.invalid_reason = environment.invalid_reason();
        trial.gait_cycles = environment.gait_cycles();
        trial.alternating_steps = environment.alternating_steps();
        trial.distance = environment.distance_travelled();
        return trial;
    }

    struct JointJacobianProbe
    {
        Vec2 ankle_central_delta{};
        Vec2 sole_central_delta{};
        float target_central_delta{};
        float current_central_delta{};
        float positive_initial_error{};
        float positive_final_error{};
        float negative_initial_error{};
        float negative_final_error{};
        float positive_actuator{};
        float negative_actuator{};
        float positive_torque{};
        float negative_torque{};
        float positive_work{};
        float negative_work{};
        float maximum_torque{};
        float maximum_positive_work{};
        float adjacent_arm_ratio{};
        bool identity{};
    };

    [[nodiscard]] JointJacobianProbe probe_joint_jacobian(
        const runner::sim::Environment& settled, bool airborne,
        bool left, std::size_t motor_index)
    {
        using namespace runner;
        sim::Environment positive = settled;
        sim::Environment negative = settled;
        sim::EnvironmentTestAccess::prepare_joint_probe(positive, airborne);
        sim::EnvironmentTestAccess::prepare_joint_probe(negative, airborne);
        JointJacobianProbe result{};
        const sim::MotorDiagnostic initial = positive.motor_diagnostic(
            motor_index);
        const sim::MotorConstraint& motor = positive.blueprint().motors[
            motor_index];
        const auto initial_particles = positive.particles();
        const float parent_length = length(
            initial_particles[motor.a].position
                - initial_particles[motor.pivot].position);
        const float driven_length = length(
            initial_particles[motor.c].position
                - initial_particles[motor.pivot].position);
        float total_mass{};
        for (const sim::Particle& particle : initial_particles)
            total_mass += 1.0f / std::max(particle.inverse_mass, 1.0e-5f);
        const float strength_fraction = clamp(motor.strength / 0.056f,
            0.0f, 1.0f);
        result.maximum_torque = total_mass * 22.0f
            * std::max(parent_length, driven_length)
            * (0.10f + 0.30f * strength_fraction);
        result.adjacent_arm_ratio = std::max(parent_length, driven_length)
            / std::max(1.0e-6f, std::min(parent_length, driven_length));
        constexpr float dt = 1.0f / 60.0f;
        result.maximum_positive_work = result.maximum_torque * 2.0f * dt;
        std::array<float, sim::action_count> positive_action{};
        std::array<float, sim::action_count> negative_action{};
        positive_action[motor_index] = 1.0f;
        negative_action[motor_index] = -1.0f;
        const sim::StepResult positive_step = positive.step(positive_action);
        const sim::StepResult negative_step = negative.step(negative_action);
        (void)positive_step;
        (void)negative_step;
        const sim::MotorDiagnostic positive_motor =
            positive.motor_diagnostic(motor_index);
        const sim::MotorDiagnostic negative_motor =
            negative.motor_diagnostic(motor_index);
        const sim::RawAuthorityTelemetry& positive_authority =
            positive.raw_authority_telemetry();
        const sim::RawAuthorityTelemetry& negative_authority =
            negative.raw_authority_telemetry();
        const std::uint16_t ankle = left
            ? sim::human_left_ankle_node : sim::human_right_ankle_node;
        const std::array<std::uint16_t, 3> sole = left
            ? std::array<std::uint16_t, 3>{ sim::human_left_heel_node,
                sim::human_left_ball_node, sim::human_left_toe_node }
            : std::array<std::uint16_t, 3>{ sim::human_right_heel_node,
                sim::human_right_ball_node, sim::human_right_toe_node };
        const auto sole_center = [&](const sim::Environment& environment)
        {
            Vec2 result{};
            for (const std::uint16_t node : sole)
                result += environment.particles()[node].position;
            return result / static_cast<float>(sole.size());
        };
        result.ankle_central_delta = 0.5f * (
            positive.particles()[ankle].position
                - negative.particles()[ankle].position);
        result.sole_central_delta = 0.5f * (
            sole_center(positive) - sole_center(negative));
        result.target_central_delta = 0.5f * wrap_angle(
            positive_motor.target_angle - negative_motor.target_angle);
        result.current_central_delta = 0.5f * wrap_angle(
            positive_motor.current_angle - negative_motor.current_angle);
        result.positive_initial_error = std::abs(wrap_angle(
            positive_motor.target_angle - initial.current_angle));
        result.positive_final_error = std::abs(wrap_angle(
            positive_motor.target_angle - positive_motor.current_angle));
        result.negative_initial_error = std::abs(wrap_angle(
            negative_motor.target_angle - initial.current_angle));
        result.negative_final_error = std::abs(wrap_angle(
            negative_motor.target_angle - negative_motor.current_angle));
        result.positive_actuator = positive_authority.actuator_actions[motor_index];
        result.negative_actuator = negative_authority.actuator_actions[motor_index];
        result.positive_torque = positive_authority.motor_torque[motor_index];
        result.negative_torque = negative_authority.motor_torque[motor_index];
        result.positive_work =
            positive_authority.motor_positive_work_joules[motor_index];
        result.negative_work =
            negative_authority.motor_positive_work_joules[motor_index];
        result.identity = positive_motor.applied_action
                == result.positive_actuator
            && negative_motor.applied_action == result.negative_actuator
            && positive_authority.commanded_actions[motor_index] == 1.0f
            && negative_authority.commanded_actions[motor_index] == -1.0f;
        return result;
    }

    struct MotorRomEquivalenceProbe
    {
        float narrow_response{};
        float wide_response{};
        float repeated_response{};
        float narrow_torque{};
        float wide_torque{};
        float narrow_work{};
        float wide_work{};
        float maximum_torque{};
        float maximum_positive_work{};
    };

    [[nodiscard]] MotorRomEquivalenceProbe probe_motor_rom_equivalence(
        const runner::sim::Environment& initial, std::size_t motor_index,
        float direction, int solver_iterations)
    {
        using namespace runner;
        constexpr float narrow_half_range = 0.35f;
        constexpr float wide_half_range = 1.20f;
        constexpr float requested_offset = 0.15f;
        sim::Environment narrow = initial;
        sim::Environment wide = initial;
        sim::Environment repeated = initial;
        sim::EnvironmentTestAccess::set_motor_half_range(
            narrow, motor_index, narrow_half_range);
        sim::EnvironmentTestAccess::set_motor_half_range(
            wide, motor_index, wide_half_range);
        sim::EnvironmentTestAccess::set_motor_half_range(
            repeated, motor_index, wide_half_range);
        const float initial_angle = initial.motor_diagnostic(
            motor_index).current_angle;
        sim::EnvironmentTestAccess::solve_powered_motor_probe(
            narrow, motor_index,
            direction * requested_offset / narrow_half_range,
            solver_iterations);
        sim::EnvironmentTestAccess::solve_powered_motor_probe(
            wide, motor_index,
            direction * requested_offset / wide_half_range,
            solver_iterations);
        sim::EnvironmentTestAccess::solve_powered_motor_probe(
            repeated, motor_index,
            direction * requested_offset / wide_half_range,
            solver_iterations);

        MotorRomEquivalenceProbe result{};
        result.narrow_response = wrap_angle(
            narrow.motor_diagnostic(motor_index).current_angle - initial_angle);
        result.wide_response = wrap_angle(
            wide.motor_diagnostic(motor_index).current_angle - initial_angle);
        result.repeated_response = wrap_angle(
            repeated.motor_diagnostic(motor_index).current_angle - initial_angle);
        const auto& narrow_authority = narrow.raw_authority_telemetry();
        const auto& wide_authority = wide.raw_authority_telemetry();
        result.narrow_torque = narrow_authority.motor_torque[motor_index];
        result.wide_torque = wide_authority.motor_torque[motor_index];
        result.narrow_work =
            narrow_authority.motor_positive_work_joules[motor_index];
        result.wide_work =
            wide_authority.motor_positive_work_joules[motor_index];

        const sim::MotorConstraint& motor = initial.blueprint().motors[
            motor_index];
        const auto particles = initial.particles();
        const float parent_length = length(particles[motor.a].position
            - particles[motor.pivot].position);
        const float driven_length = length(particles[motor.c].position
            - particles[motor.pivot].position);
        float total_mass{};
        for (const sim::Particle& particle : particles)
            total_mass += 1.0f / std::max(particle.inverse_mass, 1.0e-5f);
        const float strength_fraction = clamp(motor.strength / 0.056f,
            0.0f, 1.0f);
        result.maximum_torque = total_mass * 22.0f
            * std::max(parent_length, driven_length)
            * (0.10f + 0.30f * strength_fraction);
        result.maximum_positive_work = result.maximum_torque / 30.0f;
        return result;
    }

    void print_joint_jacobian(std::string_view support,
        std::string_view joint, const JointJacobianProbe& probe)
    {
        std::cerr << "raw-jacobian support=" << support << " joint=" << joint
            << " ankle=" << probe.ankle_central_delta.x << ','
                << probe.ankle_central_delta.y
            << " sole=" << probe.sole_central_delta.x << ','
                << probe.sole_central_delta.y
            << " angle=" << probe.current_central_delta << '/'
                << probe.target_central_delta
            << " error+=" << probe.positive_initial_error << "->"
                << probe.positive_final_error
            << " error-=" << probe.negative_initial_error << "->"
                << probe.negative_final_error
            << " actuator=" << probe.positive_actuator << ','
                << probe.negative_actuator
            << " identity=" << probe.identity << '\n';
    }

    void print_trial(std::string_view name, const RampTrial& trial)
    {
        const float total_normal = trial.normal_impulse[0]
            + trial.normal_impulse[1];
        std::cerr << name
            << " first_contacts=" << trial.first_grounded_support_nodes
            << " com_v=" << trial.center_velocity
            << " pelvis_v=" << trial.pelvis_velocity
            << " ground_pre=" << trial.ground_delta_before_ramp
            << " ground_post=" << trial.ground_delta_after_ramp
            << " tangent=" << trial.tangential_impulse[0] << ','
                << trial.tangential_impulse[1]
            << " normal=" << trial.normal_impulse[0] << ','
                << trial.normal_impulse[1]
            << " load_cop=" << (total_normal > 1.0e-6f
                ? trial.normal_impulse_moment_x / total_normal : 0.0f)
            << " startup_cop=" << (trial.startup_normal_impulse > 1.0e-6f
                ? trial.startup_normal_impulse_moment_x
                    / trial.startup_normal_impulse : 0.0f)
            << " max_com_cop_lead=" << trial.maximum_absolute_com_cop_lead
            << " max_clearance=" << trial.maximum_clearance[0] << ','
                << trial.maximum_clearance[1]
            << " max_toe_deviation="
                << trial.maximum_toe_angle_deviation[0] << ','
                << trial.maximum_toe_angle_deviation[1]
            << " toe_violation="
                << trial.first_toe_range_violation_frame[0] << ','
                << trial.first_toe_range_violation_frame[1]
            << " unload=" << trial.first_unload_frame[0] << ','
                << trial.first_unload_frame[1]
            << " replant=" << trial.first_replant_frame[0] << ','
                << trial.first_replant_frame[1]
            << " unsupported=" << trial.both_unsupported_frames
            << " distance=" << trial.distance
            << " cycles=" << trial.gait_cycles
            << " alternating=" << trial.alternating_steps
            << " loaded-handoffs=" << trial.loaded_handoffs
            << " identity=" << trial.command_mismatches << ','
                << trial.actuator_mismatches << ',' << trial.motor_mismatches
            << " powered_toe=" << trial.powered_toe_commands
            << " friction_work=" << trial.friction_work << ','
                << trial.friction_positive_work
            << " helpers=" << trial.root_or_helper_corrections
            << " invalid=" << static_cast<int>(trial.invalid_reason) << '\n';
    }

    void test_actionless_ledge_scaffold_authority()
    {
        using namespace runner;
        using namespace runner::sim;
        constexpr std::uint16_t left_hand = 9u;
        constexpr std::uint16_t right_hand = 12u;

        struct Probe
        {
            Vec2 hand_position_delta{};
            Vec2 hand_history_delta{};
            Vec2 root_position_delta{};
            Vec2 root_history_delta{};
            float maximum_position_delta{};
            float maximum_history_delta{};
            std::uint32_t grounded_mutations{};
            std::uint32_t hand_contacts{};
            bool active_grasp{};
            bool collision_mutated{};
            AuthoritySourceTelemetry scaffold{};
            AuthoritySourceTelemetry course_collision{};
        };

        const auto run = [](GuidanceMode mode, std::uint16_t node,
                             Vec2 offset)
        {
            Environment environment{ CreatureBlueprint::humanoid(),
                0x7511ED6Eu };
            require(EnvironmentTestAccess::place_node_near_ledge(
                    environment, node, offset),
                "ledge authority fixture could not place its probe node");
            environment.set_guidance_mode(mode);
            environment.set_raw_authority_telemetry_enabled(true);
            const std::vector<Particle> before{
                environment.particles().begin(), environment.particles().end() };
            const std::uint16_t root = environment.blueprint().root_node;
            const bool collided_before =
                EnvironmentTestAccess::course_collision_latched(environment);
            EnvironmentTestAccess::apply_ledge_training_scaffold_probe(
                environment);

            Probe result{};
            const auto after = environment.particles();
            result.hand_position_delta = after[node].position
                - before[node].position;
            result.hand_history_delta = after[node].previous
                - before[node].previous;
            result.root_position_delta = after[root].position
                - before[root].position;
            result.root_history_delta = after[root].previous
                - before[root].previous;
            for (std::size_t index = 0; index < after.size(); ++index)
            {
                result.maximum_position_delta = std::max(
                    result.maximum_position_delta,
                    length(after[index].position - before[index].position));
                result.maximum_history_delta = std::max(
                    result.maximum_history_delta,
                    length(after[index].previous - before[index].previous));
                result.grounded_mutations += static_cast<std::uint32_t>(
                    after[index].grounded != before[index].grounded);
            }
            result.hand_contacts = environment.hand_ledge_contacts();
            result.active_grasp = EnvironmentTestAccess::active_ledge_grasp(
                environment);
            result.collision_mutated =
                EnvironmentTestAccess::course_collision_latched(environment)
                != collided_before;
            const RawAuthorityTelemetry& authority =
                environment.raw_authority_telemetry();
            result.scaffold = authority.sources[static_cast<std::size_t>(
                AuthoritySource::ledge_training_scaffold)];
            result.course_collision = authority.sources[
                static_cast<std::size_t>(AuthoritySource::course_collision)];
            return result;
        };

        const auto require_exact_no_op = [](const Probe& value,
                                             const char* context)
        {
            const bool no_source = value.scaffold.center_velocity_delta_x == 0.0f
                && value.scaffold.center_velocity_delta_y == 0.0f
                && value.scaffold.center_position_delta_x == 0.0f
                && value.scaffold.center_position_delta_y == 0.0f
                && value.scaffold.center_history_delta_x == 0.0f
                && value.scaffold.center_history_delta_y == 0.0f
                && value.scaffold.root_position_delta_x == 0.0f
                && value.scaffold.root_position_delta_y == 0.0f
                && value.scaffold.torso_position_delta_x == 0.0f
                && value.scaffold.torso_position_delta_y == 0.0f
                && value.scaffold.applications == 0u;
            require(value.hand_position_delta.x == 0.0f
                    && value.hand_position_delta.y == 0.0f
                    && value.hand_history_delta.x == 0.0f
                    && value.hand_history_delta.y == 0.0f
                    && value.root_position_delta.x == 0.0f
                    && value.root_position_delta.y == 0.0f
                    && value.root_history_delta.x == 0.0f
                    && value.root_history_delta.y == 0.0f
                    && value.maximum_position_delta == 0.0f
                    && value.maximum_history_delta == 0.0f
                    && value.grounded_mutations == 0u
                    && value.hand_contacts == 0u && !value.active_grasp
                    && !value.collision_mutated && no_source,
                context);
        };

        constexpr Vec2 positive_offset{ 0.08f, 0.09f };
        constexpr Vec2 mirrored_offset{ -0.08f, -0.09f };
        const Probe raw_left = run(GuidanceMode::raw_policy_audit,
            left_hand, positive_offset);
        const Probe raw_right = run(GuidanceMode::raw_policy_audit,
            right_hand, mirrored_offset);
        const Probe raw_repeat = run(GuidanceMode::raw_policy_audit,
            left_hand, positive_offset);
        require_exact_no_op(raw_left,
            "Raw nearby left hand received actionless ledge authority");
        require_exact_no_op(raw_right,
            "Raw mirrored nearby right hand received actionless ledge authority");
        require_exact_no_op(raw_repeat,
            "repeated Raw nearby hand received actionless ledge authority");

        const Probe assisted_positive = run(GuidanceMode::assisted,
            left_hand, positive_offset);
        const Probe assisted_mirrored = run(GuidanceMode::assisted,
            left_hand, mirrored_offset);
        const Probe assisted_repeat = run(GuidanceMode::assisted,
            left_hand, positive_offset);
        require(authority_source_name(
                    AuthoritySource::ledge_training_scaffold)
                == "ASSISTED LEDGE SCAFFOLD",
            "assisted ledge scaffold is not explicitly labeled");
        require(assisted_positive.hand_contacts == 1u
                && assisted_positive.active_grasp
                && assisted_positive.scaffold.applications == 1u
                && assisted_positive.maximum_position_delta > 0.0f
                && assisted_positive.maximum_history_delta > 0.0f
                && assisted_positive.scaffold.center_position_delta_x != 0.0f
                && assisted_positive.scaffold.center_history_delta_x != 0.0f
                && assisted_positive.course_collision.applications == 0u,
            "Assisted hand scaffold was not isolated in its authority source");
        require(std::abs(assisted_positive.hand_position_delta.x
                    + assisted_mirrored.hand_position_delta.x) <= 1.0e-6f
                && std::abs(assisted_positive.hand_position_delta.y
                    + assisted_mirrored.hand_position_delta.y) <= 1.0e-6f
                && std::abs(assisted_positive.hand_history_delta.x
                    + assisted_mirrored.hand_history_delta.x) <= 1.0e-6f
                && std::abs(assisted_positive.hand_history_delta.y
                    + assisted_mirrored.hand_history_delta.y) <= 1.0e-6f,
            "Assisted ledge scaffold does not mirror around its physical anchor");
        require(assisted_positive.hand_position_delta.x
                    == assisted_repeat.hand_position_delta.x
                && assisted_positive.hand_position_delta.y
                    == assisted_repeat.hand_position_delta.y
                && assisted_positive.hand_history_delta.x
                    == assisted_repeat.hand_history_delta.x
                && assisted_positive.hand_history_delta.y
                    == assisted_repeat.hand_history_delta.y
                && assisted_positive.scaffold.center_position_delta_x
                    == assisted_repeat.scaffold.center_position_delta_x
                && assisted_positive.scaffold.center_position_delta_y
                    == assisted_repeat.scaffold.center_position_delta_y
                && assisted_positive.scaffold.center_history_delta_x
                    == assisted_repeat.scaffold.center_history_delta_x
                && assisted_positive.scaffold.center_history_delta_y
                    == assisted_repeat.scaffold.center_history_delta_y
                && assisted_positive.scaffold.applications
                    == assisted_repeat.scaffold.applications,
            "same-seed Assisted ledge scaffold was not deterministic");

        const Probe assisted_ineligible = run(GuidanceMode::assisted,
            CreatureBlueprint::humanoid().torso_node, positive_offset);
        require_exact_no_op(assisted_ineligible,
            "non-manipulator proximity activated the ledge scaffold");

        Environment transitioned{ CreatureBlueprint::humanoid(), 0x7511ED6Eu };
        require(EnvironmentTestAccess::place_node_near_ledge(
                transitioned, left_hand, positive_offset),
            "authority-transition fixture could not place its hand");
        transitioned.set_guidance_mode(GuidanceMode::assisted);
        transitioned.set_raw_authority_telemetry_enabled(true);
        EnvironmentTestAccess::apply_ledge_training_scaffold_probe(transitioned);
        require(EnvironmentTestAccess::active_ledge_grasp(transitioned),
            "Assisted ledge scaffold did not establish the transition fixture");
        const std::vector<Particle> transition_before{
            transitioned.particles().begin(), transitioned.particles().end() };
        const AuthoritySourceTelemetry source_before =
            transitioned.raw_authority_telemetry().sources[
                static_cast<std::size_t>(
                    AuthoritySource::ledge_training_scaffold)];
        transitioned.set_guidance_mode(GuidanceMode::raw_policy_audit);
        require(!EnvironmentTestAccess::active_ledge_grasp(transitioned),
            "entering Raw retained an Assisted ledge anchor");
        require(transitioned.hand_ledge_contacts() == 0u
                && transitioned.climb_support_transfers() == 0u
                && transitioned.ledge_climbs() == 0u
                && transitioned.controlled_descents() == 0u,
            "entering Raw retained Assisted ledge progress credit");
        const std::uint32_t raw_contacts_before =
            transitioned.hand_ledge_contacts();
        EnvironmentTestAccess::apply_ledge_training_scaffold_probe(transitioned);
        const auto transition_after = transitioned.particles();
        for (std::size_t index = 0; index < transition_after.size(); ++index)
        {
            require(transition_after[index].position.x
                        == transition_before[index].position.x
                    && transition_after[index].position.y
                        == transition_before[index].position.y
                    && transition_after[index].previous.x
                        == transition_before[index].previous.x
                    && transition_after[index].previous.y
                        == transition_before[index].previous.y
                    && transition_after[index].grounded
                        == transition_before[index].grounded,
                "Raw authority transition moved ledge-proximate body state");
        }
        const AuthoritySourceTelemetry source_after =
            transitioned.raw_authority_telemetry().sources[
                static_cast<std::size_t>(
                    AuthoritySource::ledge_training_scaffold)];
        require(transitioned.hand_ledge_contacts() == raw_contacts_before
                && source_after.center_position_delta_x
                    == source_before.center_position_delta_x
                && source_after.center_position_delta_y
                    == source_before.center_position_delta_y
                && source_after.center_history_delta_x
                    == source_before.center_history_delta_x
                && source_after.center_history_delta_y
                    == source_before.center_history_delta_y
                && source_after.applications == source_before.applications,
            "Raw authority transition registered a new actionless ledge contact");
    }

    void test_moving_sole_purchase_on_soil()
    {
        using namespace runner;
        for (const float sign : { -1.0f, 1.0f })
        {
            const auto unloading = sim::EnvironmentTestAccess::sole_patch(
                sign * 0.08f, sign * 0.4f, 0.025f, true, sign * 0.50f);
            require(unloading.used <= unloading.capacity
                    && unloading.positive_work == 0.0f
                    && std::abs(unloading.speed - sign * 0.4f) < 1.0e-5f,
                "rolling foot retained expired grip or converted it into propulsion");
        }
        for (const std::uint64_t seed : { 0x7501u, 0x7502u })
        {
            sim::Environment base{ sim::CreatureBlueprint::humanoid(), seed };
            rl::configure_policy_evaluation_environment(base, sim::CourseStage::uneven,
                0.30f, sim::GaitTask::walk, sim::GuidanceMode::raw_policy_audit, 0.0f);
            float soil_x{};
            for (int sample = 1; sample < 750; ++sample)
            {
                const float x = 0.05f * static_cast<float>(sample);
                bool yielding = true;
                for (int offset = 0; offset < 6; ++offset)
                    yielding = yielding && base.terrain_firmness_at(
                        x + 0.07f * static_cast<float>(offset)) < 0.90f;
                if (yielding)
                {
                    soil_x = x;
                    break;
                }
            }
            require(soil_x > 0.0f, "walk test did not locate a yielding terrain surface");
            auto accepted = base;
            const auto placed = sim::EnvironmentTestAccess::walk_purchase_on_soil(
                accepted, soil_x, 0.0f, 2.0f);
            require(placed[0] < -0.002f && std::abs(placed[1]) < 1.0e-6f
                    && placed[2] == 1.0f,
                "a physically flat and loaded sole was rejected at the resolved soil plane");
            auto repeated = base;
            require(placed == sim::EnvironmentTestAccess::walk_purchase_on_soil(
                    repeated, soil_x, 0.0f, 2.0f),
                "same-seed yielding sole purchase changed");
            auto weightless = base;
            require(sim::EnvironmentTestAccess::walk_purchase_on_soil(
                    weightless, soil_x, 0.0f, 0.0f)[2] == -1.0f,
                "an unloaded sole was counted as a weight transfer");
            auto heel_touch = base;
            const auto partial = sim::EnvironmentTestAccess::walk_purchase_on_soil(
                heel_touch, soil_x, 0.030f, 2.0f);
            require(partial[1] > sim::human_flat_sole_purchase_clearance
                    && partial[2] == -1.0f,
                "a partial touchdown was counted as full-sole purchase");
            auto roll_off = base;
            require(sim::EnvironmentTestAccess::walk_purchase_on_soil(
                    roll_off, soil_x, 0.0f, 2.0f, 0.035f)[2] == 1.0f,
                "loaded receiving sole could not take over while old heel rolled off");
            auto unsupported_roll_off = base;
            require(sim::EnvironmentTestAccess::walk_purchase_on_soil(
                    unsupported_roll_off, soil_x, 0.0f, 2.0f, 0.035f, 0.0f)[2] == -1.0f,
                "unloaded trailing contact was accepted as a supported exchange");
            auto early_touch = base;
            require(sim::EnvironmentTestAccess::walk_purchase_on_soil(
                    early_touch, soil_x, 0.030f, 2.0f, 0.035f)[2] == -1.0f,
                "trailing heel roll-off bypassed receiving-sole geometry");
            auto heel_without_load = base;
            require(sim::EnvironmentTestAccess::walk_purchase_on_soil(
                    heel_without_load, soil_x, 0.0f, 2.0f, 0.035f, 1.0f, false)[2] == -1.0f,
                "a near-plane but unloaded receiving heel was counted as planted");
        }
    }

    void test_unsupported_stand_contact()
    {
        using namespace runner;
        for (const float sign : { -1.0f, 1.0f })
        {
            const auto stick = sim::EnvironmentTestAccess::sole_patch(
                sign * 0.001f, sign * 0.035f, 1.0f, true);
            require(std::abs(stick.displacement) < 1.0e-6f
                    && std::abs(stick.speed) < 1.0e-6f
                    && stick.used > 0.0f && stick.used <= stick.capacity
                    && stick.positive_work <= 1.0e-7f,
                "loaded heel did not dissipatively resist geometric creep");
            const auto slip = sim::EnvironmentTestAccess::sole_patch(
                sign * 0.11f, sign * 0.80f, 0.025f, true);
            require(std::abs(slip.displacement - sign * 0.11f) < 1.0e-6f
                    && slip.speed * sign > 0.0f && slip.speed * sign < 0.80f
                    && slip.used <= slip.capacity && slip.positive_work <= 1.0e-7f,
                "over-budget heel was pinned, accelerated, or exceeded Coulomb traction");
            for (const bool grounded : { false, true })
            {
                const auto unloaded = sim::EnvironmentTestAccess::sole_patch(
                    sign * 0.001f, sign * 0.035f, grounded ? 0.0f : 1.0f,
                    grounded);
                require(std::abs(unloaded.displacement - sign * 0.001f) < 1.0e-6f
                        && std::abs(unloaded.speed - sign * 0.035f) < 1.0e-5f
                        && unloaded.used == 0.0f,
                    "an unloaded or airborne heel received traction");
            }
        }

        const auto rig = sim::CreatureBlueprint::humanoid();
        for (std::uint64_t seed = 0u; seed < 6u; ++seed)
        {
            sim::Environment environment{ rig, 0x751700u + seed * 4099u };
            rl::configure_policy_evaluation_environment(environment, sim::CourseStage::balance,
                0.25f, sim::GaitTask::walk, sim::GuidanceMode::raw_policy_audit, 0.0f);
            environment.set_trial_time_limit(25.0f);
            environment.set_raw_authority_telemetry_enabled(true);
            const std::array<std::uint16_t, 3> tracked{ rig.root_node,
                rig.left_contact_node, rig.right_contact_node };
            std::array<float, 3> initial{};
            std::array<float, 3> settled{};
            std::array<float, 3> excursion{};
            std::array<Vec2, 2> previous_arms{};
            std::array<float, 2> settled_arm_travel{};
            for (std::size_t index = 0; index < tracked.size(); ++index)
                initial[index] = environment.particles()[tracked[index]].position.x;
            for (int tick = 0; tick < 1200; ++tick)
            {
                const auto step = environment.step(rl::balance_teacher_action(environment));
                const auto& audit = environment.raw_authority_telemetry();
                require(!step.terminated && !environment.non_foot_grounded()
                        && audit.root_drive_corrections == 0u
                        && audit.torso_drive_corrections == 0u
                        && audit.course_motion_corrections == 0u
                        && audit.friction_positive_work_joules <= 1.0e-7f
                        && audit.sources[static_cast<std::size_t>(
                            sim::AuthoritySource::posture_helper)].applications == 0u,
                    "Stand fell, gained friction energy, or received external support");
                const auto pose = environment.particles();
                const std::array<Vec2, 2> arms{
                    pose[rig.motors[5].c].position - pose[rig.motors[4].pivot].position,
                    pose[rig.motors[7].c].position - pose[rig.motors[6].pivot].position };
                for (std::size_t side = 0; side < arms.size(); ++side)
                {
                    if (tick >= 300)
                        settled_arm_travel[side] += std::abs(signed_angle(
                            previous_arms[side], arms[side]));
                    previous_arms[side] = arms[side];
                }
                for (std::size_t index = 0; index < tracked.size(); ++index)
                {
                    const float x = pose[tracked[index]].position.x;
                    if (tick == 299)
                        settled[index] = x;
                    if (tick >= 300)
                        excursion[index] = std::max(excursion[index], std::abs(x - settled[index]));
                }
            }
            const auto pose = environment.particles();
            const Vec2 neck = pose[rig.head_node].position - pose[rig.torso_node].position;
            const Vec2 left = pose[rig.motors[5].c].position - pose[rig.motors[4].pivot].position;
            const Vec2 right = pose[rig.motors[7].c].position - pose[rig.motors[6].pivot].position;
            std::cout << "stationary Stand seed=" << seed << " pelvis="
                << pose[rig.root_node].position.x - initial[0]
                << " after settling=" << excursion[0] << ',' << excursion[1]
                << ',' << excursion[2]
                << " neck/arms=" << signed_angle(Vec2{0.0f, 1.0f}, neck)
                << ',' << signed_angle(Vec2{0.0f, -1.0f}, left)
                << ',' << signed_angle(Vec2{0.0f, -1.0f}, right)
                << " mean-settled-arm-rad/s=" << settled_arm_travel[0] / 15.0f
                << ',' << settled_arm_travel[1] / 15.0f << '\n';
            require(std::abs(pose[rig.root_node].position.x - initial[0]) < 0.075f
                    && excursion[0] < 0.06f && excursion[1] < 0.055f && excursion[2] < 0.055f,
                "upright Stand still drifts after settling");
            require(std::abs(signed_angle(Vec2{0.0f, 1.0f}, neck)) < 0.26f
                    && std::abs(signed_angle(Vec2{0.0f, -1.0f}, left)) < 0.32f
                    && std::abs(signed_angle(Vec2{0.0f, -1.0f}, right)) < 0.32f
                    && std::abs(signed_angle(left, right)) < 0.20f,
                "unsupported Stand lost its relaxed neck/arm posture");
            require(std::max(settled_arm_travel[0], settled_arm_travel[1]) / 15.0f < 0.45f,
                "unsupported Stand has sustained visible arm oscillation");
        }
    }

    void test_raw_stop_evidence_contract()
    {
        namespace diagnostics = runner::diagnostics;
        static_assert(diagnostics::raw_stop_replay_frames == 1200u);
        diagnostics::RawStopSeedEvidence accepted{};
        accepted.seed = 0x7510u;
        accepted.requested_direction = 0.0f;
        accepted.maximum_horizontal_drift = std::nextafter(
            diagnostics::raw_stop_maximum_horizontal_drift, 0.0f);
        accepted.maximum_horizontal_speed = std::nextafter(
            diagnostics::raw_stop_maximum_horizontal_speed, 0.0f);
        accepted.completed_frames = diagnostics::raw_stop_replay_frames;
        accepted.stop_requested = true;
        accepted.finite_motion = true;
        accepted.body_integrity = true;
        accepted.stationary_course = true;
        accepted.deterministic_repeat = true;
        accepted.passed = true;
        require(diagnostics::raw_stop_seed_qualifies(accepted),
            "valid retained Raw stop evidence was rejected");

        const auto require_rejected = [](auto mutation, const char* message)
        {
            diagnostics::RawStopSeedEvidence evidence{};
            evidence.requested_direction = 0.0f;
            evidence.maximum_horizontal_drift = 0.25f;
            evidence.maximum_horizontal_speed = 0.05f;
            evidence.completed_frames = diagnostics::raw_stop_replay_frames;
            evidence.stop_requested = true;
            evidence.finite_motion = true;
            evidence.body_integrity = true;
            evidence.stationary_course = true;
            evidence.deterministic_repeat = true;
            mutation(evidence);
            require(!diagnostics::raw_stop_seed_qualifies(evidence), message);
        };
        require_rejected([](auto& value) { value.requested_direction = 1.0f; },
            "moving intent passed the Raw stop gate");
        require_rejected([](auto& value) { value.stop_requested = false; },
            "missing stop request passed the Raw stop gate");
        require_rejected([](auto& value) {
            value.maximum_horizontal_drift =
                diagnostics::raw_stop_maximum_horizontal_drift;
        }, "drift threshold was not fail-closed");
        require_rejected([](auto& value) {
            value.maximum_horizontal_speed =
                diagnostics::raw_stop_maximum_horizontal_speed;
        }, "speed threshold was not fail-closed");
        require_rejected([](auto& value) {
            value.maximum_horizontal_drift =
                std::numeric_limits<float>::quiet_NaN();
        }, "NaN drift passed the Raw stop gate");
        require_rejected([](auto& value) {
            value.maximum_horizontal_speed =
                std::numeric_limits<float>::infinity();
        }, "infinite speed passed the Raw stop gate");
        require_rejected([](auto& value) { --value.completed_frames; },
            "truncated stop replay passed the Raw stop gate");
        require_rejected([](auto& value) { value.gait_cycles = 1u; },
            "gait cycling passed the Raw stop gate");
        require_rejected([](auto& value) { value.alternating_steps = 1u; },
            "alternating step passed the Raw stop gate");
        require_rejected([](auto& value) {
            value.non_foot_contact_frames = 1u;
        }, "non-foot contact passed the Raw stop gate");
        require_rejected([](auto& value) {
            value.raw_action_mismatches = 1u;
        }, "Raw/effective action mismatch passed the stop gate");
        require_rejected([](auto& value) {
            value.optional_authority_applications = 1u;
        }, "optional authority passed the Raw stop gate");
        require_rejected([](auto& value) {
            value.invalid_reason = runner::sim::InvalidMotion::fallen;
        }, "invalid motion passed the Raw stop gate");
        require_rejected([](auto& value) { value.body_integrity = false; },
            "broken body passed the Raw stop gate");
        require_rejected([](auto& value) { value.stationary_course = false; },
            "moving course passed the Raw stop gate");
        require_rejected([](auto& value) { value.finite_motion = false; },
            "non-finite motion passed the Raw stop gate");
        require_rejected([](auto& value) {
            value.deterministic_repeat = false;
        }, "non-repeatable stop replay passed the Raw stop gate");

        diagnostics::RawInferenceReport report{};
        report.retained_policy = true;
        report.checkpoint_truthful = true;
        report.stop_behavior_evidence = true;
        report.stop_seeds[0] = accepted;
        std::ostringstream output{};
        diagnostics::write_raw_inference_report(report, output);
        const std::string text = output.str();
        require(text.contains("stop_evidence=1")
                && text.contains("stop seed=0x7510")
                && text.contains("drift=")
                && text.contains("optional_authority=0"),
            "Raw diagnostic output omitted explicit stop evidence");
    }
}

int main(int argc, char* argv[])
try
{
    const std::string_view diagnostic = argc >= 2 ? argv[1] : "";
    const auto load_walk_motor_trace = [](const char* path)
    {
        using namespace runner;
        using Action = std::array<float, sim::action_count>;
        std::ifstream trace{ path };
        require(trace.good(), "missing Walk motor trace");
        std::string line;
        std::getline(trace, line);
        std::istringstream header{ line };
        std::size_t first_action{};
        std::string field;
        while (std::getline(header, field, ',') && field != "request0")
            ++first_action;
        require(field == "request0", "Walk trace has no motor requests");
        std::vector<Action> requests;
        while (std::getline(trace, line))
        {
            std::istringstream row{ line };
            for (std::size_t column = 0; column < first_action; ++column)
                require(static_cast<bool>(std::getline(row, field, ',')), "short Walk trace row");
            Action request{};
            for (float& value : request)
            {
                require(static_cast<bool>(std::getline(row, field, ',')), "short Walk motor row");
                value = std::stof(field);
            }
            requests.push_back(request);
        }
        return requests;
    };
    if (argc == 3 && diagnostic == "--walk-hip-ab")
    {
        using namespace runner;
        const auto requests = load_walk_motor_trace(argv[2]);
        require(requests.size() >= 324u, "Walk trace ends before the post-transfer A/B");
        const auto rig = sim::CreatureBlueprint::humanoid();
        sim::Environment reference{ rig, 0x751A770u };
        rl::configure_policy_evaluation_environment(reference, sim::CourseStage::uneven,
            0.30f, sim::GaitTask::walk, sim::GuidanceMode::raw_policy_audit, 0.0f);
        const auto pitch = [&](const sim::Environment& state)
        {
            const auto pose = state.particles();
            const Vec2 trunk = pose[rig.torso_node].position - pose[rig.root_node].position;
            return std::atan2(trunk.x * state.facing_direction(), trunk.y);
        };
        for (std::size_t frame = 0; frame <= 300u; ++frame)
        {
            if (frame >= 160u && frame % 20u == 0u)
            {
                const auto measure = [&](bool damped)
                {
                    auto branch = reference;
                    float prior = pitch(branch);
                    float peak = std::abs(prior);
                    float squared{};
                    float squared_rate{};
                    unsigned samples{};
                    for (std::size_t tick = frame; tick < frame + 24u; ++tick)
                    {
                        auto action = requests[tick];
                        const auto& support = branch.human_support_feedback();
                        const int side = rl::human_walk_dominant_support_side(support,
                            branch.left_supported(), branch.right_supported());
                        if (damped && side != 0)
                        {
                            const auto slot = side < 0 ? 0u : 2u;
                            const auto obs = branch.observation();
                            action[slot] = clamp(action[slot] + rl::human_walk_stance_hip_trim(
                                branch.human_gait_coordinator(), support, side,
                                obs[0], -6.0f * obs[sim::torso_turn_rate_observation]), -1.0f, 1.0f);
                        }
                        const auto step = branch.step(action);
                        if (step.terminated || branch.non_foot_grounded())
                            break;
                        const float angle = pitch(branch);
                        const float rate = 60.0f * wrap_angle(angle - prior);
                        prior = angle;
                        peak = std::max(peak, std::abs(angle));
                        squared += angle * angle;
                        squared_rate += rate * rate;
                        ++samples;
                    }
                    const float denominator = static_cast<float>(std::max(1u, samples));
                    return std::tuple{ samples, peak, std::sqrt(squared / denominator),
                        std::sqrt(squared_rate / denominator) };
                };
                const auto [base_life, base_peak, base_rms, base_rate] = measure(false);
                const auto [damped_life, damped_peak, damped_rms, damped_rate] = measure(true);
                std::cout << "hip-ab f=" << frame << " life=" << base_life << '/' << damped_life
                    << " peak=" << base_peak << '/' << damped_peak
                    << " rms=" << base_rms << '/' << damped_rms
                    << " rate=" << base_rate << '/' << damped_rate << '\n';
            }
            static_cast<void>(reference.step(requests[frame]));
        }
        return 0;
    }
    const bool continue_two_transfers = (argc == 3 || argc == 4)
        && (diagnostic == "--walk-third-optimize" || diagnostic == "--walk-third-prefix");
    if (continue_two_transfers || ((argc == 2 || argc == 3)
            && (diagnostic == "--walk-viability-learn" || diagnostic == "--walk-smoothness-search")))
    {
        // Offline diagnostic, not a shipping controller or a gait gate.
        // Search only ordinary requested actions on copies of the same plant.
        // Receding windows score on simulated contacts, never write state, and
        // execute only the first knot of each winning plan on the reference.
        using namespace runner;
        constexpr std::array<std::size_t, 8> slots{ 0u, 1u, 2u, 3u,
            sim::human_left_ankle_action, sim::human_right_ankle_action,
            sim::human_left_toe_action, sim::human_right_toe_action };
        using Trim = std::array<float, slots.size()>;
        using Profile = std::array<Trim, 6>;
        constexpr int frames_per_knot = 15;
        constexpr int horizon = 90;
        constexpr int windows = 18;
        constexpr int extended_windows = 32;
        const bool prefer_smooth_motion = continue_two_transfers
            || diagnostic == "--walk-smoothness-search";
        const auto saved_requests = continue_two_transfers
            ? load_walk_motor_trace(argv[2])
            : std::vector<std::array<float, sim::action_count>>{};
        struct Result
        {
            float score{};
            unsigned forecast_handoffs{};
            unsigned stage{};
            int frames{};
            float x{};
        };
        auto initial = sim::Environment{ sim::CreatureBlueprint::humanoid(), 0x751A770u };
        rl::configure_policy_evaluation_environment(initial, sim::CourseStage::uneven,
            0.30f, sim::GaitTask::walk, sim::GuidanceMode::raw_policy_audit, 0.0f);
        std::ofstream trace;
        const bool capture_requested = continue_two_transfers ? argc == 4 : argc == 3;
        if (capture_requested)
        {
            trace.open(argv[argc - 1]);
            require(trace.good(), "could not open offline Walk viability capture");
            trace << "frame,seconds,handoffs,outgoing,leftheel,rightheel,leftload,rightload,heelimpulse,bodycontact,ground";
            for (std::size_t i = 0; i < initial.particles().size(); ++i)
                trace << ",p" << i << "x,p" << i << 'y';
            trace << ",stage,phase,stage_seconds,clear_swing,stable_frames,capture_x,capture_vx,torso_pitch";
            for (std::size_t i = 0; i < sim::human_support_contact_nodes.size(); ++i)
                trace << ",sole_gap" << i << ",sole_load" << i;
            for (std::size_t i = 0; i < sim::action_count; ++i)
                trace << ",request" << i;
            trace << ",left_toe_applied,right_toe_applied";
            trace << '\n';
            trace.precision(std::numeric_limits<float>::max_digits10);
        }
        int captured_frame{};
        float peak_torso_pitch{};
        float peak_torso_rate{};
        float previous_pitch{};
        std::size_t replay_frame{};
        const auto torso_pitch = [](const sim::Environment& state)
        {
            const auto pose = state.particles();
            const auto& rig = state.blueprint();
            const Vec2 trunk = pose[rig.torso_node].position - pose[rig.root_node].position;
            return std::atan2(trunk.x, trunk.y);
        };
        const auto step_reference = [&](const auto& action)
        {
            static_cast<void>(initial.step(action));
            const auto& authority = initial.raw_authority_telemetry();
            require(authority.root_drive_corrections == 0u && authority.torso_drive_corrections == 0u
                    && authority.course_motion_corrections == 0u
                    && !initial.non_foot_grounded(),
                "offline motor viability reference fell or acquired external locomotion authority");
            const float pitch = torso_pitch(initial);
            peak_torso_pitch = std::max(peak_torso_pitch, std::abs(pitch));
            if (initial.elapsed_seconds() > 1.0f / 60.0f)
                peak_torso_rate = std::max(peak_torso_rate,
                    std::abs(wrap_angle(pitch - previous_pitch)) * 60.0f);
            previous_pitch = pitch;
            ++replay_frame;
            if (!trace.is_open())
                return;
            const auto pose = initial.particles();
            const auto& gait = initial.human_gait_coordinator();
            trace << captured_frame++ << ',' << initial.elapsed_seconds() << ','
                << gait.completed_handoffs << ',' << gait.outgoing_side << ','
                << pose[sim::human_left_heel_node].grounded << ','
                << pose[sim::human_right_heel_node].grounded << ','
                << authority.support_normal_impulse[0] << ',' << authority.support_normal_impulse[1]
                << ',' << authority.support_contact_normal_impulse[gait.outgoing_side > 0 ? 0u : 3u]
                << ',' << initial.non_foot_grounded() << ','
                << initial.ground_height_at(pose[initial.blueprint().root_node].position.x);
            for (const auto& particle : pose)
                trace << ',' << particle.position.x << ',' << particle.position.y;
            const auto& feedback = initial.human_support_feedback();
            trace << ',' << static_cast<unsigned>(gait.stage) << ',' << gait.phase
                << ',' << gait.stage_seconds << ',' << gait.established_swing_clearance
                << ',' << static_cast<unsigned>(gait.event_frames)
                << ',' << feedback.capture_point_x << ',' << feedback.center_of_mass_velocity.x << ',' << pitch;
            for (std::size_t i = 0; i < sim::human_support_contact_nodes.size(); ++i)
            {
                const auto& contact = pose[sim::human_support_contact_nodes[i]];
                const float x = contact.position.x;
                const float plane = initial.ground_height_at(x)
                    + sim::ground_contact_offset(true, contact.radius)
                    - sim::support_contact_soil_sink(initial.terrain_firmness_at(x));
                trace << ',' << contact.position.y - plane << ','
                    << authority.support_contact_normal_impulse[i];
            }
            for (float value : action)
                trace << ',' << value;
            trace << ',' << authority.actuator_actions[sim::human_left_toe_action]
                << ',' << authority.actuator_actions[sim::human_right_toe_action];
            trace << '\n';
        };
        initial.set_raw_authority_telemetry_enabled(true);
        if (continue_two_transfers)
        {
            bool observed_third_release = false;
            const bool prefix_only = diagnostic == "--walk-third-prefix";
            auto last_gait = initial.human_gait_coordinator();
            for (const auto& request : saved_requests)
            {
                step_reference(request);
                const auto gait = initial.human_gait_coordinator();
                if (prefix_only && (gait.stage != last_gait.stage
                    || gait.completed_handoffs != last_gait.completed_handoffs))
                    std::cout << "prefix frame=" << replay_frame - 1u
                        << " transfers=" << gait.completed_handoffs
                        << " stage=" << static_cast<unsigned>(gait.stage) << '\n';
                last_gait = gait;
                observed_third_release |= gait.completed_handoffs == 2u
                    && gait.stage >= sim::HumanGaitCoordinatorStage::swing;
                if (observed_third_release && !prefix_only)
                    break;
            }
            require(observed_third_release,
                "recorded motor commands did not reproduce third sole release");
            if (prefix_only)
                return 0;
        }
        else
            for (int frame = 0; frame <= 68; ++frame)
                step_reference(rl::raw_walking_teacher_action(initial));
        const auto action_for = [&](const sim::Environment& state,
            const Profile& profile, int tick, std::size_t index_origin)
        {
            auto result = rl::raw_walking_teacher_action(state);
            const auto upcoming = index_origin + static_cast<std::size_t>(tick);
            if (continue_two_transfers && upcoming < saved_requests.size()
                && state.human_gait_coordinator().completed_handoffs == 2u)
            {
                // A seed for the offline search only. A recorded action is
                // never used by the runtime teacher or the retained policy.
                for (const auto slot : slots)
                    result[slot] = std::lerp(result[slot], saved_requests[upcoming][slot], 0.85f);
            }
            const auto& trims = profile[static_cast<std::size_t>(tick / frames_per_knot)];
            for (std::size_t index = 0; index < slots.size(); ++index)
                result[slots[index]] = clamp(result[slots[index]] + trims[index], -1.0f, 1.0f);
            return result;
        };
        Profile last_plan{};
        std::uint32_t noise_state = 0xc8825e01u;
        const auto random = [&]()
        {
            noise_state ^= noise_state << 13;
            noise_state ^= noise_state >> 17;
            noise_state ^= noise_state << 5;
            return static_cast<float>(noise_state >> 8) * (1.0f / 16777216.0f);
        };
        const int window_count = continue_two_transfers ? 24
            : trace.is_open() && !prefer_smooth_motion ? extended_windows : windows;
        for (int window = 0; window < window_count; ++window)
        {
            const auto index_origin = replay_frame;
            const auto evaluate = [&](const Profile& profile)
            {
                auto state = initial;
                state.set_raw_authority_telemetry_enabled(false);
                Result result{};
                unsigned previous = state.human_gait_coordinator().completed_handoffs;
                float prior_pitch = prefer_smooth_motion ? torso_pitch(state) : 0.0f;
                for (int tick = 0; tick < horizon; ++tick)
                {
                    const auto step = state.step(action_for(state, profile, tick, index_origin));
                    const auto& support = state.human_support_feedback();
                    const auto& gait = state.human_gait_coordinator();
                    result.frames = tick + 1;
                    result.x = state.distance_travelled();
                    result.forecast_handoffs = std::max(result.forecast_handoffs, gait.completed_handoffs);
                    if (gait.completed_handoffs > previous)
                    {
                        result.score += 9.0f * static_cast<float>(gait.completed_handoffs - previous);
                        previous = gait.completed_handoffs;
                        result.stage = 0u;
                    }
                    result.stage = std::max(result.stage, static_cast<unsigned>(gait.stage));
                    if (step.terminated || state.non_foot_grounded())
                    {
                        result.score -= 0.16f * static_cast<float>(horizon - tick);
                        break;
                    }
                    const auto pose = state.particles();
                    const bool left = gait.outgoing_side < 0;
                    const auto toe = left ? sim::human_left_toe_node : sim::human_right_toe_node;
                    const auto heel = left ? sim::human_left_heel_node : sim::human_right_heel_node;
                    const auto support_toe = left ? sim::human_right_toe_node : sim::human_left_toe_node;
                    const float lead = support.valid
                        ? support.capture_point_x - pose[support_toe].position.x : 0.2f;
                    const float vx = support.center_of_mass_velocity.x;
                    const float toe_gap = pose[toe].position.y - pose[toe].radius
                        - state.ground_height_at(pose[toe].position.x);
                    const float forward_heel = pose[heel].position.x - pose[support_toe].position.x;
                    result.score += 0.03f * (1.0f + state.uprightness())
                        - 0.20f * std::max(0.0f, lead - 0.06f)
                        - 0.05f * std::abs(vx - 0.44f)
                        + 0.04f * clamp(forward_heel, -0.5f, 0.4f);
                    result.score += 0.08f * clamp(vx, -1.0f, 0.72f)
                        - 0.15f * std::max(0.0f, 0.82f - state.uprightness());
                    if (prefer_smooth_motion)
                    {
                        const float pitch = torso_pitch(state);
                        const float rate = std::abs(wrap_angle(pitch - prior_pitch)) * 60.0f;
                        prior_pitch = pitch;
                        // Reward pose quality before the first receipt as well
                        // as during recovery. These are search costs, not
                        // posture forces or weakened acceptance thresholds.
                        result.score -= 0.23f * std::max(0.0f, std::abs(pitch) - 0.14f)
                            + 0.018f * std::max(0.0f, rate - 0.65f)
                            + 0.06f * std::max(0.0f, state.stance_slip_speed() - 0.12f);
                    }
                    if (gait.stage == sim::HumanGaitCoordinatorStage::departure)
                        result.score += clamp(toe_gap, 0.0f, 0.06f) * 0.70f;
                    if (continue_two_transfers && gait.completed_handoffs >= 2u)
                    {
                        const float pitch = std::abs(torso_pitch(state));
                        result.score -= 0.70f * std::max(0.0f, pitch - 0.22f);
                        const auto ball = left ? sim::human_left_ball_node : sim::human_right_ball_node;
                        const auto plane_gap = [&](std::uint16_t node)
                        {
                            const auto& particle = pose[node];
                            const float x = particle.position.x;
                            return particle.position.y - state.ground_height_at(x)
                                - sim::ground_contact_offset(true, particle.radius)
                                + sim::support_contact_soil_sink(state.terrain_firmness_at(x));
                        };
                        const float heel_gap = plane_gap(heel);
                        const float ball_gap = plane_gap(ball);
                        const float distal_gap = plane_gap(toe);
                        if (gait.stage == sim::HumanGaitCoordinatorStage::departure)
                            result.score += 1.60f * clamp(std::min({ heel_gap, ball_gap, distal_gap }),
                                0.0f, 0.045f);
                        if (gait.stage >= sim::HumanGaitCoordinatorStage::swing
                            && gait.stage <= sim::HumanGaitCoordinatorStage::landing
                            && pose[heel].grounded)
                            result.score -= 0.85f * clamp(std::max(ball_gap, distal_gap), 0.0f, 0.15f);
                    }
                }
                result.score += static_cast<float>(result.stage) * 0.25f;
                return result;
            };
            Profile best = last_plan;
            auto result = evaluate(best);
            for (const float radius : { 0.55f, 0.40f, 0.26f, 0.17f })
            {
                const auto center = best;
                for (int trial_index = 0; trial_index < 72; ++trial_index)
                {
                    auto trial = center;
                    for (auto& knot : trial)
                        for (auto& value : knot)
                            value = clamp(value + (random() + random() - 1.0f) * radius, -1.0f, 1.0f);
                    const auto candidate = evaluate(trial);
                    if (candidate.score > result.score)
                    {
                        result = candidate;
                        best = trial;
                    }
                }
            }
            if (result.frames < frames_per_knot)
                break;
            // The forecast uses one fixed index origin. Don't shift it while
            // executing the chosen actions on the reference.
            const int execute_frames = std::min(frames_per_knot, result.frames - 1);
            for (int tick = 0; tick < execute_frames; ++tick)
                step_reference(action_for(initial, best, tick, index_origin));
            const auto& actual = initial.human_gait_coordinator();
            std::cout << "viability window=" << window
                << " actual_handoffs=" << actual.completed_handoffs
                << " actual_x=" << initial.distance_travelled()
                << " forecast_handoffs=" << result.forecast_handoffs
                << " forecast_life=" << result.frames << " forecast_x=" << result.x << std::endl;
            std::copy(best.begin() + 1, best.end(), last_plan.begin());
            last_plan.back() = best.back();
            if (actual.completed_handoffs >= 5u)
                break;
        }
        std::cout << "viability torso_peak_deg=" << peak_torso_pitch * 180.0f / pi
            << " torso_rate_peak_rad_s=" << peak_torso_rate << '\n';
        return 0;
    }
    if ((argc == 2 || argc == 3) && std::string_view{argv[1]} == "--walk-first-heel")
    {
        constexpr std::uint64_t seed = 0x751A770u;
        using runner::sim::CourseStage;
        std::ofstream capture;
        if (argc == 3)
        {
            capture.open(argv[2]);
            require(capture.good(), "could not open Walk pose capture");
        }
        const auto plant = run_ramp_trial(seed, 1.0f, true, true, CourseStage::uneven,
            false, 1.0f, false, false, nullptr, false, 192, false,
            capture.is_open() ? &capture : nullptr);
        const auto repeat = run_ramp_trial(seed, 1.0f, true, true, CourseStage::uneven,
            false, 1.0f, false, false, nullptr, false, 192);
        const auto passive = run_ramp_trial(seed, 1.0f, false, false, CourseStage::uneven,
            false, 1.0f, false, false, nullptr, false, 192);
        require(plant.loaded_handoffs > 0u && plant.first_handoff_heel_impulse > 0.05f
                && plant.root_or_helper_corrections == 0u && plant.actuator_mismatches == 0u,
            "first verified heel plant lacked normal load or acquired non-joint authority");
        require(plant.first_loaded_handoff_frame == repeat.first_loaded_handoff_frame
                && plant.first_handoff_heel_impulse == repeat.first_handoff_heel_impulse
                && passive.loaded_handoffs == 0u,
            "first heel receipt was non-repeatable or acquired with neutral motor actions");
        std::cout << "First heel receipt only: frame=" << plant.first_loaded_handoff_frame
            << " heel-impulse=" << plant.first_handoff_heel_impulse
            << " verified=" << plant.loaded_handoffs << '\n';
        return !capture.is_open() || capture.good() ? 0 : 1;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--walk-soil-contact")
    {
        test_moving_sole_purchase_on_soil();
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--stand-contact-quality")
    {
        test_unsupported_stand_contact();
        return 0;
    }
    if ((argc == 2 || argc == 4 || argc == 5)
        && std::string_view{argv[1]} == "--stand-probe")
    {
        using namespace runner;
        auto rig = sim::CreatureBlueprint::humanoid();
        rl::PolicyNetwork policy{};
        if (argc >= 4)
        {
            std::string error;
            const auto saved_rig = sim::CreatureBlueprint::load_for_species(
                argv[3], sim::CreatureSpecies::human, error);
            require(saved_rig.has_value(), error);
            rl::PpoTrainer::CheckpointData checkpoint{};
            require(rl::PpoTrainer::read_checkpoint_data(argv[2], checkpoint, error), error);
            require(checkpoint.rig_signature == saved_rig->signature()
                && checkpoint.parameters.size() == policy.parameter_count()
                && checkpoint.training_semantics == rl::training_semantics_version,
                "checkpoint and rig or policy semantics do not match");
            rig = *saved_rig;
            policy.parameters() = std::move(checkpoint.parameters);
        }
        std::ofstream capture;
        if (argc == 5)
        {
            capture.open(argv[4]);
            require(capture.good(), "could not open the Stand pose capture");
            for (const auto& bone : rig.bones)
                capture << "#bone," << bone.a << ',' << bone.b << '\n';
            for (const float radius : rig.radii)
                capture << "#node," << radius << '\n';
        }
        for (int source = 0; source < (argc >= 4 ? 3 : 2); ++source)
        {
            const std::string_view label = source == 0 ? "zero-command"
                : source == 1 ? "finite-teacher" : "saved-policy";
            unsigned passed_survival_gate{};
            for (std::uint64_t i = 0; i < 6u; ++i)
            {
                const bool capture_trial = capture.is_open() && source == 2 && i < 2u;
                sim::Environment environment{ rig, capture_trial
                    ? rl::evaluation_seed(static_cast<std::size_t>(i), 0u)
                    : 0x751700u + i * 4099u };
                environment.set_course(sim::CourseStage::balance, 0.25f);
                environment.set_course_motion_enabled(false);
                environment.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
                environment.set_physical_guidance_authority(0.0f);
                environment.set_raw_authority_telemetry_enabled(true);
                environment.set_trial_time_limit(25.0f);
                const auto start = environment.particles();
                const float start_root_x = start[rig.root_node].position.x;
                const std::array<std::uint16_t, 2> heels{
                    rig.left_contact_node, rig.right_contact_node };
                const std::array<float, 2> start_heel_x{
                    start[heels[0]].position.x, start[heels[1]].position.x };
                std::array<float, 2> maximum_heel_excursion{};
                float maximum_head_back_degrees{};
                float maximum_arm_splay_degrees{};
                bool survival_gate_passed{};
                for (int tick = 0; tick < 1200; ++tick)
                {
                    std::array<float, sim::action_count> action{};
                    if (source == 1)
                        action = rl::balance_teacher_action(environment);
                    else if (source == 2)
                        action = policy.deterministic_action(
                            rl::foundational_policy_observation(environment,
                                sim::CourseStage::balance, sim::GaitTask::walk,
                                sim::GuidanceMode::raw_policy_audit),
                            rl::action_mask_for(environment, sim::CourseStage::balance));
                    const auto step = environment.step(action);
                    const auto& authority = environment.raw_authority_telemetry();
                    require(authority.root_drive_corrections == 0u
                        && authority.torso_drive_corrections == 0u
                        && authority.course_motion_corrections == 0u,
                        "Stand probe unexpectedly applied external guidance");
                    const auto pose = environment.particles();
                    for (std::size_t side = 0; side < heels.size(); ++side)
                    {
                        if (pose[heels[side]].grounded)
                            maximum_heel_excursion[side] = std::max(
                                maximum_heel_excursion[side], std::abs(
                                    pose[heels[side]].position.x - start_heel_x[side]));
                    }
                    const float facing = environment.facing_direction();
                    const auto head = pose[rig.head_node].position
                        - pose[rig.torso_node].position;
                    constexpr float degrees_per_radian = 180.0f / pi;
                    maximum_head_back_degrees = std::max(maximum_head_back_degrees,
                        std::atan2(-head.x * facing, head.y) * degrees_per_radian);
                    if (rig.human_paired_limb_topology())
                    {
                        const auto left_arm = pose[rig.motors[5].c].position
                            - pose[rig.motors[4].pivot].position;
                        const auto right_arm = pose[rig.motors[7].c].position
                            - pose[rig.motors[6].pivot].position;
                        maximum_arm_splay_degrees = std::max(maximum_arm_splay_degrees,
                            std::abs(signed_angle(left_arm, right_arm)) * degrees_per_radian);
                    }
                    if (!survival_gate_passed && !step.terminated
                        && environment.longest_stable_stance_seconds()
                            >= rl::standing_mastery_seconds)
                        survival_gate_passed = rl::stage_motion_qualification(
                            sim::CourseStage::balance, environment).valid;
                    if (capture_trial && tick % 3 == 0)
                    {
                        capture << i << ',' << tick << ',' << environment.elapsed_seconds()
                            << ',' << (step.terminated ? tick : -1)
                            << ',' << sim::invalid_motion_name(environment.invalid_reason())
                            << ',' << environment.stable_stance_seconds()
                            << ',' << environment.left_supported() << ','
                            << environment.right_supported() << ','
                            << environment.ground_height_at(pose[rig.root_node].position.x);
                        for (const auto& particle : pose)
                            capture << ',' << particle.position.x << ',' << particle.position.y;
                        capture << '\n';
                    }
                    if (step.terminated)
                        break;
                }
                passed_survival_gate += survival_gate_passed ? 1u : 0u;
                std::cout << "raw stand source=" << label << " seed=" << i
                    << " stable=" << environment.longest_stable_stance_seconds()
                    << " end=" << environment.elapsed_seconds()
                    << " invalid=" << sim::invalid_motion_name(environment.invalid_reason())
                    << " survival-gate=" << survival_gate_passed
                    << " pelvis-drift-m=" << environment.particles()[rig.root_node].position.x
                        - start_root_x
                    << " max-grounded-heel-excursion-m=" << maximum_heel_excursion[0]
                    << ',' << maximum_heel_excursion[1]
                    << " max-head-back-deg=" << maximum_head_back_degrees
                    << " max-arm-angle-gap-deg=" << maximum_arm_splay_degrees << '\n';
            }
            std::cout << "raw stand source=" << label
                << " survival-gate=" << passed_survival_gate << "/6\n";
        }
        return !capture.is_open() || capture.good() ? 0 : 1;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--transfer-trace")
    {
        for (const float direction : {1.0f, -1.0f})
        {
            std::cerr << "DIRECTION " << direction << '\n';
            (void)run_ramp_trial(0x751A770u, direction, true, true,
                runner::sim::CourseStage::uneven, false, 1.0f, false, false,
                nullptr, false, 240, true);
        }
        return 0;
    }
    if (argc == 2
        && std::string_view{ argv[1] } == "--ledge-authority-only")
    {
        test_actionless_ledge_scaffold_authority();
        std::cout << "Runner Raw ledge authority contract completed\n";
        return 0;
    }
    test_raw_stop_evidence_contract();
    if (argc == 2
        && std::string_view{ argv[1] } == "--stop-predicate-only")
    {
        std::cout << "Runner Raw stop evidence predicate completed\n";
        return 0;
    }
    require(argc == 1, "unknown Raw authority test argument");
    test_actionless_ledge_scaffold_authority();
    constexpr std::uint64_t seed = 0x751A770u;
    const runner::sim::CreatureBlueprint physical_human =
        runner::sim::CreatureBlueprint::humanoid();
    const auto solver_bottom = [&](std::uint16_t node)
    {
        return physical_human.nodes[node].y
            - runner::sim::ground_contact_offset(true,
                physical_human.radii[node]);
    };
    require(std::abs(solver_bottom(runner::sim::human_left_heel_node)
                - solver_bottom(runner::sim::human_left_ball_node)) <= 1.0e-6f
            && std::abs(solver_bottom(runner::sim::human_left_heel_node)
                - solver_bottom(runner::sim::human_left_toe_node)) <= 1.0e-6f
            && std::abs(solver_bottom(runner::sim::human_right_heel_node)
                - solver_bottom(runner::sim::human_right_ball_node)) <= 1.0e-6f
            && std::abs(solver_bottom(runner::sim::human_right_heel_node)
                - solver_bottom(runner::sim::human_right_toe_node)) <= 1.0e-6f,
        "neutral Human support nodes do not share one solver-space plane");
    {
        constexpr float minimum_toe_deviation = -42.0f * runner::pi / 180.0f;
        constexpr float maximum_toe_deviation = 36.0f * runner::pi / 180.0f;
        const auto toe_nodes = [](bool left)
        {
            return left
                ? std::array<std::uint16_t, 3>{
                    runner::sim::human_left_heel_node,
                    runner::sim::human_left_ball_node,
                    runner::sim::human_left_toe_node }
                : std::array<std::uint16_t, 3>{
                    runner::sim::human_right_heel_node,
                    runner::sim::human_right_ball_node,
                    runner::sim::human_right_toe_node };
        };
        const auto toe_center = [&toe_nodes](
            const runner::sim::Environment& environment, bool left)
        {
            Vec2 weighted{};
            float total_mass{};
            for (const std::uint16_t node : toe_nodes(left))
            {
                const runner::sim::Particle& particle =
                    environment.particles()[node];
                const float mass = 1.0f / particle.inverse_mass;
                weighted += particle.position * mass;
                total_mass += mass;
            }
            return weighted / total_mass;
        };
        for (const bool left : { true, false })
        {
            for (const float passive : { minimum_toe_deviation + 0.05f,
                    0.0f, maximum_toe_deviation - 0.05f })
            {
                runner::sim::Environment environment{ physical_human,
                    seed + (left ? 31u : 32u) };
                runner::sim::EnvironmentTestAccess::set_passive_toe_deviation(
                    environment, left, passive);
                const auto before = environment.particles();
                runner::sim::EnvironmentTestAccess::solve_passive_toe_stop(
                    environment, left);
                for (const std::uint16_t node : toe_nodes(left))
                {
                    require(length(environment.particles()[node].position
                                - before[node].position) <= 1.0e-7f
                            && length(environment.particles()[node].previous
                                - before[node].previous) <= 1.0e-7f,
                        "passive Human toe stop applied a servo inside its range");
                }
            }
            for (const float boundary : {
                    minimum_toe_deviation, maximum_toe_deviation })
            {
                runner::sim::Environment environment{ physical_human,
                    seed + (left ? 33u : 34u) };
                runner::sim::EnvironmentTestAccess::set_passive_toe_deviation(
                    environment, left, boundary);
                const auto before = environment.particles();
                runner::sim::EnvironmentTestAccess::solve_passive_toe_stop(
                    environment, left);
                for (const std::uint16_t node : toe_nodes(left))
                    require(length(environment.particles()[node].position
                            - before[node].position) <= 1.0e-5f,
                        "passive Human toe stop moved an exact boundary pose");
            }
            for (const float outside : { minimum_toe_deviation - 0.15f,
                    maximum_toe_deviation + 0.15f })
            {
                runner::sim::Environment environment{ physical_human,
                    seed + (left ? 35u : 36u) };
                runner::sim::EnvironmentTestAccess::set_passive_toe_deviation(
                    environment, left, outside);
                const Vec2 center_before = toe_center(environment, left);
                runner::sim::EnvironmentTestAccess::solve_passive_toe_stop(
                    environment, left);
                const float resolved = runner::sim::EnvironmentTestAccess::
                    passive_toe_deviation(environment, left);
                require(resolved >= minimum_toe_deviation - 1.0e-4f
                        && resolved <= maximum_toe_deviation + 1.0e-4f
                        && std::abs(resolved - outside) > 0.10f,
                    "passive Human toe stop did not oppose both outside signs");
                require(length(toe_center(environment, left) - center_before)
                        <= 1.0e-5f,
                    "passive Human toe stop translated its joint mass center");

                runner::sim::Environment repeated{ physical_human,
                    seed + (left ? 35u : 36u) };
                runner::sim::EnvironmentTestAccess::set_passive_toe_deviation(
                    repeated, left, outside);
                runner::sim::EnvironmentTestAccess::solve_passive_toe_stop(
                    repeated, left);
                for (const std::uint16_t node : toe_nodes(left))
                    require(length(environment.particles()[node].position
                            - repeated.particles()[node].position) == 0.0f,
                        "passive Human toe stop is not deterministic on repeat");
            }
            for (const float signed_rate : { -4.0f, 0.0f, 4.0f })
            {
                runner::sim::Environment environment{ physical_human,
                    seed + (left ? 37u : 38u) };
                runner::sim::Environment repeated{ physical_human,
                    seed + (left ? 37u : 38u) };
                const auto evidence = runner::sim::EnvironmentTestAccess::
                    passive_toe_damping(environment, left, signed_rate);
                const auto repeated_evidence = runner::sim::EnvironmentTestAccess::
                    passive_toe_damping(repeated, left, signed_rate);
                require(std::abs(evidence.relative_rate_before - signed_rate)
                            <= 1.0e-4f
                        && std::abs(evidence.relative_rate_after
                            - signed_rate * 0.30f) <= 1.0e-4f
                        && evidence.kinetic_energy_after
                            <= evidence.kinetic_energy_before + 1.0e-6f
                        && length(evidence.momentum_after
                            - evidence.momentum_before) <= 1.0e-5f
                        && evidence.maximum_position_delta == 0.0f,
                    "reciprocal passive MTP damping injected energy, momentum, pose, or a preferred sign");
                require(evidence.relative_rate_after
                            == repeated_evidence.relative_rate_after
                        && evidence.kinetic_energy_after
                            == repeated_evidence.kinetic_energy_after
                        && evidence.momentum_after.x
                            == repeated_evidence.momentum_after.x
                        && evidence.momentum_after.y
                            == repeated_evidence.momentum_after.y,
                    "reciprocal passive MTP damping is not repeated-seed deterministic");
            }
        }
    }
    const auto knee_cross = [&](std::size_t hip_index,
        std::size_t knee_index)
    {
        const auto& hip = physical_human.motors[hip_index];
        const auto& knee = physical_human.motors[knee_index];
        const Vec2 upper = physical_human.nodes[hip.c]
            - physical_human.nodes[hip.pivot];
        const Vec2 lower = physical_human.nodes[knee.c]
            - physical_human.nodes[knee.pivot];
        return upper.x * lower.y - upper.y * lower.x;
    };
    require(knee_cross(0u, 1u) < -1.0e-4f
            && knee_cross(2u, 3u) < -1.0e-4f,
        "neutral Human contains a backward-facing physical knee");
    for (const int support_side : { -1, 1 })
    {
        for (const float perturbation_sign : { -1.0f, 1.0f })
        {
            runner::sim::Environment environment{ physical_human,
                seed + static_cast<std::uint64_t>(support_side + 2) };
            runner::sim::EnvironmentTestAccess::prepare_torso_balance_probe(
                environment, perturbation_sign * 0.12f,
                perturbation_sign * 0.80f, support_side);
            runner::rl::BipedGaitParameters parameters =
                runner::rl::anatomy_scaled_foundational_gait(
                    environment.blueprint(), environment.gait_task(),
                    environment.elapsed_seconds());
            runner::rl::configure_raw_human_foundational_walk(parameters);
            parameters.direction = environment.locomotion_direction()
                * environment.facing_direction();
            const auto base = runner::rl::biped_gait_teacher_action(
                environment, parameters);
            std::array<Vec2, 128> positions_before{};
            std::array<Vec2, 128> previous_before{};
            std::array<bool, 128> grounded_before{};
            for (std::size_t index = 0;
                index < environment.particles().size(); ++index)
            {
                positions_before[index] = environment.particles()[index].position;
                previous_before[index] = environment.particles()[index].previous;
                grounded_before[index] = environment.particles()[index].grounded;
            }
            const auto balanced = runner::rl::apply_raw_human_support_balance(
                environment, parameters, base);
            const bool left = support_side < 0;
            const std::size_t stance_hip = left ? 0u : 2u;
            const std::size_t stance_knee = stance_hip + 1u;
            const std::size_t stance_ankle = left
                ? runner::sim::human_left_ankle_action
                : runner::sim::human_right_ankle_action;
            const std::size_t swing_hip = left ? 2u : 0u;
            const std::size_t swing_knee = swing_hip + 1u;
            const std::size_t swing_ankle = left
                ? runner::sim::human_right_ankle_action
                : runner::sim::human_left_ankle_action;
            require(balanced.applied && balanced.support_side == support_side
                    && perturbation_sign * balanced.signed_balance > 0.0f,
                "Raw torso feedback did not select the measured support/sign");
            require(balanced.endpoint_error <= 1.0e-7f
                    && length(balanced.stance_endpoint_after
                        - balanced.stance_endpoint_before) <= 1.0e-7f
                    && balanced.action[stance_hip] == base[stance_hip]
                    && balanced.action[stance_knee] == base[stance_knee]
                    && balanced.action[swing_hip] == base[swing_hip]
                    && balanced.action[swing_knee] == base[swing_knee]
                    && balanced.action[swing_ankle] == base[swing_ankle]
                    && perturbation_sign
                        * (balanced.action[stance_ankle]
                            - base[stance_ankle])
                        * static_cast<float>(support_side) > 0.0f,
                "Raw stance balance changed the hip-to-ankle endpoint or swing chain");
            for (std::size_t index = 0;
                index < environment.particles().size(); ++index)
            {
                require(length(environment.particles()[index].position
                            - positions_before[index]) == 0.0f
                        && length(environment.particles()[index].previous
                            - previous_before[index]) == 0.0f
                        && environment.particles()[index].grounded
                            == grounded_before[index],
                    "Raw action composition mutated body/contact state");
            }
        }
    }
    {
        runner::sim::Environment airborne{ physical_human, seed + 9u };
        runner::sim::EnvironmentTestAccess::prepare_torso_balance_probe(
            airborne, 0.12f, 0.80f, 0);
        auto parameters = runner::rl::anatomy_scaled_foundational_gait(
            airborne.blueprint());
        runner::rl::configure_raw_human_foundational_walk(parameters);
        const std::array<float, runner::sim::action_count> action{};
        const auto balance = runner::rl::apply_raw_human_support_balance(
            airborne, parameters, action);
        require(!balance.applied && balance.support_side == 0
                && balance.action == action,
            "airborne Raw torso feedback invented a support-side motor action");
    }
    {
        runner::sim::Environment early{ physical_human, seed + 10u };
        runner::sim::Environment late{ physical_human, seed + 10u };
        runner::sim::EnvironmentTestAccess::prepare_torso_balance_probe(
            early, 0.12f, 0.80f, 2);
        runner::sim::EnvironmentTestAccess::prepare_torso_balance_probe(
            late, 0.12f, 0.80f, 2);
        runner::sim::EnvironmentTestAccess::set_elapsed_seconds(early, 0.125f);
        runner::sim::EnvironmentTestAccess::set_elapsed_seconds(late, 0.875f);
        const auto early_policy = runner::rl::foundational_policy_observation(
            early, runner::sim::CourseStage::uneven,
            runner::sim::GaitTask::walk,
            runner::sim::GuidanceMode::raw_policy_audit);
        const auto late_policy = runner::rl::foundational_policy_observation(
            late, runner::sim::CourseStage::uneven,
            runner::sim::GaitTask::walk,
            runner::sim::GuidanceMode::raw_policy_audit);
        require(early_policy == late_policy
                && early.observation()[
                    runner::rl::elapsed_phase_sine_observation_index]
                    != late.observation()[
                        runner::rl::elapsed_phase_sine_observation_index],
            "double-support alias fixture did not isolate private gait phase");
        auto parameters = runner::rl::anatomy_scaled_foundational_gait(
            early.blueprint());
        runner::rl::configure_raw_human_foundational_walk(parameters);
        std::array<float, runner::sim::action_count> base{};
        const auto early_balance =
            runner::rl::apply_raw_human_support_balance(
                early, parameters, base);
        const auto late_balance =
            runner::rl::apply_raw_human_support_balance(
                late, parameters, base);
        require(!early_balance.applied && !late_balance.applied
                && early_balance.support_side == 0
                && late_balance.support_side == 0
                && early_balance.action == base
                && late_balance.action == base,
            "private gait phase selected opposing double-support balance targets");
    }
    {
        runner::sim::Environment raw{ physical_human, seed + 11u };
        runner::sim::EnvironmentTestAccess::prepare_torso_balance_probe(
            raw, 0.12f, 0.80f, -1);
        auto parameters = runner::rl::anatomy_scaled_foundational_gait(
            raw.blueprint());
        runner::rl::configure_raw_human_foundational_walk(parameters);
        std::array<float, runner::sim::action_count> raw_action{};
        const auto training_target =
            runner::rl::apply_raw_human_support_balance(
                raw, parameters, raw_action);
        const auto effective = runner::rl::effective_policy_action(
            raw, raw_action, runner::sim::CourseStage::uneven, 1.0f,
            runner::sim::GuidanceMode::raw_policy_audit);
        require(training_target.applied
                && training_target.action != raw_action
                && effective == raw_action,
            "trainer-only ankle target leaked into Raw effective actions");
    }
    runner::sim::Environment settled{ physical_human, seed };
    settled.set_course(runner::sim::CourseStage::uneven, 0.30f);
    settled.set_course_motion_enabled(false);
    settled.set_guidance_mode(runner::sim::GuidanceMode::raw_policy_audit);
    settled.set_raw_authority_telemetry_enabled(true);
    std::array<float, runner::sim::action_count> zero_action{};
    for (int frame = 0; frame < 120; ++frame)
    {
        const runner::sim::StepResult settle_step = settled.step(zero_action);
        (void)settle_step;
    }
    const auto require_rom_independent_actuator = [&](const auto& rig,
        std::uint64_t probe_seed, std::size_t motor_index,
        bool require_ground_and_air)
    {
        for (const std::uint64_t seed_offset : { 0u, 101u, 202u })
        {
            runner::sim::Environment base{ rig, probe_seed + seed_offset };
            base.set_course(runner::sim::CourseStage::uneven, 0.30f);
            base.set_course_motion_enabled(false);
            base.set_guidance_mode(
                runner::sim::GuidanceMode::raw_policy_audit);
            base.set_raw_authority_telemetry_enabled(true);
            const runner::sim::StepResult contact_step = base.step(zero_action);
            (void)contact_step;
            for (const bool airborne : { false, true })
            {
                if (!require_ground_and_air && !airborne)
                    continue;
                runner::sim::Environment prepared = base;
                runner::sim::EnvironmentTestAccess::prepare_joint_probe(
                    prepared, airborne);
                for (const int solver_iterations : { 1, 14, 32 })
                {
                    for (const float direction : { -1.0f, 1.0f })
                    {
                        const MotorRomEquivalenceProbe probe =
                            probe_motor_rom_equivalence(prepared,
                                motor_index, direction, solver_iterations);
                        const bool equivalent =
                            direction * probe.narrow_response > 0.0f
                            && direction * probe.wide_response > 0.0f
                            && std::abs(probe.narrow_response
                                - probe.wide_response) <= 1.0e-5f
                            && probe.wide_response
                                == probe.repeated_response
                            && std::abs(probe.narrow_torque
                                - probe.wide_torque) <= 1.0e-3f
                            && std::isfinite(probe.narrow_work)
                            && std::isfinite(probe.wide_work)
                            && std::abs(probe.narrow_torque)
                                <= probe.maximum_torque + 1.0e-4f
                            && std::abs(probe.wide_torque)
                                <= probe.maximum_torque + 1.0e-4f
                            && probe.narrow_work
                                <= probe.maximum_positive_work + 1.0e-4f
                            && probe.wide_work
                                <= probe.maximum_positive_work + 1.0e-4f;
                        require(equivalent,
                            "finite actuator authority changed with motor ROM");
                    }
                }
            }
        }
    };
    require_rom_independent_actuator(
        physical_human, seed + 300u, 1u, true);
    const runner::sim::CreatureBlueprint non_human =
        runner::sim::CreatureBlueprint::chicken();
    require_rom_independent_actuator(
        non_human, seed + 600u, 1u, false);
    for (const bool airborne : { false, true })
    {
        const std::string_view support = airborne ? "airborne" : "grounded";
        const std::array<std::tuple<bool, std::size_t, std::string_view>, 6>
            joints{
                std::tuple{ true, 0u, std::string_view{ "left-hip" } },
                std::tuple{ true, 1u, std::string_view{ "left-knee" } },
                std::tuple{ true, runner::sim::human_left_ankle_action,
                    std::string_view{ "left-ankle" } },
                std::tuple{ false, 2u, std::string_view{ "right-hip" } },
                std::tuple{ false, 3u, std::string_view{ "right-knee" } },
                std::tuple{ false, runner::sim::human_right_ankle_action,
                    std::string_view{ "right-ankle" } }
            };
        for (const auto& [left, motor, name] : joints)
        {
            const JointJacobianProbe probe = probe_joint_jacobian(
                settled, airborne, left, motor);
            const bool physical_response = probe.identity
                    && probe.positive_actuator > 0.0f
                    && probe.negative_actuator < 0.0f
                    && probe.target_central_delta > 0.0f
                    && probe.current_central_delta > 0.0f
                    && probe.current_central_delta
                        <= probe.target_central_delta * 1.01f
                    && std::isfinite(probe.positive_torque)
                    && std::isfinite(probe.negative_torque)
                    && std::isfinite(probe.positive_work)
                    && std::isfinite(probe.negative_work)
                    && std::abs(probe.positive_torque)
                        <= probe.maximum_torque + 1.0e-4f
                    && std::abs(probe.negative_torque)
                        <= probe.maximum_torque + 1.0e-4f
                    && probe.positive_work
                        <= probe.maximum_positive_work + 1.0e-4f
                    && probe.negative_work
                        <= probe.maximum_positive_work + 1.0e-4f;
            const bool ankle = motor == runner::sim::human_left_ankle_action
                || motor == runner::sim::human_right_ankle_action;
            const bool adversarial_short_link = !ankle
                || probe.adjacent_arm_ratio >= 4.0f;
            if (!physical_response)
                print_joint_jacobian(support, name, probe);
            require(physical_response && adversarial_short_link,
                "Human joint action does not reach its declared physical motor target");
        }
    }
    const RampTrial forward_zero = run_ramp_trial(seed, 1.0f,
        false, false);
    const RampTrial reverse_zero = run_ramp_trial(seed, -1.0f,
        false, false);
    const RampTrial forward_teacher = run_ramp_trial(seed, 1.0f,
        true, true);
    const RampTrial reverse_teacher = run_ramp_trial(seed, -1.0f,
        true, true);
    const RampTrial repeated_teacher = run_ramp_trial(seed, 1.0f,
        true, true);

    require(forward_teacher.center_velocity == repeated_teacher.center_velocity
            && forward_teacher.pelvis_velocity
                == repeated_teacher.pelvis_velocity
            && forward_teacher.ground_delta_before_ramp
                == repeated_teacher.ground_delta_before_ramp
            && forward_teacher.ground_delta_after_ramp
                == repeated_teacher.ground_delta_after_ramp,
        "same-seed Raw ramp probe is not deterministic");
    for (const RampTrial* trial : { &forward_zero, &reverse_zero,
            &forward_teacher, &reverse_teacher })
        require(trial->command_mismatches == 0u
                && trial->actuator_mismatches == 0u
                && trial->motor_mismatches == 0u
                && trial->powered_toe_commands == 0u
                && trial->root_or_helper_corrections == 0u,
            "Raw request/effective/actuator/motor authority identity failed");
    require(forward_zero.first_grounded_support_nodes == 6u
            && reverse_zero.first_grounded_support_nodes == 6u,
        "neutral Human sole begins as a heel-only contact rocker");
    require(forward_zero.center_velocity == reverse_zero.center_velocity
            && forward_zero.ground_delta_before_ramp
                == reverse_zero.ground_delta_before_ramp
            && forward_zero.ground_delta_after_ramp
                == reverse_zero.ground_delta_after_ramp,
        "zero joint output acquired an intent-dependent body drive");

    const bool physical_raw_gait =
        forward_teacher.invalid_reason == runner::sim::InvalidMotion::none
        && reverse_teacher.invalid_reason == runner::sim::InvalidMotion::none
        && forward_teacher.distance >= 2.0f
        && reverse_teacher.distance <= -2.0f
        && forward_teacher.gait_cycles >= 1u
        && reverse_teacher.gait_cycles >= 1u
        && forward_teacher.alternating_steps >= 2u
        && reverse_teacher.alternating_steps >= 2u
        && forward_teacher.loaded_handoffs >= 2u
        && reverse_teacher.loaded_handoffs >= 2u
        && forward_teacher.first_unload_frame[0] >= 0
        && forward_teacher.first_unload_frame[1] >= 0
        && forward_teacher.first_replant_frame[0] >= 0
        && forward_teacher.first_replant_frame[1] >= 0
        && reverse_teacher.first_unload_frame[0] >= 0
        && reverse_teacher.first_unload_frame[1] >= 0
        && reverse_teacher.first_replant_frame[0] >= 0
        && reverse_teacher.first_replant_frame[1] >= 0
        && forward_teacher.first_toe_range_violation_frame[0] < 0
        && forward_teacher.first_toe_range_violation_frame[1] < 0
        && reverse_teacher.first_toe_range_violation_frame[0] < 0
        && reverse_teacher.first_toe_range_violation_frame[1] < 0
        && forward_teacher.maximum_absolute_com_cop_lead <= 0.55f
        && reverse_teacher.maximum_absolute_com_cop_lead <= 0.55f;
    if (!physical_raw_gait)
    {
        print_trial("forward-teacher-failure", forward_teacher);
        print_trial("reverse-teacher-failure", reverse_teacher);
    }
    require(physical_raw_gait,
        "Raw gait lacks signed progress, contact transfer, passive toe range, or bounded COM support");

    std::cout << "Runner Raw authority ramp probe completed\n";
    return 0;
}
catch (const std::exception& error)
{
    std::cerr << "Runner Raw authority test failed: " << error.what() << '\n';
    return 1;
}
