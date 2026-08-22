#include "ppo.hpp"
#include "autonomy.hpp"
#include "simulation.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace runner::sim {
struct EnvironmentTestAccess {
    static void elapsed(Environment& environment, float value) noexcept {
        environment.elapsed_seconds_ = value;
    }
    static void prepare_target(Environment& environment, float distance,
        float aim_offset = 0.0f, std::uint32_t hits = 0u) noexcept {
        environment.equipment_state_ = EquipmentState::ready;
        environment.equipment_cooldown_seconds_ = 0.0f;
        environment.target_hits_ = hits;
        environment.equipment_aim_angle_ = 0.0f;
        const Vec2 mount = environment.equipment_mount_position();
        environment.equipment_target_.position = mount + Vec2{
            std::cos(aim_offset) * distance, std::sin(aim_offset) * distance };
        environment.equipment_target_.radius = 0.48f;
        environment.equipment_target_.active = true;
    }
    static bool target_active(const Environment& environment) noexcept {
        return environment.equipment_target_.active;
    }
    static void deposit_sand(Environment& environment, float world_x,
        float amount) noexcept {
        environment.terrain_.deposit(
            terrain_sample_x(world_x, environment.course_progress()),
            amount, 0.18f, sandhybrid::Material::sand);
    }
    static std::uint64_t layout_seed(const Environment& environment) noexcept {
        return environment.course_layout_seed_;
    }
};
}

namespace {
namespace sim = runner::sim;
namespace rl = runner::rl;

void require(bool condition, std::string_view message) {
    if (condition) return;
    std::cerr << "Runner v0.7.36 authored-runtime failure: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

bool finite_action(const std::array<float, sim::action_count>& action) {
    for (const float value : action)
        if (!std::isfinite(value) || std::abs(value) > 1.00001f)
            return false;
    return true;
}

}

int main() {
    const sim::CreatureBlueprint humanoid = sim::CreatureBlueprint::humanoid();
    const auto segment_length = [&](std::size_t a, std::size_t b)
    {
        return runner::length(humanoid.nodes[b] - humanoid.nodes[a]);
    };
    require(humanoid.nodes.size() == 13u
            && humanoid.species_identity == sim::CreatureSpecies::human
            && std::abs(humanoid.nodes[0].x + 0.0572309196f) < 1.0e-5f
            && std::abs(humanoid.nodes[0].y - 2.59142852f) < 1.0e-5f
            && std::abs(humanoid.nodes[1].x + 0.171161979f) < 1.0e-5f
            && std::abs(humanoid.nodes[1].y - 3.80571461f) < 1.0e-5f
            && std::abs(humanoid.nodes[2].x + 0.177114367f) < 1.0e-5f
            && std::abs(humanoid.nodes[2].y - 4.18666649f) < 1.0e-5f
            && std::abs(segment_length(0, 3) - segment_length(0, 5)) < 1.0e-4f
            && std::abs(segment_length(3, 4) - segment_length(5, 6)) < 1.0e-4f
            && std::abs(segment_length(7, 8) - segment_length(10, 11)) < 1.0e-4f
            && std::abs(segment_length(8, 9) - segment_length(11, 12)) < 1.0e-4f
            && humanoid.nodes[9].y < humanoid.nodes[7].y
            && humanoid.nodes[12].y < humanoid.nodes[10].y,
        "saved Human rest pose and paired anatomy were not promoted exactly");

    const runner::Vec2 rest_hand{ 0.18f, -0.82f };
    const runner::Vec2 rest = rl::authored_opposed_swing_target(
        rest_hand, 0.0f, 0.12f, 0.0f, 1.0f);
    const runner::Vec2 lead = rl::authored_opposed_swing_target(
        rest_hand, runner::pi * 0.5f, 0.12f, 0.0f, 1.0f);
    const runner::Vec2 trail = rl::authored_opposed_swing_target(
        rest_hand, runner::pi * 1.5f, 0.12f, 0.0f, 1.0f);
    require(rest.x == rest_hand.x && rest.y == rest_hand.y
            && lead.x > rest_hand.x && trail.x < rest_hand.x
            && lead.y == rest_hand.y && trail.y == rest_hand.y,
        "arm cycle no longer rests at and swings around the authored endpoint");

    sim::Environment humanoid_walk{ humanoid, 0x7360u };
    humanoid_walk.set_course(sim::CourseStage::uneven, 0.30f);
    sim::EnvironmentTestAccess::elapsed(humanoid_walk, 0.0f);
    const auto rest_action = rl::walking_teacher_action(humanoid_walk);
    require(finite_action(rest_action)
            && std::abs(rest_action[4]) < 0.08f
            && std::abs(rest_action[5]) < 0.08f
            && std::abs(rest_action[6]) < 0.08f
            && std::abs(rest_action[7]) < 0.08f,
        "authored arms are driven away from the body at gait startup");
    const sim::MotorDiagnostic motor = humanoid_walk.motor_diagnostic(0u);
    const sim::MotorDiagnostic absent_motor =
        humanoid_walk.motor_diagnostic(sim::action_count);
    require(motor.available && motor.action_slot == 0u
            && std::isfinite(motor.current_angle)
            && std::isfinite(motor.authored_neutral)
            && std::isfinite(motor.target_angle)
            && motor.minimum <= motor.maximum
            && !absent_motor.available,
        "selected-motor diagnostic did not expose bounded authored/runtime state");

    {
        rl::AutonomousTrainer live_authoring{ humanoid, 2u };
        live_authoring.set_background_enabled(false);
        const std::uint64_t original_signature = live_authoring.rig_signature();
        sim::CreatureBlueprint edited = humanoid;
        edited.radii[0] += 0.01f;
        require(live_authoring.preview_blueprint(edited)
                && live_authoring.live_morphology_preview_active()
                && live_authoring.rig_signature() == edited.signature()
                && live_authoring.rig_signature() != original_signature,
            "valid morphology edit did not update the active rig immediately");
        require(live_authoring.preview_blueprint(edited)
                && live_authoring.rig_signature() == edited.signature(),
            "repeated live morphology preview was not deterministic");
        sim::CreatureBlueprint invalid = edited;
        invalid.bones.clear();
        require(!live_authoring.preview_blueprint(invalid)
                && live_authoring.rig_signature() == edited.signature(),
            "invalid live morphology edit corrupted the active preview");
        live_authoring.cancel_blueprint_preview();
        require(!live_authoring.live_morphology_preview_active()
                && live_authoring.rig_signature() == original_signature,
            "live morphology cancellation did not restore the published rig");
    }

    sim::EnvironmentTestAccess::elapsed(humanoid_walk,
        1.0f / (4.0f * sim::foundational_gait_cadence_hz));
    const auto swing_action = rl::walking_teacher_action(humanoid_walk);
    const auto pair_motion = [&](std::size_t first, std::size_t second) {
        return std::abs(swing_action[first] - rest_action[first])
            + std::abs(swing_action[second] - rest_action[second]);
    };
    const auto pair_separation = [&](std::size_t first, std::size_t second) {
        return std::abs(swing_action[first] - swing_action[second]);
    };
    require(finite_action(swing_action)
            && pair_motion(0, 1) > 0.01f
            && pair_motion(2, 3) > 0.01f
            && pair_motion(4, 5) > 0.01f
            && pair_motion(6, 7) > 0.01f
            && (pair_separation(0, 2) + pair_separation(1, 3)) > 0.03f
            && (pair_separation(4, 6) + pair_separation(5, 7)) > 0.03f,
        "paired legs and arms do not produce opposed sagittal swing");

    constexpr std::array factories{
        &sim::CreatureBlueprint::chicken, &sim::CreatureBlueprint::biped,
        &sim::CreatureBlueprint::humanoid, &sim::CreatureBlueprint::quadruped,
        &sim::CreatureBlueprint::crawler4, &sim::CreatureBlueprint::hexapod,
        &sim::CreatureBlueprint::monoped };
    for (std::size_t index = 0; index < factories.size(); ++index) {
        sim::Environment environment{ factories[index](), 0x736100u + index };
        environment.set_course(sim::CourseStage::uneven, 0.30f);
        require(finite_action(rl::walking_teacher_action(environment)),
            "topology-driven gait produced an invalid canonical-rig action");
    }

    sim::Environment walk{ humanoid, 0x7362u };
    walk.set_course(sim::CourseStage::uneven, 0.30f);
    sim::Environment shuttle{ humanoid, 0x7362u };
    shuttle.set_course(sim::CourseStage::shuttle, 0.30f);
    require(!walk.shuttle_enabled() && shuttle.shuttle_enabled()
            && sim::next_course_stage(sim::CourseStage::uneven)
                == sim::CourseStage::shuttle
            && sim::next_course_stage(sim::CourseStage::shuttle)
                == sim::CourseStage::crouch_walk
            && !sim::stage_uses_deformable_terrain(sim::CourseStage::shuttle),
        "shuttle is not isolated as a stable-ground curriculum lesson");

    sim::Environment terrain{ humanoid, 0x7363u };
    terrain.set_course(sim::CourseStage::uneven, 0.42f);
    sim::EnvironmentTestAccess::deposit_sand(terrain, 12.0f, 0.22f);
    const std::uint64_t retained_seed = sim::EnvironmentTestAccess::layout_seed(terrain);
    const float retained_height = terrain.ground_height_at(12.0f);
    const auto retained_material = terrain.terrain_surface_material_at(12.0f);
    terrain.reset(0xDEADBEEFu);
    require(sim::EnvironmentTestAccess::layout_seed(terrain) == retained_seed
            && std::abs(terrain.ground_height_at(12.0f) - retained_height) < 1.0e-6f
            && terrain.terrain_surface_material_at(12.0f) == retained_material
            && retained_material == sandhybrid::Material::sand,
        "ordinary retry regenerated or relabeled the active sand layout");
    terrain.reset(0xBAD5EEDu);
    require(sim::EnvironmentTestAccess::layout_seed(terrain) == retained_seed
            && std::abs(terrain.ground_height_at(12.0f) - retained_height) < 1.0e-6f,
        "repeated seeded retry changed persistent terrain state");
    terrain.set_course(sim::CourseStage::balance, 0.30f);
    terrain.set_course(sim::CourseStage::uneven, 0.42f);
    require(sim::EnvironmentTestAccess::layout_seed(terrain) != retained_seed,
        "explicit lesson change did not create a fresh terrain lifecycle");

    constexpr std::array weapons{ sim::WeaponClass::sidearm,
        sim::WeaponClass::carbine, sim::WeaponClass::launcher };
    for (const sim::WeaponClass weapon : weapons) {
        const sim::WeaponProfile profile = sim::weapon_profile(weapon);
        sim::Environment equipment{ humanoid, 0x7364u
            + static_cast<std::uint8_t>(weapon) };
        equipment.set_course(sim::CourseStage::balance, 0.30f);
        equipment.configure_equipment(weapon,
            (profile.minimum_engagement_distance
                + profile.maximum_engagement_distance) * 0.5f);
        sim::EnvironmentTestAccess::prepare_target(equipment,
            profile.minimum_engagement_distance - 0.01f);
        require(!equipment.equipment_engagement_ready(),
            "weapon fired inside its minimum engagement distance");
        sim::EnvironmentTestAccess::prepare_target(equipment,
            profile.minimum_engagement_distance);
        require(equipment.equipment_engagement_ready(),
            "weapon rejected its minimum engagement boundary");
        sim::EnvironmentTestAccess::prepare_target(equipment,
            profile.maximum_engagement_distance);
        require(equipment.equipment_engagement_ready(),
            "weapon rejected its maximum engagement boundary");
        sim::EnvironmentTestAccess::prepare_target(equipment,
            profile.maximum_engagement_distance + 0.01f);
        require(!equipment.equipment_engagement_ready(),
            "weapon fired beyond its maximum engagement distance");
        sim::EnvironmentTestAccess::prepare_target(equipment,
            (profile.minimum_engagement_distance
                + profile.maximum_engagement_distance) * 0.5f,
            profile.aim_tolerance + 0.01f);
        require(!equipment.equipment_engagement_ready(),
            "weapon fired outside its authored aim tolerance");
        sim::EnvironmentTestAccess::prepare_target(equipment,
            (profile.minimum_engagement_distance
                + profile.maximum_engagement_distance) * 0.5f, 0.0f,
            equipment.equipment_hit_goal());
        require(!equipment.equipment_engagement_ready(),
            "weapon continued firing after its current hit goal");
    }

    sim::Environment bounded_fire{ humanoid, 0x7368u };
    bounded_fire.set_course(sim::CourseStage::balance, 0.30f);
    bounded_fire.configure_equipment(sim::WeaponClass::sidearm, 4.0f);
    sim::EnvironmentTestAccess::prepare_target(bounded_fire, 4.0f);
    std::array<float, sim::action_count> trigger{};
    trigger[sim::equipment_state_action] = 1.0f;
    trigger[sim::equipment_trigger_action] = 1.0f;
    for (int step = 0; step < 120; ++step)
        static_cast<void>(bounded_fire.step(trigger, 1.0f / 60.0f));
    const std::uint32_t shots_at_goal = bounded_fire.shots_fired();
    require(bounded_fire.target_hits() == bounded_fire.equipment_hit_goal()
            && !sim::EnvironmentTestAccess::target_active(bounded_fire)
            && shots_at_goal >= 1u && shots_at_goal <= 2u,
        "range-gated sidearm did not stop on its hit goal");
    for (int step = 0; step < 120; ++step)
        static_cast<void>(bounded_fire.step(trigger, 1.0f / 60.0f));
    require(bounded_fire.shots_fired() == shots_at_goal,
        "held trigger resumed nonstop firing after goal completion");

    std::cout << "Runner v0.7.36 authored gait/runtime tests passed\n";
    return EXIT_SUCCESS;
}