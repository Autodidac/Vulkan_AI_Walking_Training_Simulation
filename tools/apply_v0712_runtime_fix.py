from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


def remove_between(text: str, beginning: str, ending: str, label: str) -> str:
    start = text.find(beginning)
    if start < 0:
        raise RuntimeError(f"missing beginning of {label}")
    finish = text.find(ending, start)
    if finish < 0:
        raise RuntimeError(f"missing ending of {label}")
    return text[:start] + text[finish:]


def patch_versions() -> None:
    text = read("CMakeLists.txt")
    text = replace_once(text,
        "project(Runner VERSION 0.7.11 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.12 LANGUAGES CXX)",
        "project version")
    write("CMakeLists.txt", text)

    text = read("tools/repository_audit.cmake")
    text = text.replace(
        "project(Runner VERSION 0.7.11 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.12 LANGUAGES CXX)")
    text = text.replace(
        "CMake project version is not 0.7.11",
        "CMake project version is not 0.7.12")
    write("tools/repository_audit.cmake", text)


def patch_mastery() -> None:
    text = read("src/autonomy.hpp")
    text = replace_once(text,
        "    inline constexpr int balance_mastery_lock_confirmations = 3;\n"
        "    inline constexpr float standing_mastery_joint_speed_limit = 10.0f;",
        "    inline constexpr int balance_mastery_lock_confirmations = 3;\n"
        "    inline constexpr std::uint32_t balance_mastery_invalid_seed_limit = 1u;\n"
        "    inline constexpr float standing_mastery_joint_speed_limit = 10.0f;",
        "balance seed tolerance")
    text = replace_once(text,
        "            && metrics.evaluation_invalid_runs == 0u\n"
        "            && metrics.evaluation_longest_stance >= standing_mastery_seconds",
        "            && metrics.evaluation_invalid_runs <= balance_mastery_invalid_seed_limit\n"
        "            && metrics.evaluation_longest_stance >= standing_mastery_seconds",
        "strict balance seed gate")
    marker = "    struct AutonomyStatus\n"
    helper = """    [[nodiscard]] inline bool strict_duck_press_mastery(
        const TrainingMetrics& metrics) noexcept
    {
        return metrics.evaluation_valid
            && metrics.evaluation_invalid_runs == 0u
            && metrics.evaluation_duck_recoveries >= 1.0f
            && metrics.evaluation_duck_seconds >= 1.25f
            && metrics.evaluation_longest_stance >= 2.5f
            && metrics.evaluation_survival >= 9.0f
            && metrics.evaluation_max_joint_speed <= 10.0f;
    }

"""
    text = replace_once(text, marker, helper + marker, "duck mastery helper")
    write("src/autonomy.hpp", text)

    text = read("src/autonomy_curriculum.cpp")
    old = """        case sim::CourseStage::duck_press:
            return metrics.evaluation_valid
                && metrics.evaluation_invalid_runs == 0u
                && metrics.evaluation_duck_recoveries >= 2.0f
                && metrics.evaluation_duck_seconds >= 1.25f
                && metrics.evaluation_longest_stance >= 2.5f
                && metrics.evaluation_survival >= 9.0f
                && metrics.evaluation_max_joint_speed <= 10.0f;"""
    new = """        case sim::CourseStage::duck_press:
            return strict_duck_press_mastery(metrics);"""
    text = replace_once(text, old, new, "reachable duck mastery gate")
    write("src/autonomy_curriculum.cpp", text)


def patch_topology_teachers() -> None:
    text = read("src/ppo.hpp")
    text = replace_once(text,
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'0800u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1200u;",
        "training semantics version")

    marker = "    [[nodiscard]] inline std::array<float, sim::action_count> compact_support_teacher_action(\n"
    helpers = """    [[nodiscard]] inline std::uint8_t motor_support_mask(
        const sim::CreatureBlueprint& rig,
        const sim::MotorConstraint& motor) noexcept
    {
        if (!motor.enabled || motor.pivot >= rig.nodes.size()
            || motor.c >= rig.nodes.size() || rig.nodes.size() > 128u)
            return 0u;

        std::array<bool, 128> visited{};
        std::array<std::uint16_t, 128> stack{};
        std::size_t stack_size = 0u;
        visited[motor.pivot] = true;
        visited[motor.c] = true;
        stack[stack_size++] = motor.c;
        std::uint8_t mask = 0u;
        while (stack_size > 0u)
        {
            const std::uint16_t node = stack[--stack_size];
            if (rig.is_left_support_seed(node))
                mask = static_cast<std::uint8_t>(mask | 0x1u);
            if (rig.is_right_support_seed(node))
                mask = static_cast<std::uint8_t>(mask | 0x2u);
            for (const sim::DistanceConstraint& bone : rig.bones)
            {
                std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                if (bone.a == node)
                    next = bone.b;
                else if (bone.b == node)
                    next = bone.a;
                if (next < rig.nodes.size() && !visited[next])
                {
                    visited[next] = true;
                    stack[stack_size++] = next;
                }
            }
        }
        return mask;
    }

    [[nodiscard]] inline bool motor_drives_support_branch(
        const sim::CreatureBlueprint& rig,
        const sim::MotorConstraint& motor) noexcept
    {
        return motor_support_mask(rig, motor) != 0u;
    }

"""
    text = replace_once(text, marker, helpers + marker, "support branch helpers")
    text = replace_once(text,
        "            if (!motor.enabled || motor.a >= rig.nodes.size()\n"
        "                || motor.pivot >= rig.nodes.size() || motor.c >= rig.nodes.size())\n"
        "                continue;\n"
        "            if (!rig.is_support_seed(motor.c))\n"
        "                continue;",
        "            if (!motor.enabled || motor.a >= rig.nodes.size()\n"
        "                || motor.pivot >= rig.nodes.size() || motor.c >= rig.nodes.size()\n"
        "                || !motor_drives_support_branch(rig, motor))\n"
        "                continue;",
        "support branch selection")

    start = text.index("    [[nodiscard]] inline std::array<float, sim::action_count> crouch_walk_teacher_action(\n")
    end = text.index("    [[nodiscard]] inline std::array<float, sim::action_count> effective_policy_action(\n", start)
    replacement = """    [[nodiscard]] inline std::array<float, sim::action_count> crouch_walk_teacher_action(
        const sim::Environment& environment) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const float pressure = std::max(0.72f, environment.duck_obstacle_weight());
        const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.05f;
        const float swing = std::sin(phase);
        auto action = rig.paired_leg_chains()
            ? balance_teacher_action(environment)
            : compact_support_teacher_action(environment, pressure);

        if (rig.paired_leg_chains())
        {
            action[0] = clamp(action[0] - 0.24f * pressure + 0.34f * swing, -0.82f, 0.82f);
            action[1] = clamp(action[1] + 0.50f * pressure
                + 0.34f * std::max(0.0f, swing), -0.90f, 0.90f);
            action[2] = clamp(action[2] + 0.24f * pressure - 0.34f * swing, -0.82f, 0.82f);
            action[3] = clamp(action[3] - 0.50f * pressure
                - 0.34f * std::max(0.0f, -swing), -0.90f, 0.90f);
        }
        else
        {
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
            {
                const std::uint8_t mask = motor_support_mask(rig, rig.motors[index]);
                if (mask == 0u)
                    continue;
                const float gait_direction = mask == 0x1u ? swing
                    : mask == 0x2u ? -swing
                    : ((index & 1u) == 0u ? swing : -swing);
                action[index] = clamp(action[index] + gait_direction * 0.24f,
                    -0.86f, 0.86f);
            }
        }
        for (std::size_t index = 4; index < rig.active_motor_count; ++index)
            action[index] = 0.0f;
        return bilateral_joint_synergy_action(environment, action,
            sim::CourseStage::crouch_walk);
    }

"""
    text = text[:start] + replacement + text[end:]
    write("src/ppo.hpp", text)


def patch_duck_press_geometry() -> None:
    text = read("src/simulation.hpp")
    text = replace_once(text,
        "        const float target = standing_head_top - (0.78f + clamp(difficulty, 0.0f, 1.0f) * 0.20f);",
        "        const float crouch_drop = clamp(standing_head_top * 0.20f, 0.42f, 0.90f)\n"
        "            + clamp(difficulty, 0.0f, 1.0f) * 0.08f;\n"
        "        const float target = standing_head_top - crouch_drop;",
        "body-scaled duck press")
    write("src/simulation.hpp", text)


def patch_ui_and_state() -> None:
    text = read("src/app.cpp")
    text = text.replace("runner-v0710-autosave.eppo", "runner-v0712-autosave.eppo")
    text = text.replace("runner-v0710-evolved.rig", "runner-v0712-evolved.rig")
    text = text.replace("runner-v0710-autonomy.state", "runner-v0712-autonomy.state")

    text = remove_between(text,
        "        void draw_pixel_art(render::Canvas& canvas, const art::PixelArt& artwork,\n",
        "        void fill_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color color)\n",
        "top-bar artwork renderer")
    text = remove_between(text,
        "            if (original_runner_art.loaded() && width >= 1280)\n",
        "            const float tab_width = width >= 1080 ? 184.0f : 164.0f;\n",
        "unrequested top-bar artwork panel")
    text = remove_between(text,
        "        void draw_biomechanical_overlay(const sim::Environment& environment,\n",
        "        void draw_creature(const sim::Environment& environment, Rect viewport, float camera,\n",
        "ornamental biomechanical overlay")
    text = replace_once(text,
        "            draw_biomechanical_overlay(environment, viewport, camera, scale);\n",
        "",
        "ornamental overlay call")
    write("src/app.cpp", text)


def patch_acceptance() -> None:
    text = read("src/acceptance.cpp")
    marker = "        [[nodiscard]] std::string balance_detail(const BalanceGateResult& result)\n"
    insert = """        struct CrouchGateResult
        {
            std::uint32_t accepted{};
            std::uint32_t total{};
            float shortest_duck{ std::numeric_limits<float>::max() };
            std::uint32_t fewest_recoveries{ std::numeric_limits<std::uint32_t>::max() };
        };

        [[nodiscard]] CrouchGateResult strict_crouch_gate(
            const CreatureBlueprint& blueprint, std::uint64_t seed_base)
        {
            CrouchGateResult result{};
            result.total = 4u;
            for (std::uint32_t seed_index = 0; seed_index < result.total; ++seed_index)
            {
                const std::uint64_t seed = seed_base
                    + static_cast<std::uint64_t>(seed_index) * 4099u;
                Environment environment{ blueprint, seed };
                environment.set_course(sim::CourseStage::duck_press, 0.25f);
                for (int frame = 0; frame < 1800; ++frame)
                {
                    const auto action = rl::duck_teacher_action(environment);
                    const sim::StepResult step = environment.step(action);
                    if (environment.duck_press_completed()
                        && environment.duck_recoveries() >= 1u
                        && environment.stable_stance_seconds() >= 0.75f)
                        break;
                    if (step.terminated)
                        break;
                }
                const rl::StageMotionQualification qualification =
                    rl::stage_motion_qualification(sim::CourseStage::duck_press, environment);
                const bool accepted = qualification.valid
                    && environment.body_integrity_valid()
                    && environment.duck_press_completed()
                    && environment.duck_recoveries() >= 1u;
                result.accepted += accepted ? 1u : 0u;
                result.shortest_duck = std::min(result.shortest_duck,
                    environment.duck_seconds());
                result.fewest_recoveries = std::min(result.fewest_recoveries,
                    environment.duck_recoveries());
            }
            return result;
        }

        [[nodiscard]] std::string crouch_detail(const CrouchGateResult& result)
        {
            std::ostringstream stream{};
            stream << result.accepted << '/' << result.total
                << " seeds, shortest_duck=" << result.shortest_duck
                << ", fewest_recoveries=" << result.fewest_recoveries;
            return stream.str();
        }

"""
    text = replace_once(text, marker, insert + marker, "crouch acceptance helpers")

    old = """        const BalanceGateResult humanoid_gate = strict_balance_gate(
            CreatureBlueprint::humanoid(), 0xE000u);
        add_case(report, "humanoid-strict-six-seed-balance",
            humanoid_gate.accepted == humanoid_gate.total,
            balance_detail(humanoid_gate));

        const BalanceGateResult chicken_gate = strict_balance_gate(
            CreatureBlueprint::chicken(), 0xC11C000u);
        add_case(report, "chicken-strict-six-seed-balance",
            chicken_gate.accepted == chicken_gate.total,
            balance_detail(chicken_gate));

        const CreatureBlueprint humanoid = CreatureBlueprint::humanoid();"""
    new = """        for (std::size_t index = 0; index < presets.size(); ++index)
        {
            const BalanceGateResult gate = strict_balance_gate(
                presets[index].blueprint,
                0xE000u + static_cast<std::uint64_t>(index) * 0x10000u);
            add_case(report,
                std::string{ presets[index].name } + "-strict-six-seed-stand",
                gate.accepted == gate.total,
                balance_detail(gate));
        }

        for (std::size_t index = 0; index < presets.size(); ++index)
        {
            const CrouchGateResult gate = strict_crouch_gate(
                presets[index].blueprint,
                0xD0C700u + static_cast<std::uint64_t>(index) * 0x10000u);
            add_case(report,
                std::string{ presets[index].name } + "-static-crouch-hold-recover",
                gate.accepted == gate.total,
                crouch_detail(gate));
        }

        const CreatureBlueprint humanoid = CreatureBlueprint::humanoid();"""
    text = replace_once(text, old, new, "all-rig stand and crouch gates")
    write("src/acceptance.cpp", text)

    text = read("tests/live_acceptance_tests.cpp")
    text = replace_once(text,
        "    if (report.cases.size() < 10u)",
        "    if (report.cases.size() < 22u)",
        "expanded acceptance count")
    write("tests/live_acceptance_tests.cpp", text)


def patch_docs() -> None:
    cache = read("missioncache.md")
    cache = re.sub(r"^\*\*Target:\*\*.*$", "**Target:** Runner v0.7.12",
        cache, count=1, flags=re.MULTILINE)
    cache = re.sub(r"^\*\*Release state:\*\*.*$",
        "**Release state:** REOPENED — packaged v0.7.11 runtime evidence shows rig-dependent Stand/Crouch stalls and unrequested ornamental UI clutter.",
        cache, count=1, flags=re.MULTILINE)
    for mission in ("WALK-LIVE-066", "WALK-STAND-080", "WALK-RIGSTANCE-084", "WALK-CROUCH-085"):
        cache = re.sub(
            rf"(### {mission}[^\n]*\n)\*\*Status:\*\*[^\n]*",
            rf"\1**Status:** REOPENED — contradicted by packaged v0.7.11 rig-dependent Stand/Crouch runtime behavior",
            cache, count=1)
    marker = "## v0.7.12 rig progression and UI rollback"
    if marker not in cache:
        cache = cache.rstrip() + f"""

{marker}

### WALK-RIGPROG-118 — Every preset must complete Stand then static Crouch
**Status:** IN VALIDATION

Chicken, biped, humanoid, quadruped, crawler4, hexapod, and monoped each require named deterministic multi-seed Stand and static crouch/hold/recover acceptance. Aggregate finite-soak checks and two-rig standing checks are not sufficient.

### WALK-TOPOLOGY-119 — Drive support chains by rig topology
**Status:** IN VALIDATION

Static crouch and crouch-walk teachers must discover the motor subtree that reaches semantic support nodes. Passive feet and non-biped body plans cannot be skipped because a motor's immediate driven node is not itself the final support seed.

### WALK-MASTERY-120 — Remove contradictory and impossible stage gates
**Status:** IN VALIDATION

Stand mastery accepts five of six strict evaluation seeds while retaining posture, spin, survival, and joint-speed gates. Static crouch mastery requires the one authored press hold/recovery that an episode can actually produce, not two recoveries after the press has been removed.

### WALK-UICLEAN-121 — Remove unrequested ornamental UI
**Status:** IN VALIDATION

Remove the large top-bar artwork card, labels, ghost skeleton, animated packets, pulsing rings, and floating torso chip. Do not add replacement controls or another toggle. Preserve the actual trainer, rig editor, telemetry, package validation, and artwork file support.

### WALK-STATE-122 — Isolate corrected rig training state
**Status:** IN VALIDATION

Bump training semantics and use `runner-v0712-*` autosaves so stale v0.7.10/v0.7.11 policies cannot immediately recreate the reported Stand/Crouch stalls.

### WALK-RELEASE-123 — Publish Runner v0.7.12 only after packaged progression proof
**Status:** BLOCKED — implementation and all-rig validation are not complete

Require Linux warnings-as-errors, full Windows SDL3/Vulkan tests, all-seven Stand and Crouch acceptance, installed/extracted diagnostics, package checksum/manifest audit, and a clean repository before publication.
"""
    write("missioncache.md", cache)

    changelog = read("CHANGELOG.md")
    entry = """## [0.7.12] - 2026-08-04

### Fixed

- Replaced impossible two-recovery static-crouch mastery with the single authored press hold/recovery cycle.
- Made Stand mastery use an explicit five-of-six robust seed gate instead of contradicting evaluation validity.
- Added topology-aware support-chain teaching for passive-foot, monoped, quadruped, crawler, and hexapod rigs.
- Added named Stand and static Crouch acceptance for all seven presets.
- Removed the unrequested top-bar artwork card and ornamental biomechanical overlay without adding more controls.
- Isolated corrected training with v0.7.12 semantics and autosave names.

"""
    if "## [0.7.12]" not in changelog:
        position = changelog.find("## [0.7.11]")
        changelog = (entry + changelog) if position < 0 else (
            changelog[:position] + entry + changelog[position:])
    write("CHANGELOG.md", changelog)


def main() -> None:
    patch_versions()
    patch_mastery()
    patch_topology_teachers()
    patch_duck_press_geometry()
    patch_ui_and_state()
    patch_acceptance()
    patch_docs()
    trigger = ROOT / "WORK_v0712.tmp"
    if trigger.exists():
        trigger.unlink()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
