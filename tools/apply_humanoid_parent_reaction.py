from __future__ import annotations

from pathlib import Path
import re


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


simulation_path = Path("src/simulation.cpp")
simulation = simulation_path.read_text(encoding="utf-8")
solve_motor_pattern = re.compile(
    r"    void Environment::solve_motor\(const MotorConstraint& motor, float action\) noexcept\n"
    r"    \{.*?\n    \}\n\n"
    r"(?=    bool Environment::contact_cluster_contains)",
    re.DOTALL,
)
solve_motor_replacement = r'''    void Environment::solve_motor(const MotorConstraint& motor, float action) noexcept
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

        std::array<bool, 128> driven_component{};
        std::array<bool, 128> reference_component{};
        auto collect_component = [&](std::uint16_t start, std::uint16_t blocked_a,
            std::uint16_t blocked_b, std::array<bool, 128>& component,
            const std::array<bool, 128>* excluded) noexcept
        {
            std::array<bool, 128> visited{};
            std::array<std::uint16_t, 128> stack{};
            std::size_t stack_size = 0;
            visited[blocked_a] = true;
            visited[blocked_b] = true;
            if (excluded != nullptr)
            {
                for (std::size_t index = 0; index < particles_.size(); ++index)
                    visited[index] = (*excluded)[index];
                visited[blocked_a] = true;
                visited[blocked_b] = true;
            }
            if (visited[start])
                return;
            visited[start] = true;
            component[start] = true;
            stack[stack_size++] = start;
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
                        component[next] = true;
                        stack[stack_size++] = next;
                    }
                }
            }
        };

        collect_component(motor.c, motor.pivot, motor.a, driven_component, nullptr);
        collect_component(motor.a, motor.pivot, motor.c, reference_component, &driven_component);

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

        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (driven_component[index])
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, driven_rotation);
            else if (reference_component[index])
                particles_[index].position = pivot
                    + rotate(particles_[index].position - pivot, reference_rotation);
        }
    }

'''
simulation, count = solve_motor_pattern.subn(solve_motor_replacement, simulation, count=1)
if count != 1:
    raise RuntimeError(f"expected one solve_motor replacement, got {count}")
simulation_path.write_text(simulation, encoding="utf-8")

core_tests_path = Path("tests/core_tests.cpp")
core_tests = core_tests_path.read_text(encoding="utf-8")
reaction_test = r'''
    {
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
core_tests = replace_once(
    core_tests,
    "    const sim::CreatureBlueprint quadruped = sim::CreatureBlueprint::quadruped();\n",
    reaction_test + "    const sim::CreatureBlueprint quadruped = sim::CreatureBlueprint::quadruped();\n",
    "humanoid regression test",
)
core_tests_path.write_text(core_tests, encoding="utf-8")

cmake_path = Path("CMakeLists.txt")
cmake = cmake_path.read_text(encoding="utf-8")
cmake = replace_once(
    cmake,
    "project(EpochRunner VERSION 0.7.0 LANGUAGES CXX)",
    "project(EpochRunner VERSION 0.7.1 LANGUAGES CXX)",
    "project version",
)
cmake_path.write_text(cmake, encoding="utf-8")

app_path = Path("src/app.cpp")
app = app_path.read_text(encoding="utf-8")
for old, new in (
    ("epochrunner-v070-autosave.eppo", "epochrunner-v071-autosave.eppo"),
    ("epochrunner-v070-evolved.epochrig", "epochrunner-v071-evolved.epochrig"),
    ("epochrunner-v070-autonomy.state", "epochrunner-v071-autonomy.state"),
):
    app = replace_once(app, old, new, old)
app_path.write_text(app, encoding="utf-8")

mission_path = Path("missioncache.md")
mission = mission_path.read_text(encoding="utf-8")
mission = replace_once(
    mission,
    "**Target:** EpochRunner v0.7.0",
    "**Target:** EpochRunner v0.7.1",
    "release target",
)
mission = replace_once(
    mission,
    "**Release state:** VERIFIED — EpochRunner v0.7.0 published",
    "**Release state:** VALIDATION IN PROGRESS — EpochRunner v0.7.1 parent-reaction hotfix",
    "release state",
)
mission_entry = '''## Active runtime regression\n\n### WALK-MOTOR-012 — Parent-side motor reaction\n**Status:** PARTIAL\n\nHumanoid shoulder and elbow motors must not treat the shared chest/reference side as a world-space pin. Each angular correction is split across the driven and reference components using their rotational inertia: the lighter limb receives most of the motion and the connected body receives a smaller counter-reaction. Deterministic tests must prove shoulder input changes the chest trajectory relative to an identical neutral simulation. Runtime acceptance remains open until the packaged humanoid visibly translates and rotates as one body without a fixed chest node. Physics-semantic changes use a fresh v0.7.1 autosave namespace so v0.7.0 learned state is not resumed silently.\n\n'''
mission = replace_once(
    mission,
    "## UI and release evidence\n",
    mission_entry + "## UI and release evidence\n",
    "active regression section",
)
mission_path.write_text(mission, encoding="utf-8")

Path("RELEASE_NOTES_v0.7.1.md").write_text(
    "# EpochRunner v0.7.1\n\n"
    "- Fixes humanoid arm motors treating the shared chest/reference side as an immovable parent.\n"
    "- Splits motor correction across driven and parent components using rotational inertia.\n"
    "- Adds a deterministic chest-reaction regression test.\n"
    "- Starts a fresh v0.7.1 autosave namespace because motor dynamics changed.\n"
    "- Keeps runtime visual acceptance open in missioncache.md until the packaged humanoid is confirmed.\n",
    encoding="utf-8",
)
