#include "ppo.hpp"
#include "simulation.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

int main()
{
    using namespace epochrunner;

    sim::CreatureBlueprint blueprint = sim::CreatureBlueprint::chicken();
    assert(blueprint.nodes.size() == 8);
    assert(blueprint.radii.size() == blueprint.nodes.size());
    assert(!blueprint.bones.empty());

    sim::Environment environment{ blueprint, 42 };
    const std::array<float, sim::action_count> zero_actions{};
    for (int frame = 0; frame < 600; ++frame)
    {
        const sim::StepResult result = environment.step(zero_actions);
        assert(std::isfinite(result.reward));
        assert(std::isfinite(result.forward_speed));
        const auto observation = environment.observation();
        for (float value : observation)
            assert(std::isfinite(value));
        if (result.terminated)
            environment.reset(42u + static_cast<unsigned>(frame));
    }

    rl::PpoTrainer trainer{ blueprint, 8 };
    for (int update = 0; update < 3; ++update)
    {
        trainer.train_one_update();
        const rl::TrainingMetrics& metrics = trainer.metrics();
        assert(metrics.update == static_cast<std::uint64_t>(update + 1));
        assert(std::isfinite(metrics.mean_reward));
        assert(std::isfinite(metrics.mean_speed));
        assert(std::isfinite(metrics.policy_loss));
        assert(std::isfinite(metrics.value_loss));
    }

    const std::filesystem::path temporary = std::filesystem::temp_directory_path() / "epochrunner-core-test.eppo";
    std::string error{};
    assert(trainer.policy().save(temporary, error));
    rl::PolicyNetwork loaded{ 7 };
    assert(loaded.load(temporary, error));
    assert(loaded.parameters() == trainer.policy().parameters());
    std::filesystem::remove(temporary);

    trainer.reset_preview(1234);
    trainer.step_preview();
    assert(std::isfinite(trainer.preview().forward_speed()));

    std::cout << "EpochRunner core tests passed\n";
    return 0;
}
