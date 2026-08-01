from __future__ import annotations

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


simulation_path = Path("src/simulation.cpp")
simulation = simulation_path.read_text(encoding="utf-8")
old_rotation = '''        const float driven_inverse_inertia = inverse_rotational_inertia(driven_component);
        const float reference_inverse_inertia = inverse_rotational_inertia(reference_component);
        const float total_inverse_inertia = driven_inverse_inertia + reference_inverse_inertia;
        float driven_rotation = -correction;
        float reference_rotation = 0.0f;
        if (total_inverse_inertia > 1.0e-8f)
        {
            driven_rotation = -correction * driven_inverse_inertia / total_inverse_inertia;
            reference_rotation = correction * reference_inverse_inertia / total_inverse_inertia;
        }

        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (driven_component[index])
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, driven_rotation);
            else if (reference_component[index])
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, reference_rotation);
        }
'''
new_rotation = '''        auto affected_center_of_mass = [&]() noexcept
        {
            double weighted_x = 0.0;
            double weighted_y = 0.0;
            double total_mass = 0.0;
            for (std::size_t index = 0; index < particles_.size(); ++index)
            {
                if (!driven_component[index] && !reference_component[index]
                    && index != motor.pivot)
                    continue;
                const Particle& particle = particles_[index];
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                weighted_x += static_cast<double>(particle.position.x) * mass;
                weighted_y += static_cast<double>(particle.position.y) * mass;
                total_mass += mass;
            }
            if (total_mass <= 1.0e-9)
                return pivot;
            return Vec2{
                static_cast<float>(weighted_x / total_mass),
                static_cast<float>(weighted_y / total_mass)
            };
        };

        const float driven_inverse_inertia = inverse_rotational_inertia(driven_component);
        const float reference_inverse_inertia = inverse_rotational_inertia(reference_component);
        const float total_inverse_inertia = driven_inverse_inertia + reference_inverse_inertia;
        float driven_rotation = -correction;
        float reference_rotation = 0.0f;
        if (total_inverse_inertia > 1.0e-8f)
        {
            driven_rotation = -correction * driven_inverse_inertia / total_inverse_inertia;
            reference_rotation = correction * reference_inverse_inertia / total_inverse_inertia;
        }

        const Vec2 center_before = affected_center_of_mass();
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (driven_component[index])
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, driven_rotation);
            else if (reference_component[index])
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, reference_rotation);
        }
        const Vec2 center_correction = center_before - affected_center_of_mass();
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (driven_component[index] || reference_component[index]
                || index == motor.pivot)
                particles_[index].position += center_correction;
        }
'''
simulation = replace_once(simulation, old_rotation, new_rotation, "motor rotation block")
simulation_path.write_text(simulation, encoding="utf-8")

header_path = Path("src/simulation.hpp")
header = header_path.read_text(encoding="utf-8")
header = replace_once(
    header,
    "    class Environment\n",
    "    struct EnvironmentTestAccess;\n\n    class Environment\n",
    "test access forward declaration",
)
header = replace_once(
    header,
    "    private:\n        void solve_distance",
    "    private:\n        friend struct EnvironmentTestAccess;\n\n        void solve_distance",
    "test access friendship",
)
header_path.write_text(header, encoding="utf-8")

tests_path = Path("tests/core_tests.cpp")
tests = tests_path.read_text(encoding="utf-8")
tests = replace_once(
    tests,
    "namespace\n{\n",
    '''namespace epochrunner::sim
{
    struct EnvironmentTestAccess
    {
        static void solve_motor(Environment& environment,
            const MotorConstraint& motor, float action) noexcept
        {
            environment.solve_motor(motor, action);
        }
    };
}

namespace
{
''',
    "test access definition",
)
old_test = '''    {
        sim::Environment neutral_humanoid{ humanoid, 0xC8357u };
        sim::Environment actuated_humanoid{ humanoid, 0xC8357u };
        neutral_humanoid.set_course(sim::CourseStage::balance, 0.25f);
        actuated_humanoid.set_course(sim::CourseStage::balance, 0.25f);
        const std::array<float, sim::action_count> neutral_actions{};
        std::array<float, sim::action_count> shoulder_actions{};
        shoulder_actions[4] = 1.0f;
        for (int frame = 0; frame < 6; ++frame)
        {
            neutral_humanoid.step(neutral_actions);
            actuated_humanoid.step(shoulder_actions);
        }
        const Vec2 chest_reaction =
            actuated_humanoid.particles()[humanoid.torso_node].position
            - neutral_humanoid.particles()[humanoid.torso_node].position;
        require(length(chest_reaction) > 1.0e-5f,
            "humanoid shoulder motor still pins its parent chest reference in world space");
    }

'''
new_test = '''    {
        sim::Environment motor_reaction{ humanoid, 0xC8357u };
        const sim::MotorConstraint& shoulder = humanoid.motors[4];
        const auto center_of_mass = [](std::span<const sim::Particle> particles)
        {
            double weighted_x = 0.0;
            double weighted_y = 0.0;
            double total_mass = 0.0;
            for (const sim::Particle& particle : particles)
            {
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                weighted_x += static_cast<double>(particle.position.x) * mass;
                weighted_y += static_cast<double>(particle.position.y) * mass;
                total_mass += mass;
            }
            return Vec2{
                static_cast<float>(weighted_x / total_mass),
                static_cast<float>(weighted_y / total_mass)
            };
        };
        const Vec2 chest_before = motor_reaction.particles()[humanoid.torso_node].position;
        const Vec2 pivot_before = motor_reaction.particles()[shoulder.pivot].position;
        const Vec2 driven_before = motor_reaction.particles()[shoulder.c].position;
        const Vec2 center_before = center_of_mass(motor_reaction.particles());

        sim::EnvironmentTestAccess::solve_motor(motor_reaction, shoulder, 1.0f);

        const Vec2 chest_delta =
            motor_reaction.particles()[humanoid.torso_node].position - chest_before;
        const Vec2 pivot_delta =
            motor_reaction.particles()[shoulder.pivot].position - pivot_before;
        const Vec2 driven_delta =
            motor_reaction.particles()[shoulder.c].position - driven_before;
        const Vec2 center_delta = center_of_mass(motor_reaction.particles()) - center_before;
        require(length(chest_delta) > 1.0e-6f,
            "humanoid shoulder motor still pins its parent chest reference in world space");
        require(length(pivot_delta) > 1.0e-6f,
            "humanoid shoulder pivot is still treated as a world-space anchor");
        require(length(driven_delta) > length(chest_delta),
            "massive parent body receives more shoulder correction than the driven arm");
        require(length(center_delta) < 1.0e-5f,
            "internal shoulder motor injects net center-of-mass translation");
    }

'''
tests = replace_once(tests, old_test, new_test, "direct parent reaction regression")
tests_path.write_text(tests, encoding="utf-8")
