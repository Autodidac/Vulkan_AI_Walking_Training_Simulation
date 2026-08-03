#include "acceptance.hpp"

#include "ppo.hpp"
#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace runner::acceptance
{
    namespace
    {
        using sim::CreatureBlueprint;
        using sim::Environment;

        struct NamedBlueprint
        {
            std::string_view name{};
            CreatureBlueprint blueprint{};
        };

        [[nodiscard]] std::array<NamedBlueprint, 7> all_presets()
        {
            return {
                NamedBlueprint{ "chicken", CreatureBlueprint::chicken() },
                NamedBlueprint{ "biped", CreatureBlueprint::biped() },
                NamedBlueprint{ "humanoid", CreatureBlueprint::humanoid() },
                NamedBlueprint{ "quadruped", CreatureBlueprint::quadruped() },
                NamedBlueprint{ "crawler4", CreatureBlueprint::crawler4() },
                NamedBlueprint{ "hexapod", CreatureBlueprint::hexapod() },
                NamedBlueprint{ "monoped", CreatureBlueprint::monoped() }
            };
        }

        void add_case(Report& report, std::string name, bool passed, std::string detail)
        {
            report.cases.push_back({ std::move(name), passed, std::move(detail) });
        }

        [[nodiscard]] bool finite_environment(const Environment& environment) noexcept
        {
            const bool particles_finite = std::ranges::all_of(environment.particles(),
                [](const sim::Particle& particle)
                {
                    return std::isfinite(particle.position.x)
                        && std::isfinite(particle.position.y)
                        && std::isfinite(particle.previous.x)
                        && std::isfinite(particle.previous.y)
                        && std::isfinite(particle.inverse_mass)
                        && std::isfinite(particle.radius)
                        && particle.radius > 0.0f;
                });
            if (!particles_finite)
                return false;
            const auto observation = environment.observation();
            return std::ranges::all_of(observation,
                [](float value) { return std::isfinite(value); });
        }

        [[nodiscard]] float minimum_support_clearance(const CreatureBlueprint& blueprint) noexcept
        {
            float minimum = std::numeric_limits<float>::max();
            bool compared = false;
            for (std::size_t first = 0; first < blueprint.nodes.size(); ++first)
            {
                if (!blueprint.is_support_seed(first))
                    continue;
                for (std::size_t second = first + 1; second < blueprint.nodes.size(); ++second)
                {
                    if (!blueprint.is_support_seed(second))
                        continue;
                    const auto first_index = static_cast<std::uint16_t>(first);
                    const auto second_index = static_cast<std::uint16_t>(second);
                    const bool same_plate = std::ranges::any_of(blueprint.bones,
                        [first_index, second_index](const sim::DistanceConstraint& bone)
                        {
                            return (bone.a == first_index && bone.b == second_index)
                                || (bone.a == second_index && bone.b == first_index);
                        });
                    if (same_plate)
                        continue;
                    const float first_radius = first < blueprint.radii.size()
                        ? blueprint.radii[first] : 0.12f;
                    const float second_radius = second < blueprint.radii.size()
                        ? blueprint.radii[second] : 0.12f;
                    const float delta_x = blueprint.nodes[second].x - blueprint.nodes[first].x;
                    const float delta_y = blueprint.nodes[second].y - blueprint.nodes[first].y;
                    const float clearance = std::hypot(delta_x, delta_y)
                        - first_radius - second_radius;
                    minimum = std::min(minimum, clearance);
                    compared = true;
                }
            }
            return compared ? minimum : 0.0f;
        }

        struct BalanceGateResult
        {
            std::uint32_t accepted{};
            std::uint32_t total{};
            float worst_spin{};
            float shortest_stance{ std::numeric_limits<float>::max() };
        };

        [[nodiscard]] BalanceGateResult strict_balance_gate(
            const CreatureBlueprint& blueprint, std::uint64_t seed_base)
        {
            BalanceGateResult result{};
            result.total = 6u;
            const std::array<float, sim::action_count> raw_action{};
            for (std::uint32_t seed_index = 0; seed_index < result.total; ++seed_index)
            {
                const std::uint64_t seed = seed_base
                    + static_cast<std::uint64_t>(seed_index) * 4099u;
                Environment environment{ blueprint, seed };
                environment.set_course(sim::CourseStage::balance, 0.25f);
                for (int frame = 0; frame < 1200; ++frame)
                {
                    const auto action = rl::effective_policy_action(
                        environment, raw_action, sim::CourseStage::balance);
                    const sim::StepResult step = environment.step(action);
                    if (environment.valid_motion()
                        && environment.longest_stable_stance_seconds()
                            >= rl::standing_mastery_seconds)
                        break;
                    if (step.terminated)
                        break;
                }
                const rl::StageMotionQualification qualification =
                    rl::stage_motion_qualification(sim::CourseStage::balance, environment);
                const bool accepted = qualification.valid
                    && environment.body_integrity_valid()
                    && environment.longest_stable_stance_seconds()
                        >= rl::standing_mastery_seconds
                    && environment.uncontrolled_spin_turns() <= 0.55f;
                result.accepted += accepted ? 1u : 0u;
                result.worst_spin = std::max(result.worst_spin,
                    environment.uncontrolled_spin_turns());
                result.shortest_stance = std::min(result.shortest_stance,
                    environment.longest_stable_stance_seconds());
            }
            return result;
        }

        [[nodiscard]] std::string balance_detail(const BalanceGateResult& result)
        {
            std::ostringstream stream{};
            stream << result.accepted << '/' << result.total
                << " seeds, shortest_stance=" << result.shortest_stance
                << ", worst_spin=" << result.worst_spin;
            return stream.str();
        }
    }

    bool Report::passed() const noexcept
    {
        return !cases.empty() && std::ranges::all_of(cases,
            [](const CaseResult& result) { return result.passed; });
    }

    std::size_t Report::passed_count() const noexcept
    {
        return static_cast<std::size_t>(std::ranges::count_if(cases,
            [](const CaseResult& result) { return result.passed; }));
    }

    Report run_live_acceptance_matrix()
    {
        Report report{};
        const auto presets = all_presets();

        bool blueprints_valid = true;
        std::string invalid_blueprints{};
        for (const NamedBlueprint& preset : presets)
        {
            if (preset.blueprint.valid())
                continue;
            blueprints_valid = false;
            if (!invalid_blueprints.empty())
                invalid_blueprints += ", ";
            invalid_blueprints += preset.name;
        }
        add_case(report, "preset-blueprint-integrity", blueprints_valid,
            blueprints_valid ? "all seven authored presets are valid"
                             : "invalid presets: " + invalid_blueprints);

        bool finite_soak = true;
        std::string failed_soak{};
        const std::array<float, sim::action_count> raw_action{};
        for (std::size_t index = 0; index < presets.size(); ++index)
        {
            Environment environment{ presets[index].blueprint,
                0xA110000u + static_cast<std::uint64_t>(index) * 8191u };
            environment.set_course(sim::CourseStage::balance, 0.25f);
            for (int frame = 0; frame < 240; ++frame)
            {
                const auto action = rl::effective_policy_action(
                    environment, raw_action, sim::CourseStage::balance);
                const sim::StepResult step = environment.step(action);
                if (!finite_environment(environment))
                {
                    finite_soak = false;
                    failed_soak = std::string{ presets[index].name };
                    break;
                }
                if (step.terminated)
                    break;
            }
            if (!finite_soak)
                break;
        }
        add_case(report, "preset-live-physics-soak", finite_soak,
            finite_soak ? "all presets remained finite through deterministic live stepping"
                        : "non-finite runtime state in " + failed_soak);

        bool supports_separate = true;
        float tightest_clearance = std::numeric_limits<float>::max();
        std::string fused_preset{};
        for (const NamedBlueprint& preset : presets)
        {
            const float clearance = minimum_support_clearance(preset.blueprint);
            tightest_clearance = std::min(tightest_clearance, clearance);
            if (clearance > -0.005f)
                continue;
            supports_separate = false;
            fused_preset = std::string{ preset.name };
            break;
        }
        std::ostringstream support_detail{};
        support_detail << "tightest authored semantic-support clearance="
            << tightest_clearance;
        if (!supports_separate)
            support_detail << " in " << fused_preset;
        add_case(report, "semantic-support-separation", supports_separate,
            support_detail.str());

        const BalanceGateResult humanoid_gate = strict_balance_gate(
            CreatureBlueprint::humanoid(), 0xE000u);
        add_case(report, "humanoid-strict-six-seed-balance",
            humanoid_gate.accepted == humanoid_gate.total,
            balance_detail(humanoid_gate));

        const BalanceGateResult chicken_gate = strict_balance_gate(
            CreatureBlueprint::chicken(), 0xC11C000u);
        add_case(report, "chicken-strict-six-seed-balance",
            chicken_gate.accepted == chicken_gate.total,
            balance_detail(chicken_gate));

        const CreatureBlueprint humanoid = CreatureBlueprint::humanoid();
        const bool shoulder_geometry = humanoid.nodes.size() > 10u
            && humanoid.torso_node < humanoid.nodes.size()
            && humanoid.nodes[humanoid.torso_node].y > humanoid.nodes[7].y
            && humanoid.nodes[humanoid.torso_node].y > humanoid.nodes[10].y;
        add_case(report, "humanoid-raised-central-shoulder", shoulder_geometry,
            shoulder_geometry
                ? "central semantic torso/shoulder pivot is above both lateral shoulders"
                : "central shoulder pivot is not above both lateral shoulders");

        Environment duck_environment{ humanoid, 0xD0C700u };
        duck_environment.set_course(sim::CourseStage::duck_press, 0.35f);
        const auto duck_action = rl::duck_teacher_action(duck_environment);
        float arm_energy = 0.0f;
        float leg_energy = 0.0f;
        for (std::size_t index = 0; index < 4u; ++index)
            leg_energy += std::abs(duck_action[index]);
        for (std::size_t index = 4u; index < humanoid.active_motor_count; ++index)
            arm_energy += std::abs(duck_action[index]);
        const bool duck_legs_only = arm_energy < 0.0001f && leg_energy > 0.0f;
        std::ostringstream duck_detail{};
        duck_detail << "leg_energy=" << leg_energy << ", arm_energy=" << arm_energy;
        add_case(report, "duck-lesson-legs-only-authority", duck_legs_only,
            duck_detail.str());

        Environment pip_environment{ humanoid, 0x710000u };
        const bool pip_visible = rl::training_preview_frame_renderable(pip_environment)
            && rl::training_preview_priority(
                sim::CourseStage::balance, pip_environment) > 0;
        add_case(report, "training-pip-current-frame-fallback", pip_visible,
            pip_visible
                ? "finite current attempts remain renderable even before qualification"
                : "a finite current training attempt can disappear from the PIP");

        const CreatureBlueprint monoped = CreatureBlueprint::monoped();
        const bool monoped_distinct = monoped.monopedal_gait()
            && !humanoid.monopedal_gait()
            && monoped.valid();
        add_case(report, "monoped-single-leg-gait-identity", monoped_distinct,
            monoped_distinct
                ? "monoped uses its dedicated single-leg gait path"
                : "monoped is still forced through alternating biped semantics");

        const bool curriculum_order = sim::course_stage_count == 8u
            && sim::course_stage_name(sim::CourseStage::balance) == "1. STAND"
            && sim::course_stage_name(sim::CourseStage::duck_press)
                == "2. STATIC CROUCH / HOLD / RECOVER"
            && sim::course_stage_name(sim::CourseStage::uneven) == "3. WALK / RUN"
            && sim::course_stage_name(sim::CourseStage::crouch_walk)
                == "4. CROUCH WALK / UNEVEN AVOID"
            && sim::course_stage_name(sim::CourseStage::ramps) == "5. JUMP / LAND"
            && sim::course_stage_name(sim::CourseStage::hurdles)
                == "6. MOVING LOW BAR / HURDLE"
            && sim::course_stage_name(sim::CourseStage::duck_bars)
                == "7. CONTROLLED FLIPS"
            && sim::course_stage_name(sim::CourseStage::moving_hazards)
                == "8. MIXED GOAL COURSE"
            && sim::stage_skill_evidence(sim::CourseStage::balance,
                0u, 0.0f, 0u, 0.0f, 0u, 0u)
            && !sim::stage_skill_evidence(sim::CourseStage::uneven,
                0u, 0.0f, 0u, 0.0f, 0u, 0u)
            && sim::stage_skill_evidence(sim::CourseStage::uneven,
                4u, 0.0f, 0u, 0.0f, 0u, 0u)
            && sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
                5u, 3.0f, 0u, 0.0f, 0u, 4u);
        add_case(report, "ordered-stage-evidence-matrix", curriculum_order,
            curriculum_order
                ? "stand, crouch, gait, crouch-walk, jump, hurdle, flip, mixed order is enforced"
                : "stage order or qualification evidence regressed");

        return report;
    }
}
