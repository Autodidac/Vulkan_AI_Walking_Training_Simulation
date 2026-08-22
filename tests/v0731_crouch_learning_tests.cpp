#include "autonomy.hpp"
#include "ppo.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
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
        runner::sim::CrouchPostureEvidence peak_posture{};
        float maximum_pressure{};
        float minimum_clearance{ std::numeric_limits<float>::infinity() };
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

    CrouchReplay replay_teacher(const runner::sim::CreatureBlueprint& rig,
        std::uint64_t seed)
    {
        runner::sim::Environment environment{ rig, seed };
        environment.set_course(runner::sim::CourseStage::duck_press, 0.30f);
        environment.set_course_motion_enabled(false);
        runner::sim::CrouchPostureEvidence peak_posture{};
        float maximum_pressure{};
        float minimum_clearance = std::numeric_limits<float>::infinity();
        for (int step = 0; step < 1200; ++step)
        {
            const auto action = runner::rl::effective_policy_action(
                environment, {}, runner::sim::CourseStage::duck_press, 1.0f);
            if (environment.step(action).terminated)
                break;
            const auto posture = environment.current_crouch_posture();
            if (posture.pelvis_drop > peak_posture.pelvis_drop)
                peak_posture = posture;
            maximum_pressure = std::max(maximum_pressure,
                environment.duck_obstacle_weight());
            minimum_clearance = std::min(minimum_clearance,
                environment.duck_clearance_margin());
        }
        return {
            runner::rl::stage_motion_qualification(
                runner::sim::CourseStage::duck_press, environment).valid,
            environment.elapsed_seconds(), environment.duck_seconds(),
            environment.longest_valid_crouch_seconds(),
            environment.duck_recoveries(), environment.invalid_reason(),
            peak_posture, maximum_pressure, minimum_clearance };
    }
}

int main()
{
    const runner::sim::CreatureBlueprint rig =
        runner::sim::CreatureBlueprint::humanoid();
    const CrouchReplay teacher = replay_teacher(rig, 0x731u);
    std::cout << "Human crouch teacher: valid=" << teacher.qualified
        << " survival=" << teacher.survival
        << " duck=" << teacher.duck_seconds
        << " longest=" << teacher.longest_crouch
        << " recoveries=" << teacher.recoveries
        << " reason=" << runner::sim::invalid_motion_name(teacher.reason)
        << " peak_drop=" << teacher.peak_posture.pelvis_drop
        << " knees=" << teacher.peak_posture.left_knee_flex
        << '/' << teacher.peak_posture.right_knee_flex
        << " torso=" << teacher.peak_posture.torso_pitch
        << " support=" << teacher.peak_posture.support_margin
        << " feet=" << teacher.peak_posture.feet_supported
        << " non_foot=" << teacher.peak_posture.non_foot_grounded
        << " pressure=" << teacher.maximum_pressure
        << " clearance=" << teacher.minimum_clearance << '\n';
    require(teacher.qualified && teacher.recoveries >= 1u
            && teacher.reason == runner::sim::InvalidMotion::none,
        "authored Human crouch teacher cannot demonstrate crouch, hold, and recovery");

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
    const auto& crouch_metrics = trainer.metrics();
    std::cout << "Crouch diagnostic: lesson=" << trainer.lesson_update()
        << " best_update=" << crouch_metrics.best_update
        << " eval_valid=" << crouch_metrics.evaluation_valid
        << " eval_invalid_runs=" << crouch_metrics.evaluation_invalid_runs
        << " rejection=" << crouch_metrics.evaluation_rejection_mask
        << " reason=" << static_cast<int>(crouch_metrics.evaluation_invalid_reason)
        << " recoveries=" << crouch_metrics.evaluation_duck_recoveries
        << " duck=" << crouch_metrics.evaluation_duck_seconds
        << " stance=" << crouch_metrics.evaluation_stable_stance
        << " longest=" << crouch_metrics.evaluation_longest_stance
        << " survival=" << crouch_metrics.evaluation_survival
        << " quality=" << crouch_metrics.evaluation_quality_key
        << '\n';
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