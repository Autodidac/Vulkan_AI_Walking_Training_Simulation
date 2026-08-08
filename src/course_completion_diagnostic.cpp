#include "course_completion_diagnostic.hpp"

#include "ppo.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

namespace runner::diagnostics
{
    namespace
    {
        [[nodiscard]] bool close(float lhs, float rhs,
            float tolerance = 1.0e-5f) noexcept
        {
            return std::abs(lhs - rhs) <= tolerance;
        }

        [[nodiscard]] bool same_course_preview(
            const rl::PpoTrainer& lhs, const rl::PpoTrainer& rhs) noexcept
        {
            const sim::Environment& a = lhs.preview();
            const sim::Environment& b = rhs.preview();
            if (lhs.preview_reset_count() != rhs.preview_reset_count()
                || lhs.preview_last_reset_reason() != rhs.preview_last_reset_reason()
                || a.course_stage() != b.course_stage()
                || !close(a.course_difficulty(), b.course_difficulty())
                || a.invalid_reason() != b.invalid_reason()
                || a.material_event_count() != b.material_event_count()
                || a.gait_cycles() != b.gait_cycles()
                || a.hand_ledge_contacts() != b.hand_ledge_contacts()
                || a.climb_support_transfers() != b.climb_support_transfers()
                || a.ledge_climbs() != b.ledge_climbs()
                || a.controlled_descents() != b.controlled_descents()
                || a.shots_fired() != b.shots_fired()
                || a.target_hits() != b.target_hits()
                || a.equipment_transitions() != b.equipment_transitions()
                || a.equipment_state() != b.equipment_state()
                || a.weapon_class() != b.weapon_class()
                || a.particles().size() != b.particles().size()
                || a.material_particles().size() != b.material_particles().size()
                || a.course_features().size() != b.course_features().size()
                || a.equipment_projectiles().size() != b.equipment_projectiles().size()
                || !close(a.elapsed_seconds(), b.elapsed_seconds(), 1.0e-6f)
                || !close(a.distance_travelled(), b.distance_travelled())
                || !close(a.water_depth(), b.water_depth())
                || !close(a.water_submersion(), b.water_submersion())
                || !close(a.burial_depth(), b.burial_depth()))
                return false;
            for (std::size_t index = 0; index < a.particles().size(); ++index)
            {
                const sim::Particle& left = a.particles()[index];
                const sim::Particle& right = b.particles()[index];
                if (!close(left.position.x, right.position.x)
                    || !close(left.position.y, right.position.y)
                    || !close(left.previous.x, right.previous.x)
                    || !close(left.previous.y, right.previous.y)
                    || left.grounded != right.grounded)
                    return false;
            }
            for (std::size_t index = 0; index < a.material_particles().size(); ++index)
            {
                const sim::MaterialParticle& left = a.material_particles()[index];
                const sim::MaterialParticle& right = b.material_particles()[index];
                if (left.kind != right.kind || left.active != right.active
                    || !close(left.position.x, right.position.x)
                    || !close(left.position.y, right.position.y)
                    || !close(left.velocity.x, right.velocity.x)
                    || !close(left.velocity.y, right.velocity.y))
                    return false;
            }
            for (std::size_t index = 0; index < a.course_features().size(); ++index)
            {
                const sim::CourseFeature& left = a.course_features()[index];
                const sim::CourseFeature& right = b.course_features()[index];
                if (left.kind != right.kind
                    || left.marker_sequence != right.marker_sequence
                    || !close(left.center.x, right.center.x)
                    || !close(left.center.y, right.center.y)
                    || !close(left.velocity.x, right.velocity.x)
                    || !close(left.velocity.y, right.velocity.y))
                    return false;
            }
            const auto& terrain_a = a.terrain().cells();
            const auto& terrain_b = b.terrain().cells();
            if (terrain_a.size() != terrain_b.size())
                return false;
            for (std::size_t index = 0; index < terrain_a.size(); ++index)
            {
                if (terrain_a[index].region != terrain_b[index].region
                    || terrain_a[index].surface_material
                        != terrain_b[index].surface_material
                    || !close(terrain_a[index].height, terrain_b[index].height)
                    || !close(terrain_a[index].firmness, terrain_b[index].firmness)
                    || !close(terrain_a[index].water_depth,
                        terrain_b[index].water_depth))
                    return false;
            }
            for (std::size_t index = 0; index < a.equipment_projectiles().size(); ++index)
            {
                const sim::EquipmentProjectile& left = a.equipment_projectiles()[index];
                const sim::EquipmentProjectile& right = b.equipment_projectiles()[index];
                if (left.active != right.active
                    || !close(left.position.x, right.position.x)
                    || !close(left.position.y, right.position.y))
                    return false;
            }
            const sim::EquipmentTarget& target_a = a.equipment_target();
            const sim::EquipmentTarget& target_b = b.equipment_target();
            if (target_a.active != target_b.active
                || target_a.sequence != target_b.sequence
                || !close(target_a.position.x, target_b.position.x)
                || !close(target_a.position.y, target_b.position.y))
                return false;
            const auto observation_a = a.observation();
            const auto observation_b = b.observation();
            for (std::size_t index = 0; index < observation_a.size(); ++index)
                if (!close(observation_a[index], observation_b[index]))
                    return false;
            return true;
        }

        void advance(rl::PpoTrainer& trainer, int frames, float dt)
        {
            for (int frame = 0; frame < frames; ++frame)
                trainer.step_preview(dt);
        }
    }

    CourseCompletionReport run_course_completion_diagnostic()
    {
        CourseCompletionReport report{};
        std::array<bool, 5> seen_regions{};
        bool water_seen = false;
        bool hole_seen = false;
        bool every_seed_complete = true;
        bool varied = false;
        std::array<float, sim::DeformableTerrain::cell_count> baseline{};

        for (std::uint64_t seed = 0x728100u; seed < 0x728108u; ++seed)
        {
            sim::DeformableTerrain terrain{};
            terrain.reset(seed, 0.65f);
            std::array<bool, 5> local_regions{};
            for (std::size_t column = 0; column < sim::DeformableTerrain::cell_count; ++column)
            {
                const float x = (static_cast<float>(column) + 0.5f)
                    * sim::DeformableTerrain::fine_cell_spacing;
                const sim::TerrainRegion region = terrain.region_at(x);
                local_regions[static_cast<std::size_t>(region)] = true;
                seen_regions[static_cast<std::size_t>(region)] = true;
                water_seen = water_seen || terrain.water_depth_at(x) > 0.05f;
                if (region == sim::TerrainRegion::hole)
                {
                    hole_seen = hole_seen
                        || terrain.height_at(x) < terrain.height_at(x - 2.0f) - 0.25f;
                }
                if (seed == 0x728100u)
                    baseline[column] = terrain.height_at(x);
                else if (std::abs(baseline[column] - terrain.height_at(x)) > 0.015f)
                    varied = true;
            }
            every_seed_complete = every_seed_complete
                && std::ranges::all_of(local_regions, [](bool value) { return value; });
        }
        report.material_regions = every_seed_complete
            && std::ranges::all_of(seen_regions, [](bool value) { return value; });
        report.seed_variation = varied;
        report.water_and_holes = water_seen && hole_seen;

        sim::Environment runway{ sim::CreatureBlueprint::humanoid(), 0x728200u };
        runway.set_course(sim::CourseStage::moving_hazards, 0.70f);
        std::set<int> markers{};
        bool protected_distance = true;
        for (const sim::CourseFeature& feature : runway.course_features())
        {
            protected_distance = protected_distance && feature.center.x >= 32.0f;
            protected_distance = protected_distance
                && markers.insert(feature.marker_sequence).second;
        }
        const std::array<float, sim::action_count> idle{};
        for (int frame = 0; frame < 600; ++frame)
            static_cast<void>(runway.step(idle));
        report.safe_runway = protected_distance
            && runway.material_event_count() == 0u;
        report.delayed_material_pressure = runway.material_particles().empty();

        sim::Environment observed{ sim::CreatureBlueprint::humanoid(), 0x728300u };
        observed.set_course(sim::CourseStage::uneven, 0.65f);
        const auto observation = observed.observation();
        report.observation_truth = observation.size() == sim::observation_count
            && std::ranges::all_of(observation, [](float value) { return std::isfinite(value); })
            && observation[52] >= 0.0f && observation[52] <= 1.0f;

        sim::Environment climb{ sim::CreatureBlueprint::humanoid(), 0x728400u };
        climb.set_course(sim::CourseStage::climb_descent, 0.65f);
        const auto ledge = std::ranges::find_if(climb.course_features(),
            [](const sim::CourseFeature& feature)
            {
                return feature.kind == sim::CourseFeatureKind::ledge;
            });
        const rl::StageMotionQualification empty_climb =
            rl::stage_motion_qualification(sim::CourseStage::climb_descent, climb);
        report.climb_contract = ledge != climb.course_features().end()
            && ledge->center.x - ledge->half_extent.x >= 10.0f
            && climb.course_speed() == 0.0f
            && !empty_climb.valid
            && (empty_climb.rejection_mask
                & rl::evidence_bit(rl::MotionEvidenceFailure::missing_skill)) != 0u;

        sim::Environment equipment{ sim::CreatureBlueprint::humanoid(), 0x728500u };
        equipment.set_course(sim::CourseStage::equipment_targets, 0.55f);
        equipment.configure_equipment(sim::WeaponClass::sidearm, 6.0f);
        for (int frame = 0; frame < 240 && equipment.target_hits() == 0u; ++frame)
        {
            const auto action = rl::effective_policy_action(equipment,
                std::array<float, sim::action_count>{},
                sim::CourseStage::equipment_targets);
            static_cast<void>(equipment.step(action));
        }
        report.equipment_contract = equipment.weapon_class() == sim::WeaponClass::sidearm
            && equipment.equipment_state() == sim::EquipmentState::ready
            && equipment.equipment_transitions() >= 1u
            && equipment.shots_fired() >= 1u
            && equipment.target_hits() >= 1u;

        sim::Environment off_a{ sim::CreatureBlueprint::humanoid(), 0x728600u };
        sim::Environment off_b{ sim::CreatureBlueprint::humanoid(), 0x728600u };
        off_a.set_course(sim::CourseStage::uneven, 0.55f);
        off_b.set_course(sim::CourseStage::uneven, 0.55f);
        bool identical = true;
        for (int frame = 0; frame < 120; ++frame)
        {
            std::array<float, sim::action_count> a{};
            std::array<float, sim::action_count> b{};
            for (std::size_t slot = 0; slot < sim::anatomy_action_count; ++slot)
                a[slot] = b[slot] = ((frame + static_cast<int>(slot)) & 1) == 0 ? 0.12f : -0.12f;
            b[sim::equipment_state_action] = 1.0f;
            b[sim::equipment_aim_action] = -1.0f;
            b[sim::equipment_trigger_action] = 1.0f;
            static_cast<void>(off_a.step(a));
            static_cast<void>(off_b.step(b));
        }
        const auto particles_a = off_a.particles();
        const auto particles_b = off_b.particles();
        identical = identical && particles_a.size() == particles_b.size();
        for (std::size_t index = 0; identical && index < particles_a.size(); ++index)
        {
            identical = particles_a[index].position.x == particles_b[index].position.x
                && particles_a[index].position.y == particles_b[index].position.y
                && particles_a[index].previous.x == particles_b[index].previous.x
                && particles_a[index].previous.y == particles_b[index].previous.y;
        }
        report.equipment_off_identity = identical
            && off_a.weapon_class() == sim::WeaponClass::none
            && off_b.weapon_class() == sim::WeaponClass::none;

        const sim::CreatureBlueprint rig = sim::CreatureBlueprint::humanoid();
        constexpr std::array stages{
            sim::CourseStage::uneven,
            sim::CourseStage::moving_hazards,
            sim::CourseStage::climb_descent,
            sim::CourseStage::equipment_targets,
            sim::CourseStage::combat_course
        };
        bool every_stage_frame_independent = true;
        for (const sim::CourseStage stage : stages)
        {
            rl::PpoTrainer at_60{ rig, 8u, false };
            rl::PpoTrainer at_20{ rig, 8u, false };
            rl::PpoTrainer at_240{ rig, 8u, false };
            const std::uint64_t seed = 0x728700u
                + static_cast<std::uint64_t>(stage) * 97u;
            for (rl::PpoTrainer* trainer : { &at_60, &at_20, &at_240 })
            {
                trainer->set_course(stage, 0.65f, false);
                trainer->reset_preview(seed);
            }
            advance(at_60, 120, 1.0f / 60.0f);
            advance(at_20, 40, 1.0f / 20.0f);
            advance(at_240, 480, 1.0f / 240.0f);
            every_stage_frame_independent = every_stage_frame_independent
                && same_course_preview(at_60, at_20)
                && same_course_preview(at_60, at_240);
        }
        report.frame_independent = every_stage_frame_independent;
        return report;
    }
}