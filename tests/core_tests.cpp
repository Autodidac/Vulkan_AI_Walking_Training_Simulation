#include "ppo.hpp"
#include "simulation.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

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

    const std::array<sim::CreatureBlueprint, 5> presets{
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid(),
        sim::CreatureBlueprint::quadruped(),
        sim::CreatureBlueprint::monoped()
    };
    for (const sim::CreatureBlueprint& preset : presets)
    {
        require(preset.nodes.size() >= 3, "preset has too few nodes");
        require(preset.radii.size() == preset.nodes.size(), "node/radius count mismatch");
        require(!preset.bones.empty(), "preset has no bones");
        require(preset.root_node < preset.nodes.size(), "root semantic index is invalid");
        require(preset.torso_node < preset.nodes.size(), "torso semantic index is invalid");
        require(preset.head_node < preset.nodes.size(), "head semantic index is invalid");
        require(preset.left_contact_node < preset.nodes.size(), "left contact semantic index is invalid");
        require(preset.right_contact_node < preset.nodes.size(), "right contact semantic index is invalid");
        for (std::size_t motor_index = 0; motor_index < preset.motors.size(); ++motor_index)
        {
            const sim::MotorConstraint& motor = preset.motors[motor_index];
            require(motor.a < preset.nodes.size() && motor.pivot < preset.nodes.size() && motor.c < preset.nodes.size(),
                "preset motor endpoint is invalid");
            require(motor.minimum_angle <= motor.neutral_angle && motor.neutral_angle <= motor.maximum_angle,
                "preset motor rest angle is outside its limits");
            require(std::abs(epochrunner::wrap_angle(preset.rest_joint_angle(motor_index) - motor.neutral_angle)) < 0.001f,
                "preset motor was not calibrated to its rest pose");
            require(motor.strength <= 0.07f, "preset motor power is too aggressive");
        }
        require(preset.signature() != 0, "preset signature is empty");

        sim::Environment preset_environment{ preset, 19 };
        const std::array<float, sim::action_count> preset_actions{};
        for (int frame = 0; frame < 120; ++frame)
        {
            const sim::StepResult result = preset_environment.step(preset_actions);
            require(std::isfinite(result.reward), "preset reward is not finite");
            require(std::isfinite(result.forward_speed), "preset speed is not finite");
            if (result.terminated)
                preset_environment.reset(19u + static_cast<unsigned>(frame));
        }
    }

    sim::CreatureBlueprint blueprint = sim::CreatureBlueprint::chicken();
    sim::Environment environment{ blueprint, 42 };
    const std::array<float, sim::action_count> zero_actions{};
    for (int frame = 0; frame < 600; ++frame)
    {
        const sim::StepResult result = environment.step(zero_actions);
        require(std::isfinite(result.reward), "simulation reward is not finite");
        require(std::isfinite(result.forward_speed), "simulation speed is not finite");

        const auto observation = environment.observation();
        for (const float value : observation)
            require(std::isfinite(value), "simulation observation is not finite");

        if (result.terminated)
            environment.reset(42u + static_cast<unsigned>(frame));
    }

    rl::PpoTrainer trainer{ blueprint, 8 };
    require(trainer.exploration() < 0.25f, "fresh controller exploration is still too aggressive");
    for (int update = 0; update < 3; ++update)
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
        std::filesystem::temp_directory_path() / "epochrunner-core-test.eppo";
    std::string error{};
    require(trainer.save_checkpoint(temporary, error), "failed to save checkpoint: " + error);

    rl::PpoTrainer resumed{ blueprint, 8 };
    require(resumed.load_checkpoint(temporary, error, false), "failed to resume checkpoint: " + error);
    require(resumed.policy().parameters() == trainer.policy().parameters(), "checkpoint policy mismatch");
    require(resumed.metrics().update == trainer.metrics().update, "checkpoint update count was not restored");
    require(resumed.optimizer_step() == trainer.optimizer_step(), "checkpoint optimizer state was not restored");

    rl::PpoTrainer wrong_rig{ sim::CreatureBlueprint::quadruped(), 8 };
    require(!wrong_rig.load_checkpoint(temporary, error, false), "mismatched rig checkpoint resumed silently");
    require(wrong_rig.load_checkpoint(temporary, error, true), "intentional transfer failed: " + error);
    require(wrong_rig.metrics().update == 0, "transfer retained incompatible progress metrics");
    require(wrong_rig.optimizer_step() == 0, "transfer retained incompatible optimizer state");
    std::filesystem::remove(temporary);

    trainer.reset_preview(1234);
    trainer.step_preview();
    require(std::isfinite(trainer.preview().forward_speed()), "preview speed is not finite");

    std::cout << "EpochRunner core tests passed\n";
    return EXIT_SUCCESS;
}
