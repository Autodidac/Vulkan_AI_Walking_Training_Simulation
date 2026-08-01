#include "autonomy.hpp"
#include "ppo.hpp"
#include "simulation.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "EpochRunner core test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    using namespace epochrunner;

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
    require(sim::classify_motion_gate(1.0f, 0.0f, { 301.0f, 3.0f }, 0.0f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::out_of_bounds, "course bounds gate missing");
    require(sim::classify_motion_gate(1.0f, 0.0f, { 0.0f, 3.0f }, 0.8f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::sustained_flight, "unpowered flight hard gate missing");
    require(sim::powered_joint_launch(sim::CourseStage::ramps, 1.0f, 0.08f),
        "joint-powered jump is not recognized");
    require(!sim::powered_joint_launch(sim::CourseStage::walk, 1.0f, 0.08f),
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
        && sim::course_stage_name(sim::CourseStage::walk) == "2. DUCK / RECOVER"
        && sim::course_stage_name(sim::CourseStage::ramps) == "3. JUMP / LAND"
        && sim::course_stage_name(sim::CourseStage::uneven) == "4. WALK / RUN"
        && sim::course_stage_name(sim::CourseStage::hurdles) == "5. MOVING DUCK / JUMP"
        && sim::course_stage_name(sim::CourseStage::duck_bars) == "6. CONTROLLED FLIPS"
        && sim::course_stage_name(sim::CourseStage::moving_hazards) == "7. MIXED GOAL COURSE",
        "skill curriculum is not ordered by prerequisite");
    require(sim::stage_skill_evidence(sim::CourseStage::walk, 0u, 0.6f, 0u, 0.0f, 0u, 0u),
        "duck evidence cannot complete the duck lesson");
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
    require(sim::wheel_sliding_motion(0.45f, true, true, 0.50f),
        "double-supported wheel-like sliding is not detected");
    require(!sim::wheel_sliding_motion(0.45f, true, false, 0.50f),
        "single-support walking is incorrectly classified as wheel sliding");
    require(!sim::rolling_gate_active(1.0f)
            && sim::rolling_gate_active(sim::rolling_gate_activation_seconds),
        "rolling hard gate does not provide a bounded startup settle window");
    require(sim::body_rolling_limit(sim::CourseStage::walk, 1.8f)
            > sim::body_rolling_limit(sim::CourseStage::walk, 4.0f),
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
    require(rl::elite_motion_eligible(sim::CourseStage::uneven, true, 3, 1.2f, 4.0f),
        "valid stepped best result cannot seed self-imitation");
    require(rl::elite_motion_eligible(sim::CourseStage::walk, true, 0, 0.0f, 4.0f, 0.8f),
        "valid duck result cannot seed self-imitation");
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

    const std::array<sim::CreatureBlueprint, 7> presets{
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
    require(humanoid.nodes.size() >= 17,
        "human-calibrated rig should include passive heel/toe feet and articulated arms");
    require(std::abs(humanoid.nodes[0].y - 2.8127f) < 0.01f,
        "uploaded humanoid pelvis calibration not applied");
    require(humanoid.bones.size() >= 17,
        "humanoid feet or arms are not structurally connected");
    require(humanoid.active_motor_count == sim::action_count,
        "humanoid does not expose independent shoulder and elbow motors");
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

        std::array<bool, 5> found{};
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
    }

    const std::filesystem::path temporary =
        std::filesystem::temp_directory_path() / "epochrunner-v061-core-test.eppo";
    std::string error{};
    require(trainer.save_checkpoint(temporary, error), "failed to save checkpoint: " + error);
    rl::PpoTrainer resumed{ humanoid, 16 };
    require(resumed.load_checkpoint(temporary, error, false), "failed to resume checkpoint: " + error);
    require(resumed.policy().parameters() == trainer.policy().parameters(), "checkpoint policy mismatch");
    require(resumed.metrics().update == trainer.metrics().update, "checkpoint update count was not restored");
    require(resumed.optimizer_step() == trainer.optimizer_step(), "checkpoint optimizer state was not restored");
    require(resumed.course_stage() == trainer.course_stage(), "checkpoint curriculum stage was not restored");

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

    std::cout << "EpochRunner v0.6.2 obstacle observation, recovery, concurrency, gait, and rig-edit tests passed\n";
    return EXIT_SUCCESS;
}
