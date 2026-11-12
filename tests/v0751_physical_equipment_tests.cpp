#include "integration.hpp"
#include "ppo.hpp"
#include "simulation.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static void support(Environment& environment, bool left, bool right) noexcept
        {
            for (Particle& particle : environment.particles_)
                particle.grounded = false;
            if (environment.valid_node(environment.blueprint_.left_contact_node))
                environment.particles_[environment.blueprint_.left_contact_node]
                    .grounded = left;
            if (environment.valid_node(environment.blueprint_.right_contact_node))
                environment.particles_[environment.blueprint_.right_contact_node]
                    .grounded = right;
        }

        static void physical_ready(Environment& environment, float speed = 0.0f,
            bool left = true, bool right = true,
            float aim_offset = 0.0f) noexcept
        {
            support(environment, left, right);
            const std::uint16_t root = environment.blueprint_.root_node;
            if (environment.valid_node(root))
            {
                environment.particles_[root].previous.x =
                    environment.particles_[root].position.x - speed / 60.0f;
                environment.previous_pelvis_.x =
                    environment.particles_[root].position.x - speed / 60.0f;
            }
            environment.equipment_state_ = EquipmentState::ready;
            environment.equipment_state_seconds_ = 1.0f;
            environment.equipment_body_speed_ = std::abs(speed);
            environment.equipment_plant_seconds_ = left && right ? 1.0f : 0.0f;
            environment.equipment_stopped_ = left && right && std::abs(speed) <= 0.18f;
            const std::uint16_t rear = environment.equipment_mount_node();
            const std::uint16_t front = environment.equipment_foregrip_node();
            const std::uint16_t shoulder =
                environment.equipment_mount_shoulder_node();
            if (environment.valid_node(rear) && environment.valid_node(front)
                && environment.valid_node(shoulder) && rear != front)
            {
                const WeaponGeometry geometry = weapon_geometry(
                    environment.weapon_class_);
                const Vec2 body_up = normalized(
                    environment.particles_[environment.blueprint_.torso_node]
                        .position
                        - environment.particles_[environment.blueprint_.root_node]
                            .position,
                    { 0.0f, 1.0f });
                const float facing = environment.facing_direction();
                const Vec2 body_forward = facing
                    * Vec2{ body_up.y, -body_up.x };
                for (int iteration = 0; iteration < 3; ++iteration)
                {
                    const float angle = wrap_angle(
                        environment.equipment_recommended_aim_angle()
                        + aim_offset);
                    const Vec2 direction{ std::cos(angle), std::sin(angle) };
                    const Vec2 shoulder_position =
                        environment.particles_[shoulder].position;
                    const Vec2 pocket = geometry.shouldered
                        ? shoulder_position + body_forward * 0.04f
                            - body_up * 0.10f
                        : shoulder_position + body_forward * 0.28f
                            - body_up * 0.16f;
                    const Vec2 rear_position = geometry.shouldered
                        ? pocket + direction * geometry.butt_to_rear_grip
                        : pocket;
                    environment.particles_[rear].position = rear_position;
                    environment.particles_[front].position = rear_position
                        + direction * geometry.rear_to_foregrip;
                    environment.particles_[rear].previous = rear_position;
                    environment.particles_[front].previous =
                        environment.particles_[front].position;
                    environment.equipment_aim_angle_ = angle;
                }
            }
            else
            {
                environment.equipment_aim_angle_ = wrap_angle(
                    environment.equipment_recommended_aim_angle()
                        + aim_offset);
            }
            environment.previous_equipment_aim_angle_ =
                environment.equipment_aim_angle_;
            environment.equipment_aim_rate_ = 0.0f;
            environment.equipment_aim_settle_seconds_ = 1.0f;
            environment.equipment_cooldown_seconds_ = 0.0f;
            environment.equipment_recoil_recovery_seconds_ = 0.0f;
        }

        static void invalidate_grip(Environment& environment,
            int failure) noexcept
        {
            const EquipmentGripFrame frame = environment.equipment_grip_frame();
            if (!environment.valid_node(frame.rear_node)
                || !environment.valid_node(frame.front_node))
                return;
            if (failure == 0)
                environment.particles_[frame.front_node].position = frame.rear;
            else if (failure == 1)
                environment.particles_[frame.front_node].position = frame.rear
                    - frame.axis * weapon_geometry(environment.weapon_class_)
                        .rear_to_foregrip;
            else if (failure == 2)
            {
                environment.particles_[frame.rear_node].position.y += 0.30f;
                environment.particles_[frame.front_node].position.y += 0.30f;
            }
            else if (failure == 3)
            {
                environment.particles_[frame.front_node].position.x =
                    std::numeric_limits<float>::quiet_NaN();
            }
            else
            {
                for (MotorConstraint& motor : environment.blueprint_.motors)
                {
                    if (motor.c == frame.rear_node)
                        motor.enabled = false;
                }
            }
        }

        static void invalidate_root_index(Environment& environment) noexcept
        {
            environment.blueprint_.root_node =
                std::numeric_limits<std::uint16_t>::max();
        }

        static void invalidate_torso_index(Environment& environment) noexcept
        {
            environment.blueprint_.torso_node =
                std::numeric_limits<std::uint16_t>::max();
        }

        static std::uint16_t rear_grip_parent(
            const Environment& environment) noexcept
        {
            return environment.equipment_mount_parent_node();
        }

        static float equipment_body_speed(
            const Environment& environment) noexcept
        {
            return environment.equipment_body_speed_;
        }

        static void offset_target_y(Environment& environment,
            float offset) noexcept
        {
            environment.equipment_target_.position.y += offset;
        }

        static void occlude_target(Environment& environment) noexcept
        {
            const Vec2 mount = environment.equipment_muzzle_position();
            const Vec2 target = environment.equipment_target_.position;
            const Vec2 middle = lerp(mount, target, 0.50f);
            environment.course_features_.push_back({
                CourseFeatureKind::rock, middle, {}, 0.62f, {}, 75'100 });
        }

        static void empty_magazine(Environment& environment) noexcept
        {
            environment.equipment_rounds_remaining_ = 0u;
        }

        static void unsettle(Environment& environment) noexcept
        {
            environment.equipment_aim_settle_seconds_ = 0.0f;
        }

        static void deactivate_target_and_launch(Environment& environment) noexcept
        {
            environment.equipment_target_.active = false;
            const WeaponProfile profile = weapon_profile(environment.weapon_class_);
            const Vec2 muzzle = environment.equipment_muzzle_position();
            environment.equipment_projectiles_.push_back({
                environment.weapon_class_, muzzle + Vec2{ 0.0f, 2.0f },
                { 0.0f, 0.0f }, profile.projectile_radius, 1u, true, false,
                2.0f / 60.0f });
        }

        static void launch_crossing_miss(Environment& environment,
            float vertical_error) noexcept
        {
            const WeaponProfile profile = weapon_profile(
                environment.weapon_class_);
            const float facing = environment.facing_direction();
            environment.equipment_projectiles_.push_back({
                environment.weapon_class_,
                environment.equipment_target_.position
                    + Vec2{ -facing * 0.30f, vertical_error },
                { facing * profile.projectile_speed, 0.0f },
                profile.projectile_radius, 1u, true, false, 1.0f });
        }

        static void equipment_tick(Environment& environment,
            float state_action, float trigger = -1.0f) noexcept
        {
            std::array<float, action_count> action{};
            action[equipment_state_action] = state_action;
            action[equipment_trigger_action] = trigger;
            environment.update_equipment(action, 1.0f / 60.0f);
        }

        static void face_left(Environment& environment) noexcept
        {
            environment.shuttle_state_.phase = ShuttlePhase::traverse;
            environment.shuttle_state_.facing_direction = -1.0f;
            environment.shuttle_state_.locomotion_direction = -1.0f;
            environment.mirror_rig_about_root();
            environment.reset_equipment();
        }

    };
}

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Epoch2DWalkEngine v0.7.51 Runner-example physical equipment failure: "
            << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    [[nodiscard]] bool same_motor(const runner::sim::MotorConstraint& first,
        const runner::sim::MotorConstraint& second) noexcept
    {
        return first.a == second.a && first.pivot == second.pivot
            && first.c == second.c
            && first.minimum_angle == second.minimum_angle
            && first.maximum_angle == second.maximum_angle
            && first.neutral_angle == second.neutral_angle
            && first.strength == second.strength
            && first.enabled == second.enabled;
    }

    struct ArmChain
    {
        std::size_t shoulder_motor{};
        std::size_t elbow_motor{};
        std::uint16_t shoulder{};
        std::uint16_t elbow{};
        std::uint16_t hand{};
    };

    [[nodiscard]] std::array<ArmChain, 2u> human_arm_chains(
        const runner::sim::CreatureBlueprint& rig)
    {
        std::array<ArmChain, 2u> result{};
        std::size_t count{};
        for (std::size_t shoulder_index = 0u;
            shoulder_index < rig.active_motor_count; ++shoulder_index)
        {
            const runner::sim::MotorConstraint& shoulder =
                rig.motors[shoulder_index];
            if (!shoulder.enabled
                || runner::rl::motor_drives_support_branch(rig, shoulder)
                || shoulder.a != rig.torso_node)
                continue;
            for (std::size_t elbow_index = 0u;
                elbow_index < rig.active_motor_count; ++elbow_index)
            {
                const runner::sim::MotorConstraint& elbow =
                    rig.motors[elbow_index];
                if (!elbow.enabled || elbow_index == shoulder_index
                    || runner::rl::motor_drives_support_branch(rig, elbow)
                    || elbow.pivot != shoulder.c)
                    continue;
                if (count < result.size())
                {
                    result[count++] = ArmChain{ shoulder_index, elbow_index,
                        shoulder.pivot, shoulder.c, elbow.c };
                }
                break;
            }
        }
        require(count == result.size(),
            "Human physical arm topology does not expose two shoulder/elbow/hand chains");
        return result;
    }

    struct PhysicalAimPose
    {
        runner::Vec2 primary_shoulder{};
        runner::Vec2 primary_elbow{};
        runner::Vec2 primary_hand{};
        runner::Vec2 secondary_shoulder{};
        runner::Vec2 secondary_elbow{};
        runner::Vec2 secondary_hand{};
        runner::Vec2 root{};
        runner::Vec2 physical_axis{};
        runner::Vec2 muzzle{};
        float foregrip_projection{};
        float foregrip_axis_error{};
        float rear_grip_shoulder_distance{};
        float stock_seat_error{};
        std::array<float, 4u> joint_offsets{};
        std::array<float, runner::sim::action_count> final_action{};
        std::uint32_t ready_frame{};
        bool left_supported{};
        bool right_supported{};
        float plant_seconds{};
        float settle_seconds{};
    };

    [[nodiscard]] PhysicalAimPose acquire_physical_aim_pose(
        std::uint64_t seed, float target_height_offset = 0.0f)
    {
        using namespace runner;
        sim::Environment environment{ sim::CreatureBlueprint::humanoid(), seed };
        environment.set_course(sim::CourseStage::equipment_targets, 0.30f);
        environment.configure_equipment(sim::WeaponClass::carbine, 7.0f);
        sim::EnvironmentTestAccess::offset_target_y(
            environment, target_height_offset);
        environment.set_equipment_directive(
            sim::EquipmentDirective::acquire_and_aim);

        std::array<float, sim::action_count> no_policy{};
        PhysicalAimPose pose{};
        bool ready{};
        for (std::uint32_t frame = 0u;
            frame < 720u && environment.valid_motion(); ++frame)
        {
            pose.final_action = rl::effective_policy_action(environment,
                no_policy, sim::CourseStage::equipment_targets, 1.0f,
                sim::GuidanceMode::assisted);
            static_cast<void>(environment.step(
                pose.final_action, 1.0f / 60.0f));
            require(environment.shots_fired() == 0u
                    && environment.equipment_projectiles().empty(),
                "acquire-and-aim launched a shot before an explicit fire directive");
            if (environment.equipment_state() == sim::EquipmentState::ready
                && environment.equipment_engagement_ready())
            {
                pose.ready_frame = frame;
                ready = true;
                break;
            }
        }
        if (!ready || !environment.valid_motion())
        {
            std::cerr << "physical-pose seed=" << seed
                << " facing=" << environment.facing_direction()
                << " valid=" << environment.valid_motion()
                << " reason=" << sim::invalid_motion_name(
                    environment.invalid_reason())
                << " state=" << sim::equipment_state_name(
                    environment.equipment_state())
                << " stopped=" << environment.equipment_stopped()
                << " body-speed="
                << sim::EnvironmentTestAccess::equipment_body_speed(environment)
                << " plant=" << environment.equipment_plant_seconds()
                << " settle=" << environment.equipment_aim_settle_seconds()
                << " aim=" << environment.equipment_aim_angle()
                << " desired=" << environment.equipment_recommended_aim_angle()
                << " block=" << sim::equipment_fire_block_name(
                    environment.equipment_fire_block());
            const sim::EquipmentGripFrame grip =
                environment.equipment_grip_frame();
            std::cerr << " grip-resolved=" << grip.resolved
                << " grip-valid=" << grip.valid << " span=" << grip.span
                << " seat=" << grip.stock_seat_error
                << " root-x=" << environment.particles()[
                    environment.blueprint().root_node].position.x << '\n';
        }
        require(ready && environment.valid_motion(),
            "finite physical arm chain never reached a settled READY pose");

        const sim::CreatureBlueprint& rig = environment.blueprint();
        const std::array<ArmChain, 2u> chains = human_arm_chains(rig);
        const std::uint16_t mount = environment.equipment_mount_node_index();
        const std::size_t primary = chains[0].hand == mount ? 0u : 1u;
        const std::size_t secondary = primary == 0u ? 1u : 0u;
        require(chains[primary].hand == mount,
            "weapon mount is not a real Human hand endpoint");
        const auto& particles = environment.particles();
        auto position = [&](std::uint16_t node)
        {
            require(node < particles.size(),
                "physical arm topology referenced a missing particle");
            return particles[node].position;
        };
        pose.primary_shoulder = position(chains[primary].shoulder);
        pose.primary_elbow = position(chains[primary].elbow);
        pose.primary_hand = position(chains[primary].hand);
        pose.secondary_shoulder = position(chains[secondary].shoulder);
        pose.secondary_elbow = position(chains[secondary].elbow);
        pose.secondary_hand = position(chains[secondary].hand);
        pose.root = position(rig.root_node);
        const sim::EquipmentGripFrame grip = environment.equipment_grip_frame();
        require(grip.resolved && grip.valid
                && grip.rear_node == chains[primary].hand
                && grip.front_node == chains[secondary].hand,
            "READY pose did not resolve its two real hand endpoints");
        pose.physical_axis = grip.axis;
        pose.muzzle = grip.muzzle;
        const Vec2 grip_delta = pose.secondary_hand - pose.primary_hand;
        pose.foregrip_projection = dot(grip_delta, pose.physical_axis);
        pose.foregrip_axis_error = std::abs(cross(
            pose.physical_axis, grip_delta));
        pose.rear_grip_shoulder_distance = length(
            pose.primary_hand - pose.primary_shoulder);
        pose.stock_seat_error = grip.stock_seat_error;
        pose.left_supported = environment.left_supported();
        pose.right_supported = environment.right_supported();
        pose.plant_seconds = environment.equipment_plant_seconds();
        pose.settle_seconds = environment.equipment_aim_settle_seconds();
        for (std::size_t chain = 0u; chain < chains.size(); ++chain)
        {
            for (std::size_t member = 0u; member < 2u; ++member)
            {
                const std::size_t motor_index = member == 0u
                    ? chains[chain].shoulder_motor : chains[chain].elbow_motor;
                const sim::MotorConstraint& motor = rig.motors[motor_index];
                const sim::MotorDiagnostic diagnostic =
                    environment.motor_diagnostic(motor_index);
                pose.joint_offsets[chain * 2u + member] = std::abs(wrap_angle(
                    diagnostic.current_angle - motor.neutral_angle));
            }
        }
        return pose;
    }
}

int main()
{
    using namespace runner;

    require(sim::swept_circle_hit_fraction({ 0.0f, 0.0f }, { 10.0f, 0.0f },
                { 5.0f, 0.0f }, 0.25f) > 0.40f
            && sim::swept_circle_hit_fraction({ 0.0f, 0.0f }, { 10.0f, 0.0f },
                { 5.0f, 0.0f }, 0.25f) < 0.50f
            && sim::swept_circle_hit_fraction({ 0.0f, 0.0f }, { 10.0f, 0.0f },
                { 5.0f, 1.0f }, 0.25f) > 1.0f,
        "swept trajectory does not report the first real contact fraction");

    sim::Environment default_human{
        sim::CreatureBlueprint::humanoid(), 0x751000u };
    default_human.set_course(sim::CourseStage::uneven, 0.30f);
    require(default_human.weapon_class() == sim::WeaponClass::carbine
            && default_human.equipment_state() == sim::EquipmentState::safe_carry
            && default_human.equipment_rounds_remaining()
                == default_human.equipment_magazine_capacity()
            && default_human.equipment_magazine_capacity() > 0u,
        "default Human does not own a finite, loaded safe-carry carbine");

    {
        constexpr std::array<sim::WeaponClass, 3u> weapons{
            sim::WeaponClass::sidearm, sim::WeaponClass::carbine,
            sim::WeaponClass::launcher
        };
        std::uint64_t seed = 0x751010u;
        for (const sim::WeaponClass weapon : weapons)
        {
            const sim::WeaponGeometry geometry = sim::weapon_geometry(weapon);
            require(geometry.overall_length > 0.0f
                    && geometry.butt_to_rear_grip > 0.0f
                    && geometry.rear_to_foregrip > 0.0f
                    && std::abs(geometry.rear_to_muzzle
                        - (geometry.overall_length
                            - geometry.butt_to_rear_grip)) <= 1.0e-6f,
                "weapon geometry does not share one stock/grip/muzzle frame");
            sim::Environment environment{
                sim::CreatureBlueprint::humanoid(), seed++ };
            environment.configure_equipment(weapon, 7.0f);
            sim::EnvironmentTestAccess::physical_ready(environment);
            const sim::EquipmentGripFrame frame =
                environment.equipment_grip_frame();
            require(frame.resolved && frame.valid
                    && length(environment.equipment_display_direction()
                        - frame.axis) <= 1.0e-6f
                    && length(environment.equipment_muzzle_position()
                        - frame.muzzle) <= 1.0e-6f
                    && length(frame.muzzle - (frame.rear + frame.axis
                        * geometry.rear_to_muzzle)) <= 1.0e-6f
                    && (!geometry.shouldered
                        || frame.stock_seat_error <= 0.12f)
                    && environment.equipment_fire_block()
                        == sim::EquipmentFireBlock::none,
                "valid two-hand fixture did not own display, muzzle, and fire authority");

            const auto& before_particles = environment.particles();
            const Vec2 rear_previous = before_particles[frame.rear_node].previous;
            const Vec2 front_previous = before_particles[frame.front_node].previous;
            const Vec2 root_position = before_particles[
                environment.blueprint().root_node].position;
            const Vec2 root_previous = before_particles[
                environment.blueprint().root_node].previous;
            const std::uint16_t elbow =
                sim::EnvironmentTestAccess::rear_grip_parent(environment);
            const Vec2 elbow_previous = before_particles[elbow].previous;
            sim::EnvironmentTestAccess::equipment_tick(environment, 1.0f, 1.0f);
            const float recoil_step = sim::weapon_profile(weapon).recoil / 60.0f;
            const Vec2 inferred_origin = environment.equipment_projectiles().empty()
                ? Vec2{} : environment.equipment_projectiles()[0].position
                    - environment.equipment_projectiles()[0].velocity
                        * (1.0f / 60.0f);
            require(environment.shots_fired() == 1u
                    && environment.equipment_projectiles().size() == 1u
                    && length(inferred_origin
                        - frame.muzzle) <= 1.0e-6f
                    && length(environment.particles()[frame.rear_node].previous
                        - (rear_previous + frame.axis
                            * (recoil_step * 0.60f))) <= 1.0e-6f
                    && length(environment.particles()[frame.front_node].previous
                        - (front_previous + frame.axis
                            * (recoil_step * 0.40f))) <= 1.0e-6f
                    && length(environment.particles()[elbow].previous
                        - elbow_previous) <= 1.0e-6f
                    && length(environment.particles()[
                        environment.blueprint().root_node].position
                        - root_position) <= 1.0e-6f
                    && length(environment.particles()[
                        environment.blueprint().root_node].previous
                        - root_previous) <= 1.0e-6f,
                "shot did not originate/recoil through the same finite grip frame");
        }
    }

    for (int failure = 0; failure < 5; ++failure)
    {
        sim::Environment malformed{
            sim::CreatureBlueprint::humanoid(), 0x751020u
                + static_cast<std::uint64_t>(failure) };
        malformed.configure_equipment(sim::WeaponClass::carbine, 7.0f);
        sim::EnvironmentTestAccess::physical_ready(malformed);
        require(malformed.equipment_grip_valid(),
            "malformed-grip fixture did not begin with a valid frame");
        const Vec2 root_position = malformed.particles()[
            malformed.blueprint().root_node].position;
        const Vec2 root_previous = malformed.particles()[
            malformed.blueprint().root_node].previous;
        sim::EnvironmentTestAccess::invalidate_grip(malformed, failure);
        require(!malformed.equipment_grip_valid()
                && malformed.equipment_fire_block()
                    == sim::EquipmentFireBlock::invalid_grip,
            "coincident, crossed, unseated, nonfinite, or incomplete grip did not fail closed");
        sim::EnvironmentTestAccess::equipment_tick(malformed, 1.0f, 1.0f);
        require(malformed.shots_fired() == 0u
                && malformed.equipment_projectiles().empty()
                && length(malformed.particles()[
                    malformed.blueprint().root_node].position
                    - root_position) <= 1.0e-6f
                && length(malformed.particles()[
                    malformed.blueprint().root_node].previous
                    - root_previous) <= 1.0e-6f,
            "invalid grip launched a round or wrote the root");
    }
    {
        sim::Environment malformed_root{
            sim::CreatureBlueprint::humanoid(), 0x75102fu };
        malformed_root.configure_equipment(sim::WeaponClass::carbine, 7.0f);
        sim::EnvironmentTestAccess::invalidate_root_index(malformed_root);
        require(!malformed_root.equipment_grip_frame().resolved,
            "invalid root topology was dereferenced into a grip frame");
        sim::Environment malformed_torso{
            sim::CreatureBlueprint::humanoid(), 0x751030u };
        malformed_torso.configure_equipment(sim::WeaponClass::carbine, 7.0f);
        sim::EnvironmentTestAccess::invalidate_torso_index(malformed_torso);
        require(!malformed_torso.equipment_grip_frame().resolved,
            "invalid torso topology was dereferenced into a grip frame");
    }

    {
        const sim::CreatureBlueprint human = sim::CreatureBlueprint::humanoid();
        const sim::CreatureBlueprint repeated =
            sim::CreatureBlueprint::humanoid();
        for (std::size_t index = 0u; index < human.active_motor_count; ++index)
            require(same_motor(human.motors[index], repeated.motors[index]),
                "repeated Human construction changed an authored motor");
        for (const std::size_t index : { 4u, 5u, 6u, 7u })
        {
            const sim::MotorConstraint base = human.motors[index];
            const sim::MotorConstraint equipment =
                sim::human_equipment_motor_envelope(base, index);
            const float expected_travel = (index & 1u) == 0u
                ? sim::human_equipment_shoulder_travel_radians
                : sim::human_equipment_elbow_travel_radians;
            require(same_motor(human.motors[index], base)
                    && equipment.a == base.a
                    && equipment.pivot == base.pivot
                    && equipment.c == base.c
                    && equipment.enabled == base.enabled
                    && equipment.neutral_angle == base.neutral_angle
                    && std::isfinite(equipment.minimum_angle)
                    && std::isfinite(equipment.maximum_angle)
                    && std::isfinite(equipment.strength)
                    && std::abs(equipment.minimum_angle
                        - (base.neutral_angle - expected_travel)) <= 1.0e-6f
                    && std::abs(equipment.maximum_angle
                        - (base.neutral_angle + expected_travel)) <= 1.0e-6f
                    && equipment.strength >= base.strength
                    && equipment.strength >= 0.056f,
                "equipment-only Human arm envelope is nonfinite, asymmetric, or mutated authored rest");
        }
        require(same_motor(sim::human_equipment_motor_envelope(
                    human.motors[0], 0u), human.motors[0]),
            "equipment arm envelope changed a locomotion motor");
        sim::MotorConstraint disabled = human.motors[4];
        disabled.enabled = false;
        require(same_motor(sim::human_equipment_motor_envelope(disabled, 4u),
                    disabled),
            "equipment arm envelope revived a disabled motor");
        sim::MotorConstraint malformed = human.motors[4];
        malformed.neutral_angle = std::numeric_limits<float>::quiet_NaN();
        const sim::MotorConstraint rejected =
            sim::human_equipment_motor_envelope(malformed, 4u);
        require(std::isnan(rejected.neutral_angle)
                && rejected.minimum_angle == malformed.minimum_angle
                && rejected.maximum_angle == malformed.maximum_angle
                && rejected.strength == malformed.strength,
            "equipment arm envelope manufactured a pose from malformed calibration");
    }

    {
        default_human.set_equipment_directive(
            sim::EquipmentDirective::safe_carry_walk);
        std::array<float, sim::action_count> neutral{};
        const auto carry = rl::effective_policy_action(default_human, neutral,
            sim::CourseStage::uneven, 1.0f, sim::GuidanceMode::assisted);
        std::array<float, 2u> elbow_flexion{};
        for (std::size_t chain = 0; chain < 2u; ++chain)
        {
            const std::size_t shoulder_index = 4u + chain * 2u;
            const std::size_t elbow_index = shoulder_index + 1u;
            const sim::MotorConstraint shoulder =
                sim::human_equipment_motor_envelope(
                    default_human.blueprint().motors[shoulder_index],
                    shoulder_index);
            const sim::MotorConstraint elbow =
                sim::human_equipment_motor_envelope(
                    default_human.blueprint().motors[elbow_index],
                    elbow_index);
            const float shoulder_target = sim::motor_target_angle(
                shoulder, carry[shoulder_index]);
            const float elbow_target = sim::motor_target_angle(
                elbow, carry[elbow_index]);
            elbow_flexion[chain] = pi - std::abs(wrap_angle(elbow_target));
            require(std::isfinite(shoulder_target)
                    && std::isfinite(elbow_target)
                    && shoulder_target >= shoulder.minimum_angle
                    && shoulder_target <= shoulder.maximum_angle
                    && elbow_target >= elbow.minimum_angle
                    && elbow_target <= elbow.maximum_angle
                    && elbow_flexion[chain] >= 0.35f,
                "safe-carry teacher produced a disconnected, nonfinite, "
                "out-of-range, or nearly straight physical arm chain");
        }
        require(std::abs(elbow_flexion[0] - elbow_flexion[1]) >= 0.08f,
            "safe-carry two-hand pose collapsed into one collinear diagonal stick");

        default_human.set_equipment_directive(
            sim::EquipmentDirective::gun_stance);
        const auto stance = rl::effective_policy_action(default_human, neutral,
            sim::CourseStage::uneven, 1.0f, sim::GuidanceMode::assisted);
        float arm_difference{};
        for (std::size_t index = 4u; index < 8u; ++index)
            arm_difference += std::abs(stance[index] - carry[index]);
        require(arm_difference >= 0.30f
                && stance[sim::equipment_state_action]
                    > carry[sim::equipment_state_action],
            "safe carry and planted gun stance still request the same physical pose");
    }

    {
        sim::Environment right{ sim::CreatureBlueprint::humanoid(), 0x751070u };
        right.set_course(sim::CourseStage::shuttle, 0.30f);
        right.configure_equipment(sim::WeaponClass::carbine, 7.0f);
        right.set_equipment_directive(sim::EquipmentDirective::gun_stance);
        std::array<float, sim::action_count> neutral{};
        const auto right_action = rl::effective_policy_action(right, neutral,
            sim::CourseStage::shuttle, 1.0f, sim::GuidanceMode::assisted);

        sim::Environment left{ sim::CreatureBlueprint::humanoid(), 0x751070u };
        left.set_course(sim::CourseStage::shuttle, 0.30f);
        left.configure_equipment(sim::WeaponClass::carbine, 7.0f);
        sim::EnvironmentTestAccess::face_left(left);
        left.set_equipment_directive(sim::EquipmentDirective::gun_stance);
        const auto left_action = rl::effective_policy_action(left, neutral,
            sim::CourseStage::shuttle, 1.0f, sim::GuidanceMode::assisted);
        for (std::size_t index = 4u; index < 8u; ++index)
        {
            if (std::abs(right_action[index] - left_action[index]) > 1.0e-5f)
                std::cerr << "mirrored-action motor=" << index
                    << " right=" << right_action[index]
                    << " left=" << left_action[index] << '\n';
            require(std::abs(right_action[index] - left_action[index])
                    <= 1.0e-5f,
                "mirrored gun stance changed its local physical arm request");
        }
        require(std::abs(right_action[sim::equipment_aim_action]
                    - left_action[sim::equipment_aim_action]) <= 1.0e-5f
                && right.equipment_target().position.x
                    > right.equipment_mount_position().x
                && left.equipment_target().position.x
                    < left.equipment_mount_position().x,
            "mirrored equipment request or physical target kept the wrong facing");
    }

    {
        const PhysicalAimPose right = acquire_physical_aim_pose(0x751071u);
        const PhysicalAimPose right_repeated =
            acquire_physical_aim_pose(0x751071u);
        const sim::WeaponGeometry carbine = sim::weapon_geometry(
            sim::WeaponClass::carbine);
        for (const PhysicalAimPose* pose : { &right, &right_repeated })
        {
            require(pose->left_supported && pose->right_supported
                    && pose->plant_seconds >= 0.30f
                    && pose->settle_seconds >= 0.20f,
                "settled physical aim pose lost its required two-foot plant");
            require(std::abs(pose->foregrip_projection
                        - carbine.rear_to_foregrip) <= 0.10f
                    && pose->foregrip_axis_error <= 1.0e-5f
                    && pose->stock_seat_error <= 0.12f
                    && length(pose->muzzle - (pose->primary_hand
                        + pose->physical_axis * carbine.rear_to_muzzle))
                        <= 1.0e-5f,
                "second real hand did not form a separated foregrip on the physical barrel axis");
            const float rear_grip_drop = pose->primary_shoulder.y
                - pose->primary_hand.y;
            if (!(pose->rear_grip_shoulder_distance >= 0.12f
                    && pose->rear_grip_shoulder_distance <= 0.34f))
            {
                std::cerr << "rear-grip distance="
                    << pose->rear_grip_shoulder_distance
                    << " drop=" << rear_grip_drop
                    << " ready-frame=" << pose->ready_frame << '\n';
            }
            require(pose->rear_grip_shoulder_distance >= 0.12f
                    && pose->rear_grip_shoulder_distance <= 0.34f,
                "rear physical grip escaped its finite arm-chain envelope");
            for (std::size_t joint = 0u;
                joint < pose->joint_offsets.size(); ++joint)
            {
                const float travel = (joint & 1u) == 0u
                    ? sim::human_equipment_shoulder_travel_radians
                    : sim::human_equipment_elbow_travel_radians;
                require(std::isfinite(pose->joint_offsets[joint])
                        && pose->joint_offsets[joint] <= travel + 0.025f,
                    "physical ready pose escaped its finite equipment joint envelope");
            }
        }
        require(right.ready_frame == right_repeated.ready_frame
                && length(right.primary_hand - right_repeated.primary_hand)
                    <= 1.0e-6f
                && length(right.secondary_hand - right_repeated.secondary_hand)
                    <= 1.0e-6f
                && right.final_action == right_repeated.final_action,
            "same-seed physical ready pose is not deterministic");
        sim::Environment mirrored{
            sim::CreatureBlueprint::humanoid(), 0x751071u };
        mirrored.set_course(sim::CourseStage::shuttle, 0.30f);
        mirrored.configure_equipment(sim::WeaponClass::carbine, 7.0f);
        sim::EnvironmentTestAccess::face_left(mirrored);
        sim::EnvironmentTestAccess::physical_ready(mirrored);
        const sim::EquipmentGripFrame mirrored_frame =
            mirrored.equipment_grip_frame();
        require(mirrored.left_supported() && mirrored.right_supported()
                && mirrored_frame.valid
                && mirrored_frame.axis.x < -0.85f
                && right.physical_axis.x > 0.85f
                && std::abs(mirrored_frame.span
                    - carbine.rear_to_foregrip) <= 1.0e-5f
                && mirrored_frame.stock_seat_error <= 0.12f,
            "mirrored READY pose did not preserve the physical two-hand frame");

        const std::array<PhysicalAimPose, 3u> sweep{
            acquire_physical_aim_pose(0x751072u, -0.80f),
            acquire_physical_aim_pose(0x751072u, 0.0f),
            acquire_physical_aim_pose(0x751072u, 0.80f)
        };
        std::array<float, 3u> sweep_angles{};
        for (std::size_t index = 0u; index < sweep.size(); ++index)
        {
            sweep_angles[index] = std::atan2(
                sweep[index].physical_axis.y, sweep[index].physical_axis.x);
            require(std::isfinite(sweep_angles[index])
                    && std::abs(sweep[index].foregrip_projection
                        - carbine.rear_to_foregrip) <= 0.10f
                    && sweep[index].stock_seat_error <= 0.12f,
                "aim sweep escaped the real two-hand frame");
        }
        require(sweep_angles[0] + 0.04f < sweep_angles[1]
                && sweep_angles[1] + 0.04f < sweep_angles[2],
            "finite arm controller did not sweep the real grip axis across target heights");
    }

    sim::Environment low_ready_disarm{
        sim::CreatureBlueprint::humanoid(), 0x751000u };
    sim::EnvironmentTestAccess::equipment_tick(low_ready_disarm, 0.45f);
    require(low_ready_disarm.equipment_state()
            == sim::EquipmentState::low_ready,
        "explicit low-ready control was rejected");
    low_ready_disarm.disarm_equipment();
    require(low_ready_disarm.equipment_state()
            == sim::EquipmentState::disarmed,
        "player/external disarm was ignored from low ready");

    sim::Environment moving{ sim::CreatureBlueprint::humanoid(), 0x751001u };
    moving.configure_equipment(sim::WeaponClass::carbine, 7.0f);
    sim::EnvironmentTestAccess::physical_ready(moving, 0.90f);
    sim::EnvironmentTestAccess::equipment_tick(moving, 1.0f, 1.0f);
    require(moving.shots_fired() == 0u
            && moving.equipment_fire_block() == sim::EquipmentFireBlock::moving,
        "moving trigger request created a projectile");

    sim::Environment one_foot{ sim::CreatureBlueprint::humanoid(), 0x751002u };
    one_foot.configure_equipment(sim::WeaponClass::carbine, 7.0f);
    sim::EnvironmentTestAccess::physical_ready(one_foot, 0.0f, true, false);
    sim::EnvironmentTestAccess::equipment_tick(one_foot, 1.0f, 1.0f);
    require(one_foot.shots_fired() == 0u
            && one_foot.equipment_fire_block()
                == sim::EquipmentFireBlock::unplanted,
        "one supported foot was accepted as a planted gun stance");

    sim::Environment unsettled{ sim::CreatureBlueprint::humanoid(), 0x751002u };
    unsettled.configure_equipment(sim::WeaponClass::carbine, 7.0f);
    sim::EnvironmentTestAccess::physical_ready(unsettled);
    sim::EnvironmentTestAccess::unsettle(unsettled);
    require(unsettled.equipment_fire_block()
            == sim::EquipmentFireBlock::unsettled,
        "fresh READY state did not report its aim-settle interlock");
    sim::EnvironmentTestAccess::equipment_tick(unsettled, 1.0f, 1.0f);
    require(unsettled.shots_fired() == 0u,
        "unsettled trigger request created a projectile");

    sim::Environment occluded{ sim::CreatureBlueprint::humanoid(), 0x751003u };
    occluded.configure_equipment(sim::WeaponClass::carbine, 7.0f);
    sim::EnvironmentTestAccess::physical_ready(occluded);
    sim::EnvironmentTestAccess::occlude_target(occluded);
    require(!occluded.equipment_target_visible()
            && occluded.equipment_fire_block()
                == sim::EquipmentFireBlock::occluded,
        "physical obstacle did not occlude the ballistic path");
    sim::EnvironmentTestAccess::equipment_tick(occluded, 1.0f, 1.0f);
    require(occluded.shots_fired() == 0u
            && occluded.equipment_projectiles().empty(),
        "occluded trigger request scored or launched a shot");

    sim::Environment reload{ sim::CreatureBlueprint::humanoid(), 0x751004u };
    reload.configure_equipment(sim::WeaponClass::carbine, 7.0f);
    sim::EnvironmentTestAccess::physical_ready(reload);
    sim::EnvironmentTestAccess::empty_magazine(reload);
    sim::EnvironmentTestAccess::equipment_tick(reload, 0.82f, 1.0f);
    require(reload.equipment_state() == sim::EquipmentState::reloading
            && reload.shots_fired() == 0u
            && reload.equipment_reload_remaining() > 0.0f,
        "empty magazine fired or skipped the explicit reload state");
    for (int frame = 0; frame < 120; ++frame)
    {
        sim::EnvironmentTestAccess::support(reload, true, true);
        sim::EnvironmentTestAccess::equipment_tick(reload, 0.82f, 1.0f);
    }
    require(reload.equipment_state() != sim::EquipmentState::reloading
            && reload.equipment_rounds_remaining()
                == reload.equipment_magazine_capacity(),
        "finite reload did not restore exactly one magazine");

    sim::Environment finite_flight{
        sim::CreatureBlueprint::humanoid(), 0x751005u };
    finite_flight.configure_equipment(sim::WeaponClass::carbine, 7.0f);
    sim::EnvironmentTestAccess::deactivate_target_and_launch(finite_flight);
    for (int frame = 0; frame < 4; ++frame)
        sim::EnvironmentTestAccess::equipment_tick(finite_flight, 0.0f);
    require(finite_flight.equipment_projectiles().empty(),
        "projectile survived beyond its finite flight lifetime");

    float repeated_correction{};
    for (const std::uint64_t seed : { 0x751006u, 0x751006u })
    {
        sim::Environment miss{ sim::CreatureBlueprint::humanoid(), seed };
        miss.configure_equipment(sim::WeaponClass::carbine, 7.0f);
        const float before = miss.equipment_recommended_aim_angle();
        const integration::RuntimeSnapshot previous = integration::snapshot(
            miss, 3u, 10u);
        sim::EnvironmentTestAccess::launch_crossing_miss(miss, 0.90f);
        for (int frame = 0; frame < 4 && miss.shots_missed() == 0u; ++frame)
            sim::EnvironmentTestAccess::equipment_tick(miss, 0.0f);
        const integration::RuntimeSnapshot current = integration::snapshot(
            miss, 3u, 11u);
        const float correction = wrap_angle(
            miss.equipment_recommended_aim_angle() - before);
        require(miss.shots_missed() == 1u
                && miss.last_shot_miss_error() > 0.0f
                && correction < 0.0f
                && std::abs(correction) <= 0.22f,
            "observed high miss did not create a bounded lower next-shot aim");
        bool diagnosed{};
        integration::FixedStepResult step{};
        step.snapshot = current;
        const integration::DiagnosticEvents events =
            integration::diagnostic_events(&previous, step);
        for (const integration::DiagnosticEvent& event : events.span())
            diagnosed = diagnosed
                || event.kind == integration::DiagnosticKind::target_missed;
        require(current.shots_missed == 1u && diagnosed,
            "external host hid an observed physical miss");
        if (repeated_correction != 0.0f)
            require(correction == repeated_correction,
                "same seed and impact produced different aim correction");
        repeated_correction = correction;
    }

    runner::director::PhysicalTrialEvidence repeated_physical_proof{};
    for (const std::uint64_t seed : { 0x751060u, 0x751060u })
    {
        sim::Environment corrected{ sim::CreatureBlueprint::humanoid(), seed };
        corrected.configure_equipment(sim::WeaponClass::carbine, 7.0f);
        sim::EnvironmentTestAccess::physical_ready(
            corrected, 0.0f, true, true, 0.080f);
        const float initial_recommendation =
            corrected.equipment_recommended_aim_angle();
        const float first_physical_error = std::abs(wrap_angle(
            corrected.equipment_aim_angle() - initial_recommendation));
        require(corrected.left_supported() && corrected.right_supported()
                && corrected.equipment_plant_seconds() >= 1.0f
                && corrected.equipment_aim_settle_seconds() >= 1.0f
                && first_physical_error > 0.070f
                && corrected.equipment_fire_block()
                    == sim::EquipmentFireBlock::none,
            "physical miss fixture was not two-foot planted and jointly aimed");

        std::uint64_t tick = 20u;
        sim::EnvironmentTestAccess::equipment_tick(corrected, 1.0f, 1.0f);
        require(corrected.shots_fired() == 1u
                && !corrected.equipment_projectiles().empty(),
            "first physical trigger did not launch a real ballistic round");
        while (corrected.shots_missed() == 0u && tick < 180u)
        {
            ++tick;
            sim::EnvironmentTestAccess::equipment_tick(corrected, 1.0f);
        }
        const std::uint64_t miss_tick = tick;
        const float miss_error = corrected.last_shot_miss_error();
        const float corrected_recommendation =
            corrected.equipment_recommended_aim_angle();
        const float correction = wrap_angle(
            corrected_recommendation - initial_recommendation);
        require(corrected.shots_missed() == 1u
                && corrected.target_hits() == 0u
                && std::abs(miss_error) > 0.01f
                && correction * miss_error < 0.0f
                && std::abs(correction) > 1.0e-5f
                && std::abs(correction)
                    <= director::maximum_bounded_aim_correction,
            "real first-shot miss did not yield an observed bounded correction");

        sim::EnvironmentTestAccess::physical_ready(corrected);
        const float second_physical_error = std::abs(wrap_angle(
            corrected.equipment_aim_angle()
                - corrected.equipment_recommended_aim_angle()));
        require(second_physical_error < first_physical_error
                && corrected.equipment_fire_block()
                    == sim::EquipmentFireBlock::none,
            "physical arm chain did not apply the improved corrected aim");
        ++tick;
        sim::EnvironmentTestAccess::equipment_tick(corrected, 1.0f, 1.0f);
        require(corrected.shots_fired() == 2u,
            "corrected physical trigger did not launch a second real round");
        while (corrected.target_hits() == 0u
            && corrected.shots_missed() == 1u && tick < 360u)
        {
            ++tick;
            sim::EnvironmentTestAccess::equipment_tick(corrected, 1.0f);
        }
        require(corrected.target_hits() == 1u
                && corrected.shots_missed() == 1u
                && second_physical_error < std::abs(miss_error),
            "bounded correction did not improve the subsequent physical shot");

        const director::TaskGraph graph = director::default_training_graph();
        const director::TaskNode& plant = graph.nodes[12];
        director::PhysicalTrialEvidence proof{};
        proof.schema = director::physical_trial_evidence_schema_version;
        proof.kind = director::PhysicalEvidenceKind::equipment_correction;
        proof.trial_id = seed;
        proof.stable_task_id = plant.stable_id;
        proof.stable_challenge_id = plant.stable_challenge_id;
        proof.first_tick = 1u;
        proof.last_tick = tick;
        proof.equipment = { 2u, 10u, miss_tick, miss_tick + 1u, tick,
            1.0f, 1.0f, miss_error, correction, second_physical_error,
            corrected.shots_fired(), corrected.shots_missed(),
            corrected.target_hits(), true };
        require(director::physical_trial_evidence_valid(plant, proof),
            "observed plant/aim/miss/correction/shot sequence did not form valid Director proof");
        director::Evidence ready_for_plant{};
        ready_for_plant.mastery[0] = graph.nodes[0].minimum_evidence;
        ready_for_plant.mastery[1] = graph.nodes[1].minimum_evidence;
        ready_for_plant.mastery[10] = graph.nodes[10].minimum_evidence;
        ready_for_plant.mastery[11] = graph.nodes[11].minimum_evidence;
        const director::TrialEvidence trial{ plant.stable_id, 12u,
            director::TrialOutcome::completed, plant.minimum_evidence, proof };
        const director::EvidenceReduction mastered = director::reduce_evidence(
            graph, ready_for_plant, trial,
            director::Profile::autonomous_npc);
        require(mastered.accepted
                && director::task_complete(graph, mastered.evidence, 12u),
            "actual physical correction sequence did not grant bound Director mastery");
        if (repeated_physical_proof.trial_id != 0u)
            require(proof == repeated_physical_proof,
                "same seed produced different physical correction proof");
        repeated_physical_proof = proof;
    }

    for (const std::uint64_t seed : { 0x751100u, 0x751101u })
    {
        sim::Environment npc{ sim::CreatureBlueprint::humanoid(), seed };
        npc.set_course(sim::CourseStage::equipment_targets, 0.30f);
        npc.configure_equipment(sim::WeaponClass::carbine, 7.0f);
        npc.set_equipment_directive(sim::EquipmentDirective::fire_and_correct);
        const Vec2 fixed_target = npc.equipment_target().position;
        bool saw_low_ready{};
        bool saw_gun_stance{};
        bool saw_aiming{};
        bool saw_ready{};
        bool saw_projectile{};
        std::array<int, 3u> shot_frames{};
        std::size_t shot_frame_count{};
        std::uint32_t prior_shots{};
        int first_planted_frame{ -1 };
        int first_aiming_frame{ -1 };
        int minimum_aim_error_frame{ -1 };
        float minimum_aim_error{ std::numeric_limits<float>::max() };
        float maximum_aim_settle_seconds{};
        float minimum_error_actual_aim{};
        float minimum_error_desired_aim{};
        std::array<sim::MotorDiagnostic, 4u> minimum_error_arm{};
        std::array<float, sim::action_count> no_policy{};
        for (int frame = 0; frame < 900 && npc.valid_motion(); ++frame)
        {
            const auto action = rl::effective_policy_action(npc, no_policy,
                sim::CourseStage::equipment_targets, 1.0f,
                sim::GuidanceMode::assisted);
            static_cast<void>(npc.step(action, 1.0f / 60.0f));
            saw_low_ready = saw_low_ready
                || npc.equipment_state() == sim::EquipmentState::low_ready;
            saw_gun_stance = saw_gun_stance
                || npc.equipment_state() == sim::EquipmentState::gun_stance;
            saw_aiming = saw_aiming
                || npc.equipment_state() == sim::EquipmentState::aiming;
            saw_ready = saw_ready
                || npc.equipment_state() == sim::EquipmentState::ready;
            saw_projectile = saw_projectile
                || !npc.equipment_projectiles().empty();
            if (first_planted_frame < 0 && npc.equipment_stopped())
                first_planted_frame = frame;
            if (first_aiming_frame < 0
                && npc.equipment_state() == sim::EquipmentState::aiming)
                first_aiming_frame = frame;
            const float physical_aim_error = std::abs(wrap_angle(
                npc.equipment_recommended_aim_angle()
                    - npc.equipment_aim_angle()));
            maximum_aim_settle_seconds = std::max(maximum_aim_settle_seconds,
                npc.equipment_aim_settle_seconds());
            if (npc.equipment_state() == sim::EquipmentState::aiming
                && physical_aim_error < minimum_aim_error)
            {
                minimum_aim_error = physical_aim_error;
                minimum_aim_error_frame = frame;
                minimum_error_actual_aim = npc.equipment_aim_angle();
                minimum_error_desired_aim =
                    npc.equipment_recommended_aim_angle();
                for (std::size_t motor = 0u; motor < minimum_error_arm.size();
                    ++motor)
                    minimum_error_arm[motor] = npc.motor_diagnostic(motor + 4u);
            }
            if (npc.shots_fired() > prior_shots)
            {
                if (shot_frame_count < shot_frames.size())
                    shot_frames[shot_frame_count++] = frame;
                prior_shots = npc.shots_fired();
            }
            if (npc.equipment_target().active)
                require(length(npc.equipment_target().position - fixed_target)
                        <= 1.0e-6f,
                    "range target teleported between physical shots");
            if (npc.target_hits() >= npc.equipment_hit_goal()
                && npc.equipment_state() == sim::EquipmentState::safe_carry)
                break;
        }
        if (!(npc.valid_motion() && saw_low_ready && saw_gun_stance
                && saw_aiming && saw_ready && saw_projectile))
        {
            std::cerr << "equipment-sequence seed=" << seed
                << " state=" << sim::equipment_state_name(npc.equipment_state())
                << " valid=" << npc.valid_motion()
                << " low=" << saw_low_ready << " stance=" << saw_gun_stance
                << " aiming=" << saw_aiming << " ready=" << saw_ready
                << " projectile=" << saw_projectile
                << " stopped=" << npc.equipment_stopped()
                << " plant=" << npc.equipment_plant_seconds()
                << " aim-rate=" << npc.equipment_aim_rate()
                << " actual=" << npc.equipment_aim_angle()
                << " desired=" << npc.equipment_recommended_aim_angle()
                << " error=" << std::abs(wrap_angle(
                    npc.equipment_recommended_aim_angle()
                        - npc.equipment_aim_angle()))
                << " settle=" << npc.equipment_aim_settle_seconds()
                << " block=" << sim::equipment_fire_block_name(
                    npc.equipment_fire_block())
                << " shots=" << npc.shots_fired()
                << " hits=" << npc.target_hits()
                << " misses=" << npc.shots_missed() << '\n';
            std::cerr << "  first-plant-frame=" << first_planted_frame
                << " first-aiming-frame=" << first_aiming_frame
                << " min-error-frame=" << minimum_aim_error_frame
                << " min-error=" << minimum_aim_error
                << " min-actual=" << minimum_error_actual_aim
                << " min-desired=" << minimum_error_desired_aim
                << " max-settle=" << maximum_aim_settle_seconds << '\n';
            for (std::size_t motor = 4u; motor < 8u; ++motor)
            {
                const sim::MotorDiagnostic diagnostic =
                    npc.motor_diagnostic(motor);
                std::cerr << "  motor=" << motor
                    << " neutral=" << diagnostic.authored_neutral
                    << " bounds=[" << diagnostic.minimum << ','
                    << diagnostic.maximum << "] current="
                    << diagnostic.current_angle << " action="
                    << diagnostic.applied_action << " target="
                    << diagnostic.target_angle << '\n';
            }
            for (std::size_t motor = 0u; motor < minimum_error_arm.size(); ++motor)
            {
                const sim::MotorDiagnostic& diagnostic =
                    minimum_error_arm[motor];
                std::cerr << "  min-error-motor=" << motor + 4u
                    << " current=" << diagnostic.current_angle
                    << " action=" << diagnostic.applied_action
                    << " target=" << diagnostic.target_angle << '\n';
            }
        }
        require(npc.valid_motion() && saw_low_ready && saw_gun_stance
                && saw_aiming && saw_ready && saw_projectile,
            "NPC skipped a physical stop/plant/stance/aim/ready/flight state");
        if (!(npc.target_hits() == npc.equipment_hit_goal()
                && npc.shots_fired() >= npc.target_hits()
                && npc.equipment_state() == sim::EquipmentState::safe_carry))
        {
            std::cerr << "equipment-final seed=" << seed
                << " state=" << sim::equipment_state_name(npc.equipment_state())
                << " valid=" << npc.valid_motion()
                << " shots=" << npc.shots_fired()
                << " hits=" << npc.target_hits()
                << " misses=" << npc.shots_missed()
                << " stopped=" << npc.equipment_stopped()
                << " plant=" << npc.equipment_plant_seconds()
                << " settle=" << npc.equipment_aim_settle_seconds()
                << " aim-rate=" << npc.equipment_aim_rate()
                << " aim-error=" << std::abs(wrap_angle(
                    npc.equipment_recommended_aim_angle()
                        - npc.equipment_aim_angle()))
                << " block=" << sim::equipment_fire_block_name(
                    npc.equipment_fire_block()) << '\n';
            for (std::size_t motor = 4u; motor < 8u; ++motor)
            {
                const sim::MotorDiagnostic diagnostic =
                    npc.motor_diagnostic(motor);
                std::cerr << "  equipment-final-motor=" << motor
                    << " current=" << diagnostic.current_angle
                    << " action=" << diagnostic.applied_action
                    << " target(base-envelope)="
                    << diagnostic.target_angle << '\n';
            }
        }
        require(npc.target_hits() == npc.equipment_hit_goal()
                && npc.shots_fired() >= npc.target_hits()
                && npc.equipment_state() == sim::EquipmentState::safe_carry,
            "NPC failed to hit with real rounds and recover to safe carry");
        require(shot_frame_count == npc.equipment_hit_goal(),
            "finite goal was not served by one real round per target hit");
        const int minimum_shot_gap = static_cast<int>(std::ceil(
            sim::weapon_profile(sim::WeaponClass::carbine).cooldown_seconds
                * 60.0f));
        for (std::size_t shot = 1u; shot < shot_frame_count; ++shot)
            require(shot_frames[shot] - shot_frames[shot - 1u]
                    >= minimum_shot_gap,
                "recoil/cooldown interlock allowed an early follow-up shot");

        const integration::RuntimeSnapshot snapshot = integration::snapshot(
            npc, 1u, 900u);
        require(snapshot.weapon == sim::WeaponClass::carbine
                && snapshot.rounds_remaining == npc.equipment_rounds_remaining()
                && snapshot.magazine_capacity == npc.equipment_magazine_capacity()
                && snapshot.fire_block
                    == sim::EquipmentFireBlock::goal_complete
                && !snapshot.target_visible,
            "external-host snapshot hid physical equipment authority");
    }

    std::cout << "Epoch2DWalkEngine v0.7.51 Runner-example physical equipment contracts passed\n";
    return EXIT_SUCCESS;
}
