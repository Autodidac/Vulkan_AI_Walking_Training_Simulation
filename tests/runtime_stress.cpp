#include "autonomy.hpp"
#include "simulation.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace
{
    using Clock = std::chrono::steady_clock;

    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "EpochRunner runtime stress failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    void record_latency(std::atomic<std::int64_t>& maximum_nanoseconds,
        Clock::time_point started) noexcept
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - started).count();
        std::int64_t observed = maximum_nanoseconds.load(std::memory_order_relaxed);
        while (observed < elapsed
            && !maximum_nanoseconds.compare_exchange_weak(
                observed, elapsed, std::memory_order_relaxed, std::memory_order_relaxed))
        {
        }
    }

    template <class Predicate>
    bool wait_until(epochrunner::rl::AutonomousTrainer& trainer,
        std::chrono::milliseconds timeout, Predicate&& predicate)
    {
        const auto deadline = Clock::now() + timeout;
        while (Clock::now() < deadline)
        {
            trainer.synchronize();
            trainer.step_preview();
            if (predicate())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        trainer.synchronize();
        return predicate();
    }
}

int main()
{
    using namespace epochrunner;
    using namespace std::chrono_literals;

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "epochrunner-v070-runtime-stress";
    const std::filesystem::path checkpoint = root / "stress.eppo";
    const std::filesystem::path rig = root / "stress.epochrig";
    const std::filesystem::path state = root / "stress.state";
    std::error_code filesystem_error{};
    std::filesystem::remove_all(root, filesystem_error);
    filesystem_error.clear();
    std::filesystem::create_directories(root, filesystem_error);
    require(!filesystem_error, "could not create stress-test directory");

    auto trainer = std::make_unique<rl::AutonomousTrainer>(
        sim::CreatureBlueprint::humanoid(), 24);
    trainer->set_autosave_paths(checkpoint, rig, state);
    trainer->set_updates_per_cycle(4);
    trainer->set_background_enabled(true);

    std::atomic<std::int64_t> maximum_call_nanoseconds{};
    std::atomic_bool producer_failure{};

    std::jthread hip_editor([&](std::stop_token stop_token)
    {
        for (int iteration = 0; iteration < 160 && !stop_token.stop_requested(); ++iteration)
        {
            sim::CreatureBlueprint edited = sim::CreatureBlueprint::humanoid();
            const float offset = static_cast<float>((iteration % 17) - 8) * 0.006f;
            edited.motors[0].neutral_angle = std::clamp(
                edited.motors[0].neutral_angle + offset,
                edited.motors[0].minimum_angle,
                edited.motors[0].maximum_angle);
            edited.motors[1].neutral_angle = std::clamp(
                edited.motors[1].neutral_angle - offset,
                edited.motors[1].minimum_angle,
                edited.motors[1].maximum_angle);
            edited.rebuild_rest_lengths();
            const auto started = Clock::now();
            trainer->set_blueprint(edited, true);
            record_latency(maximum_call_nanoseconds, started);
            std::this_thread::sleep_for(1ms);
        }
    });

    std::jthread mode_switcher([&](std::stop_token stop_token)
    {
        constexpr int modes[]{ 1, 2, 4, 2 };
        for (int iteration = 0; iteration < 240 && !stop_token.stop_requested(); ++iteration)
        {
            const auto started = Clock::now();
            trainer->set_updates_per_cycle(modes[iteration % 4]);
            trainer->set_exploration(0.05f + static_cast<float>(iteration % 8) * 0.01f);
            if (iteration % 11 == 0)
            {
                trainer->set_background_enabled(false);
                trainer->train_one_update();
                trainer->set_background_enabled(true);
            }
            record_latency(maximum_call_nanoseconds, started);
            std::this_thread::sleep_for(1ms);
        }
    });

    std::jthread checkpoint_writer([&](std::stop_token stop_token)
    {
        for (int iteration = 0; iteration < 80 && !stop_token.stop_requested(); ++iteration)
        {
            std::string error{};
            const auto started = Clock::now();
            if (!trainer->save_checkpoint(checkpoint, error))
                producer_failure.store(true, std::memory_order_relaxed);
            record_latency(maximum_call_nanoseconds, started);
            std::this_thread::sleep_for(2ms);
        }
    });

    const auto concurrent_deadline = Clock::now() + 4s;
    while (Clock::now() < concurrent_deadline)
    {
        trainer->synchronize();
        trainer->step_preview();
        require(trainer->autonomy_status().pending_commands <= 8,
            "coalescing command queue grew without bound");
        std::this_thread::sleep_for(1ms);
    }

    hip_editor.join();
    mode_switcher.join();
    checkpoint_writer.join();
    require(!producer_failure.load(std::memory_order_relaxed),
        "a concurrent checkpoint request was rejected");

    trainer->set_background_enabled(false);
    require(wait_until(*trainer, 10s, [&]
    {
        return trainer->autonomy_status().pending_commands == 0
            && !trainer->autonomy_status().worker_busy;
    }), "runtime did not drain coalesced commands after pause");

    const std::uint64_t update_before_single_step = trainer->metrics().update;
    trainer->train_one_update();
    require(wait_until(*trainer, 15s, [&]
    {
        return trainer->metrics().update > update_before_single_step;
    }), "paused single-update request never completed");

    require(wait_until(*trainer, 10s, [&]
    {
        return std::filesystem::exists(checkpoint)
            && trainer->autonomy_status().persistence_completed > 0
            && !trainer->autonomy_status().persistence_pending;
    }), "asynchronous checkpoint coalescing never completed");

    std::string load_message{};
    const auto load_started = Clock::now();
    require(trainer->load_checkpoint(checkpoint, load_message, false),
        "valid checkpoint load request was rejected");
    record_latency(maximum_call_nanoseconds, load_started);
    require(wait_until(*trainer, 10s, [&]
    {
        return trainer->controller_state_name() == "RESUMED";
    }), "queued checkpoint load was never applied by the owner thread");

    require(trainer->autonomy_status().pipeline_suspensions > 0,
        "coroutine pipeline did not suspend for persistent worker completion");
    require(!trainer->autonomy_status().pipeline_stage.empty(),
        "pipeline stage telemetry disappeared under stress");

    const auto maximum_call = std::chrono::nanoseconds(
        maximum_call_nanoseconds.load(std::memory_order_relaxed));
    require(maximum_call < 50ms,
        "UI-facing command exceeded the 50 ms non-blocking stress bound");

    trainer->set_background_enabled(true);
    trainer->set_updates_per_cycle(4);
    std::this_thread::sleep_for(20ms);
    const auto shutdown_started = Clock::now();
    trainer.reset();
    require(Clock::now() - shutdown_started < 5s,
        "stop-token shutdown stalled in a suspended pipeline or I/O stage");

    std::filesystem::remove_all(root, filesystem_error);
    std::cout << "EpochRunner v0.7 runtime stress passed; max command latency "
        << std::chrono::duration_cast<std::chrono::microseconds>(maximum_call).count()
        << " us\n";
    return EXIT_SUCCESS;
}
