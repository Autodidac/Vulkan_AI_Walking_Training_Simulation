#include "autonomy.hpp"
#include "ppo.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner v0.7.31 crouch-learning failure: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    struct CrouchReplay
    {
        bool qualified{};
        float survival{};
        float duck_seconds{};
        float longest_crouch{};
        std::uint32_t recoveries{};
        runner::sim::InvalidMotion reason{ runner::sim::InvalidMotion::none };
    };

    CrouchReplay replay(const runner::sim::CreatureBlueprint& rig,
        const runner::rl::PolicyNetwork& policy, std::uint64_t seed)
    {
        runner::sim::Environment environment{ rig, seed };
        environment.set_course(runner::sim::CourseStage::duck_press, 0.30f);
        environment.set_course_motion_enabled(false);
        for (int step = 0; step < 1200; ++step)
        {
            const auto action = policy.deterministic_action(environment.observation());
            if (environment.step(action).terminated)
                break;
        }
        return {
            runner::rl::stage_motion_qualification(
                runner::sim::CourseStage::duck_press, environment).valid,
            environment.elapsed_seconds(), environment.duck_seconds(),
            environment.longest_valid_crouch_seconds(),
            environment.duck_recoveries(), environment.invalid_reason() };
    }
}

int main()
{
    const runner::sim::CreatureBlueprint rig =
        runner::sim::CreatureBlueprint::humanoid();
    runner::rl::PpoTrainer trainer{ rig, 64u };
    trainer.set_cpu_mode(4);
    trainer.set_course(runner::sim::CourseStage::balance, 0.25f, false);
    constexpr std::uint64_t stand_updates = 80u;
    for (std::uint64_t update = 0; update < stand_updates; ++update)
        trainer.train_one_update();
    require(trainer.lesson_update() == stand_updates,
        "Stand did not own its lesson-local update clock");

    const std::uint64_t rig_updates_before_crouch = trainer.metrics().total_updates;
    trainer.set_course(runner::sim::CourseStage::duck_press, 0.30f, false);
    require(trainer.lesson_update() == 0u
            && trainer.metrics().total_updates == rig_updates_before_crouch,
        "normal Stand-to-Crouch entry reset rig work or retained lesson age");

    constexpr std::uint64_t crouch_updates = 320u;
    for (std::uint64_t update = 0; update < crouch_updates; ++update)
        trainer.train_one_update();
    require(trainer.lesson_update() == crouch_updates
            && runner::rl::lesson_teacher_authority(
                trainer.lesson_update(), trainer.course_stage(), rig) == 0.0f,
        "bounded Crouch run did not reach exact zero teacher authority");
    require(trainer.has_best_policy()
            && trainer.metrics().best_update
                >= stand_updates + runner::rl::crouch_teacher_handoff_update,
        "no post-handoff raw crouch controller was retained");
    require(trainer.restore_best_policy(),
        "post-handoff crouch controller could not be restored for replay");

    constexpr std::array seeds{
        std::uint64_t{ 0x7310u }, std::uint64_t{ 0x7311u },
        std::uint64_t{ 0xE000u }, std::uint64_t{ 0xE000u + 4099u },
        std::uint64_t{ 0xE000u + 2u * 4099u },
        std::uint64_t{ 0xE000u + 3u * 4099u } };
    float total_duck_seconds{};
    float total_survival{};
    std::uint32_t total_recoveries{};
    for (const std::uint64_t seed : seeds)
    {
        const CrouchReplay result = replay(rig, trainer.policy(), seed);
        std::cout << "crouch seed=" << seed
            << " valid=" << result.qualified
            << " survival=" << result.survival
            << " duck=" << result.duck_seconds
            << " longest=" << result.longest_crouch
            << " recoveries=" << result.recoveries
            << " reason=" << runner::sim::invalid_motion_name(result.reason) << '\n';
        require(result.qualified && result.reason == runner::sim::InvalidMotion::none,
            "zero-authority retained crouch failed a deterministic replay seed");
        total_duck_seconds += result.duck_seconds;
        total_survival += result.survival;
        total_recoveries += result.recoveries;
    }
    const float inverse_seed_count = 1.0f / static_cast<float>(seeds.size());
    require(total_duck_seconds * inverse_seed_count >= 1.25f
            && total_survival * inverse_seed_count >= 9.0f
            && total_recoveries >= seeds.size(),
        "raw retained Crouch missed depth, hold, balance, or recovery mastery");

    const std::uint64_t rig_updates_before_walk = trainer.metrics().total_updates;
    trainer.set_course(runner::sim::CourseStage::uneven, 0.30f, false);
    require(trainer.lesson_update() == 0u
            && trainer.metrics().total_updates == rig_updates_before_walk
            && trainer.blueprint().signature() == rig.signature(),
        "Crouch-to-Walk required a rig switch or erased rig-scoped work");
    trainer.step_preview(1.0f / 20.0f);
    require(trainer.lesson_update() == 0u,
        "render cadence advanced the policy lesson clock");

    std::cout << "Runner v0.7.31 cold raw Crouch ownership passed\n";
    return EXIT_SUCCESS;
}