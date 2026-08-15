#include "autonomy.hpp"
#include "ppo.hpp"
#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{
    namespace rl = runner::rl;
    namespace sim = runner::sim;

    void require(bool value, std::string_view message)
    {
        if (value)
            return;
        std::cerr << "Runner v0.7.34 curriculum-rig failure: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    bool cumulative_metrics_equal(const rl::TrainingMetrics& left,
        const rl::TrainingMetrics& right) noexcept
    {
        return left.total_updates == right.total_updates
            && left.total_environment_steps == right.total_environment_steps
            && left.total_episodes == right.total_episodes
            && left.total_valid_episodes == right.total_valid_episodes
            && left.total_invalid_episodes == right.total_invalid_episodes
            && left.total_resets == right.total_resets
            && left.total_alternating_steps == right.total_alternating_steps
            && left.total_falls == right.total_falls
            && left.total_collisions == right.total_collisions
            && left.total_powered_jumps == right.total_powered_jumps
            && left.total_landed_jumps == right.total_landed_jumps
            && left.total_landed_flips == right.total_landed_flips
            && left.total_obstacles_passed == right.total_obstacles_passed
            && left.total_distance == right.total_distance
            && left.total_training_seconds == right.total_training_seconds
            && left.evaluation_count == right.evaluation_count;
    }
}

int main()
{
    const sim::CreatureBlueprint humanoid = sim::CreatureBlueprint::humanoid();
    constexpr std::uint64_t crouch_updates = rl::crouch_teacher_handoff_update;
    constexpr std::uint64_t crouch_episodes = 4u;
    constexpr std::uint64_t mastery_tests = 8u;

    require(!rl::rig_optimization_ready(sim::CourseStage::balance,
            10'000u, 10'000u, 10'000u, humanoid, 0),
        "Stand admitted an automatic rig candidate");
    require(!rl::rig_optimization_ready(sim::CourseStage::duck_press,
            crouch_updates - 1u, crouch_episodes, mastery_tests, humanoid, 0)
        && rl::rig_optimization_ready(sim::CourseStage::duck_press,
            crouch_updates, crouch_episodes, mastery_tests, humanoid, 0),
        "Crouch candidate gate does not honor the exact raw-policy boundary");
    require(!rl::rig_optimization_ready(sim::CourseStage::duck_press,
            crouch_updates, crouch_episodes, mastery_tests, humanoid, 1),
        "rig candidate interrupted an in-progress mastery confirmation streak");

    require(!rl::rig_optimization_ready(sim::CourseStage::duck_press,
            crouch_updates, crouch_episodes - 1u, mastery_tests, humanoid, 0)
        && !rl::rig_optimization_ready(sim::CourseStage::duck_press,
            crouch_updates, crouch_episodes, mastery_tests - 1u, humanoid, 0),
        "Crouch candidate gate ignored episode or mastery-test readiness");

    const std::uint64_t walk_handoff =
        rl::foundational_walk_teacher_handoff_update(humanoid);
    require(!rl::rig_optimization_ready(sim::CourseStage::uneven,
            walk_handoff - 1u, 8u, mastery_tests, humanoid, 0)
        && rl::rig_optimization_ready(sim::CourseStage::uneven,
            walk_handoff, 8u, mastery_tests, humanoid, 0),
        "Walk candidate gate does not honor topology-scoped teacher handoff");
    for (const int cadence : std::array{ 20, 60, 240 })
    {
        (void)cadence;
        require(rl::rig_optimization_ready(sim::CourseStage::duck_press,
                crouch_updates, crouch_episodes, mastery_tests, humanoid, 0),
            "render cadence changed curriculum-safe rig readiness");
    }

    rl::PpoTrainer champion{ humanoid, 4u, false };
    rl::PpoTrainer::CheckpointData source = champion.checkpoint_data();
    source.optimizer_step = 575u;
    source.lesson_update = crouch_updates;
    source.stage = sim::CourseStage::duck_press;
    source.difficulty = 0.30f;
    source.metrics.update = 575u;
    source.metrics.environment_steps = 147'200u;
    source.metrics.total_updates = 8'000u;
    source.metrics.total_environment_steps = 2'048'000u;
    source.metrics.total_episodes = 7'500u;
    source.metrics.total_valid_episodes = 7'100u;
    source.metrics.total_invalid_episodes = 400u;
    source.metrics.total_resets = 900u;
    source.metrics.total_alternating_steps = 12'345u;
    source.metrics.total_falls = 222u;
    source.metrics.total_collisions = 111u;
    source.metrics.total_powered_jumps = 73u;
    source.metrics.total_landed_jumps = 68u;
    source.metrics.total_landed_flips = 9u;
    source.metrics.total_obstacles_passed = 321u;
    source.metrics.total_distance = 12'345.75;
    source.metrics.total_training_seconds = 6'789.50;
    source.metrics.evaluation_count = 1'100u;
    source.metrics.evaluation_valid = true;
    source.metrics.evaluation_quality_key = 42u;
    source.metrics.evaluation_distance = 9.0f;
    source.metrics.best_evaluation_distance = 9.0f;
    source.metrics.best_evaluation_score = 18.0f;
    source.metrics.best_quality_key = 42u;
    source.metrics.best_update = 550u;
    source.best_parameters = source.parameters;
    std::fill(source.first_moment.begin(), source.first_moment.end(), 1.0f);
    std::fill(source.second_moment.begin(), source.second_moment.end(), 1.0f);

    sim::CreatureBlueprint candidate = humanoid;
    candidate.motors[0].strength = std::max(0.01f,
        candidate.motors[0].strength * 0.99f);
    require(candidate.valid() && candidate.signature() != humanoid.signature(),
        "control candidate fixture did not change the rig signature");

    const rl::PpoTrainer::CheckpointData retargeted =
        rl::PpoTrainer::retarget_checkpoint_for_rig(source,
            candidate.signature());
    require(retargeted.rig_signature == candidate.signature()
        && retargeted.parameters == source.parameters
        && retargeted.first_moment == source.first_moment
        && retargeted.second_moment == source.second_moment
        && retargeted.optimizer_step == source.optimizer_step
        && retargeted.lesson_update == source.lesson_update
        && retargeted.stage == source.stage
        && retargeted.difficulty == source.difficulty,
        "candidate retarget discarded compatible controller, optimizer, or lesson state");
    require(cumulative_metrics_equal(retargeted.metrics, source.metrics),
        "candidate retarget replaced the all-time ledger");
    require(retargeted.best_parameters.empty()
        && retargeted.metrics.best_quality_key == 0u
        && retargeted.metrics.best_update == 0u
        && !retargeted.metrics.evaluation_valid
        && retargeted.metrics.evaluation_quality_key == 0u,
        "candidate retarget retained evidence measured on the previous physical rig");

    rl::PpoTrainer nursery{ candidate, 4u, false };
    std::string error{};
    require(nursery.apply_checkpoint_data(retargeted, error, false),
        "retargeted candidate checkpoint did not resume: " + error);
    require(nursery.lesson_update() == crouch_updates
        && nursery.optimizer_step() == source.optimizer_step
        && nursery.course_stage() == sim::CourseStage::duck_press
        && cumulative_metrics_equal(nursery.metrics(), source.metrics),
        "candidate resume reset Crouch age, optimizer, or cumulative metrics");

    nursery.neutralize_action_slot(0u);
    const rl::PpoTrainer::CheckpointData neutralized = nursery.checkpoint_data();
    const auto first_zero_count = std::count(
        neutralized.first_moment.begin(), neutralized.first_moment.end(), 0.0f);
    const auto second_zero_count = std::count(
        neutralized.second_moment.begin(), neutralized.second_moment.end(), 0.0f);
    require(first_zero_count
            == static_cast<std::ptrdiff_t>(rl::PolicyNetwork::hidden_size + 2u)
        && second_zero_count
            == static_cast<std::ptrdiff_t>(rl::PolicyNetwork::hidden_size + 2u),
        "new motor neutralization retained stale actor or exploration moments");

    const rl::TrainingMetrics before_adaptation = nursery.metrics();
    nursery.train_one_update();
    require(nursery.lesson_update() == crouch_updates + 1u
        && nursery.optimizer_step() > source.optimizer_step
        && nursery.metrics().total_updates
            == before_adaptation.total_updates + 1u
        && nursery.metrics().total_environment_steps
            >= before_adaptation.total_environment_steps
        && nursery.metrics().total_episodes >= before_adaptation.total_episodes
        && nursery.metrics().total_resets >= before_adaptation.total_resets
        && nursery.metrics().total_distance >= before_adaptation.total_distance
        && nursery.metrics().total_training_seconds
            >= before_adaptation.total_training_seconds,
        "bounded candidate adaptation was not monotonic from the main ledger");

    const std::filesystem::path checkpoint =
        std::filesystem::temp_directory_path()
        / "runner-v0734-curriculum-rig-roundtrip.eppo";
    require(rl::PpoTrainer::write_checkpoint_data(
            nursery.checkpoint_data(), checkpoint, error),
        "could not write candidate checkpoint: " + error);
    rl::PpoTrainer resumed{ candidate, 4u, false };
    require(resumed.load_checkpoint(checkpoint, error, false),
        "could not resume candidate checkpoint: " + error);
    require(resumed.lesson_update() == nursery.lesson_update()
        && resumed.optimizer_step() == nursery.optimizer_step()
        && cumulative_metrics_equal(resumed.metrics(), nursery.metrics()),
        "candidate checkpoint round-trip lost lesson or lifetime ownership");
    std::error_code filesystem_error{};
    std::filesystem::remove(checkpoint, filesystem_error);

    std::cout << "Runner v0.7.34 curriculum-safe rig optimization passed\n";
    return EXIT_SUCCESS;
}