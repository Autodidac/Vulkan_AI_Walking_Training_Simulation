#include "autonomy.hpp"
#include "simulation.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <thread>

namespace
{
    using namespace std::chrono_literals;

    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "EpochRunner runtime pipeline test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    template <typename Predicate>
    bool wait_until(Predicate&& predicate, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (predicate())
                return true;
            std::this_thread::sleep_for(5ms);
        }
        return predicate();
    }
}

int main()
{
    using namespace epochrunner;

    const sim::CreatureBlueprint humanoid = sim::CreatureBlueprint::humanoid();
    require(humanoid.valid(), "articulated humanoid is invalid");
    require(humanoid.active_motor_count == sim::action_count,
        "humanoid does not expose all eight leg and arm motors");
    require(humanoid.nodes.size() >= 13 && humanoid.bones.size() >= 13,
        "humanoid arm nodes or bones are missing");

    for (const sim::CreatureBlueprint& rig : {
        sim::CreatureBlueprint::biped(), sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::quadruped(), sim::CreatureBlueprint::crawler4(),
        sim::CreatureBlueprint::hexapod(), sim::CreatureBlueprint::monoped() })
    {
        require(rig.valid(), "four-motor built-in rig became invalid after policy expansion");
        require(rig.active_motor_count == 4,
            "non-humanoid rig did not preserve its four active motors");
    }

    {
        sim::Environment neutral{ humanoid, 0xA11CEu };
        sim::Environment arms{ humanoid, 0xA11CEu };
        neutral.set_course(sim::CourseStage::balance, 0.25f);
        arms.set_course(sim::CourseStage::balance, 0.25f);
        std::array<float, sim::action_count> zero{};
        std::array<float, sim::action_count> arm_action{};
        arm_action[4] = 1.0f;
        arm_action[5] = -1.0f;
        arm_action[6] = -1.0f;
        arm_action[7] = 1.0f;
        for (int frame = 0; frame < 80; ++frame)
        {
            neutral.step(zero);
            arms.step(arm_action);
        }
        const auto& neutral_particles = neutral.particles();
        const auto& arm_particles = arms.particles();
        require(neutral_particles.size() > 12 && arm_particles.size() > 12,
            "humanoid arm particles were not simulated");
        const float hand_delta = length(arm_particles[9].position - neutral_particles[9].position)
            + length(arm_particles[12].position - neutral_particles[12].position);
        require(hand_delta > 0.04f, "independent arm actions did not affect hand positions");
    }

    {
        rl::PpoTrainer first{ humanoid, 8, false };
        rl::PpoTrainer second{ humanoid, 8, false };
        first.set_cpu_mode(1);
        second.set_cpu_mode(1);
        first.begin_staged_update();
        second.begin_staged_update();
        first.compute_staged_advantages();
        second.compute_staged_advantages();
        first.optimize_staged_update();
        second.optimize_staged_update();
        first.finish_staged_update();
        second.finish_staged_update();
        require(first.metrics().update == 1 && second.metrics().update == 1,
            "staged PPO update did not finish");
        require(first.policy().parameters() == second.policy().parameters(),
            "identical staged updates were not deterministic");
    }

    const std::filesystem::path checkpoint =
        std::filesystem::temp_directory_path() / "epochrunner-v070-runtime-test.eppo";
    std::filesystem::remove(checkpoint);
    std::filesystem::remove(checkpoint.string() + ".tmp");

    {
        rl::AutonomousTrainer trainer{ humanoid, 16 };
        trainer.set_updates_per_cycle(4);
        trainer.set_background_enabled(true);

        sim::CreatureBlueprint edited = humanoid;
        const auto edit_started = std::chrono::steady_clock::now();
        for (int edit = 0; edit < 64; ++edit)
        {
            edited.nodes[3].x = humanoid.nodes[3].x + static_cast<float>(edit % 5) * 0.001f;
            edited.rebuild_rest_lengths();
            trainer.set_blueprint(edited, true);
        }
        const auto edit_elapsed = std::chrono::steady_clock::now() - edit_started;
        require(edit_elapsed < 100ms, "coalesced hip edits blocked on trainer work");

        require(wait_until([&]
        {
            trainer.synchronize();
            return trainer.metrics().update >= 2;
        }, 20s), "background staged pipeline completed no updates");

        trainer.synchronize();
        const std::uint32_t required_pipeline_stages = (1u << 6u) - 1u;
        require((trainer.autonomy_status().pipeline_stage_mask & required_pipeline_stages)
                == required_pipeline_stages,
            "coroutine diagnostics did not record commands through immutable publication");
        require(!trainer.autonomy_status().pipeline_stage.empty(),
            "current coroutine stage is not published");

        trainer.set_updates_per_cycle(1);
        trainer.set_updates_per_cycle(2);
        trainer.set_updates_per_cycle(4);
        require(trainer.updates_per_cycle() == 4, "speed-mode switching did not latch");

        trainer.set_background_enabled(false);
        const std::uint64_t paused_update = trainer.metrics().update;
        trainer.train_one_update();
        require(wait_until([&]
        {
            trainer.synchronize();
            return trainer.metrics().update > paused_update;
        }, 15s), "paused single-update request was not processed");
        trainer.set_background_enabled(true);

        std::string error{};
        const auto save_started = std::chrono::steady_clock::now();
        require(trainer.save_checkpoint(checkpoint, error), "checkpoint save was not accepted");
        require(std::chrono::steady_clock::now() - save_started < 25ms,
            "checkpoint save request performed blocking serialization or disk I/O");
        for (int save = 0; save < 8; ++save)
            require(trainer.save_checkpoint(checkpoint, error), "coalesced save request failed");
        require(wait_until([&]
        {
            trainer.synchronize();
            return std::filesystem::exists(checkpoint)
                && std::filesystem::file_size(checkpoint) > 64;
        }, 20s), "asynchronous checkpoint was not published");

        const auto load_started = std::chrono::steady_clock::now();
        require(trainer.load_checkpoint(checkpoint, error, false),
            "checkpoint load was not accepted");
        require(std::chrono::steady_clock::now() - load_started < 25ms,
            "checkpoint load request performed blocking disk I/O");
        require(wait_until([&]
        {
            trainer.synchronize();
            return trainer.controller_state_name() == "RESUMED";
        }, 20s), "asynchronous checkpoint load was not applied by worker ownership");

        trainer.set_blueprint(sim::CreatureBlueprint::quadruped(), false);
        trainer.set_blueprint(sim::CreatureBlueprint::hexapod(), false);
        trainer.set_blueprint(humanoid, false);
        require(wait_until([&]
        {
            trainer.synchronize();
            return trainer.rig_signature() == humanoid.signature();
        }, 15s), "rapid preset swaps were not coalesced and published");

        trainer.set_background_enabled(false);
    }

    std::filesystem::remove(checkpoint);
    std::filesystem::remove(checkpoint.string() + ".tmp");

    std::cout << "EpochRunner v0.7 runtime pipeline, arms, and async persistence tests passed\n";
    return EXIT_SUCCESS;
}
