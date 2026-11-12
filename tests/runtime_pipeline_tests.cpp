#include "autonomy.hpp"
#include "simulation.hpp"
#include "training_budget.hpp"
#include "raw_learning_test_access.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string_view>
#include <thread>

namespace
{
    using namespace std::chrono_literals;

    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner runtime pipeline test failed: " << message << '\n';
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

    void test_replay_ownership()
    {
        using namespace runner;
        rl::AutonomousTrainer subject{ sim::CreatureBlueprint::humanoid(), 8 };
        subject.set_autosave_paths({}, {}, {});
        require(!subject.has_training_preview(),
            "untouched spawn pose masqueraded as a worker sample");
        subject.set_preview_guidance_mode(sim::GuidanceMode::raw_policy_audit);
        subject.reset_preview(0x7511u);
        for (int tick = 0; tick < 6000 && !subject.preview_terminal(); ++tick)
            subject.step_preview(1.0f / 60.0f);
        require(subject.preview_terminal(), "fixture did not reach a replay terminal");

        const auto terminal_trial = subject.preview_trial_id();
        const auto terminal_time = subject.preview().elapsed_seconds();
        const auto pinned_update = subject.replay_controller_update();
        const auto terminal_cause = subject.preview_terminal_cause();
        subject.set_background_enabled(true);
        require(wait_until([&]
        {
            subject.synchronize();
            return subject.training_preview_is_rollout_sample()
                && subject.training_preview().elapsed_seconds() > 0.0f
                && subject.metrics().update == pinned_update;
        }, 10s), "no rollout sample arrived before the first update completed");
        require(wait_until([&]
        {
            subject.synchronize();
            subject.step_preview(1.0f / 60.0f);
            return subject.metrics().update > pinned_update
                && subject.has_training_preview();
        }, 60s), "the trainer did not publish while a replay was terminal");
        require(subject.preview_trial_id() == terminal_trial
                && subject.preview().elapsed_seconds() == terminal_time
                && subject.preview_terminal_cause() == terminal_cause
                && subject.replay_controller_update() == pinned_update,
            "worker publication restarted or modified a terminal replay");
        require(&subject.preview() != &subject.training_preview()
                && subject.training_preview().elapsed_seconds() > 0.0f,
            "worker sample is missing or aliases the replay");

        const auto training_update = subject.metrics().update;
        subject.reset_preview(0x7512u);
        require(subject.preview_trial_id() == terminal_trial + 1u
                && !subject.preview_terminal()
                && subject.preview().elapsed_seconds() == 0.0f
                && subject.preview().raw_policy_audit()
                && subject.has_training_preview()
                && subject.background_enabled()
                && subject.metrics().update >= training_update,
            "R did not start a fresh replay independently of the trainer");
        require(subject.replay_controller_update()
                == (subject.has_best_policy() ? subject.metrics().best_update
                                             : subject.metrics().update),
            "R did not pin the latest available replay controller");

        subject.set_background_enabled(false);
        for (const std::uint64_t seed : { 0x7513u, 0x7514u })
        {
            subject.reset_preview(seed);
            const auto prior_trial = subject.preview_trial_id();
            subject.step_preview(1.0f / 60.0f);
            require(subject.preview_trial_id() == prior_trial
                    && subject.preview().elapsed_seconds() > 0.0f,
                "replay depends on the worker running to advance");
        }
    }

    void test_autopilot_unaided_stand()
    {
        using namespace runner;
        const auto rig = sim::CreatureBlueprint::humanoid();
        const auto directory = std::filesystem::temp_directory_path()
            / ("epoch-unaided-stand-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        require(std::filesystem::create_directory(directory), "cannot isolate Stand test files");
        const auto old_checkpoint = directory / "epoch2dwalk-v0751-human-autosave.eppo";
        const auto old_rig = directory / "epoch2dwalk-v0751-human-evolved.rig";
        const auto old_state = directory / "epoch2dwalk-v0751-human-autonomy.state";
        const auto new_checkpoint = directory / "epoch2dwalk-v0751-raw-stand-human-autosave.eppo";
        const auto new_rig = directory / "epoch2dwalk-v0751-raw-stand-human-evolved.rig";
        const auto new_state = directory / "epoch2dwalk-v0751-raw-stand-human-autonomy.state";
        for (const auto& file : { old_checkpoint, old_rig, old_state })
        {
            std::ofstream sentinel(file, std::ios::binary);
            sentinel << "LEGACY HUMAN SAVE\n";
            require(sentinel.good(), "could not seed preserved legacy save fixture");
        }
        std::string error;
        {
            rl::AutonomousTrainer subject{ rig, 4 };
            subject.set_autosave_paths(old_checkpoint, old_rig, old_state);
            require(!subject.load_autosave(error), "Human Stand tried loading the legacy save");
            subject.set_updates_per_cycle(2);
            subject.set_gait_task(sim::GaitTask::walk);
            require(wait_until([&]
            {
                subject.synchronize();
                return subject.autonomy_status().message.find("REJECTED")
                    != std::string::npos;
            }, 3s), "manual Walk request bypassed the Human Stand prerequisite");
            subject.set_background_enabled(true);
            require(wait_until([&]
            {
                subject.synchronize();
                return subject.training_preview_is_rollout_sample()
                    && subject.training_preview().raw_policy_audit()
                    && subject.training_preview().effective_physical_guidance_authority() == 0.0f
                    && subject.training_preview().elapsed_seconds() > 0.0f;
            }, 20s), "Human Stand was not trained by raw policy rollout");
            std::uint64_t seen_evaluation{};
            int maximum_streak{};
            const bool entered_walk = wait_until([&]
            {
                subject.synchronize();
                const auto& status = subject.autonomy_status();
                const auto& metrics = subject.metrics();
                maximum_streak = std::max(maximum_streak, status.mastery_streak);
                if (seen_evaluation != metrics.evaluation_count)
                {
                    seen_evaluation = metrics.evaluation_count;
                    std::cout << "Autopilot update=" << metrics.total_updates
                        << " evaluation=" << seen_evaluation
                        << " invalid=" << metrics.evaluation_invalid_runs
                        << " stance=" << metrics.evaluation_longest_stance
                        << " confirmations=" << status.mastery_streak
                        << " " << status.message << std::endl;
                }
                return status.stage == sim::CourseStage::uneven
                    && status.director_task == director::TaskKind::walk;
            }, 180s);
            subject.set_background_enabled(false);
            require(entered_walk, "Autopilot did not qualify Human Stand and enter Walk");
            require(maximum_streak >= rl::balance_mastery_lock_confirmations - 1
                    && subject.metrics().total_updates >=
                        rl::stage_minimum_fresh_updates(sim::CourseStage::balance),
                "Autopilot did not confirm unsupported Stand before Walk");
            require(wait_until([&] { return std::filesystem::exists(new_state); }, 5s),
                "Walk handoff was not written to the isolated save");
        }
        {
            rl::AutonomousTrainer resumed{ rig, 4 };
            resumed.set_autosave_paths(old_checkpoint, old_rig, old_state);
            require(resumed.load_autosave(error), "isolated verified save was not found");
            require(wait_until([&]
            {
                resumed.synchronize();
                return resumed.autonomy_status().stage == sim::CourseStage::uneven;
            }, 5s), "exact verified Stand-to-Walk handoff did not resume");
        }
        {
            // Valid, but different controller bytes next to the accepted
            // Director state. Do not recover its Walk credit from the sidecar.
            rl::PpoTrainer alternate{ rig, 4, false };
            alternate.set_course(sim::CourseStage::uneven, 0.10f, false);
            require(rl::PpoTrainer::write_checkpoint_data(
                alternate.checkpoint_data(), new_checkpoint, error),
                "could not write mismatched checkpoint fixture");
            rl::AutonomousTrainer rejected{ rig, 4 };
            rejected.set_autosave_paths(old_checkpoint, old_rig, old_state);
            require(rejected.load_autosave(error), "mismatched save was not queued");
            require(wait_until([&]
            {
                rejected.synchronize();
                return rejected.autonomy_status().message.find("AUTOSAVE REJECTED")
                    != std::string::npos;
            }, 5s) && rejected.autonomy_status().stage == sim::CourseStage::balance,
                "mismatched checkpoint borrowed Walk credit");
        }
        for (const auto& file : { old_checkpoint, old_rig, old_state })
        {
            std::ifstream sentinel(file, std::ios::binary);
            std::string saved{};
            std::getline(sentinel, saved);
            require(saved == "LEGACY HUMAN SAVE"
                    && sentinel.peek() == std::char_traits<char>::eof(),
                "the old Human save was modified");
        }
        for (const auto& file : { old_checkpoint, old_rig, old_state,
                new_checkpoint, new_rig, new_state })
            std::filesystem::remove(file);
        std::filesystem::remove(directory);
    }

    void test_incompatible_same_rig_autosave()
    {
        using namespace runner;
        const auto directory = std::filesystem::temp_directory_path()
            / ("epoch-resource-reset-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory);
        const auto old_path = directory / "old.eppo";
        const auto fresh_path = directory / "fresh.eppo";
        const auto rig = sim::CreatureBlueprint::humanoid();
        rl::PpoTrainer reference{ rig, 8, false };
        auto old = reference.checkpoint_data();
        old.training_semantics = 0x0007'5114u;
        old.metrics.total_updates = 73u;
        old = rl::RawLearningTestAccess::reseal(std::move(old));
        std::string error;
        require(rl::PpoTrainer::write_checkpoint_data(old, old_path, error),
            "could not write isolated old-semantics fixture");
        {
            rl::AutonomousTrainer subject{ rig, 8 };
            subject.set_autosave_paths(old_path, {}, {});
            subject.reset_policy(0x123456u);
            require(wait_until([&]
            {
                subject.synchronize();
                return subject.autonomy_status().message.find("CONTROLLER RESET")
                    != std::string::npos;
            }, 2s), "same-rig reset setup was not applied");
            require(subject.load_autosave(error), "old autosave load was not queued");
            require(wait_until([&]
            {
                subject.synchronize();
                return subject.metrics().total_updates == 73u;
            }, 3s), "old autosave lifetime totals were not imported");
            require(!subject.background_enabled() && subject.metrics().update == 0u
                    && subject.autonomy_status().stage == sim::CourseStage::balance
                    && subject.autonomy_status().mastery_streak == 0
                    && subject.autonomy_status().director_task == director::TaskKind::stand,
                "incompatible autosave restored training/mastery instead of paused Stand");
            require(subject.save_checkpoint(fresh_path, error), "fresh checkpoint save failed");
            require(wait_until([&] { return std::filesystem::exists(fresh_path); }, 3s),
                "paused worker did not publish reset checkpoint");
            rl::PpoTrainer::CheckpointData actual;
            require(rl::PpoTrainer::read_checkpoint_data(fresh_path, actual, error),
                "reset checkpoint cannot be read");
            reference.reset_policy(0xC0FFEEu, false);
            reference.set_course(sim::CourseStage::balance, 0.25f, false);
            reference.set_gait_task(sim::GaitTask::walk, false);
            reference.set_equipment_directive(sim::EquipmentDirective::passive);
            require(actual.parameters == reference.policy().parameters()
                    && actual.optimizer_step == 0 && actual.best_parameters.empty(),
                "same-rig fallback retained old weights, optimizer or best policy");
            require(subject.rig_signature() == rig.signature()
                    && std::filesystem::exists(old_path),
                "migration damaged authored rig or old checkpoint");
        }
        std::filesystem::remove_all(directory);
    }
}

int main(int argc, char** argv)
{
    using namespace runner;

    if (argc == 2 && std::string_view{ argv[1] } == "--replay-ownership-only")
    {
        test_replay_ownership();
        return EXIT_SUCCESS;
    }
    if (argc == 2 && std::string_view{ argv[1] } == "--autopilot-unaided-stand")
    {
        test_autopilot_unaided_stand();
        return EXIT_SUCCESS;
    }

    for (int repeat = 0; repeat < 32; ++repeat)
    {
        for (std::size_t capacity = 0; capacity <= 16; ++capacity)
        {
            require(rl::training_budget::active_workers(1, capacity) == 1u,
                "Eco must use exactly one worker");
            require(rl::training_budget::active_workers(2, capacity)
                    == std::min<std::size_t>(2u, std::max<std::size_t>(1u, capacity)),
                "Balanced worker count exceeds its budget");
            require(rl::training_budget::active_workers(4, capacity)
                    == std::max<std::size_t>(1u, capacity),
                "Maximum must honor explicit capacity");
        }
        require(rl::training_budget::idle_nanoseconds(1, 2'000'000'000LL) == 6'000'000'000LL
                && rl::training_budget::idle_nanoseconds(2, 2'000'000'000LL) == 2'000'000'000LL,
            "long updates evade proportional CPU duty budget");
        for (const auto work : { std::int64_t{0}, std::int64_t{-1},
                std::numeric_limits<std::int64_t>::min() })
            require(rl::training_budget::idle_nanoseconds(1, work) == 0,
                "invalid work duration produces a wait");
        require(rl::training_budget::idle_nanoseconds(1,
                std::numeric_limits<std::int64_t>::max()) > 0
                && rl::training_budget::idle_nanoseconds(4, 999) == 0,
            "budget overflow or Maximum throttle regression");
    }

    if (argc == 2 && (std::string_view{argv[1]} == "--resource-budget-only"
            || std::string_view{argv[1]} == "--resource-duty-only"))
    {
        const auto rig = sim::CreatureBlueprint::humanoid();
        rl::PpoTrainer pool{ rig, 8 };
        require(pool.maximum_worker_count() <= 4u && pool.rollout_worker_count() == 1u,
            "default pool starts unrestricted");
        rl::AutonomousTrainer paused{ rig, 8 };
        paused.set_autosave_paths({}, {}, {});
        const auto baseline = paused.metrics().update;
        std::this_thread::sleep_for(300ms);
        paused.synchronize();
        require(!paused.background_enabled() && paused.metrics().update == baseline,
            "idle trainer starts work without authorization");
        paused.set_exploration(0.04f);
        require(wait_until([&]
        {
            paused.synchronize();
            return std::abs(paused.exploration() - 0.04f) < 0.001f;
        }, 2s), "paused worker does not service commands");
        require(paused.metrics().update == baseline,
            "paused command caused a training update");
        test_incompatible_same_rig_autosave();
        if (std::string_view{argv[1]} == "--resource-duty-only")
        {
            const auto started = std::chrono::steady_clock::now();
            paused.set_background_enabled(true);
            require(wait_until([&]
            {
                paused.synchronize();
                return paused.metrics().update > baseline;
            }, 60s), "bounded Eco update did not finish");
            const auto work = std::chrono::steady_clock::now() - started;
            const auto completed = paused.metrics().update;
            const auto idle = std::min(1000ms,
                std::chrono::duration_cast<std::chrono::milliseconds>(work / 2));
            std::this_thread::sleep_for(idle);
            paused.synchronize();
            require(paused.metrics().update == completed,
                "Eco ran another update during its proportional idle interval");
            paused.set_background_enabled(false);
            const auto pause_started = std::chrono::steady_clock::now();
            paused.set_exploration(0.06f);
            require(wait_until([&]
            {
                paused.synchronize();
                return std::abs(paused.exploration() - 0.06f) < 0.001f;
            }, 2s), "pause/command could not interrupt CPU budget wait");
            require(paused.metrics().update == completed && !paused.background_enabled(),
                "pause/command restarted training");
            std::cout << "Eco update wall seconds="
                << std::chrono::duration<double>(work).count()
                << " idle window ms=" << idle.count()
                << " pause command ms="
                << std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - pause_started).count() << '\n';
        }
        std::cout << "Resource budget and paused command contracts passed (not gait acceptance)\n";
        return EXIT_SUCCESS;
    }

    const sim::CreatureBlueprint humanoid = sim::CreatureBlueprint::humanoid();
    require(humanoid.valid(), "articulated humanoid is invalid");
    require(humanoid.active_motor_count == sim::anatomy_action_count,
        "humanoid does not expose all canonical anatomy motors");
    require(humanoid.nodes.size() == 19u && humanoid.bones.size() == 23u,
        "humanoid arm or canonical articulated-foot anatomy is missing");

    const std::array<std::pair<sim::CreatureBlueprint, std::size_t>, 4> motor_contracts{
        std::pair{ sim::CreatureBlueprint::humanoid(), sim::anatomy_action_count },
        std::pair{ sim::CreatureBlueprint::chicken(), 4u },
        std::pair{ sim::CreatureBlueprint::crawler4(), 8u },
        std::pair{ sim::CreatureBlueprint::hexapod(), 8u }
    };
    for (const auto& [rig, expected_motors] : motor_contracts)
    {
        require(rig.valid(), "built-in rig became invalid after policy expansion");
        require(rig.active_motor_count == expected_motors,
            "production rig does not expose its authored active motor count");
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
            static_cast<void>(neutral.step(zero));
            static_cast<void>(arms.step(arm_action));
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
        std::filesystem::temp_directory_path() / "runner-v070-runtime-test.eppo";
    test_replay_ownership();
    std::filesystem::remove(checkpoint);
    std::filesystem::remove(checkpoint.string() + ".tmp");

    {
        rl::AutonomousTrainer trainer{ humanoid, 16 };
        // One completed staged cycle proves the entire publication pipeline.
        // Requiring two four-update batches made this a build-mode speed test.
        trainer.set_updates_per_cycle(1);
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
            return trainer.metrics().update >= 1;
        }, 60s), "background staged pipeline completed no updates");

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

        trainer.set_blueprint(sim::CreatureBlueprint::crawler4(), false);
        trainer.set_blueprint(sim::CreatureBlueprint::hexapod(), false);
        trainer.set_blueprint(humanoid, false);
        require(wait_until([&]
        {
            trainer.synchronize();
            return trainer.rig_signature() == humanoid.signature();
        }, 15s), "rapid preset swaps were not coalesced and published");

        trainer.set_background_enabled(false);
    }

    {
        // Paused manual work is a command/state contract, not a throughput
        // benchmark. Isolate it from the hot 16-environment pipeline above and
        // prove repeated requests are consumed while background mode remains
        // disabled.
        rl::AutonomousTrainer paused{ humanoid, 4 };
        require(!paused.background_enabled(),
            "manual-update trainer did not start paused");
        const std::uint64_t starting_update = paused.metrics().update;
        paused.train_one_update();
        paused.train_one_update();
        require(wait_until([&]
        {
            paused.synchronize();
            return paused.metrics().update >= starting_update + 2u;
        }, 60s), "repeated paused update requests were not processed");
        require(!paused.background_enabled(),
            "manual updates silently enabled background training");
    }

    std::filesystem::remove(checkpoint);
    std::filesystem::remove(checkpoint.string() + ".tmp");

    std::cout << "Runner runtime pipeline, arms, and async persistence tests passed\n";
    return EXIT_SUCCESS;
}
