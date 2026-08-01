from __future__ import annotations

import argparse
from pathlib import Path
import re


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


def apply() -> None:
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
            if (excluded != nullptr)
            {
                for (std::size_t index = 0; index < particles_.size(); ++index)
                    visited[index] = (*excluded)[index];
            }
            visited[blocked_a] = true;
            visited[blocked_b] = true;
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

        auto affected_center_of_mass = [&]() noexcept
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
    }

'''
    simulation, count = solve_motor_pattern.subn(solve_motor_replacement, simulation, count=1)
    if count != 1:
        raise RuntimeError(f"expected one solve_motor replacement, got {count}")
    simulation_path.write_text(simulation, encoding="utf-8")

    header_path = Path("src/simulation.hpp")
    header = header_path.read_text(encoding="utf-8")
    header = replace_once(
        header,
        "    class Environment\n",
        "    struct EnvironmentTestAccess;\n\n    class Environment\n",
        "test access declaration",
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
    tests = replace_once(tests, "#include <array>\n", "#include <algorithm>\n#include <array>\n", "algorithm include")
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
    reaction_test = r'''
    {
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
        require(length(chest_delta) > 1.0e-7f,
            "humanoid shoulder motor still pins its parent chest reference in world space");
        require(length(pivot_delta) > 1.0e-7f,
            "humanoid shoulder pivot is still treated as a world-space anchor");
        require(length(driven_delta) > length(chest_delta),
            "massive parent body receives more shoulder correction than the driven arm");
        require(length(center_delta) < 2.0e-5f,
            "internal shoulder motor injects net center-of-mass translation");
    }

'''
    tests = replace_once(
        tests,
        "    const sim::CreatureBlueprint quadruped = sim::CreatureBlueprint::quadruped();\n",
        reaction_test + "    const sim::CreatureBlueprint quadruped = sim::CreatureBlueprint::quadruped();\n",
        "parent reaction regression",
    )
    tests_path.write_text(tests, encoding="utf-8")

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
    mission = replace_once(mission, "**Target:** EpochRunner v0.7.0", "**Target:** EpochRunner v0.7.1", "target")
    mission = replace_once(
        mission,
        "**Release state:** VERIFIED — EpochRunner v0.7.0 published",
        "**Release state:** VALIDATION IN PROGRESS — EpochRunner v0.7.1",
        "release state",
    )
    mission_entry = '''## Active runtime regression

### WALK-MOTOR-012 — Reciprocal parent-side motor reaction
**Status:** IMPLEMENTED

Humanoid shoulder and elbow motors no longer treat the shared chest/reference side or joint pivot as a world-space pin. Motor corrections are split between driven and reference components according to rotational inertia, so the lighter limb receives most of the angular motion while the connected body receives the physically required counter-reaction. A center-of-mass correction removes numerical translation introduced by the finite constraint rotation. Direct solver tests require chest motion, pivot motion, driven-side dominance, and whole-body center-of-mass preservation.

The motor-physics change uses a fresh v0.7.1 autosave namespace so incompatible v0.7.0 learned state is not silently resumed.

'''
    mission = replace_once(mission, "## UI and release evidence\n", mission_entry + "## UI and release evidence\n", "mission section")
    mission_path.write_text(mission, encoding="utf-8")

    Path("RELEASE_NOTES_v0.7.1.md").write_text(
        "# EpochRunner v0.7.1\n\n"
        "- Fixes humanoid shoulder and elbow motors pinning their parent chest/reference side.\n"
        "- Adds reciprocal, rotational-inertia-weighted parent/body reaction.\n"
        "- Preserves whole-body center of mass during internal motor correction.\n"
        "- Adds direct solver regression coverage for chest motion, pivot motion, and driven-side dominance.\n"
        "- Starts a fresh v0.7.1 autosave namespace because motor dynamics changed.\n",
        encoding="utf-8",
    )


def validate(run_id: str) -> None:
    mission_path = Path("missioncache.md")
    mission = mission_path.read_text(encoding="utf-8")
    mission = replace_once(
        mission,
        "**Release state:** VALIDATION IN PROGRESS — EpochRunner v0.7.1",
        "**Release state:** VALIDATED — EpochRunner v0.7.1 publication pending",
        "validated release state",
    )
    mission = replace_once(
        mission,
        "### WALK-MOTOR-012 — Reciprocal parent-side motor reaction\n**Status:** IMPLEMENTED",
        "### WALK-MOTOR-012 — Reciprocal parent-side motor reaction\n**Status:** VERIFIED",
        "verified motor mission",
    )
    mission_path.write_text(mission, encoding="utf-8")
    Path("validation/v0.7.1-prepublish.md").write_text(
        "# EpochRunner v0.7.1 prepublication validation\n\n"
        f"- Workflow run: `{run_id}`\n"
        "- Linux GCC 14 C++23 core build and deterministic/runtime tests: passed.\n"
        "- Windows 2025 full SDL3/Vulkan/EpochGui Release build: passed.\n"
        "- Windows deterministic, runtime-pipeline, concurrency, persistence, arm, and UI tests: passed.\n"
        "- Executable version and Vulkan diagnostic: passed.\n"
        "- Direct motor solver regression: chest reacts, pivot reacts, lighter arm moves more, center of mass is preserved.\n",
        encoding="utf-8",
    )


def finalize(source_sha: str, run_id: str, archive: str, checksum: str) -> None:
    mission_path = Path("missioncache.md")
    mission = mission_path.read_text(encoding="utf-8")
    mission = replace_once(
        mission,
        "**Release state:** VALIDATED — EpochRunner v0.7.1 publication pending",
        "**Release state:** VERIFIED — EpochRunner v0.7.1 published",
        "published release state",
    )
    release_entry = f'''### WALK-REL-013 — Verified v0.7.1 parent-reaction hotfix
**Status:** VERIFIED

- Exact tested source commit: `{source_sha}`;
- workflow run: `{run_id}`;
- Linux GCC 14 C++23 build and tests: passed;
- Windows 2025 full SDL3/Vulkan/EpochGui build and tests: passed;
- executable version and Vulkan diagnostic: passed;
- Windows package: `{archive}`;
- package SHA-256: `{checksum}`;
- repository state after publication: only `main`, zero open pull requests.

'''
    mission = replace_once(mission, "### WALK-REL-011 — Verified v0.7.0 release\n", release_entry + "### WALK-REL-011 — Verified v0.7.0 release\n", "v0.7.1 release evidence")
    mission_path.write_text(mission, encoding="utf-8")
    Path("validation/v0.7.1.md").write_text(
        "# EpochRunner v0.7.1 release evidence\n\n"
        f"- Exact tested source commit: `{source_sha}`\n"
        f"- Workflow run: `{run_id}`\n"
        "- Linux GCC 14 C++23 build and tests: passed\n"
        "- Windows 2025 full application build and all tests: passed\n"
        "- Vulkan diagnostic: passed\n"
        f"- Package: `{archive}`\n"
        f"- Package SHA-256: `{checksum}`\n"
        "- Remaining branches: `main`\n"
        "- Open pull requests: `0`\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("apply", "validate", "finalize"))
    parser.add_argument("--run-id", default="")
    parser.add_argument("--source-sha", default="")
    parser.add_argument("--archive", default="")
    parser.add_argument("--checksum", default="")
    args = parser.parse_args()
    if args.mode == "apply":
        apply()
    elif args.mode == "validate":
        if not args.run_id:
            raise RuntimeError("validate requires --run-id")
        validate(args.run_id)
    else:
        if not all((args.source_sha, args.run_id, args.archive, args.checksum)):
            raise RuntimeError("finalize requires source SHA, run ID, archive, and checksum")
        finalize(args.source_sha, args.run_id, args.archive, args.checksum)


if __name__ == "__main__":
    main()
