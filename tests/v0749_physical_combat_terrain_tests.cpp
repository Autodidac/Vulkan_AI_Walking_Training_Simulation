#include "autonomy.hpp"
#include "pixel_art.hpp"
#include "ppo.hpp"
#include "runtime_diagnostics.hpp"
#include "simulation.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static void force_physical_ready(Environment& environment,
            bool stopped) noexcept
        {
            environment.equipment_state_ = EquipmentState::ready;
            environment.equipment_stopped_ = stopped;
            environment.equipment_aim_rate_ = 0.0f;
            environment.equipment_aim_settle_seconds_ = 1.0f;
            environment.equipment_cooldown_seconds_ = 0.0f;
            environment.equipment_aim_angle_ =
                environment.equipment_recommended_aim_angle();
        }

        static void begin_moving(Environment& environment) noexcept
        {
            environment.forward_speed_ = 0.85f;
        }

        static void record_ground_miss(Environment& environment) noexcept
        {
            const Vec2 mount = environment.equipment_mount_position();
            environment.equipment_target_.position = mount + Vec2{ 4.0f, 1.0f };
            environment.equipment_target_.radius = 0.20f;
            environment.equipment_target_.active = true;
            environment.equipment_projectiles_.push_back({
                environment.weapon_class_, { mount.x + 0.5f, -0.10f },
                {}, 0.05f, 1u, true, false });
            std::array<float, action_count> neutral{};
            environment.update_equipment(neutral, 1.0f / 60.0f);
        }
    };
}

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "Runner v0.7.49 physical combat/terrain failure: "
                << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    using namespace runner;
    for (const std::string_view error : {
            "VK_KHR_surface is unavailable",
            "VK_KHR_win32_surface is unavailable",
            "Installed Vulkan doesn't implement VK_KHR_xcb_surface",
            "Installed Vulkan doesn't implement VK_KHR_xlib_surface",
            "No available video device",
            "No dynamic Vulkan support" })
        require(runtime::is_headless_surface_error(error),
            "a supported headless Vulkan surface diagnostic was rejected");
    require(!runtime::is_headless_surface_error("vulkan-1 loader is missing")
            && !runtime::is_headless_surface_error("shader compilation failed"),
        "a real Vulkan/package fault was misclassified as headless");

    static_assert(sizeof(sandhybrid::SceneCell) == 16u);
    static_assert(sim::DeformableTerrain::cell_count == 640u);
    static_assert(sim::DeformableTerrain::vertical_cell_count == 360u);
    static_assert(sim::DeformableTerrain::macro_cell_side == 8u);
    require(sim::DeformableTerrain::training_world_dimensions.width == 5120u
            && sim::DeformableTerrain::training_world_dimensions.height == 1440u,
        "training cells are not embedded in the exact compact SandHybrid world");
    require(sim::DeformableTerrain::training_district_origin_x == 0u
            && sim::DeformableTerrain::training_world_surface_y == 1040u
            && sim::DeformableTerrain::global_cell_x(0.0f) == 0u
            && sim::DeformableTerrain::global_cell_y(0.0f) == 1040u,
        "Sandbox district coordinates do not match the pinned world layout");

    sim::DeformableTerrain terrain{};
    terrain.reset(0x749001u, 0.55f);
    const sim::DeformableTerrain::FineCell& canonical = terrain.fine_cell(0u, 0u);
    require(canonical.canonical_cell().material
            == static_cast<std::uint32_t>(canonical.material())
            && canonical.authored(),
        "terrain presentation is not backed by a canonical authored cell");

    sim::Environment static_world{ sim::CreatureBlueprint::humanoid(), 0x749002u };
    static_world.set_course(sim::CourseStage::uneven, 0.30f);
    static_world.set_course_motion_enabled(true);
    require(!static_world.course_motion_enabled()
            && static_world.course_speed() == 0.0f
            && static_world.course_progress() == 0.0f
            && sim::terrain_relative_frame_progress(2.0f, 2.0f, 8.0f, 1.0f) == 0.0f,
        "a treadmill/conveyor path can still manufacture locomotion progress");

    sim::Environment speed_walk{ sim::CreatureBlueprint::humanoid(), 0x749020u };
    speed_walk.set_course(sim::CourseStage::uneven, 0.30f);
    speed_walk.set_gait_task(sim::GaitTask::speed_walk);
    const std::array<float, sim::action_count> no_policy{};
    bool speed_walk_valid = true;
    for (int frame = 0; frame < 1200; ++frame)
    {
        const auto physical_teacher = rl::effective_policy_action(
            speed_walk, no_policy, sim::CourseStage::uneven);
        const sim::StepResult result = speed_walk.step(
            physical_teacher, 1.0f / 60.0f);
        if (result.terminated)
        {
            speed_walk_valid = false;
            break;
        }
    }
    const float speed_walk_mean_speed = speed_walk.distance_travelled() / 20.0f;
    if (!speed_walk_valid || speed_walk.distance_travelled()
            < rl::speed_walk_qualification_distance
        || static_cast<float>(speed_walk.gait_cycles())
            < rl::speed_walk_mastery_stride_events
        || speed_walk_mean_speed < rl::gait_task_mastery_speed(
            sim::GaitTask::speed_walk))
        std::cerr << "speedwalk evidence distance=" << speed_walk.distance_travelled()
            << " steps=" << speed_walk.gait_cycles()
            << " mean_speed=" << speed_walk_mean_speed
            << " collisions=" << speed_walk.collision_count() << '\n';
    require(speed_walk_valid && speed_walk.distance_travelled()
            >= rl::speed_walk_qualification_distance
            && static_cast<float>(speed_walk.gait_cycles())
                >= rl::speed_walk_mastery_stride_events
            && speed_walk_mean_speed >= rl::gait_task_mastery_speed(
                sim::GaitTask::speed_walk),
        "physical Speed Walk teacher cannot reach its own distance/step gate");
    const sim::CreatureBlueprint human = sim::CreatureBlueprint::humanoid();
    const std::uint64_t handoff =
        rl::foundational_walk_teacher_handoff_update(human);
    require(rl::foundational_walk_raw_rollout_count(handoff - 1u,
                sim::CourseStage::uneven, human, 8u) == 0u
            && rl::foundational_walk_raw_rollout_count(handoff,
                sim::CourseStage::uneven, human, 8u) == 1u
            && rl::foundational_walk_raw_rollout_count(handoff + 149u,
                sim::CourseStage::uneven, human, 8u) == 4u
            && rl::foundational_walk_raw_rollout_count(handoff + 299u,
                sim::CourseStage::uneven, human, 8u) == 8u,
        "post-handoff Raw rollout cohort does not grow to the full learner");
    for (const std::uint64_t update : { handoff, handoff + 149u,
            handoff + 299u })
    {
        std::size_t selected{};
        for (std::size_t environment = 0; environment < 8u; ++environment)
            selected += rl::foundational_walk_raw_rollout_environment(update,
                sim::CourseStage::uneven, human, environment, 8u) ? 1u : 0u;
        require(selected == rl::foundational_walk_raw_rollout_count(update,
                    sim::CourseStage::uneven, human, 8u),
            "Raw rollout cohort selection is not deterministic and complete");
    }
    require(!rl::foundational_walk_uses_raw_evaluation(handoff - 1u,
                sim::CourseStage::uneven, human)
            && rl::foundational_walk_uses_raw_evaluation(handoff,
                sim::CourseStage::uneven, human),
        "retained Human gait evaluation does not switch to Raw at handoff");
    sim::Environment evaluation_configuration{ human, 0x749021u };
    rl::configure_policy_evaluation_environment(evaluation_configuration,
        sim::CourseStage::uneven, 0.30f, sim::GaitTask::speed_walk,
        sim::GuidanceMode::raw_policy_audit);
    require(evaluation_configuration.course_stage() == sim::CourseStage::uneven
            && evaluation_configuration.gait_task() == sim::GaitTask::speed_walk
            && !evaluation_configuration.course_motion_enabled()
            && evaluation_configuration.raw_policy_audit(),
        "fresh mastery evaluation lost the selected gait or Raw/static authority");
    const std::uint64_t translated_speed_walk =
        rl::gait_task_incremental_quality(sim::GaitTask::speed_walk, true,
            6u, 30.0f, 108.0f, 120.0f, 0.90f);
    const std::uint64_t in_place_speed_walk =
        rl::gait_task_incremental_quality(sim::GaitTask::speed_walk, true,
            6u, 80.0f, 70.0f, 120.0f, 0.58f);
    require(translated_speed_walk > in_place_speed_walk,
        "Speed Walk still ranks in-place cadence ahead of translation");
    const std::uint64_t rhythmic_walk =
        rl::gait_task_incremental_quality(sim::GaitTask::walk, true,
            6u, 40.0f, 70.0f, 120.0f, 0.58f);
    const std::uint64_t sliding_walk =
        rl::gait_task_incremental_quality(sim::GaitTask::walk, true,
            6u, 20.0f, 108.0f, 120.0f, 0.90f);
    require(rhythmic_walk > sliding_walk,
        "casual Walk lost its anti-sliding stride-first selection contract");
    constexpr std::array gait_tasks{ sim::GaitTask::walk,
        sim::GaitTask::speed_walk, sim::GaitTask::walk_run_transition,
        sim::GaitTask::run };
    for (const sim::GaitTask task : gait_tasks)
    {
        rl::TrainingMetrics evidence{};
        evidence.evaluation_valid = true;
        evidence.evaluation_quality_key = 1u;
        evidence.evaluation_distance = rl::gait_task_mastery_distance(task);
        evidence.evaluation_stride_events = rl::gait_task_mastery_stride_events(task);
        evidence.evaluation_speed = rl::gait_task_mastery_speed(task);
        evidence.evaluation_survival = 18.0f;
        evidence.evaluation_collisions = 1.0f;
        require(rl::gait_task_mastery_evidence(evidence, task),
            "an exact selected-gait boundary cannot complete its lesson");
        evidence.evaluation_speed -= 0.001f;
        require(!rl::gait_task_mastery_evidence(evidence, task),
            "a selected gait completed below its declared speed gate");
    }

    require(sim::swept_circle_hit({ 0.0f, 1.0f }, { 20.0f, 1.0f },
                { 10.0f, 1.0f }, 0.1f)
            && !sim::swept_circle_hit({ 0.0f, 1.0f }, { 20.0f, 1.0f },
                { 10.0f, 1.3f }, 0.1f),
        "swept projectile collision tunnels or creates a false hit");
    const float low_correction = sim::aim_correction_from_miss(0.0f, 0.40f, 10.0f);
    const float high_correction = sim::aim_correction_from_miss(0.0f, -0.40f, 10.0f);
    require(low_correction < 0.0f && high_correction > 0.0f
            && low_correction == sim::aim_correction_from_miss(
                0.0f, 0.40f, 10.0f),
        "miss feedback does not produce deterministic next-shot correction");
    sim::Environment terrain_miss{ sim::CreatureBlueprint::humanoid(), 0x749002u };
    terrain_miss.set_course(sim::CourseStage::balance, 0.30f);
    terrain_miss.configure_equipment(sim::WeaponClass::sidearm, 4.0f);
    sim::EnvironmentTestAccess::record_ground_miss(terrain_miss);
    require(terrain_miss.shots_missed() == 1u
            && terrain_miss.last_shot_miss_error() < 0.0f,
        "a low terrain impact was not recorded as next-shot aiming evidence");

    sim::Environment stopped_gate{ sim::CreatureBlueprint::humanoid(), 0x749003u };
    stopped_gate.set_course(sim::CourseStage::balance, 0.30f);
    stopped_gate.configure_equipment(sim::WeaponClass::sidearm, 4.0f);
    sim::EnvironmentTestAccess::force_physical_ready(stopped_gate, false);
    require(!stopped_gate.equipment_engagement_ready(),
        "weapon can fire while the physical body is still moving");
    sim::EnvironmentTestAccess::force_physical_ready(stopped_gate, true);
    require(stopped_gate.equipment_engagement_ready(),
        "stopped, supported, settled physical aim was rejected");

    sim::Environment safe_carry{ sim::CreatureBlueprint::humanoid(), 0x749003u };
    const std::uint16_t carry_mount = safe_carry.equipment_mount_node_index();
    std::uint16_t carry_parent = safe_carry.blueprint().torso_node;
    float shortest_link = std::numeric_limits<float>::infinity();
    for (const sim::DistanceConstraint& bone : safe_carry.blueprint().bones)
    {
        if (bone.a != carry_mount && bone.b != carry_mount)
            continue;
        const std::uint16_t candidate = bone.a == carry_mount ? bone.b : bone.a;
        const float span = length(safe_carry.blueprint().nodes[candidate]
            - safe_carry.blueprint().nodes[carry_mount]);
        if (span < shortest_link)
        {
            shortest_link = span;
            carry_parent = candidate;
        }
    }
    const Vec2 carry_link = safe_carry.particles()[carry_mount].position
        - safe_carry.particles()[carry_parent].position;
    const float physical_carry_angle = std::atan2(carry_link.y, carry_link.x);
    require(safe_carry.equipment_state() == sim::EquipmentState::safe_carry
            && std::abs(wrap_angle(safe_carry.equipment_aim_angle()
                - physical_carry_angle)) <= 1.0e-5f,
        "safe-carry art invented a horizontal gun instead of following the hand link");

    sim::Environment scalar_cheat{ sim::CreatureBlueprint::humanoid(), 0x749004u };
    scalar_cheat.set_course(sim::CourseStage::balance, 0.30f);
    scalar_cheat.configure_equipment(sim::WeaponClass::sidearm, 4.0f);
    std::array<float, sim::action_count> scalar_only{};
    scalar_only[sim::equipment_state_action] = 1.0f;
    const float local = wrap_angle(scalar_cheat.equipment_recommended_aim_angle());
    scalar_only[sim::equipment_aim_action] = clamp(
        local / (pi * 0.42f), -1.0f, 1.0f);
    scalar_only[sim::equipment_trigger_action] = 1.0f;
    for (int frame = 0; frame < 180; ++frame)
        static_cast<void>(scalar_cheat.step(scalar_only, 1.0f / 60.0f));
    require(scalar_cheat.shots_fired() == 0u,
        "free aim scalar fired without raising the articulated arm/weapon chain");

    for (const std::uint64_t seed : { 0x749100u, 0x749101u })
    {
        sim::Environment physical{ sim::CreatureBlueprint::humanoid(), seed };
        physical.set_course(sim::CourseStage::balance, 0.30f);
        physical.configure_equipment(sim::WeaponClass::sidearm, 4.0f);
        sim::EnvironmentTestAccess::begin_moving(physical);
        bool saw_gun_stance = false;
        bool saw_aiming = false;
        const std::array<float, sim::action_count> neutral{};
        for (int frame = 0; frame < 300 && physical.target_hits() == 0u; ++frame)
        {
            const auto action = rl::effective_policy_action(physical, neutral,
                sim::CourseStage::equipment_targets);
            static_cast<void>(physical.step(action, 1.0f / 60.0f));
            saw_gun_stance = saw_gun_stance
                || physical.equipment_state() == sim::EquipmentState::gun_stance;
            saw_aiming = saw_aiming
                || physical.equipment_state() == sim::EquipmentState::aiming;
        }
        require(saw_gun_stance && saw_aiming,
            "NPC skipped the stop then aim weapon-state transition");
        require(physical.target_hits() == 1u && physical.shots_fired() >= 1u
                && physical.shots_fired() <= 2u,
            "physical arm aim did not produce a bounded real projectile hit");
    }

    const art::HandArtDimensions hand = art::hand_art_dimensions(
        24.0f, 40, 28, 4.0f, 80.0f);
    const art::BootArtDimensions boot = art::articulated_boot_dimensions(
        28.0f, 64.0f, 9.0f);
    require(hand.length <= 80.0f * 0.43f
            && hand.thickness <= 80.0f * 0.27f
            && art::fitted_joint_overlap(80.0f, 32.0f, false) > 0.0f
            && art::fitted_joint_overlap(80.0f, 32.0f, false) <= 80.0f * 0.08f,
        "arm/hand art escapes its physical forearm bone envelope");
    require(boot.width <= 64.0f * 0.88f && boot.height <= 64.0f * 0.40f,
        "boot art escapes heel/toe and lower-leg geometry");

    std::cout << "Runner v0.7.49 physical combat/terrain contracts passed\n";
    return EXIT_SUCCESS;
}
