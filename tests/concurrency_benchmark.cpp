#include "autonomy.hpp"
#include "simulation.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>

namespace
{
    struct Result
    {
        int mode{};
        std::uint64_t updates{};
        std::uint64_t environment_steps{};
        std::size_t workers{};
        double seconds{};
        double updates_per_second{};
        double steps_per_second{};
    };

    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner concurrency benchmark failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    Result measure(int mode)
    {
        using namespace std::chrono_literals;
        using namespace runner;

        rl::AutonomousTrainer trainer{ sim::CreatureBlueprint::humanoid(), 64 };
        trainer.set_updates_per_cycle(mode);
        trainer.set_background_enabled(true);

        constexpr auto measurement_time = 4s;
        const auto started = std::chrono::steady_clock::now();
        const auto deadline = started + measurement_time;
        while (std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(10ms);
            trainer.synchronize();
        }
        trainer.set_background_enabled(false);
        trainer.synchronize();
        const auto finished = std::chrono::steady_clock::now();

        const rl::TrainingMetrics metrics = trainer.metrics();
        const rl::AutonomyStatus status = trainer.autonomy_status();
        const double seconds = std::chrono::duration<double>(finished - started).count();
        return {
            mode,
            metrics.update,
            metrics.environment_steps,
            status.rollout_threads,
            seconds,
            static_cast<double>(metrics.update) / seconds,
            static_cast<double>(metrics.environment_steps) / seconds
        };
    }
}

int main()
{
    const std::array<Result, 3> results{
        measure(1),
        measure(2),
        measure(4)
    };

    for (const Result& result : results)
    {
        std::cout << "mode=" << result.mode
            << " workers=" << result.workers
            << " updates=" << result.updates
            << " updates_per_second=" << result.updates_per_second
            << " environment_steps_per_second=" << result.steps_per_second
            << '\n';
    }

    const Result& normal = results[0];
    const Result& faster = results[1];
    const Result& maximum = results[2];
    require(normal.updates > 0 && faster.updates > 0 && maximum.updates > 0,
        "one or more speed modes completed no training updates");
    require(normal.workers <= faster.workers && faster.workers <= maximum.workers,
        "worker budgets are not monotonic across speed modes");
    require(faster.updates_per_second > normal.updates_per_second,
        "FASTER did not exceed NORMAL measured throughput");
    require(maximum.updates_per_second > normal.updates_per_second,
        "MAX CPU did not exceed NORMAL measured throughput");

    std::cout << "Runner speed-mode throughput benchmark passed\n";
    return EXIT_SUCCESS;
}
