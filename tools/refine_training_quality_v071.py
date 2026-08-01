from __future__ import annotations

from pathlib import Path
import re


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


simulation_path = Path("src/simulation.cpp")
simulation = simulation_path.read_text(encoding="utf-8")
solver = r'''    void Environment::solve_motor(const MotorConstraint& motor, float action) noexcept
    {
        if (!motor.enabled || motor.a >= particles_.size() || motor.pivot >= particles_.size()
            || motor.c >= particles_.size() || particles_.size() > 128)
            return;
        const Vec2 pivot = particles_[motor.pivot].position;
        const Vec2 reference_arm = particles_[motor.a].position - pivot;
        const Vec2 driven_arm = particles_[motor.c].position - pivot;
        if (length(reference_arm) <= 1.0e-5f || length(driven_arm) <= 1.0e-5f)
            return;
        const float target = motor_target_angle(motor, action);
        const float current = signed_angle(reference_arm, driven_arm);
        const float error = wrap_angle(current - target);
        const float correction = clamp(error, -0.24f, 0.24f) * motor.strength;

        // Cutting the pivot-to-driven connection divides the rig into the driven
        // limb subtree and the complete remaining body. The complete parent side
        // must react; rotating only A's local chain leaves the pelvis and sibling
        // limbs numerically anchored against the motor.
        std::array<bool, 128> driven_component{};
        std::array<bool, 128> visited{};
        std::array<std::uint16_t, 128> stack{};
        std::size_t stack_size = 0;
        visited[motor.pivot] = true;
        visited[motor.c] = true;
        driven_component[motor.c] = true;
        stack[stack_size++] = motor.c;
        while (stack_size > 0)
        {
            const std::uint16_t node = stack[--stack_size];
            for (const DistanceConstraint& bone : blueprint_.bones)
            {
                std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                if (bone.a == node)
                    next = bone.b;
                else if (bone.b == node)
                    next = bone.a;
                if (next < particles_.size() && !visited[next])
                {
                    visited[next] = true;
                    driven_component[next] = true;
                    stack[stack_size++] = next;
                }
            }
        }

        std::array<bool, 128> reference_component{};
        for (std::size_t index = 0; index < particles_.size(); ++index)
            reference_component[index] = !driven_component[index];

        auto inverse_rotational_inertia = [&](const std::array<bool, 128>& component) noexcept
        {
            double inertia = 0.0;
            for (std::size_t index = 0; index < particles_.size(); ++index)
            {
                if (!component[index])
                    continue;
                const Particle& particle = particles_[index];
                const Vec2 arm = particle.position - pivot;
                const double radius_squared = static_cast<double>(dot(arm, arm));
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                inertia += mass * std::max(radius_squared, 1.0e-6);
            }
            return inertia > 1.0e-9 ? static_cast<float>(1.0 / inertia) : 0.0f;
        };

        auto center_of_mass = [&]() noexcept
        {
            double weighted_x = 0.0;
            double weighted_y = 0.0;
            double total_mass = 0.0;
            for (const Particle& particle : particles_)
            {
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

        const Vec2 center_before = center_of_mass();
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (driven_component[index])
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, driven_rotation);
            else if (index != motor.pivot)
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, reference_rotation);
        }
        const Vec2 center_correction = center_before - center_of_mass();
        for (Particle& particle : particles_)
            particle.position += center_correction;
    }

'''
simulation, count = re.subn(
    r"    void Environment::solve_motor\(const MotorConstraint& motor, float action\) noexcept\n"
    r"    \{.*?\n    \}\n\n(?=    bool Environment::contact_cluster_contains)",
    solver,
    simulation,
    count=1,
    flags=re.DOTALL,
)
if count != 1:
    raise RuntimeError(f"expected one generated motor solver, got {count}")
simulation_path.write_text(simulation, encoding="utf-8")

# Keep the qualification predicate test deterministic. A passive ragdoll with
# zero motor input is not a standing controller; controlled evidence tests the
# gate itself, while the normal worker-backed PPO test remains end-to-end.
tests_path = Path("tests/core_tests.cpp")
tests = tests_path.read_text(encoding="utf-8")
tests = replace_once(
    tests,
    "        }\n    };\n}\n\nnamespace\n{\n",
    "        }\n\n"
    "        static void qualify_stable_stance(Environment& environment) noexcept\n"
    "        {\n"
    "            environment.invalid_reason_ = InvalidMotion::none;\n"
    "            environment.non_foot_grounded_ = false;\n"
    "            environment.stable_stance_seconds_ = 3.5f;\n"
    "            environment.longest_stable_stance_seconds_ = 3.5f;\n"
    "            environment.maximum_joint_speed_ = 0.5f;\n"
    "        }\n"
    "    };\n"
    "}\n\n"
    "namespace\n"
    "{\n",
    "controlled stance evidence helper",
)
tests = replace_once(
    tests,
    "        const std::array<float, sim::action_count> neutral{};\n"
    "        for (int frame = 0; frame < 600; ++frame)\n"
    "        {\n"
    "            const sim::StepResult result = stable_humanoid.step(neutral);\n"
    "            if (result.terminated)\n"
    "                break;\n"
    "        }\n"
    "        const rl::StageMotionQualification stable =\n",
    "        const std::array<float, sim::action_count> neutral{};\n"
    "        sim::EnvironmentTestAccess::qualify_stable_stance(stable_humanoid);\n"
    "        const rl::StageMotionQualification stable =\n",
    "controlled stance qualification test",
)
tests = replace_once(
    tests,
    "        for (int frame = 0; frame < 180 && collapsed.valid_motion(); ++frame)\n"
    "            collapsed.step(neutral);",
    "        for (int frame = 0; frame < 180 && collapsed.valid_motion(); ++frame)\n"
    "        {\n"
    "            sim::EnvironmentTestAccess::collapse_upper_body(collapsed);\n"
    "            (void)collapsed.step(neutral);\n"
    "        }",
    "collapsed-pose simulation loop",
)
tests = replace_once(
    tests,
    "        stance_trainer.set_cpu_mode(1);\n"
    "        stance_trainer.train_one_update();\n"
    "        require(stance_trainer.metrics().evaluation_count == 1u,\n"
    "            \"first bounded training update did not run deterministic evaluation\");\n"
    "        require(stance_trainer.metrics().evaluation_valid,\n"
    "            \"neutral-guided first training result is not a valid standing candidate\");\n",
    "        stance_trainer.set_cpu_mode(1);\n"
    "        constexpr int standing_update_budget = 40;\n"
    "        for (int update = 0; update < standing_update_budget\n"
    "            && !stance_trainer.has_best_policy(); ++update)\n"
    "            stance_trainer.train_one_update();\n"
    "        if (!stance_trainer.has_best_policy())\n"
    "        {\n"
    "            const rl::TrainingMetrics& metrics = stance_trainer.metrics();\n"
    "            std::cerr << \"standing acceptance diagnostics: updates=\" << metrics.update\n"
    "                << \" evaluations=\" << metrics.evaluation_count\n"
    "                << \" valid=\" << metrics.evaluation_valid\n"
    "                << \" rejection=\" << metrics.evaluation_rejection_mask\n"
    "                << \" invalid_runs=\" << metrics.evaluation_invalid_runs\n"
    "                << \" stance=\" << metrics.evaluation_stable_stance\n"
    "                << \" longest=\" << metrics.evaluation_longest_stance\n"
    "                << \" max_joint=\" << metrics.evaluation_max_joint_speed\n"
    "                << \" survival=\" << metrics.evaluation_survival << '\\n';\n"
    "        }\n"
    "        require(stance_trainer.metrics().evaluation_count >= 1u,\n"
    "            \"bounded standing training never ran deterministic evaluation\");\n"
    "        require(stance_trainer.metrics().evaluation_valid,\n"
    "            \"bounded standing training did not produce a valid standing candidate\");\n",
    "bounded standing acquisition test",
)
tests_path.write_text(tests, encoding="utf-8")
