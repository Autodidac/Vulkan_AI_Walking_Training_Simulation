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

    sim::CreatureBlueprint blueprint = sim::CreatureBlueprint::chicken();
    require(blueprint.nodes.size() == 8, "unexpected default node count");
    require(blueprint.radii.size() == blueprint.nodes.size(), "node/radius count mismatch");
    require(!blueprint.bones.empty(), "default blueprint has no bones");

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
    require(trainer.policy().save(temporary, error), "failed to save policy: " + error);

    rl::PolicyNetwork loaded{ 7 };
    require(loaded.load(temporary, error), "failed to load policy: " + error);
    require(loaded.parameters() == trainer.policy().parameters(), "policy serialization mismatch");
    std::filesystem::remove(temporary);

    trainer.reset_preview(1234);
    trainer.step_preview();
    require(std::isfinite(trainer.preview().forward_speed()), "preview speed is not finite");

    std::cout << "EpochRunner core tests passed\n";
    return EXIT_SUCCESS;
}
