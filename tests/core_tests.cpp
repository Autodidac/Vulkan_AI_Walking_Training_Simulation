#include "autonomy.hpp"
#include "ppo.hpp"
#include "simulation.hpp"
#include "ui_layout.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static void solve_motor(Environment& environment,
            const MotorConstraint& motor, float action) noexcept
        {
            environment.solve_motor(motor, action);
        }

        static bool articulated_toes_move(Environment& environment) noexcept
        {
            MotorConstraint left{};
            MotorConstraint right{};
            if (!environment.articulated_toe_motor(true, left)
                || !environment.articulated_toe_motor(false, right))
                return false;
            const float left_before = environment.joint_angle(left);
            const float right_before = environment.joint_angle(right);
            std::array<float, action_count> crouch{};
            crouch[0] = -0.45f;
            crouch[1] = 0.65f;
            crouch[2] = 0.45f;
            crouch[3] = -0.65f;
            for (int frame = 0; frame < 48; ++frame)
            {
                environment.update_articulated_toe_commands(crouch, 1.0f / 60.0f);
                for (int iteration = 0; iteration < 14; ++iteration)
                    environment.solve_articulated_toes();
                environment.limit_articulated_toe_rates(1.0f / 60.0f);
            }
            return std::abs(wrap_angle(environment.joint_angle(left) - left_before)) > 0.01f
                && std::abs(wrap_angle(environment.joint_angle(right) - right_before)) > 0.01f;
        }

        static bool articulated_toe_rate_is_bounded(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.60f);
            MotorConstraint left{};
            if (!environment.articulated_toe_motor(true, left))
                return false;
            std::array<float, action_count> action{};
            float previous = environment.joint_angle(left);
            constexpr float dt = 1.0f / 60.0f;
            for (int frame = 0; frame < 180; ++frame)
            {
                const float sign = (frame & 1) == 0 ? 1.0f : -1.0f;
                action[0] = sign;
                action[1] = -sign;
                environment.update_articulated_toe_commands(action, dt);
                for (int iteration = 0; iteration < 14; ++iteration)
                    environment.solve_articulated_toes();
                environment.limit_articulated_toe_rates(dt);
                const float current = environment.joint_angle(left);
                const float delta = std::abs(wrap_angle(current - previous));
                const bool supported = environment.contact_supported(
                    environment.blueprint_.left_contact_node);
                if (delta > toe_angular_rate_limit(supported,
                        environment.course_stage_) * dt + 0.0002f)
                    return false;
                previous = current;
            }
            return true;
        }

        static void collapse_upper_body(Environment& environment) noexcept
        {
            if (environment.blueprint_.root_node >= environment.particles_.size()
                || environment.blueprint_.torso_node >= environment.particles_.size()
                || environment.blueprint_.head_node >= environment.particles_.size())
                return;
            const Vec2 root = environment.particles_[environment.blueprint_.root_node].position;
            environment.particles_[environment.blueprint_.torso_node].position = root + Vec2{ 0.05f, 0.20f };
            environment.particles_[environment.blueprint_.head_node].position = root + Vec2{ 0.12f, 0.28f };
            environment.particles_[environment.blueprint_.torso_node].previous =
                environment.particles_[environment.blueprint_.torso_node].position;
            environment.particles_[environment.blueprint_.head_node].previous =
                environment.particles_[environment.blueprint_.head_node].position;
        }

        static void detach_left_support_cluster(Environment& environment) noexcept
        {
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (!environment.blueprint_.is_left_support_seed(index))
                    continue;
                environment.particles_[index].position.x += 4.0f;
                environment.particles_[index].previous = environment.particles_[index].position;
            }
        }

        static void set_duck_pressure(Environment& environment, float pressure) noexcept
        {
            environment.duck_obstacle_weight_ = pressure;
        }

        static bool hip_hinge_is_rejected(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            auto pin = [&](std::size_t node)
            {
                if (node >= environment.particles_.size())
                    return;
                Particle& particle = environment.particles_[node];
                particle.position.y = environment.ground_height_at(particle.position.x)
                    + ground_contact_offset(true, particle.radius);
                particle.previous = particle.position;
                particle.grounded = true;
            };
            pin(environment.blueprint_.left_contact_node);
            pin(environment.blueprint_.right_contact_node);
            for (const std::uint16_t node : environment.blueprint_.additional_left_contact_nodes)
                pin(node);
            for (const std::uint16_t node : environment.blueprint_.additional_right_contact_nodes)
                pin(node);
            const Vec2 root = environment.particles_[environment.blueprint_.root_node].position;
            environment.particles_[environment.blueprint_.torso_node].position =
                root + Vec2{ 1.05f, 0.42f };
            environment.particles_[environment.blueprint_.head_node].position =
                root + Vec2{ 1.72f, 0.58f };
            environment.particles_[environment.blueprint_.torso_node].previous =
                environment.particles_[environment.blueprint_.torso_node].position;
            environment.particles_[environment.blueprint_.head_node].previous =
                environment.particles_[environment.blueprint_.head_node].position;
            return !environment.crouch_posture_valid();
        }

        static bool guided_squat_is_valid(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            environment.elapsed_seconds_ = 6.0f;
            environment.duck_press_contact_seen_ = true;
            for (int iteration = 0; iteration < 48; ++iteration)
            {
                environment.stabilize_duck_posture();
                environment.solve_ground(1.0f / 60.0f);
            }
            const CrouchPostureEvidence evidence =
                environment.current_crouch_posture();
            return crouch_posture_qualified(evidence)
                && evidence.pelvis_drop >= 0.30f
                && evidence.left_knee_flex >= 0.16f
                && evidence.right_knee_flex >= 0.16f
                && evidence.torso_pitch <= 0.55f;
        }

        static bool press_collision_resolves_below(Environment& environment) noexcept
        {
            if (!environment.valid_node(environment.blueprint_.head_node))
                return false;
            Particle& head = environment.particles_[environment.blueprint_.head_node];
            head.previous = head.position - Vec2{ 0.07f, -0.11f };
            const Vec2 velocity_before = head.position - head.previous;
            const float x_before = head.position.x;
            const float bottom = head.position.y + head.radius * 0.45f;
            environment.course_features_.clear();
            environment.course_features_.push_back({
                CourseFeatureKind::duck_press,
                { head.position.x, bottom + 0.16f }, { 1.5f, 0.16f }, 0.0f,
                { 0.0f, -0.5f }, -2
            });
            environment.duck_press_contact_this_step_ = false;
            environment.duck_press_max_penetration_ = 0.0f;
            environment.solve_course();
            const Vec2 velocity_after = head.position - head.previous;
            return environment.duck_press_contact_this_step_
                && head.position.y + head.radius <= bottom + 0.0001f
                && std::abs(head.position.x - x_before) < 0.000001f
                && length(velocity_after - velocity_before) < 0.000001f;
        }

        static bool press_anchor_remains_fixed(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            if (!environment.valid_node(environment.blueprint_.root_node))
                return false;
            const float expected = environment.blueprint_.nodes[
                environment.blueprint_.root_node].x;
            Particle& root = environment.particles_[environment.blueprint_.root_node];
            root.position.x += 1.75f;
            root.previous.x += 1.75f;
            environment.elapsed_seconds_ = 3.5f;
            environment.duck_press_completed_ = false;
            environment.rebuild_course_features();
            return environment.course_features_.size() == 1u
                && environment.course_features_.front().kind == CourseFeatureKind::duck_press
                && std::abs(environment.course_features_.front().center.x - expected) < 0.000001f;
        }

        static void force_fused_supports(Environment& environment) noexcept
        {
            if (!environment.valid_node(environment.blueprint_.left_contact_node)
                || !environment.valid_node(environment.blueprint_.right_contact_node))
                return;
            const float left_anchor = environment.particles_[
                environment.blueprint_.left_contact_node].position.x;
            const float right_anchor = environment.particles_[
                environment.blueprint_.right_contact_node].position.x;
            const float center = 0.5f * (left_anchor + right_anchor);
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                const bool left = environment.blueprint_.is_left_support_seed(index);
                const bool right = environment.blueprint_.is_right_support_seed(index);
                if (!left && !right)
                    continue;
                // Fuse the two feet by translating each complete cluster. Do
                // not collapse heel, ball, and toe into one impossible point.
                const float anchor = left ? left_anchor : right_anchor;
                const float offset = environment.particles_[index].position.x - anchor;
                environment.particles_[index].position.x = center + offset;
                environment.particles_[index].previous.x = center + offset;
            }
        }

        static void separate_supports(Environment& environment) noexcept
        {
            environment.separate_support_clusters();
        }

        static bool moving_stage_allows_leg_crossing(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.45f);
            if (!environment.valid_node(environment.blueprint_.left_contact_node)
                || !environment.valid_node(environment.blueprint_.right_contact_node))
                return false;
            const float left_before = environment.particles_[
                environment.blueprint_.left_contact_node].position.x;
            const float right_before = environment.particles_[
                environment.blueprint_.right_contact_node].position.x;
            const float left_shift = right_before - left_before + 0.36f;
            const float right_shift = left_before - right_before - 0.36f;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (environment.blueprint_.is_left_support_seed(index))
                {
                    environment.particles_[index].position.x += left_shift;
                    environment.particles_[index].previous.x += left_shift;
                }
                if (environment.blueprint_.is_right_support_seed(index))
                {
                    environment.particles_[index].position.x += right_shift;
                    environment.particles_[index].previous.x += right_shift;
                }
            }
            const float crossed_gap = primary_support_gap(environment);
            environment.separate_support_clusters();
            return crossed_gap < 0.0f
                && primary_support_gap(environment) < 0.0f;
        }

        static float primary_support_gap(const Environment& environment) noexcept
        {
            return environment.particles_[environment.blueprint_.right_contact_node].position.x
                - environment.particles_[environment.blueprint_.left_contact_node].position.x;
        }

        static float semantic_support_cluster_gap(
            const Environment& environment) noexcept
        {
            float left = 0.0f;
            float right = 0.0f;
            std::size_t left_count = 0u;
            std::size_t right_count = 0u;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (environment.blueprint_.is_left_support_seed(index))
                {
                    left += environment.particles_[index].position.x;
                    ++left_count;
                }
                if (environment.blueprint_.is_right_support_seed(index))
                {
                    right += environment.particles_[index].position.x;
                    ++right_count;
                }
            }
            if (left_count == 0u || right_count == 0u)
                return 0.0f;
            return std::abs(right / static_cast<float>(right_count)
                - left / static_cast<float>(left_count));
        }

        static float minimum_semantic_support_clearance(
            const Environment& environment) noexcept
        {
            std::array<std::uint16_t, 32> supports{};
            std::size_t count = 0;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (environment.blueprint_.is_support_seed(index))
                    supports[count++] = static_cast<std::uint16_t>(index);
            }
            float minimum = 1000.0f;
            for (std::size_t first = 0; first < count; ++first)
            {
                for (std::size_t second = first + 1; second < count; ++second)
                {
                    const std::uint16_t lhs_index = supports[first];
                    const std::uint16_t rhs_index = supports[second];
                    const bool same_foot =
                        (environment.blueprint_.is_left_support_seed(lhs_index)
                            && environment.blueprint_.is_left_support_seed(rhs_index))
                        || (environment.blueprint_.is_right_support_seed(lhs_index)
                            && environment.blueprint_.is_right_support_seed(rhs_index));
                    if (same_foot)
                        continue;
                    const Particle& lhs = environment.particles_[lhs_index];
                    const Particle& rhs = environment.particles_[rhs_index];
                    const float clearance = std::abs(rhs.position.x - lhs.position.x)
                        - lhs.radius - rhs.radius;
                    minimum = std::min(minimum, clearance);
                }
            }
            return minimum;
        }

        static void complete_duck_press(Environment& environment) noexcept
        {
            environment.duck_press_completed_ = true;
            environment.duck_walk_started_seconds_ = 9.0f;
            environment.elapsed_seconds_ = 10.0f;
            environment.rebuild_course_features();
        }

        static void qualify_crouch_walk(Environment& environment) noexcept
        {
            qualify_stable_stance(environment);
            environment.duck_press_completed_ = true;
            environment.duck_walk_started_seconds_ = 1.0f;
            environment.duck_active_ = true;
            environment.duck_recovery_count_ = 1u;
            environment.duck_seconds_ = 3.0f;
            environment.crouch_walk_seconds_ = 2.5f;
            environment.crouch_walk_distance_ = 1.2f;
            environment.alternating_steps_ = 5u;
            environment.obstacles_passed_ = 4u;
        }

        static void force_non_foot_contact(Environment& environment) noexcept
        {
            environment.non_foot_grounded_ = true;
        }

        static void qualify_stable_stance(Environment& environment) noexcept
        {
            environment.invalid_reason_ = InvalidMotion::none;
            environment.non_foot_grounded_ = false;
            environment.elapsed_seconds_ = 6.5f;
            environment.stable_stance_seconds_ = 6.5f;
            environment.longest_stable_stance_seconds_ = 6.5f;
            environment.maximum_joint_speed_ = 0.5f;
            environment.uncontrolled_spin_turns_ = 0.0f;
        }

        static void force_standing_spin(Environment& environment, float turns) noexcept
        {
            environment.uncontrolled_spin_turns_ = turns;
        }

        static void force_arms_overhead(Environment& environment) noexcept
        {
            if (environment.particles_.size() < 13u)
                return;
            const Vec2 left = environment.particles_[7].position;
            const Vec2 right = environment.particles_[10].position;
            environment.particles_[8].position = left + Vec2{ -0.08f, 0.70f };
            environment.particles_[9].position = left + Vec2{ -0.02f, 1.34f };
            environment.particles_[11].position = right + Vec2{ 0.08f, 0.70f };
            environment.particles_[12].position = right + Vec2{ 0.02f, 1.34f };
            for (const std::size_t index : { 8u, 9u, 11u, 12u })
                environment.particles_[index].previous = environment.particles_[index].position;
        }

        struct StanceFrame
        {
            bool supported{};
            bool body_clear{};
            bool upright{};
            bool head_high{};
            bool low_slip{};
            bool low_torso_turn{};
            bool low_joint_speed{};
            bool low_vertical_speed{};
        };

        static StanceFrame stance_frame(const Environment& environment) noexcept
        {
            float joint_speed = 0.0f;
            for (std::size_t index = 0;
                index < environment.blueprint_.active_motor_count; ++index)
            {
                joint_speed = std::max(joint_speed,
                    std::abs(environment.angular_velocities_[index]));
            }
            const std::uint16_t root = environment.blueprint_.root_node;
            const float vertical_speed = root < environment.particles_.size()
                ? (environment.particles_[root].position.y
                    - environment.particles_[root].previous.y) * 60.0f
                : 0.0f;
            const std::uint16_t head = environment.blueprint_.head_node;
            const float head_clearance = head < environment.particles_.size()
                ? environment.particles_[head].position.y
                    - environment.ground_height_at(
                        environment.particles_[head].position.x)
                : 0.0f;
            const float rest_head_clearance = head < environment.blueprint_.nodes.size()
                ? environment.blueprint_.nodes[head].y : 0.0f;
            const float head_ratio = rest_head_clearance > 1.0e-5f
                ? head_clearance / rest_head_clearance : 0.0f;
            return {
                environment.contact_supported(environment.blueprint_.left_contact_node)
                    || environment.contact_supported(environment.blueprint_.right_contact_node),
                !environment.non_foot_grounded_,
                environment.torso_uprightness() >= 0.84f,
                head_ratio >= 0.62f,
                environment.stance_slip_speed_ <= 0.10f,
                std::abs(environment.torso_turn_speed_) <= 2.00f,
                joint_speed <= 12.0f,
                std::abs(vertical_speed) <= 1.50f
            };
        }
    };
}

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner core test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    using namespace runner;

    const sim::CreatureBlueprint scaffold = sim::CreatureBlueprint::scaffold();
    require(scaffold.valid() && scaffold.active_motor_count == 4u
            && scaffold.paired_leg_chains(),
        "minimal scaffold is not a valid two-joint-per-side training rig");
    bool topology_mutation_seen = false;
    bool parametric_mutation_seen = false;
    for (std::uint64_t generation = 0; generation < 22u; ++generation)
    {
        const rl::RigMutationCandidate mutation = rl::evolve_rig_candidate(
            scaffold, generation);
        if (!mutation.changed)
            continue;
        require(mutation.blueprint.valid(),
            "rig evolution published a structurally invalid candidate");
        const auto articulated_foot_intact = [](const sim::CreatureBlueprint& rig,
            bool left)
        {
            const auto& support_nodes = left
                ? rig.additional_left_contact_nodes
                : rig.additional_right_contact_nodes;
            if (support_nodes.size() < 2u)
                return true;
            const std::uint16_t ball = support_nodes[0];
            const std::uint16_t toe = support_nodes[1];
            return std::ranges::any_of(rig.bones,
                [ball, toe](const sim::DistanceConstraint& bone)
                {
                    return (bone.a == ball && bone.b == toe)
                        || (bone.a == toe && bone.b == ball);
                });
        };
        require(mutation.blueprint.support_seed_count() >= scaffold.support_seed_count(),
            "topology evolution removed a semantic foot contact");
        require(articulated_foot_intact(mutation.blueprint, true)
                && articulated_foot_intact(mutation.blueprint, false),
            "topology evolution split or detached an articulated toe edge");
        topology_mutation_seen = topology_mutation_seen || mutation.topology_changed;
        parametric_mutation_seen = parametric_mutation_seen || !mutation.topology_changed;
    }
    require(topology_mutation_seen && parametric_mutation_seen,
        "rig evolution does not produce both topology and parameter candidates");
    const rl::RigMutationCandidate articulated_growth =
        rl::evolve_rig_candidate(scaffold, 5u);
    require(articulated_growth.changed && articulated_growth.topology_changed
            && articulated_growth.blueprint.active_motor_count
                == scaffold.active_motor_count + 1u
            && articulated_growth.activated_motor_mask
                == static_cast<std::uint8_t>(1u << scaffold.active_motor_count),
        "bone split does not activate one neutral trainable joint slot");
    const std::size_t grown_slot = scaffold.active_motor_count;
    require(articulated_growth.blueprint.motors[grown_slot].enabled
            && articulated_growth.blueprint.motors[grown_slot].a
                < articulated_growth.blueprint.nodes.size()
            && articulated_growth.blueprint.motors[grown_slot].pivot
                < articulated_growth.blueprint.nodes.size()
            && articulated_growth.blueprint.motors[grown_slot].c
                < articulated_growth.blueprint.nodes.size(),
        "newly activated topology motor is not structurally valid");

    rl::PolicyNetwork neutral_policy{ 0xA4710u };
    std::array<float, sim::observation_count> neutral_observation{};
    neutral_observation.fill(0.5f);
    neutral_policy.neutralize_action_slot(grown_slot);
    require(std::abs(neutral_policy.evaluate(neutral_observation).mean[grown_slot])
            < 1.0e-7f,
        "new topology action slot retains stale actor motion after neutralization");

    const sim::DuckPressProfile press_clear = sim::duck_press_profile(1.0f, 0.5f, 5.0f);
    const sim::DuckPressProfile press_descend = sim::duck_press_profile(3.5f, 0.5f, 5.0f);
    const sim::DuckPressProfile press_hold = sim::duck_press_profile(5.5f, 0.5f, 5.0f);
    const sim::DuckPressProfile press_retract = sim::duck_press_profile(8.0f, 0.5f, 5.0f);
    require(press_clear.bottom_y > 6.0f && press_descend.descending
            && press_descend.vertical_velocity < 0.0f,
        "duck press does not begin clear and descend gradually");
    require(press_hold.holding && press_hold.bottom_y < 4.2f,
        "duck press does not hold a meaningful crouch target");
    require(press_retract.retracting && press_retract.vertical_velocity > 0.0f,
        "duck press does not retract after the hold");
    auto articulated_forward_foot = [](const sim::CreatureBlueprint& rig,
        bool left)
    {
        const std::uint16_t heel = left ? rig.left_contact_node : rig.right_contact_node;
        const auto& extra = left
            ? rig.additional_left_contact_nodes : rig.additional_right_contact_nodes;
        if (heel >= rig.nodes.size() || extra.size() < 2u
            || extra[0] >= rig.nodes.size() || extra[1] >= rig.nodes.size())
            return false;
        const std::uint16_t ball = extra[0];
        const std::uint16_t toe = extra[1];
        const bool toe_hinge = std::ranges::any_of(rig.bones,
            [ball, toe](const sim::DistanceConstraint& bone)
            {
                return (bone.a == ball && bone.b == toe)
                    || (bone.a == toe && bone.b == ball);
            });
        const bool rigid_heel_to_toe = std::ranges::any_of(rig.bones,
            [heel, toe](const sim::DistanceConstraint& bone)
            {
                return (bone.a == heel && bone.b == toe)
                    || (bone.a == toe && bone.b == heel);
            });
        return rig.nodes[heel].x < rig.nodes[ball].x
            && rig.nodes[ball].x < rig.nodes[toe].x
            && toe_hinge && !rigid_heel_to_toe;
    };
    const std::array articulated_rigs{
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid()
    };
    for (const sim::CreatureBlueprint& rig : articulated_rigs)
    {
        require(rig.support_seed_count() == 6u
                && articulated_forward_foot(rig, true)
                && articulated_forward_foot(rig, false),
            "paired rig lacks forward articulated heel-ball-toe feet");
    }
    sim::Environment toe_environment(sim::CreatureBlueprint::biped(), 79u);
    toe_environment.set_course(sim::CourseStage::duck_press, 0.25f);
    require(sim::EnvironmentTestAccess::articulated_toes_move(toe_environment),
        "coordinated leg action does not actuate both toe hinges");
    sim::Environment rate_limited_toes(sim::CreatureBlueprint::humanoid(), 181u);
    require(sim::EnvironmentTestAccess::articulated_toe_rate_is_bounded(
            rate_limited_toes),
        "articulated toe hinge can chatter faster than its stance/swing rate gate");
    require(std::abs(sim::rate_limited_toe_command(0.0f, 1.0f,
                1.0f / 60.0f, true, sim::CourseStage::uneven))
            <= sim::toe_command_slew_rate(true, sim::CourseStage::uneven)
                / 60.0f + 0.000001f,
        "toe command slew gate permits an instantaneous stabilization snap");
    const sim::Environment discovery_environment(sim::CreatureBlueprint::biped(), 83u);
    const std::size_t crouch_lane = 2u
        * discovery_environment.blueprint().active_motor_count + 6u;
    const rl::MotorDiscoveryProbe crouch_probe = rl::motor_discovery_probe(
        discovery_environment, crouch_lane, 120u, 0u);
    require(crouch_probe.action[0] < 0.0f && crouch_probe.action[1] > 0.0f
            && crouch_probe.action[2] > 0.0f && crouch_probe.action[3] < 0.0f,
        "motor discovery does not explore simultaneous bilateral hip-knee flexion");
    sim::Environment press_collision_environment(sim::CreatureBlueprint::humanoid(), 71u);
    require(sim::EnvironmentTestAccess::press_collision_resolves_below(
            press_collision_environment),
        "duck press collision injects velocity or horizontal drag");
    sim::Environment press_anchor_environment(sim::CreatureBlueprint::humanoid(), 73u);
    require(sim::EnvironmentTestAccess::press_anchor_remains_fixed(
            press_anchor_environment),
        "duck press follows a sliding rig instead of staying fixed over the station");

    require(ui_layout::top_bar_box(1970.0f).width == 1970.0f,
        "top GUI background does not span the full drawable width");
    require(std::abs(ui_layout::course_reference_marker_spacing_m(
            ui_layout::DistanceUnits::metric) - 250.0f) < 0.001f,
        "metric markers are not quarter-kilometre spaced");
    require(std::abs(ui_layout::course_reference_marker_spacing_m(
            ui_layout::DistanceUnits::imperial) - 402.336f) < 0.001f,
        "imperial markers are not quarter-mile spaced");
    require(ui_layout::lifetime_delta(120u, 20u) == 100u
            && ui_layout::lifetime_delta(20u, 120u) == 0u,
        "rig lifetime counters can underflow");
    rl::TrainingMetrics cumulative{};
    cumulative.total_episodes = 12u;
    cumulative.total_valid_episodes = 9u;
    cumulative.total_invalid_episodes = 3u;
    cumulative.total_resets = 14u;
    cumulative.total_alternating_steps = 48u;
    cumulative.total_falls = 2u;
    cumulative.total_collisions = 7u;
    cumulative.total_powered_jumps = 5u;
    cumulative.total_landed_jumps = 4u;
    cumulative.total_landed_flips = 1u;
    cumulative.total_obstacles_passed = 11u;
    cumulative.total_distance = 123.5;
    require(cumulative.total_valid_episodes + cumulative.total_invalid_episodes
            == cumulative.total_episodes
            && cumulative.total_landed_jumps <= cumulative.total_powered_jumps,
        "cumulative runtime statistics are internally inconsistent");
    require(ui_layout::live_layout_valid(1100.0f, 902.0f),
        "supported minimum live layout overlaps its panel, telemetry, or PIP");
    require(!ui_layout::supported_window(1099.0f, 902.0f)
            && !ui_layout::supported_window(1100.0f, 901.0f),
        "undersized windows are incorrectly treated as fully supported");
    const ui_layout::Box minimum_content = ui_layout::content_box(1100.0f, 902.0f);
    const ui_layout::Box minimum_world = ui_layout::live_world_box(minimum_content);
    const ui_layout::Box minimum_pip = ui_layout::training_pip_box(minimum_world);
    require(ui_layout::contains(minimum_world, minimum_pip),
        "training PIP escapes the world viewport");
    require(!ui_layout::overlaps(minimum_pip,
                ui_layout::primary_telemetry_box(minimum_world))
            && !ui_layout::overlaps(minimum_pip,
                ui_layout::bottom_telemetry_box(minimum_world)),
        "training PIP overlaps primary telemetry at the supported minimum window");

    require(sim::classify_motion_gate(1.0f, 50.0f, { 0.0f, 3.0f }, 0.0f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::overspeed, "50 km/h hard gate missing");
    require(sim::classify_motion_gate(-0.2f, 0.0f, { 0.0f, 3.0f }, 0.0f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::flipped, "flip hard gate missing outside flip lessons");
    require(sim::classify_motion_gate(-0.2f, 0.0f, { 0.0f, 4.0f }, 0.4f, 2.7f, 0.0f,
            false, sim::CourseStage::duck_bars, 0.5f)
        == sim::InvalidMotion::none, "controlled flip lesson still rejects an airborne flip");
    require(sim::classify_motion_gate(0.4f, 0.0f, { 0.0f, 4.0f }, 1.2f, 2.7f, 0.0f,
            false, sim::CourseStage::duck_bars, 3.21f)
        == sim::InvalidMotion::excessive_spins, "more than three spins is not rejected");
    require(rl::balance_mastery_lock_confirmations == 3
            && rl::mastery_lock_confirmations >= 8
            && rl::required_mastery_confirmations(sim::CourseStage::balance) == 3
            && rl::required_mastery_confirmations(sim::CourseStage::duck_press) >= 8,
        "standing and later-stage mastery confirmation counts are incorrect");
    rl::TrainingMetrics standing_mastery{};
    standing_mastery.evaluation_valid = true;
    standing_mastery.evaluation_invalid_runs = 0u;
    standing_mastery.evaluation_longest_stance = rl::standing_mastery_seconds;
    standing_mastery.evaluation_survival = rl::standing_mastery_seconds;
    standing_mastery.evaluation_spin_turns = rl::standing_mastery_spin_limit;
    standing_mastery.evaluation_max_joint_speed =
        rl::standing_mastery_joint_speed_limit;
    require(rl::strict_balance_mastery(standing_mastery),
        "all-six-seed standing evidence cannot satisfy the mastery gate");
    standing_mastery.evaluation_max_joint_speed += 0.01f;
    require(!rl::strict_balance_mastery(standing_mastery),
        "standing mastery accepts joint speed above its visible limit");
    require(sim::controlled_somersault_allowed(
            sim::CourseStage::duck_bars, 2.75f, 1.2f, true),
        "controlled somersault is rejected without a powered-launch flag");
    require(!sim::controlled_somersault_allowed(
            sim::CourseStage::uneven, 2.0f, 1.2f, true)
            && !sim::controlled_somersault_allowed(
                sim::CourseStage::duck_bars, 3.01f, 1.2f, true)
            && !sim::controlled_somersault_allowed(
                sim::CourseStage::duck_bars, 2.0f, 0.10f, true),
        "wrong-stage, over-three-turn, or non-rotating tumbling is accepted");
    require(sim::forward_prone_allowed(sim::CourseStage::uneven,
            true, true, 0.25f, 0.10f)
            && !sim::forward_prone_allowed(sim::CourseStage::duck_press,
                true, true, 0.25f, 0.10f)
            && !sim::forward_prone_allowed(sim::CourseStage::crouch_walk,
                true, true, 0.25f, 0.10f)
            && !sim::forward_prone_allowed(sim::CourseStage::uneven,
                true, false, 0.25f, 0.10f),
        "forward-prone recovery rules do not preserve crouch foot-only contact");
    require(sim::classify_motion_gate(1.0f, 0.0f, { 301.0f, 3.0f }, 0.0f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::out_of_bounds, "course bounds gate missing");
    require(!sim::friction_driven_shuffle(0.42f, true, false, 0.30f, 0u, 0.0f)
            && !sim::friction_driven_shuffle(0.42f, true, true, 0.30f, 1u, 0.0f)
            && !sim::friction_driven_shuffle(0.42f, true, true, 0.30f, 0u, 0.10f),
        "normal single-support, established-gait, or foot-repositioning slide is penalized");
    require(sim::friction_driven_shuffle(0.42f, true, true, 0.30f, 0u, 0.0f),
        "friction-driven double-support shuffling is not recognized");
    require(sim::classify_motion_gate(1.0f, 0.0f, { 0.0f, 3.0f }, 0.8f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::sustained_flight, "unpowered flight hard gate missing");
    require(!sim::duck_ground_contact_allowed(true, true)
            && sim::duck_ground_contact_allowed(true, false)
            && sim::duck_ground_contact_allowed(false, true),
        "foot-only duck contact rule is not strict");
    sim::CrouchPostureEvidence hinge{};
    hinge.paired_leg_chains = true;
    hinge.feet_supported = true;
    hinge.pelvis_drop = 0.08f;
    hinge.left_knee_flex = 0.03f;
    hinge.right_knee_flex = 0.02f;
    hinge.torso_pitch = 1.10f;
    hinge.support_margin = 0.12f;
    require(!sim::crouch_posture_qualified(hinge),
        "forward hip hinge is accepted as a crouch");
    sim::CrouchPostureEvidence squat{};
    squat.paired_leg_chains = true;
    squat.feet_supported = true;
    squat.pelvis_drop = 0.44f;
    squat.left_knee_flex = 0.32f;
    squat.right_knee_flex = 0.31f;
    squat.torso_pitch = 0.20f;
    squat.support_margin = 0.14f;
    require(sim::crouch_posture_qualified(squat),
        "bilateral pelvis-down squat cannot satisfy crouch evidence");
    require(sim::stage_skill_evidence(sim::CourseStage::balance,
            0u, 0.0f, 0u, 0.0f, 0u, 0u),
        "standing incorrectly requires movement");
    require(sim::stage_skill_evidence(sim::CourseStage::duck_press,
            0u, 0.80f, 0u, 0.0f, 0u, 1u),
        "static crouch incorrectly requires walking or running");
    require(!sim::stage_skill_evidence(sim::CourseStage::uneven,
            0u, 0.0f, 0u, 0.0f, 0u, 0u)
            && sim::stage_skill_evidence(sim::CourseStage::uneven,
                4u, 0.0f, 0u, 0.0f, 0u, 0u),
        "walking/running stage uses the wrong movement evidence");
    require(sim::CreatureBlueprint::monoped().monopedal_gait()
            && !sim::CreatureBlueprint::humanoid().monopedal_gait(),
        "monoped gait is not distinguished from alternating biped gait");
    require(!sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
            3u, 3.0f, 0u, 0.0f, 0u, 4u)
            && !sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
                5u, 1.5f, 0u, 0.0f, 0u, 4u)
            && sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
                5u, 3.0f, 0u, 0.0f, 0u, 4u),
        "duck stage can qualify without sustained crouch walking and obstacles");
    require(sim::powered_joint_launch(sim::CourseStage::ramps, 1.0f, 0.08f),
        "joint-powered jump is not recognized");
    require(!sim::powered_joint_launch(sim::CourseStage::duck_press, 1.0f, 0.08f),
        "duck lesson incorrectly enables flight");
    require(sim::allowed_airtime_for_stage(sim::CourseStage::duck_bars, true) > 2.0f,
        "controlled flip lesson does not allow bounded powered airtime");
    require(sim::classify_motion_gate(1.0f, 0.0f, { 0.0f, 3.0f }, 0.0f, 0.7f, 3.0f, false)
        == sim::InvalidMotion::micro_motion, "micro-motion gate missing");
    require(!sim::recovery_should_start(true, 0.95f, false, false),
        "harmless upright obstacle contact created a rewardable recovery event");
    require(!sim::recovery_should_start(true, 0.80f, false, false),
        "ordinary obstacle contact still opens a rewardable recovery event");
    require(sim::recovery_should_start(false, 0.68f, false, false),
        "major balance loss did not start recovery without a collision");
    require(!sim::recovery_should_start(true, 0.40f, true, true),
        "hard ground impact incorrectly opened a recovery window");

    require(!sim::recovery_terminal_fall(true, false, true),
        "recoverable near-fall terminated during its recovery window");
    require(sim::recovery_terminal_fall(true, false, false),
        "unrecovered geometric fall was not terminal");
    require(sim::recovery_terminal_fall(true, true, true),
        "hard ground impact incorrectly received recovery grace");

    require(!sim::qualifies_alternating_step(-1, 0, 0.30f, 0.10f),
        "simultaneous two-foot landing counted as a step");
    require(!sim::qualifies_alternating_step(-1, 1, 0.05f, 0.10f),
        "rapid hopping counted as an alternating step");
    require(!sim::qualifies_alternating_step(-1, 1, 0.30f, 0.005f),
        "in-place foot twitch counted as walking");
    require(sim::qualifies_alternating_step(-1, 1, 0.30f, 0.08f),
        "real spaced alternating step was rejected");
    require(!sim::qualifies_supported_step(-1, 1, 0.30f, 0.08f, 0.03f, 0.02f),
        "tiny contact wiggle still counts as a supported walking step");
    require(sim::qualifies_supported_step(-1, 1, 0.30f, 0.08f, 0.16f, 0.12f),
        "real lifted swing and landing is rejected as a walking step");
    require(!sim::qualifies_crossing_step(-1, 1, 0.30f, 0.08f,
            0.16f, 0.12f, false, true)
            && sim::qualifies_crossing_step(-1, 1, 0.30f, 0.08f,
                0.16f, 0.12f, true, true)
            && sim::qualifies_crossing_step(-1, 1, 0.30f, 0.08f,
                0.16f, 0.12f, false, false),
        "paired gait crossing is either optional or incorrectly forced on nonpaired rigs");
    require(sim::classify_foot_contact_phase(false, false, false)
                == sim::FootContactPhase::airborne
            && sim::classify_foot_contact_phase(true, false, false)
                == sim::FootContactPhase::heel_strike
            && sim::classify_foot_contact_phase(true, true, true)
                == sim::FootContactPhase::flat
            && sim::classify_foot_contact_phase(false, true, true)
                == sim::FootContactPhase::toe_off,
        "heel, flat-foot, toe-off, and airborne phases are not distinct");
    require(sim::foot_friction_retention(0.04f, 1.0f, 0.0f, false, false) == 0.0f,
        "loaded low-speed foot does not enter static friction");
    require(sim::foot_friction_retention(0.45f, 1.0f, 0.0f, false, false)
            < sim::foot_friction_retention(0.45f, 0.25f, 0.75f, false, false),
        "firm ground does not provide more dynamic traction than loose ground");
    require(sim::foot_friction_retention(0.45f, 1.0f, 0.0f, true, false)
            < sim::foot_friction_retention(0.45f, 1.0f, 0.0f, false, false),
        "static lessons do not apply stronger planted-foot friction");
    require(sim::rig_test_motor_input(sim::RigTestPattern::crouch,
                0u, 0.0f, 0.0f) < 0.0f
            && sim::rig_test_motor_input(sim::RigTestPattern::crouch,
                1u, 0.0f, 0.0f) > 0.0f
            && sim::rig_test_motor_input(sim::RigTestPattern::gait,
                0u, pi * 0.5f, 0.0f)
                * sim::rig_test_motor_input(sim::RigTestPattern::gait,
                    2u, pi * 0.5f, 0.0f) < 0.0f,
        "rig lab crouch and alternating gait test patterns are incorrect");

    const sim::CourseFeature rock_feature{
        sim::CourseFeatureKind::rock, {}, {}, 0.27f, {}
    };
    const sim::CourseFeature projectile_feature{
        sim::CourseFeatureKind::projectile, {}, {}, 0.19f, { -4.0f, 1.0f }
    };
    const sim::CourseFeature hurdle_feature{
        sim::CourseFeatureKind::hurdle, {}, { 0.14f, 0.42f }, 0.0f, {}
    };
    require(std::abs(sim::course_feature_observation_size(rock_feature) - 0.27f) < 0.0001f,
        "rock radius is absent from policy observations");
    require(std::abs(sim::course_feature_observation_size(projectile_feature) - 0.19f) < 0.0001f,
        "projectile radius is absent from policy observations");
    require(std::abs(sim::course_feature_observation_size(hurdle_feature) - 0.42f) < 0.0001f,
        "rectangular obstacle extent is incorrect in policy observations");

    const sim::CreatureBlueprint chicken = sim::CreatureBlueprint::chicken();
    require(chicken.head_node < chicken.nodes.size()
            && chicken.nodes[chicken.head_node].x > chicken.nodes[chicken.torso_node].x
            && chicken.nodes[chicken.head_node].y > chicken.nodes[chicken.torso_node].y,
        "chicken preset does not have a raised forward bird head");
    require(chicken.nodes[6].x < chicken.nodes[chicken.root_node].x - 1.0f
            && chicken.nodes[4].x > chicken.nodes[chicken.head_node].x,
        "chicken preset lacks a distinct tail and beak");
    require(chicken.nodes[chicken.torso_node].y
            > chicken.nodes[chicken.root_node].y + 0.55f,
        "chicken semantic torso axis is not vertically load-bearing");
    require(std::ranges::any_of(chicken.bones, [&](const sim::DistanceConstraint& bone)
        {
            return (bone.a == chicken.root_node && bone.b == chicken.torso_node)
                || (bone.b == chicken.root_node && bone.a == chicken.torso_node);
        }) && std::ranges::any_of(chicken.bones, [&](const sim::DistanceConstraint& bone)
        {
            return (bone.a == chicken.torso_node && bone.b == chicken.head_node)
                || (bone.b == chicken.torso_node && bone.a == chicken.head_node);
        }),
        "chicken root, torso, and head do not form an intact semantic spine");

    {
        constexpr std::size_t chicken_seed_count = 6u;
        std::uint32_t valid_chicken_seeds = 0u;
        for (std::size_t seed_index = 0; seed_index < chicken_seed_count; ++seed_index)
        {
            const std::uint64_t seed = 0xC11C000u
                + static_cast<std::uint64_t>(seed_index) * 4099u;
            sim::Environment environment{ chicken, seed };
            environment.set_course(sim::CourseStage::balance, 0.25f);
            const std::array<float, sim::action_count> raw_action{};
            for (int frame = 0; frame < 1200; ++frame)
            {
                const auto action = rl::effective_policy_action(
                    environment, raw_action, sim::CourseStage::balance);
                const sim::StepResult step = environment.step(action);
                if (environment.valid_motion()
                    && environment.longest_stable_stance_seconds()
                        >= rl::standing_mastery_seconds)
                    break;
                if (step.terminated)
                    break;
            }
            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(sim::CourseStage::balance, environment);
            const bool accepted = qualification.valid
                && environment.body_integrity_valid()
                && environment.longest_stable_stance_seconds()
                    >= rl::standing_mastery_seconds
                && environment.uncontrolled_spin_turns() <= 0.55f;
            valid_chicken_seeds += accepted ? 1u : 0u;
            if (!accepted)
            {
                std::cerr << "chicken balance seed " << seed
                    << " rejection=" << qualification.rejection_mask
                    << " invalid=" << static_cast<int>(environment.invalid_reason())
                    << " stance=" << environment.longest_stable_stance_seconds()
                    << " spin=" << environment.uncontrolled_spin_turns()
                    << " survival=" << environment.elapsed_seconds() << std::endl;
            }
        }
        require(valid_chicken_seeds == chicken_seed_count,
            "chicken balance still reproduces the live 0/6 valid-seed regression");
    }

    sim::Environment crossing_feet(sim::CreatureBlueprint::humanoid(), 18);
    require(sim::EnvironmentTestAccess::moving_stage_allows_leg_crossing(crossing_feet),
        "support separation prevents one side-view leg from passing the other");

    sim::Environment fused_feet(sim::CreatureBlueprint::humanoid(), 19);
    sim::EnvironmentTestAccess::force_fused_supports(fused_feet);
    sim::EnvironmentTestAccess::separate_supports(fused_feet);
    require(sim::EnvironmentTestAccess::primary_support_gap(fused_feet) > 0.18f,
        "left and right feet can remain fused into one support blob");

    const std::array<sim::CreatureBlueprint, 7> support_presets{
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid(),
        sim::CreatureBlueprint::quadruped(),
        sim::CreatureBlueprint::crawler4(),
        sim::CreatureBlueprint::hexapod(),
        sim::CreatureBlueprint::monoped()
    };
    for (std::size_t preset = 0; preset < support_presets.size(); ++preset)
    {
        sim::Environment environment(support_presets[preset], 100u + preset);
        sim::EnvironmentTestAccess::force_fused_supports(environment);
        for (int iteration = 0; iteration < 64; ++iteration)
            sim::EnvironmentTestAccess::separate_supports(environment);
        require(sim::EnvironmentTestAccess::semantic_support_cluster_gap(environment)
                > 0.18f,
            "a preset can retain fused left/right foot clusters");
    }

    sim::CreatureBlueprint editor_bone_rig = sim::CreatureBlueprint::scaffold();
    require(editor_bone_rig.valid(), "editor scaffold starts invalid");
    editor_bone_rig.bones.front().stiffness = 0.42f;
    require(editor_bone_rig.valid()
            && std::abs(editor_bone_rig.bones.front().stiffness - 0.42f) < 0.0001f,
        "editor bone stiffness control cannot preserve a valid rig");

    const sim::CreatureBlueprint humanoid_rig = sim::CreatureBlueprint::humanoid();
    const sim::CreatureBlueprint quad_rig = sim::CreatureBlueprint::quadruped();
    require(humanoid_rig.paired_leg_chains() && !quad_rig.paired_leg_chains(),
        "biped-only control path is not separated from quadruped control");

    sim::Environment neutral_humanoid(humanoid_rig, 131);
    neutral_humanoid.set_course(sim::CourseStage::balance, 0.25f);
    const auto standing_teacher = rl::balance_teacher_action(neutral_humanoid);
    require(std::abs(standing_teacher[0]) < 0.20f
            && std::abs(standing_teacher[2]) < 0.20f,
        "standing teacher still forces a jumping-jack hip pose");
    std::array<float, sim::action_count> exploratory{};
    exploratory.fill(0.80f);
    const auto residual_standing = rl::effective_policy_action(
        neutral_humanoid, exploratory, sim::CourseStage::balance);
    require(std::abs(residual_standing[0] - standing_teacher[0]) > 0.08f,
        "standing teacher still erases nearly all PPO exploration");

    sim::Environment neutral_quad(quad_rig, 137);
    neutral_quad.set_course(sim::CourseStage::balance, 0.25f);
    const auto quad_teacher = rl::balance_teacher_action(neutral_quad);
    require(std::ranges::all_of(quad_teacher, [](float value)
            {
                return std::abs(value) < 0.30f;
            }),
        "quadruped standing receives biped-like forced leg motion");

    sim::Environment hinge_humanoid(humanoid_rig, 139);
    require(sim::EnvironmentTestAccess::hip_hinge_is_rejected(hinge_humanoid),
        "live humanoid forward bow passes the physical crouch gate");
    sim::Environment guided_squat(humanoid_rig, 140);
    require(sim::EnvironmentTestAccess::guided_squat_is_valid(guided_squat),
        "authored crouch guide cannot produce a pelvis-down bilateral squat");

    sim::Environment crouch_humanoid(humanoid_rig, 141);
    crouch_humanoid.set_course(sim::CourseStage::duck_press, 0.30f);
    sim::EnvironmentTestAccess::set_duck_pressure(crouch_humanoid, 1.0f);
    const auto walk_teacher = rl::walking_teacher_action(neutral_humanoid);
    require(walk_teacher[0] * walk_teacher[2] < 0.0f
            || walk_teacher[1] * walk_teacher[3] < 0.0f,
        "walking teacher does not alternate the near and far leg chains");

    const auto crouch_teacher = rl::duck_teacher_action(crouch_humanoid);
    require(std::abs(crouch_teacher[0]) < std::abs(crouch_teacher[1])
            && std::abs(crouch_teacher[2]) < std::abs(crouch_teacher[3]),
        "static crouch teacher still spreads hips before bending knees");

    const rl::MotorDiscoveryProbe positive_probe = rl::motor_discovery_probe(
        neutral_humanoid, 0u, 0u, 0u);
    const rl::MotorDiscoveryProbe negative_probe = rl::motor_discovery_probe(
        neutral_humanoid, humanoid_rig.active_motor_count, 0u, 0u);
    const rl::MotorDiscoveryProbe synchronized_probe = rl::motor_discovery_probe(
        neutral_humanoid, humanoid_rig.active_motor_count * 2u, 0u, 0u);
    const rl::MotorDiscoveryProbe alternating_probe = rl::motor_discovery_probe(
        neutral_humanoid, humanoid_rig.active_motor_count * 2u + 3u, 0u, 0u);
    require(positive_probe.weight > 0.80f && positive_probe.action[0] > 0.0f
            && negative_probe.action[0] < 0.0f,
        "motor discovery does not test one joint in both directions");
    require(std::ranges::all_of(
            std::span(synchronized_probe.action).first(humanoid_rig.active_motor_count),
            [](float value) { return value > 0.0f; }),
        "motor discovery does not test synchronized joint motion");
    require(alternating_probe.action[0] * alternating_probe.action[1] < 0.0f,
        "motor discovery does not test alternating joint motion");
    require(rl::motor_discovery_probe(neutral_humanoid, 0u, 480u, 0u).weight == 0.0f,
        "motor discovery never yields control back to PPO");

    sim::Environment press_environment(sim::CreatureBlueprint::humanoid(), 17);
    press_environment.set_course(sim::CourseStage::duck_press, 0.5f);
    sim::EnvironmentTestAccess::set_duck_pressure(press_environment, 1.0f);
    const auto press_teacher = rl::duck_teacher_action(press_environment);
    require(std::abs(press_teacher[4]) < 0.0001f
            && std::abs(press_teacher[5]) < 0.0001f
            && std::abs(press_teacher[6]) < 0.0001f
            && std::abs(press_teacher[7]) < 0.0001f,
        "duck teacher still prefers shoulder or arm swing over leg compression");
    require(std::abs(press_teacher[1]) + std::abs(press_teacher[3]) > 0.60f,
        "duck teacher does not apply meaningful leg compression");
    require(sim::EnvironmentTestAccess::press_collision_resolves_below(press_environment),
        "duck press clips through the model instead of resolving below the platen");
    sim::EnvironmentTestAccess::complete_duck_press(press_environment);
    require(std::abs(press_environment.ground_height_at(0.0f)
            - press_environment.ground_height_at(1.25f)) < 0.0001f,
        "static crouch lesson incorrectly requires uneven-ground movement");
    require(std::ranges::none_of(press_environment.course_features(),
            [](const sim::CourseFeature& feature)
            {
                return feature.kind == sim::CourseFeatureKind::overhead_bar;
            }),
        "static crouch lesson incorrectly contains the moving low-bar course");

    sim::Environment crouch_environment(sim::CreatureBlueprint::humanoid(), 23);
    crouch_environment.set_course(sim::CourseStage::crouch_walk, 0.5f);
    require(std::abs(crouch_environment.ground_height_at(0.0f)
            - crouch_environment.ground_height_at(1.25f)) > 0.005f,
        "crouch-walk lesson ground remains flat and stable");
    const auto later_bar_iterator = std::ranges::find_if(
        crouch_environment.course_features(), [](const sim::CourseFeature& feature)
        {
            return feature.kind == sim::CourseFeatureKind::overhead_bar;
        });
    require(later_bar_iterator != crouch_environment.course_features().end(),
        "crouch-walk lesson has no later moving low bar");
    const sim::CourseFeature& later_bar = *later_bar_iterator;
    require(later_bar.center.x
            - crouch_environment.particles()[crouch_environment.blueprint().root_node].position.x >= 5.5f,
        "crouch-walk low bar starts too close for a meaningful response");
    require(later_bar.half_extent.x > later_bar.half_extent.y * 5.0f,
        "crouch-walk low bar is not horizontal or is effectively a wall");
    require(sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
            5u, 3.0f, 0u, 0.0f, 0u, 4u),
        "valid foot-only crouch-walk evidence is rejected");

    require(sim::ground_velocity_retention(true, 0.0f)
        < sim::ground_velocity_retention(false, 0.0f),
        "feet do not receive more ground traction than head, tail, or body nodes");
    require(sim::ground_velocity_retention(true, 0.0f) == 0.0f,
        "grounded orange foot nodes still retain wheel-like horizontal velocity");
    require(std::abs(sim::ground_contact_offset(true, 0.20f) - 0.065f) < 0.0001f,
        "semantic feet still collide with the ground as rolling circles");
    require(sim::ground_velocity_retention(false, 0.0f) >= 0.95f,
        "non-foot body contact can still pin the creature to the ground");
    const int anchored_sequence = sim::first_course_feature_sequence(1.0f, 3.0f);
    const float anchored_x = sim::course_feature_world_x(anchored_sequence, 3.0f);
    const float advanced_x = sim::course_feature_world_x(anchored_sequence, 4.0f);
    require(std::abs((advanced_x - anchored_x) + 1.0f) < 0.0001f,
        "course debris does not advance in world space solely from course progress");
    require(std::abs(sim::course_marker_distance_m(4) - 32.0f) < 0.0001f,
        "course mile-marker spacing is not shared with obstacle scheduling");
    require(sim::course_stage_name(sim::CourseStage::balance) == "1. STAND"
        && sim::course_stage_name(sim::CourseStage::duck_press)
            == "2. STATIC CROUCH / HOLD / RECOVER"
        && sim::course_stage_name(sim::CourseStage::uneven) == "3. WALK / RUN"
        && sim::course_stage_name(sim::CourseStage::crouch_walk)
            == "4. CROUCH WALK / UNEVEN AVOID"
        && sim::course_stage_name(sim::CourseStage::ramps) == "5. JUMP / LAND"
        && sim::course_stage_name(sim::CourseStage::hurdles) == "6. MOVING LOW BAR / HURDLE"
        && sim::course_stage_name(sim::CourseStage::duck_bars) == "7. CONTROLLED FLIPS"
        && sim::course_stage_name(sim::CourseStage::moving_hazards) == "8. MIXED GOAL COURSE"
        && static_cast<std::uint8_t>(sim::CourseStage::balance)
            < static_cast<std::uint8_t>(sim::CourseStage::duck_press)
        && static_cast<std::uint8_t>(sim::CourseStage::duck_press)
            < static_cast<std::uint8_t>(sim::CourseStage::uneven)
        && static_cast<std::uint8_t>(sim::CourseStage::uneven)
            < static_cast<std::uint8_t>(sim::CourseStage::crouch_walk),
        "stand, static crouch, walk/run, and crouch-walk prerequisites are misordered");
    require(!sim::stage_skill_evidence(sim::CourseStage::duck_press, 0u, 0.6f, 0u, 0.0f, 0u, 0u),
        "duck lesson completes without moving crouch evidence");
    require(sim::stage_skill_evidence(sim::CourseStage::crouch_walk, 5u, 3.0f, 0u, 0.0f, 0u, 4u),
        "foot-only crouch walk and obstacle evidence cannot complete the duck lesson");
    require(sim::stage_skill_evidence(sim::CourseStage::ramps, 0u, 0.0f, 1u, 0.0f, 0u, 0u),
        "landed jump cannot complete the jump lesson");
    require(sim::stage_skill_evidence(sim::CourseStage::duck_bars, 0u, 0.0f, 1u, 1.0f, 1u, 0u),
        "controlled landed flip cannot complete the flip lesson");
    require(!sim::stage_skill_evidence(sim::CourseStage::moving_hazards, 2u, 0.0f, 0u, 0.0f, 0u, 0u),
        "mixed goal lesson can complete without passing an obstacle");
    require(sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 5)
            == sim::CourseFeatureKind::rock
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 6)
            == sim::CourseFeatureKind::hurdle
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 7)
            == sim::CourseFeatureKind::overhead_bar
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 8)
            == sim::CourseFeatureKind::moving_hazard
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 9)
            == sim::CourseFeatureKind::projectile,
        "moving-hazard lesson does not schedule every obstacle class on consecutive markers");
    require(sim::course_marker_distance_m(sim::course_safe_runway_markers) >= 40.0f,
        "course does not provide the requested longer learning runway");
    sim::CourseFeature rock_order{};
    rock_order.kind = sim::CourseFeatureKind::rock;
    rock_order.center = { 1.0f, 0.25f };
    rock_order.radius = 0.25f;
    require(!sim::knee_crosses_before_foot(1.12f, 0.92f, 0.34f, rock_order),
        "normal bent-knee lead is still over-constrained");
    require(sim::knee_crosses_before_foot(1.42f, 0.82f, 0.20f, rock_order),
        "egregious low-foot body-first rock shove is not detected");
    require(!sim::knee_crosses_before_foot(1.42f, 1.32f, 0.34f, rock_order),
        "foot-first rock traversal is incorrectly penalized");
    require(!sim::knee_crosses_before_foot(1.42f, 0.82f, 0.58f, rock_order),
        "useful foot clearance is incorrectly rejected because the knee leads");
    require(sim::gait_progress_multiplier(0, false, 0.0f) == 0.0f,
        "sliding without a real step still receives walking progress credit");
    require(sim::gait_progress_multiplier(2, true, 0.18f)
            > sim::gait_progress_multiplier(0, true, 0.18f),
        "alternating lifted-foot gait does not receive stronger progress credit");
    require(sim::friction_driven_shuffle(0.45f, true, true, 0.50f, 0u, 0.0f),
        "double-supported wheel-like sliding is not detected");
    require(!sim::friction_driven_shuffle(0.45f, true, false, 0.50f, 0u, 0.0f),
        "single-support walking is incorrectly classified as wheel sliding");
    require(!sim::rolling_gate_active(1.0f)
            && sim::rolling_gate_active(sim::rolling_gate_activation_seconds),
        "rolling hard gate does not provide a bounded startup settle window");
    require(sim::body_rolling_limit(sim::CourseStage::duck_press, 1.8f)
            > sim::body_rolling_limit(sim::CourseStage::duck_press, 4.0f),
        "rolling gate does not become strict after startup");
    require(sim::foot_pivot_rolling_limit(1.8f) > sim::foot_pivot_rolling_limit(4.0f),
        "orange-foot rolling gate does not become strict after startup");
    require(sim::zero_progress_window(0.0f, 0u, 0.0f, false),
        "zero movement is not classified for reset");
    require(!sim::zero_progress_window(0.08f, 0u, 0.0f, false),
        "meaningful translation is incorrectly classified as zero movement");
    require(!sim::zero_progress_window(0.0f, 1u, 0.0f, false),
        "a new gait step is incorrectly classified as zero movement");
    require(!sim::zero_progress_window(0.0f, 0u, 0.18f, false),
        "useful leg lift is incorrectly classified as zero movement");
    require(!sim::zero_progress_window(0.0f, 0u, 0.0f, true),
        "active recovery is incorrectly reset as idle");
    require(sim::update_zero_progress_seconds(1.0f, true, 1.0f)
            >= sim::zero_progress_reset_seconds,
        "two idle windows do not reach the reset threshold");
    require(sim::update_zero_progress_seconds(1.0f, false, 1.0f) == 0.0f,
        "useful motion does not rapidly clear the idle-reset accumulator");
    require(rl::self_imitation_prior_weight(0, 128) > rl::self_imitation_prior_weight(500, 128)
            && rl::self_imitation_prior_weight(500, 128) > 0.0f,
        "best-result imitation guide does not decay into a light prior");
    require(rl::self_imitation_prior_weight(0, 0) == 0.0f,
        "empty imitation memory still changes PPO gradients");
    require(rl::policy_regression_guard(10.0f, 8.5f, true),
        "large valid-policy degradation does not restore the champion");
    require(rl::policy_regression_guard(10.0f, 10.5f, false),
        "invalid policy does not restore the champion");
    require(!rl::policy_regression_guard(10.0f, 9.4f, true),
        "small exploration change triggers an unnecessary champion rollback");
    require(rl::elite_motion_eligible(sim::CourseStage::uneven, true, 4, 1.2f, 4.0f),
        "valid stepped best result cannot seed self-imitation");
    require(!rl::elite_motion_eligible(sim::CourseStage::duck_press, true, 0, 0.0f, 4.0f, 0.8f),
        "ducking without clearing a low bar can still seed self-imitation");
    require(rl::elite_motion_eligible(sim::CourseStage::duck_press, true, 5, 1.2f, 12.0f,
            3.0f, 0u, 0.0f, 0u, 4u),
        "valid foot-only crouch-walk result cannot seed self-imitation");
    require(!rl::elite_motion_eligible(sim::CourseStage::uneven, false, 8, 12.0f, 20.0f),
        "invalid rolling result can seed self-imitation");
    require(sim::hazard_approach_weight(0.40f) == 1.0f,
        "near obstacle does not activate full leg-lift training");
    require(sim::hazard_quiver_motion(0.50f, 0.02f, 0.03f, 0.40f, 0.20f),
        "high-energy no-lift obstacle quiver is not detected");
    require(!sim::hazard_quiver_motion(0.50f, 0.02f, 0.35f, 0.40f, 0.20f),
        "useful obstacle leg lift is incorrectly classified as quivering");
    require(sim::rolling_body_motion(0.20f, 0.70f, 0.35f, false, true),
        "head, tail, or body rolling is not detected");
    require(!sim::rolling_body_motion(0.20f, 0.70f, 0.95f, true, false),
        "normal foot-supported walking is incorrectly classified as rolling");
    require(sim::foot_pivot_rolling_motion(0.24f, true, true, 0.01f, 0.02f, 0.50f),
        "double-supported rolling around stationary orange foot nodes is not detected");
    require(!sim::foot_pivot_rolling_motion(0.24f, true, false, 0.01f, 0.18f, 0.50f),
        "single-support lifted-foot walking is incorrectly rejected as foot-node rolling");
    require(sim::foot_pivot_rolling_motion(0.22f, true, true, 0.01f, 0.02f, 0.02f),
        "straight double-supported skating around planted feet is not rejected");
    require(sim::course_zone_is_flat(24.0f) && sim::course_zone_is_flat(48.0f),
        "long flat sand-sim patrol zones are missing");
    require(!sim::course_zone_is_flat(32.0f) && !sim::course_zone_is_flat(40.0f),
        "sand mounds are not separated from flat patrol zones");
    require(sim::obstacles_require_flat_zone(sim::CourseStage::hurdles, 1.0f),
        "early debris training can place obstacles on hills");
    require(!sim::obstacles_require_flat_zone(sim::CourseStage::moving_hazards, 0.75f),
        "advanced combat traversal never combines hazards with terrain");
    require(sim::first_course_feature_sequence(0.0f, 29.9f) <= 3,
        "a contacted obstacle is culled like a pickup before it passes behind the actor");

    const std::array<sim::CreatureBlueprint, 8> presets{
        sim::CreatureBlueprint::scaffold(),
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid(),
        sim::CreatureBlueprint::quadruped(),
        sim::CreatureBlueprint::crawler4(),
        sim::CreatureBlueprint::hexapod(),
        sim::CreatureBlueprint::monoped()
    };
    for (const sim::CreatureBlueprint& preset : presets)
    {
        require(preset.valid(), "preset is structurally invalid");
        require(preset.signature() != 0, "preset signature is empty");
        for (std::size_t motor_index = 0; motor_index < preset.active_motor_count; ++motor_index)
        {
            const sim::MotorConstraint& motor = preset.motors[motor_index];
            require(motor.minimum_angle <= motor.neutral_angle && motor.neutral_angle <= motor.maximum_angle,
                "preset motor rest angle is outside limits");
            require(std::abs(wrap_angle(preset.rest_joint_angle(motor_index) - motor.neutral_angle)) < 0.001f,
                "preset motor is not calibrated to rest geometry");
            require(motor.strength <= 0.060f, "default joint speed remains too strong");
            require((motor.maximum_angle - motor.minimum_angle) * 180.0f / pi >= 60.0f,
                "preset cannot articulate enough to lift a leg over debris");
        }
    }

    sim::CreatureBlueprint disconnected = sim::CreatureBlueprint::humanoid();
    disconnected.motors[0].c = disconnected.head_node;
    disconnected.motors[0].enabled = true;
    require(!disconnected.valid(), "enabled motor without direct A-pivot-C bones was accepted");

    const sim::CreatureBlueprint humanoid = sim::CreatureBlueprint::humanoid();
    require(humanoid.torso_node < humanoid.nodes.size()
            && humanoid.nodes[humanoid.torso_node].y > humanoid.nodes[7].y + 0.10f
            && humanoid.nodes[humanoid.torso_node].y > humanoid.nodes[10].y + 0.10f,
        "humanoid central shoulder pivot is not above both lateral shoulder pivots");
    require(humanoid.nodes[8].y < humanoid.nodes[7].y
            && humanoid.nodes[11].y < humanoid.nodes[10].y,
        "humanoid rest arms do not hang below the shoulder pivots");
    require(std::ranges::any_of(humanoid.bones, [](const sim::DistanceConstraint& bone)
            { return (bone.a == 2u && bone.b == 7u) || (bone.a == 7u && bone.b == 2u); })
            && std::ranges::any_of(humanoid.bones, [](const sim::DistanceConstraint& bone)
            { return (bone.a == 2u && bone.b == 10u) || (bone.a == 10u && bone.b == 2u); }),
        "raised humanoid shoulder girdle can still invert through the upper spine");
    require(humanoid.nodes.size() >= 17,
        "human-calibrated rig should include passive heel/toe feet and articulated arms");
    require(std::abs(humanoid.nodes[0].y - 2.8127f) < 0.01f,
        "uploaded humanoid pelvis calibration not applied");
    require(humanoid.bones.size() >= 19,
        "humanoid feet or arms are not structurally connected");
    require(humanoid.active_motor_count == sim::action_count,
        "humanoid does not expose independent shoulder and elbow motors");
    require(humanoid.left_contact_node != humanoid.motors[1].c
            && humanoid.right_contact_node != humanoid.motors[3].c,
        "semantic feet are still the lower-leg motor endpoints");
    require(humanoid.additional_left_contact_nodes.size() == 2u
            && humanoid.additional_right_contact_nodes.size() == 2u,
        "articulated foot does not include heel, ball, and toe contacts");
    require(humanoid.nodes[humanoid.motors[1].c].y
            - humanoid.nodes[humanoid.left_contact_node].y >= 0.18f
            && humanoid.nodes[humanoid.motors[3].c].y
                - humanoid.nodes[humanoid.right_contact_node].y >= 0.18f,
        "passive foot adapter leaves an ankle on the contact plane");
    require(std::ranges::none_of(humanoid.motors,
            [&humanoid](const sim::MotorConstraint& motor)
            {
                return motor.c == humanoid.left_contact_node
                    || motor.c == humanoid.right_contact_node;
            }),
        "a policy motor still terminates directly on a semantic foot contact");
    for (std::size_t motor_index = 0; motor_index < humanoid.active_motor_count; ++motor_index)
    {
        const sim::MotorConstraint& motor = humanoid.motors[motor_index];
        require(motor.enabled, "humanoid active motor is disabled");
        if (motor_index < 4u)
        {
            const float driven_arm = length(humanoid.nodes[motor.c] - humanoid.nodes[motor.pivot]);
            const float expected_linear_gain = (motor_index % 2u) == 0u ? 0.045f : 0.051f;
            const float expected_travel = (motor_index % 2u) == 0u ? 36.0f : 58.0f;
            require(std::abs(motor.strength * std::max(0.75f, driven_arm) - expected_linear_gain) < 0.003f,
                "humanoid leg motor does not use the bounded obstacle-capable effective gain");
            require(std::abs((motor.neutral_angle - motor.minimum_angle) * 180.0f / pi
                - expected_travel) < 0.05f, "obstacle-capable backward leg travel was not applied");
            require(std::abs((motor.maximum_angle - motor.neutral_angle) * 180.0f / pi
                - expected_travel) < 0.05f, "obstacle-capable forward leg travel was not applied");
        }
        else
        {
            require((motor.maximum_angle - motor.minimum_angle) * 180.0f / pi >= 180.0f,
                "humanoid arm motor lacks useful acrobatic travel");
            require(motor.strength <= 0.040f,
                "humanoid arm motor is too strong for balance and controlled flips");
        }
    }


    {
        sim::Environment motor_reaction{ humanoid, 0xC8357u };
        const sim::MotorConstraint& shoulder = humanoid.motors[4];
        const auto center_of_mass = [](std::span<const sim::Particle> particles)
        {
            double weighted_x = 0.0;
            double weighted_y = 0.0;
            double total_mass = 0.0;
            for (const sim::Particle& particle : particles)
            {
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                weighted_x += static_cast<double>(particle.position.x) * mass;
                weighted_y += static_cast<double>(particle.position.y) * mass;
                total_mass += mass;
            }
            return Vec2{
                static_cast<float>(weighted_x / total_mass),
                static_cast<float>(weighted_y / total_mass)
            };
        };
        const Vec2 chest_before = motor_reaction.particles()[humanoid.torso_node].position;
        const Vec2 pivot_before = motor_reaction.particles()[shoulder.pivot].position;
        const Vec2 driven_before = motor_reaction.particles()[shoulder.c].position;
        const Vec2 center_before = center_of_mass(motor_reaction.particles());
        sim::EnvironmentTestAccess::solve_motor(motor_reaction, shoulder, 1.0f);
        const Vec2 chest_delta =
            motor_reaction.particles()[humanoid.torso_node].position - chest_before;
        const Vec2 pivot_delta =
            motor_reaction.particles()[shoulder.pivot].position - pivot_before;
        const Vec2 driven_delta =
            motor_reaction.particles()[shoulder.c].position - driven_before;
        const Vec2 center_delta = center_of_mass(motor_reaction.particles()) - center_before;
        require(length(chest_delta) > 1.0e-7f,
            "humanoid shoulder still pins the parent chest in world space");
        require(length(pivot_delta) > 1.0e-7f,
            "humanoid shoulder pivot is still a world-space anchor");
        require(length(driven_delta) > length(chest_delta),
            "parent body receives more correction than the driven arm");
        require(length(center_delta) < 2.0e-5f,
            "internal shoulder correction injects center-of-mass translation");
    }

    {
        sim::Environment stable_humanoid{ humanoid, 0x57A8u };
        stable_humanoid.set_course(sim::CourseStage::balance, 0.25f);
        const std::array<float, sim::action_count> neutral{};
        sim::EnvironmentTestAccess::qualify_stable_stance(stable_humanoid);
        const rl::StageMotionQualification stable =
            rl::stage_motion_qualification(sim::CourseStage::balance, stable_humanoid);
        require(stable.valid,
            "neutral humanoid cannot produce a sustained stage-valid standing baseline");
        require(stable_humanoid.stable_stance_seconds() >= 3.0f,
            "standing baseline never accumulates sustained stance evidence");

        sim::Environment collapsed{ humanoid, 0xC011A9u };
        collapsed.set_course(sim::CourseStage::balance, 0.25f);
        sim::EnvironmentTestAccess::collapse_upper_body(collapsed);
        for (int frame = 0; frame < 180 && collapsed.valid_motion(); ++frame)
        {
            sim::EnvironmentTestAccess::collapse_upper_body(collapsed);
            (void)collapsed.step(neutral);
        }
        const rl::StageMotionQualification rejected =
            rl::stage_motion_qualification(sim::CourseStage::balance, collapsed);
        require(!rejected.valid,
            "collapsed humanoid can still qualify as a standing best result");
        require((rejected.rejection_mask
                & rl::evidence_bit(rl::MotionEvidenceFailure::no_stable_stance)) != 0u
            || !collapsed.valid_motion(),
            "collapsed standing rejection does not expose posture evidence");
    }

    {
        sim::Environment duck_lesson{ humanoid, 0xD0C7u };
        duck_lesson.set_course(sim::CourseStage::duck_press, 0.35f);
        require(std::ranges::any_of(duck_lesson.course_features(),
                [](const sim::CourseFeature& feature)
                {
                    return feature.kind == sim::CourseFeatureKind::duck_press;
                }),
            "duck lesson has no explicit compression platen");
        std::array<float, sim::action_count> unrelated{
            0.9f, -0.8f, 0.2f, 0.7f, -0.9f, 0.6f, 0.1f, -0.5f
        };
        const auto coordinated = rl::bilateral_joint_synergy_action(
            duck_lesson, unrelated, sim::CourseStage::duck_press);
        require(std::abs(coordinated[0] + coordinated[2])
                < std::abs(unrelated[0] + unrelated[2])
            && std::abs(coordinated[1] + coordinated[3])
                < std::abs(unrelated[1] + unrelated[3]),
            "AI outputs are still eight unrelated joint commands");
        sim::EnvironmentTestAccess::set_duck_pressure(duck_lesson, 1.0f);
        const std::array<float, sim::action_count> neutral{};
        const auto duck = rl::effective_policy_action(
            duck_lesson, neutral, sim::CourseStage::duck_press);
        require(duck[0] < -0.05f && duck[1] > 0.10f
                && duck[2] > 0.05f && duck[3] < -0.10f,
            "compression pressure does not trigger a coordinated leg-driven duck primitive");
        require(std::abs(duck[4]) < 0.01f && std::abs(duck[5]) < 0.01f
                && std::abs(duck[6]) < 0.01f && std::abs(duck[7]) < 0.01f,
            "compression lesson still drives shoulders or elbows");
    }

    {
        sim::Environment observation_environment{ humanoid, 0x0B5E7u };
        const auto observation = observation_environment.observation();
        static_assert(sim::observation_count == 50);
        require(observation.size() == 50u,
            "eight-motor and material observation layout is not fifty floats");
        require(observation[20] == 0.0f && observation[21] == 0.0f,
            "contact channels overlap motor channels at reset");
        require(std::isfinite(observation[18]) && std::isfinite(observation[19]),
            "right-arm angular velocity channels are missing");
    }

    {
        sim::Environment assisted_stance{ humanoid, 0xBA1A9CEu };
        assisted_stance.set_course(sim::CourseStage::balance, 0.25f);
        const std::array<float, sim::action_count> raw_action{};
        std::array<std::uint32_t, 8> stance_failures{};
        for (int frame = 0; frame < 720; ++frame)
        {
            const auto action = rl::effective_policy_action(
                assisted_stance, raw_action, sim::CourseStage::balance);
            const sim::StepResult result = assisted_stance.step(action);
            const bool lesson_complete = assisted_stance.valid_motion()
                && assisted_stance.longest_stable_stance_seconds()
                    >= rl::standing_mastery_seconds;
            const auto diagnostics =
                sim::EnvironmentTestAccess::stance_frame(assisted_stance);
            const std::array<bool, 8> passed{
                diagnostics.supported,
                diagnostics.body_clear,
                diagnostics.upright,
                diagnostics.head_high,
                diagnostics.low_slip,
                diagnostics.low_torso_turn,
                diagnostics.low_joint_speed,
                diagnostics.low_vertical_speed
            };
            for (std::size_t index = 0; index < passed.size(); ++index)
                stance_failures[index] += passed[index] ? 0u : 1u;
            if (lesson_complete || result.terminated)
                break;
        }
        const rl::StageMotionQualification qualification =
            rl::stage_motion_qualification(sim::CourseStage::balance, assisted_stance);
        if (!qualification.valid)
        {
            std::cerr << "balance controller diagnostics: rejection="
                << qualification.rejection_mask
                << " invalid=" << static_cast<int>(assisted_stance.invalid_reason())
                << " stance=" << assisted_stance.stable_stance_seconds()
                << " longest=" << assisted_stance.longest_stable_stance_seconds()
                << " max_joint=" << assisted_stance.maximum_joint_speed()
                << " survival=" << assisted_stance.elapsed_seconds()
                << " failures[support,body,upright,head,slip,turn,joint,vertical]=";
            for (const std::uint32_t failures : stance_failures)
                std::cerr << failures << ',';
            std::cerr << std::endl;
        }
        require(qualification.valid
                && assisted_stance.longest_stable_stance_seconds()
                    >= rl::standing_mastery_seconds,
            "shared balance controller cannot sustain a strict neutral physics stance");
        require(rl::training_preview_priority(
                sim::CourseStage::balance, assisted_stance) > 0,
            "stage-qualified standing environment disappears from the training PIP");
        sim::EnvironmentTestAccess::collapse_upper_body(assisted_stance);
        require(!assisted_stance.current_display_posture_valid(),
            "fresh geometric posture check accepts a collapsed current body");
        require(!rl::stage_display_sample_eligible(
                sim::CourseStage::balance, assisted_stance),
            "collapsed current frame is still published as a valid sample");
    }

    {
        constexpr std::size_t evaluation_agents = 6;
        std::uint32_t valid_agents = 0;
        for (std::size_t agent = 0; agent < evaluation_agents; ++agent)
        {
            const std::uint64_t seed = 0xE000u
                + static_cast<std::uint64_t>(agent) * 4099u;
            sim::Environment environment{ humanoid, seed };
            environment.set_course(sim::CourseStage::balance, 0.25f);
            const std::array<float, sim::action_count> raw_action{};
            for (int frame = 0; frame < 1200; ++frame)
            {
                const auto action = rl::effective_policy_action(
                    environment, raw_action, sim::CourseStage::balance);
                const sim::StepResult result = environment.step(action);
                if (environment.valid_motion()
                    && environment.longest_stable_stance_seconds()
                        >= rl::standing_mastery_seconds)
                    break;
                if (result.terminated)
                    break;
            }
            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(sim::CourseStage::balance, environment);
            const bool integrity = environment.body_integrity_valid();
            valid_agents += qualification.valid && integrity ? 1u : 0u;
            if (!qualification.valid || !integrity)
            {
                std::cerr << "evaluation seed " << seed
                    << " rejection=" << qualification.rejection_mask
                    << " invalid=" << static_cast<int>(environment.invalid_reason())
                    << " integrity=" << integrity
                    << " stance=" << environment.stable_stance_seconds()
                    << " longest=" << environment.longest_stable_stance_seconds()
                    << " max_joint=" << environment.maximum_joint_speed()
                    << " survival=" << environment.elapsed_seconds() << std::endl;
            }
        }
        require(valid_agents == evaluation_agents,
            "shared balance controller fails the strict six-of-six PPO seed gate");
    }

    {
            sim::Environment intact{ humanoid, 0x1A7E6u };
        require(intact.body_integrity_valid(),
            "fresh humanoid body fails the full skeleton integrity gate");
        sim::EnvironmentTestAccess::qualify_stable_stance(intact);
        sim::EnvironmentTestAccess::detach_left_support_cluster(intact);
        require(!intact.body_integrity_valid(),
            "detached foot cluster passes the full skeleton integrity gate");
        require(!rl::stage_display_sample_eligible(sim::CourseStage::balance, intact),
            "detached feet can still publish as a qualified training preview");
        require(rl::training_preview_frame_renderable(intact)
                && rl::training_preview_priority(sim::CourseStage::balance, intact) == 1,
            "finite rejected training attempts disappear instead of remaining diagnosable in PIP");
    }

    {
        sim::Environment authority{ humanoid, 0xA4710u };
        authority.set_course(sim::CourseStage::balance, 0.25f);
        const auto teacher = rl::balance_teacher_action(authority);
        float leg_energy = 0.0f;
        float arm_energy = 0.0f;
        for (std::size_t index = 0; index < 4; ++index)
            leg_energy += std::abs(teacher[index]);
        for (std::size_t index = 4; index < 8; ++index)
            arm_energy += std::abs(teacher[index]);
        require(leg_energy > arm_energy * 4.0f + 0.02f,
            "balance teacher still reaches for the arms before loading the feet");

        std::array<float, sim::action_count> arm_heavy{};
        arm_heavy.fill(1.0f);
        const auto effective = rl::effective_policy_action(
            authority, arm_heavy, sim::CourseStage::balance);
        float effective_legs = 0.0f;
        float effective_arms = 0.0f;
        for (std::size_t index = 0; index < 4; ++index)
            effective_legs += std::abs(effective[index]);
        for (std::size_t index = 4; index < 8; ++index)
            effective_arms += std::abs(effective[index]);
        require(effective_arms < effective_legs * 0.40f + 0.02f,
            "early balance still grants more authority to arms than legs");
    }

    {
        sim::Environment neutral_stance{ humanoid, 0x576A6Eu };
        sim::EnvironmentTestAccess::qualify_stable_stance(neutral_stance);
        require(rl::stage_motion_qualification(
                sim::CourseStage::balance, neutral_stance).valid,
            "neutral strict standing evidence is rejected");
        require(rl::stage_display_sample_eligible(
                sim::CourseStage::balance, neutral_stance),
            "current neutral strict stance is not eligible for top-priority PIP display");
        sim::EnvironmentTestAccess::force_arms_overhead(neutral_stance);
        const auto raised = rl::stage_motion_qualification(
            sim::CourseStage::balance, neutral_stance);
        require(!raised.valid
                && (raised.rejection_mask & rl::evidence_bit(
                    rl::MotionEvidenceFailure::non_neutral_posture)) != 0u,
            "arms-up standing exploit is still stage-valid");

        sim::Environment spinning_stance{ humanoid, 0x5A1E7u };
        sim::EnvironmentTestAccess::qualify_stable_stance(spinning_stance);
        sim::EnvironmentTestAccess::force_standing_spin(
            spinning_stance, rl::standing_qualification_spin_limit + 0.01f);
        const auto spinning = rl::stage_motion_qualification(
            sim::CourseStage::balance, spinning_stance);
        require(!spinning.valid
                && (spinning.rejection_mask & rl::evidence_bit(
                    rl::MotionEvidenceFailure::excessive_rotation)) != 0u,
            "rotating standing exploit is still stage-valid");

        rl::TrainingMetrics strict{};
        strict.evaluation_valid = true;
        strict.evaluation_invalid_runs = 0u;
        strict.evaluation_longest_stance = rl::standing_mastery_seconds;
        strict.evaluation_survival = rl::standing_mastery_seconds;
        strict.evaluation_spin_turns = rl::standing_mastery_spin_limit;
        strict.evaluation_max_joint_speed = 7.5f;
        require(rl::strict_balance_mastery(strict),
            "strict standing values cannot advance mastery");
        strict.evaluation_invalid_runs = 1u;
        require(rl::strict_balance_mastery(strict),
            "five-of-six strict standing seeds cannot advance robust mastery");
        strict.evaluation_invalid_runs = 2u;
        require(!rl::strict_balance_mastery(strict),
            "four-of-six standing seeds incorrectly advance mastery");
    }

    {
        const std::array<sim::CreatureBlueprint, 4> passive_rigs{
            sim::CreatureBlueprint::chicken(),
            sim::CreatureBlueprint::quadruped(),
            sim::CreatureBlueprint::crawler4(),
            sim::CreatureBlueprint::hexapod()
        };
        for (std::size_t index = 0; index < passive_rigs.size(); ++index)
        {
            sim::Environment environment{ passive_rigs[index], 0x7000u + index };
            const std::array<float, sim::action_count> zero{};
            for (int frame = 0; frame < 180; ++frame)
            {
                (void)environment.step(zero);
                if (!environment.body_integrity_valid())
                {
                    std::cerr << "passive integrity failure rig=" << index
                        << " frame=" << frame
                        << " invalid=" << static_cast<int>(environment.invalid_reason())
                        << std::endl;
                }
                require(environment.body_integrity_valid(),
                    "head or passive tail escaped the articulated body");
                if (!environment.valid_motion())
                    environment.reset(0x7100u + index * 257u + static_cast<std::size_t>(frame));
            }
        }
    }

    {
        sim::Environment flip_semantics{ humanoid, 0xF11Fu };
        require(flip_semantics.maximum_flip_turns() == 0.0f
                && flip_semantics.uncontrolled_spin_turns() == 0.0f,
            "fresh rig does not separate flip and spin counters");
        sim::EnvironmentTestAccess::force_standing_spin(flip_semantics, 0.25f);
        require(flip_semantics.uncontrolled_spin_turns() == 0.25f,
            "grounded standing rotation cannot be represented by the strict gate");
        flip_semantics.reset(0xF120u);
        require(flip_semantics.uncontrolled_spin_turns() == 0.0f,
            "standing spin evidence leaks across episode resets");
    }

    require(rl::policy_candidate_better(2u, 1.0f, 1u, 1000.0f, true),
        "higher stage-valid evidence loses to scalar reward");
    require(!rl::policy_candidate_better(1u, 1000.0f, 2u, 1.0f, true),
        "high-reward lower-quality exploit can replace a valid controller");

    const sim::CreatureBlueprint quadruped = sim::CreatureBlueprint::quadruped();
    const sim::CreatureBlueprint crawler4 = sim::CreatureBlueprint::crawler4();
    const sim::CreatureBlueprint hexapod = sim::CreatureBlueprint::hexapod();
    require(quadruped.support_seed_count() == 4,
        "quadruped is still semantically a two-foot biped");
    require(quadruped.additional_left_contact_nodes.size() == 1
            && quadruped.additional_right_contact_nodes.size() == 1,
        "quadruped diagonal support pairs are missing");
    require(std::abs(quadruped.nodes[4].x - quadruped.nodes[5].x) > 0.30f
            && std::abs(quadruped.nodes[6].x - quadruped.nodes[7].x) > 0.30f,
        "quadruped near/far legs overlap in the side-view geometry");
    require(crawler4.nodes.size() >= 9 && crawler4.bones.size() >= 10
            && crawler4.support_seed_count() == 4,
        "four-legged crawler geometry or support semantics are incomplete");
    require(hexapod.nodes.size() >= 12 && hexapod.bones.size() >= 16
            && hexapod.support_seed_count() == 6,
        "six-legged hexapod geometry or support semantics are incomplete");

    {
        sim::Environment biped_support{ humanoid, 0xFEE7u };
        biped_support.set_course(sim::CourseStage::balance, 0.25f);
        const std::array<float, sim::action_count> zero_actions{};
        bool support_observed = false;
        for (int frame = 0; frame < 60; ++frame)
        {
            const sim::StepResult result = biped_support.step(zero_actions);
            support_observed = support_observed
                || biped_support.left_supported() || biped_support.right_supported();
            if (result.terminated)
                break;
        }
        require(support_observed,
            "passive biped heel/toe nodes never became valid support contacts");
        require(biped_support.invalid_reason() != sim::InvalidMotion::sustained_flight,
            "grounded passive biped feet were still classified as flying");
    }

    for (std::size_t stage_index = 0; stage_index < sim::course_stage_count; ++stage_index)
    {
        sim::Environment environment{ humanoid, 42u + stage_index };
        const auto stage = static_cast<sim::CourseStage>(stage_index);
        environment.set_course(stage, 0.45f);
        if (stage == sim::CourseStage::hurdles
            || stage == sim::CourseStage::moving_hazards)
            require(!environment.course_features().empty(),
                "moving obstacle curriculum stage has no course features");
        const std::array<float, sim::action_count> zero_actions{};
        for (int frame = 0; frame < 120; ++frame)
        {
            const sim::StepResult result = environment.step(zero_actions);
            require(std::isfinite(result.reward), "course reward is not finite");
            require(std::isfinite(result.forward_speed), "course speed is not finite");
            const auto observation = environment.observation();
            for (const float value : observation)
                require(std::isfinite(value), "course observation is not finite");
            if (result.terminated)
                environment.reset(42u + stage_index + static_cast<std::size_t>(frame));
        }
    }


    {
        sim::Environment procedural{ humanoid, 0xC0A57u };
        procedural.set_course(sim::CourseStage::moving_hazards, 0.75f);
        const float initial_progress = procedural.course_progress();
        const float initial_height = procedural.ground_height_at(29.0f);
        const std::array<float, sim::action_count> zero_actions{};
        for (int frame = 0; frame < 90; ++frame)
        {
            const sim::StepResult result = procedural.step(zero_actions);
            require(std::isfinite(result.reward), "procedural obstacle reward is not finite");
            (void)result.terminated;
        }
        require(procedural.course_progress() > initial_progress,
            "procedural course does not advance when the creature is stationary");
        require(std::abs(procedural.ground_height_at(29.0f) - initial_height) > 0.001f,
            "procedural inclines and hills do not move through the training lane");

        std::array<bool, 6> found{};
        for (const sim::CourseFeature& feature : procedural.course_features())
            found[static_cast<std::size_t>(feature.kind)] = true;
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::hurdle)],
            "procedural course omitted hurdles");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::overhead_bar)],
            "procedural course omitted overhead bars");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::moving_hazard)],
            "procedural course omitted moving hazards");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::rock)],
            "procedural course omitted rocks");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::projectile)],
            "procedural course omitted thrown objects");
    }

    {
        sim::Environment flat_obstacles{ humanoid, 0xF1A7u };
        flat_obstacles.set_course(sim::CourseStage::hurdles, 0.45f);
        require(!flat_obstacles.course_features().empty(),
            "flat debris lesson has no obstacles");
        for (const sim::CourseFeature& feature : flat_obstacles.course_features())
        {
            require(sim::course_zone_is_flat(sim::course_marker_distance_m(feature.marker_sequence)),
                "early obstacle curriculum placed debris on a hill or slope");
        }
    }


    {
        rl::PpoTrainer stance_trainer{ humanoid, 8 };
        stance_trainer.set_cpu_mode(1);
        constexpr int standing_update_budget = 40;
        for (int update = 0; update < standing_update_budget
            && !stance_trainer.has_best_policy(); ++update)
            stance_trainer.train_one_update();
        if (!stance_trainer.has_best_policy())
        {
            const rl::TrainingMetrics& metrics = stance_trainer.metrics();
            std::cerr << "standing acceptance diagnostics: updates=" << metrics.update
                << " evaluations=" << metrics.evaluation_count
                << " valid=" << metrics.evaluation_valid
                << " rejection=" << metrics.evaluation_rejection_mask
                << " invalid_runs=" << metrics.evaluation_invalid_runs
                << " stance=" << metrics.evaluation_stable_stance
                << " longest=" << metrics.evaluation_longest_stance
                << " max_joint=" << metrics.evaluation_max_joint_speed
                << " survival=" << metrics.evaluation_survival << '\n';
        }
        require(stance_trainer.metrics().evaluation_count >= 1u,
            "bounded standing training never ran deterministic evaluation");
        require(stance_trainer.metrics().evaluation_valid,
            "bounded standing training did not produce a valid standing candidate");
        require(stance_trainer.metrics().evaluation_quality_key != 0u,
            "valid standing candidate has no lexicographic quality evidence");
        require(stance_trainer.has_best_policy(),
            "first valid standing candidate was not retained as the best controller");
    }

    rl::PpoTrainer trainer{ humanoid, 16 };
    require(trainer.rollout_worker_count() >= 1, "parallel rollout worker count is invalid");
    trainer.set_cpu_mode(1);
    const std::size_t normal_workers = trainer.rollout_worker_count();
    trainer.set_cpu_mode(2);
    const std::size_t faster_workers = trainer.rollout_worker_count();
    trainer.set_cpu_mode(4);
    const std::size_t maximum_workers = trainer.rollout_worker_count();
    require(normal_workers <= faster_workers && faster_workers <= maximum_workers,
        "speed modes do not increase persistent worker budget");
    require(maximum_workers == trainer.maximum_worker_count(),
        "MAX CPU does not enable the full persistent worker pool");
    require(trainer.exploration() < 0.20f, "fresh exploration is too aggressive");
    require(trainer.exploration() <= 0.081f, "fresh policy still applies an aggressive spawn impulse");
    trainer.set_course(sim::CourseStage::balance, 0.25f, false);
    for (int update = 0; update < 2; ++update)
    {
        trainer.train_one_update();
        const rl::TrainingMetrics& metrics = trainer.metrics();
        require(metrics.update == static_cast<std::uint64_t>(update + 1), "PPO update counter mismatch");
        require(std::isfinite(metrics.mean_reward), "PPO mean reward is not finite");
        require(std::isfinite(metrics.mean_speed), "PPO mean speed is not finite");
        require(std::isfinite(metrics.policy_loss), "PPO policy loss is not finite");
        require(std::isfinite(metrics.value_loss), "PPO value loss is not finite");
        require(metrics.total_updates == metrics.update,
            "fresh cumulative update count does not track policy updates");
        require(metrics.total_environment_steps == metrics.environment_steps,
            "fresh cumulative environment count does not track policy steps");
        require(metrics.total_training_seconds > 0.0,
            "cumulative training time did not advance");
    }

    const std::filesystem::path temporary =
        std::filesystem::temp_directory_path() / "runner-v061-core-test.eppo";
    std::string error{};
    require(trainer.save_checkpoint(temporary, error), "failed to save checkpoint: " + error);
    rl::PpoTrainer resumed{ humanoid, 16 };
    require(resumed.load_checkpoint(temporary, error, false), "failed to resume checkpoint: " + error);
    require(resumed.policy().parameters() == trainer.policy().parameters(), "checkpoint policy mismatch");
    require(resumed.metrics().update == trainer.metrics().update, "checkpoint update count was not restored");
    require(resumed.metrics().total_updates == trainer.metrics().total_updates
            && resumed.metrics().total_environment_steps
                == trainer.metrics().total_environment_steps,
        "checkpoint cumulative update/environment totals were not restored");
    require(resumed.metrics().total_training_seconds == trainer.metrics().total_training_seconds,
        "checkpoint cumulative training time was not restored");
    require(resumed.optimizer_step() == trainer.optimizer_step(), "checkpoint optimizer state was not restored");
    require(trainer.checkpoint_data().training_semantics == rl::training_semantics_version,
        "checkpoint does not persist the current training-semantics signature");
    require(resumed.course_stage() == trainer.course_stage(), "checkpoint curriculum stage was not restored");

    rl::PpoTrainer::CheckpointData legacy = trainer.checkpoint_data();
    legacy.training_semantics = rl::training_semantics_version - 1u;
    legacy.first_moment.clear();
    legacy.second_moment.clear();
    legacy.best_parameters.clear();
    rl::PpoTrainer blocked_legacy{ humanoid, 16 };
    require(!blocked_legacy.apply_checkpoint_data(legacy, error, false),
        "legacy semantics resumed as valid mastery instead of requiring transfer");
    rl::PpoTrainer transferred_legacy{ humanoid, 16 };
    require(transferred_legacy.apply_checkpoint_data(legacy, error, true),
        "explicit dimension-compatible legacy weight transfer failed: " + error);
    require(transferred_legacy.policy().parameters() == trainer.policy().parameters()
            && transferred_legacy.metrics().update == 0u
            && transferred_legacy.optimizer_step() == 0u
            && transferred_legacy.controller_state() == rl::ControllerState::transferred,
        "legacy transfer retained optimizer, mastery, or non-transfer controller state");
    const std::filesystem::path legacy_path =
        std::filesystem::temp_directory_path() / "runner-v0715-legacy-transfer-test.eppo";
    require(rl::PpoTrainer::write_checkpoint_data(legacy, legacy_path, error),
        "failed to write legacy transfer fixture: " + error);
    rl::PpoTrainer loaded_legacy{ humanoid, 16 };
    require(!loaded_legacy.load_checkpoint(legacy_path, error, false)
            && loaded_legacy.load_checkpoint(legacy_path, error, true),
        "file-based legacy checkpoint is not resume-blocked and transfer-enabled");
    std::filesystem::remove(legacy_path);

    rl::PpoTrainer wrong_rig{ sim::CreatureBlueprint::quadruped(), 16 };
    require(!wrong_rig.load_checkpoint(temporary, error, false), "mismatched rig checkpoint resumed silently");
    require(wrong_rig.load_checkpoint(temporary, error, true), "intentional v0.4 transfer failed: " + error);
    require(wrong_rig.metrics().update == 0, "transfer retained incompatible progress metrics");
    require(wrong_rig.optimizer_step() == 0, "transfer retained incompatible optimizer state");
    std::filesystem::remove(temporary);

    {
        rl::AutonomousTrainer autonomous{ humanoid, 16 };
        autonomous.set_background_enabled(false);
        autonomous.synchronize();
        require(autonomous.autonomy_status().stage == sim::CourseStage::balance,
            "autonomous trainer did not start with balance curriculum");
        require(autonomous.autonomy_status().environment_count == 16,
            "autonomous trainer environment count mismatch");
        autonomous.train_one_update();
        for (int attempt = 0; attempt < 400 && autonomous.metrics().update == 0; ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            autonomous.synchronize();
        }
        require(autonomous.metrics().update >= 1, "coroutine background worker did not process requested update");

        autonomous.set_updates_per_cycle(4);
        autonomous.set_background_enabled(true);
        sim::CreatureBlueprint edited = humanoid;
        edited.nodes[1].x += 0.01f;
        edited.rebuild_rest_lengths();
        const auto command_started = std::chrono::steady_clock::now();
        autonomous.set_blueprint(edited, true);
        const auto command_elapsed = std::chrono::steady_clock::now() - command_started;
        require(command_elapsed < std::chrono::milliseconds(20),
            "hip edit blocked the caller on active training work");
        for (int attempt = 0; attempt < 1200 && autonomous.rig_signature() != edited.signature(); ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            autonomous.synchronize();
        }
        require(autonomous.rig_signature() == edited.signature(),
            "queued hip edit was not eventually published");
        autonomous.set_updates_per_cycle(1);
        require(autonomous.updates_per_cycle() == 1, "NORMAL speed mode did not latch");
        autonomous.set_updates_per_cycle(2);
        require(autonomous.updates_per_cycle() == 2, "FASTER speed mode did not latch");
        autonomous.set_updates_per_cycle(4);
        require(autonomous.updates_per_cycle() == 4, "MAX CPU speed mode did not latch");
    }

    std::cout << "Runner core standing, PIP, obstacle, integrity, telemetry, concurrency, gait, and rig-edit tests passed\n";
    return EXIT_SUCCESS;
}
