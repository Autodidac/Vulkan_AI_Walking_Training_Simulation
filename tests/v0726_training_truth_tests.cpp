#include "autonomy.hpp"
#include "ppo.hpp"
#include "simulation.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
namespace {
int fail(const char* message) { std::cerr << "v0.7.26 training-truth failure: " << message << '\n'; return EXIT_FAILURE; }
bool all_support_motors(const runner::sim::CreatureBlueprint& rig) {
    for (std::size_t i = 0; i < rig.active_motor_count; ++i)
        if (!runner::rl::motor_drives_support_branch(rig, rig.motors[i])) return false;
    return true;
}}
int main() {
    using namespace runner;
    const auto humanoid = sim::CreatureBlueprint::humanoid();
    const auto quadruped = sim::CreatureBlueprint::quadruped();
    const auto crawler = sim::CreatureBlueprint::crawler4();
    const auto hexapod = sim::CreatureBlueprint::hexapod();
    for (std::size_t i = 0; i < 4u; ++i) if (!rl::motor_drives_support_branch(humanoid, humanoid.motors[i])) return fail("humanoid leg was not support");
    for (std::size_t i = 4u; i < humanoid.active_motor_count; ++i) if (rl::motor_drives_support_branch(humanoid, humanoid.motors[i])) return fail("humanoid arm was support");
    if (!all_support_motors(quadruped)) return fail("quadruped support classification");
    if (!all_support_motors(crawler)) return fail("crawler support classification");
    if (!all_support_motors(hexapod)) return fail("hexapod support classification");
    sim::Environment quad{quadruped, 0x726u}; quad.set_course(sim::CourseStage::duck_press, 0.25f);
    std::array<float, sim::action_count> full{}; full.fill(1.0f);
    const auto duck = rl::effective_policy_action(quad, full, sim::CourseStage::duck_press);
    if (std::abs(duck[4]) < 0.02f || std::abs(duck[6]) < 0.02f) return fail("quadruped front legs still zeroed");
    sim::Environment preview{quadruped, 0x727u}; preview.set_course(sim::CourseStage::uneven, 0.30f);
    if (preview.course_speed() <= 0.0f) return fail("training course motion missing");
    preview.set_course_motion_enabled(false);
    if (preview.course_speed() != 0.0f || preview.course_progress() != 0.0f) return fail("preview conveyor still moving");
    rl::PpoTrainer trainer{humanoid, 8u, false}; trainer.train_one_update();
    if (trainer.metrics().total_updates == 0u) return fail("no setup update");
    trainer.reset_policy(0x1234u);
    if (trainer.metrics().total_updates == 0u) return fail("same-rig retry erased totals");
    trainer.set_blueprint(quadruped, false);
    if (trainer.metrics().total_updates != 0u || trainer.metrics().total_episodes != 0u || trainer.metrics().evaluation_count != 0u) return fail("rig switch retained totals");
    rl::AutonomousTrainer autonomous{quadruped, 8u}; autonomous.synchronize();
    if (autonomous.preview().course_motion_enabled()) return fail("large preview conveyor enabled");
    std::cout << "Runner v0.7.26 training-truth checks passed\n";
    return EXIT_SUCCESS;
}
