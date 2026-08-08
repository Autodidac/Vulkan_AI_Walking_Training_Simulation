#include "course_completion_diagnostic.hpp"
#include "ppo.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static bool force_material_after_runway(Environment& environment) noexcept
        {
            environment.distance_travelled_ = 20.0f;
            environment.alternating_steps_ = 2u;
            environment.elapsed_seconds_ = environment.next_material_event_seconds_;
            environment.update_materials(1.0f / 60.0f);
            return environment.material_event_count() > 0u
                && !environment.material_particles().empty();
        }

        static bool force_climb_and_descent(Environment& environment) noexcept
        {
            const auto ledge = std::ranges::find_if(environment.course_features_,
                [](const CourseFeature& feature)
                {
                    return feature.kind == CourseFeatureKind::ledge;
                });
            if (ledge == environment.course_features_.end())
                return false;
            const float left = ledge->center.x - ledge->half_extent.x;
            const float top = ledge->center.y + ledge->half_extent.y;
            const std::uint16_t hand = environment.blueprint_.motors[7].c;
            const std::uint16_t foot = environment.blueprint_.left_contact_node;
            const std::uint16_t root = environment.blueprint_.root_node;
            if (!environment.valid_node(hand) || !environment.valid_node(foot)
                || !environment.valid_node(root))
                return false;

            environment.particles_[hand].position = { left, top };
            environment.particles_[hand].previous = environment.particles_[hand].position;
            environment.solve_course();
            environment.particles_[foot].position = {
                left + 0.40f, top + environment.particles_[foot].radius };
            environment.particles_[foot].previous = environment.particles_[foot].position;
            environment.particles_[foot].grounded = true;
            environment.particles_[root].position = { left + 0.55f, top + 0.30f };
            environment.previous_root_height_ = environment.particles_[root].position.y;
            environment.update_climb_metrics(1.0f / 60.0f);
            if (environment.ledge_climbs_ != 1u
                || environment.climb_support_transfers_ != 1u)
            {
                std::cerr << "climb ascent evidence: contacts=" << environment.hand_ledge_contacts_
                    << " climbs=" << environment.ledge_climbs_
                    << " transfers=" << environment.climb_support_transfers_ << std::endl;
                return false;
            }

            environment.particles_[hand].position = { left, top };
            environment.particles_[hand].previous = environment.particles_[hand].position;
            environment.solve_course();
            environment.particles_[root].position.x = left + 0.40f;
            environment.previous_root_height_ = environment.particles_[root].position.y + 0.001f;
            environment.update_climb_metrics(1.0f / 60.0f);
            if (environment.climb_support_transfers_ != 2u)
            {
                std::cerr << "climb regrasp evidence: contacts=" << environment.hand_ledge_contacts_
                    << " transfers=" << environment.climb_support_transfers_ << std::endl;
                return false;
            }

            environment.particles_[foot].position = {
                left - 0.40f, environment.particles_[foot].radius };
            environment.particles_[foot].previous = environment.particles_[foot].position;
            environment.particles_[foot].grounded = true;
            environment.particles_[root].position = { left - 0.20f, top + 0.40f };
            environment.previous_root_height_ = environment.particles_[root].position.y + 0.01f;
            environment.update_climb_metrics(1.0f / 60.0f);
            const bool valid_descent = environment.controlled_descents_ == 1u
                && environment.powered_jump_count_ == 0u;
            if (!valid_descent)
                std::cerr << "climb descent evidence: descents=" << environment.controlled_descents_
                    << " jumps=" << environment.powered_jump_count_ << std::endl;
            return valid_descent;
        }
    };
}

int main()
{
    const auto report = runner::diagnostics::run_course_completion_diagnostic();
    const auto repeated = runner::diagnostics::run_course_completion_diagnostic();
    const bool fields[] = { report.safe_runway, report.material_regions,
        report.seed_variation, report.water_and_holes, report.observation_truth,
        report.delayed_material_pressure, report.climb_contract,
        report.equipment_contract, report.equipment_off_identity,
        report.frame_independent };
    const auto names = runner::diagnostics::course_completion_case_names();
    for (std::size_t index = 0; index < names.size(); ++index)
        std::cout << names[index] << '=' << (fields[index] ? "passed" : "failed") << '\n';

    runner::sim::Environment material{ runner::sim::CreatureBlueprint::humanoid(), 0x728800u };
    material.set_course(runner::sim::CourseStage::moving_hazards, 0.75f);
    const bool positive_material = runner::sim::EnvironmentTestAccess::force_material_after_runway(material);
    runner::sim::Environment climb{ runner::sim::CreatureBlueprint::humanoid(), 0x728900u };
    climb.set_course(runner::sim::CourseStage::climb_descent, 0.65f);
    const bool physical_climb = runner::sim::EnvironmentTestAccess::force_climb_and_descent(climb);
    std::cout << "repeated-determinism=" << (repeated.passed() ? "passed" : "failed") << std::endl;
    std::cout << "material-positive-control=" << (positive_material ? "passed" : "failed") << std::endl;
    std::cout << "climb-positive-control=" << (physical_climb ? "passed" : "failed") << std::endl;
    if (!report.passed() || !repeated.passed() || !positive_material || !physical_climb)
    {
        std::cerr << "Runner v0.7.28 course completion diagnostic failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Runner v0.7.28 course completion diagnostic passed\n";
    return EXIT_SUCCESS;
}