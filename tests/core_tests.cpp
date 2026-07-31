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
        == sim::InvalidMotion::flipped, "flip hard gate missing");
    require(sim::classify_motion_gate(1.0f, 0.0f, { 301.0f, 3.0f }, 0.0f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::out_of_bounds, "course bounds gate missing");
    require(sim::classify_motion_gate(1.0f, 0.0f, { 0.0f, 3.0f }, 0.8f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::sustained_flight, "flight hard gate missing");
    require(sim::classify_motion_gate(1.0f, 0.0f, { 0.0f, 3.0f }, 0.0f, 0.7f, 3.0f, false)
        == sim::InvalidMotion::micro_motion, "micro-motion gate missing");
    require(!sim::recovery_should_start(true, 0.95f, false, false),
        "harmless upright obstacle contact created a rewardable recovery event");
    require(sim::recovery_should_start(true, 0.80f, false, false),
        "destabilizing collision did not start recovery");
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

    const std::array<sim::CreatureBlueprint, 5> presets{
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid(),
        sim::CreatureBlueprint::quadruped(),
        sim::CreatureBlueprint::monoped()
    };
    for (const sim::CreatureBlueprint& preset : presets)
    {
        require(preset.valid(), "preset is structurally invalid");
        require(preset.signature() != 0, "preset signature is empty");
        for (std::size_t motor_index = 0; motor_index < preset.motors.size(); ++motor_index)
        {
            const sim::MotorConstraint& motor = preset.motors[motor_index];
            require(motor.minimum_angle <= motor.neutral_angle && motor.neutral_angle <= motor.maximum_angle,
                "preset motor rest angle is outside limits");
            require(std::abs(wrap_angle(preset.rest_joint_angle(motor_index) - motor.neutral_angle)) < 0.001f,
                "preset motor is not calibrated to rest geometry");
            require(motor.strength <= 0.060f, "default joint speed remains too strong");
        }
    }

    sim::CreatureBlueprint disconnected = sim::CreatureBlueprint::humanoid();
    disconnected.motors[0].c = disconnected.head_node;
    disconnected.motors[0].enabled = true;
    require(!disconnected.valid(), "enabled motor without direct A-pivot-C bones was accepted");

    const sim::CreatureBlueprint humanoid = sim::CreatureBlueprint::humanoid();
    require(humanoid.nodes.size() == 11, "human-calibrated rig should include passive heel/toe feet");
    require(std::abs(humanoid.nodes[0].y - 2.8127f) < 0.01f, "uploaded humanoid pelvis calibration not applied");
    require(humanoid.nodes.size() == 11, "humanoid passive heel/toe feet were not created");
    require(humanoid.bones.size() == 12, "humanoid feet are not structurally connected");
    for (std::size_t motor_index = 0; motor_index < humanoid.motors.size(); ++motor_index)
    {
        const sim::MotorConstraint& motor = humanoid.motors[motor_index];
        const float driven_arm = length(humanoid.nodes[motor.c] - humanoid.nodes[motor.pivot]);
        const float expected_linear_gain = (motor_index % 2u) == 0u ? 0.0525f : 0.0575f;
        const float expected_travel = (motor_index % 2u) == 0u ? 22.0f : 30.0f;
        require(std::abs(motor.strength * std::max(0.75f, driven_arm) - expected_linear_gain) < 0.002f,
            "non-quadruped motor does not use the quadruped-stable effective gain");
        require(std::abs((motor.neutral_angle - motor.minimum_angle) * 180.0f / pi - expected_travel) < 0.05f,
            "quadruped-stable backward travel was not applied");
        require(std::abs((motor.maximum_angle - motor.neutral_angle) * 180.0f / pi - expected_travel) < 0.05f,
            "quadruped-stable forward travel was not applied");
    }

    for (std::size_t stage_index = 0; stage_index < sim::course_stage_count; ++stage_index)
    {
        sim::Environment environment{ humanoid, 42u + stage_index };
        const auto stage = static_cast<sim::CourseStage>(stage_index);
        environment.set_course(stage, 0.45f);
        if (stage >= sim::CourseStage::hurdles)
            require(!environment.course_features().empty(), "obstacle curriculum stage has no course features");
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
        procedural.set_course(sim::CourseStage::moving_hazards, 0.65f);
        const float initial_progress = procedural.course_progress();
        const float initial_height = procedural.ground_height_at(7.5f);
        const std::array<float, sim::action_count> zero_actions{};
        for (int frame = 0; frame < 90; ++frame)
        {
            const sim::StepResult result = procedural.step(zero_actions);
            require(std::isfinite(result.reward), "procedural obstacle reward is not finite");
            (void)result.terminated;
        }
        require(procedural.course_progress() > initial_progress,
            "procedural course does not advance when the creature is stationary");
        require(std::abs(procedural.ground_height_at(7.5f) - initial_height) > 0.001f,
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
        rl::PpoTrainer monolithic{ humanoid, 16 };
        rl::PpoTrainer staged{ humanoid, 16 };
        monolithic.set_cpu_mode(2);
        staged.set_cpu_mode(2);
        monolithic.train_one_update();

        staged.begin_update();
        require(staged.wait_for_rollout({}), "staged rollout wait was cancelled");
        staged.finish_rollout();
        staged.compute_advantages();
        staged.begin_policy_update();
        std::size_t gradient_batches{};
        while (!staged.policy_update_complete())
        {
            require(staged.wait_for_policy_batch({}), "staged gradient wait was cancelled");
            staged.finish_policy_batch();
            ++gradient_batches;
            require(gradient_batches < 1024, "staged optimizer failed to terminate");
        }
        staged.finalize_update_metrics();
        if (staged.evaluation_due())
        {
            staged.begin_evaluation();
            require(staged.wait_for_evaluation({}), "staged evaluation wait was cancelled");
            staged.finish_evaluation();
        }

        require(staged.metrics().update == 1, "staged PPO update did not finalize");
        require(staged.optimizer_step() == monolithic.optimizer_step(),
            "staged PPO changed the number of Adam applications");
        require(staged.policy().parameters().size() == monolithic.policy().parameters().size(),
            "staged PPO changed policy dimensions");
        for (std::size_t index = 0; index < staged.policy().parameters().size(); ++index)
        {
            require(std::abs(staged.policy().parameters()[index]
                    - monolithic.policy().parameters()[index]) < 1.0e-5f,
                "staged PPO diverged from deterministic monolithic compatibility path");
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
        std::filesystem::temp_directory_path() / "epochrunner-v060-core-test.eppo";
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
        require(autonomous.autonomy_status().pipeline_suspensions > 0,
            "coroutine pipeline never suspended for persistent worker completion");
        require(!autonomous.autonomy_status().pipeline_stage.empty(),
            "coroutine pipeline stage telemetry is missing");

        const std::filesystem::path async_checkpoint =
            std::filesystem::temp_directory_path() / "epochrunner-v070-async-save.eppo";
        std::string async_error{};
        const auto save_started = std::chrono::steady_clock::now();
        require(autonomous.save_checkpoint(async_checkpoint, async_error),
            "asynchronous checkpoint request was rejected");
        require(std::chrono::steady_clock::now() - save_started < std::chrono::milliseconds(20),
            "checkpoint save blocked the caller on serialization or disk I/O");
        for (int attempt = 0; attempt < 2000 && !std::filesystem::exists(async_checkpoint); ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            autonomous.synchronize();
        }
        require(std::filesystem::exists(async_checkpoint),
            "asynchronous checkpoint was never written");
        require(autonomous.autonomy_status().persistence_completed > 0,
            "asynchronous persistence completion was not published");
        std::error_code remove_error{};
        std::filesystem::remove(async_checkpoint, remove_error);
        autonomous.set_updates_per_cycle(1);
        require(autonomous.updates_per_cycle() == 1, "NORMAL speed mode did not latch");
        autonomous.set_updates_per_cycle(2);
        require(autonomous.updates_per_cycle() == 2, "FASTER speed mode did not latch");
        autonomous.set_updates_per_cycle(4);
        require(autonomous.updates_per_cycle() == 4, "MAX CPU speed mode did not latch");
    }

    std::cout << "EpochRunner v0.6.0 procedural course, recovery, concurrency, gait, and rig-edit tests passed\n";
    return EXIT_SUCCESS;
}
