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


def patch_press_geometry() -> None:
    text = read("src/simulation.hpp")
    text = replace_once(text,
        "const float crouch_drop = clamp(standing_head_top * 0.20f, 0.58f, 0.95f)",
        "const float crouch_drop = clamp(standing_head_top * 0.16f, 0.58f, 0.68f)",
        "safe crouch press depth")
    write("src/simulation.hpp", text)

    text = read("src/simulation.cpp")
    old = '''                const float half_width = clamp(
                    (maximum_x - minimum_x) * 0.42f + 0.45f, 0.82f, 1.20f);
                const DuckPressProfile profile = duck_press_profile(
                    elapsed_seconds_, course_difficulty_, rest_head_top);
                const float press_anchor_x = blueprint_.root_node < blueprint_.nodes.size()
                    ? blueprint_.nodes[blueprint_.root_node].x : root_x;'''
    new = '''                const float authored_center_x = (minimum_x + maximum_x) * 0.5f;
                const float half_width = clamp(
                    (maximum_x - minimum_x) * 0.5f + 0.34f, 0.82f, 2.20f);
                const DuckPressProfile profile = duck_press_profile(
                    elapsed_seconds_, course_difficulty_, rest_head_top);
                // The station remains fixed in authored world space, but spans
                // the complete rig instead of crushing only the root end of
                // quadrupeds, crawlers, and hexapods.
                const float press_anchor_x = authored_center_x;'''
    text = replace_once(text, old, new, "whole-rig fixed press station")
    text = replace_once(text,
        "if (duck_press_max_penetration_ > 0.24f)\n            invalidate(InvalidMotion::press_penetration);",
        "if (duck_press_max_penetration_ > 0.44f)\n            invalidate(InvalidMotion::press_penetration);",
        "transient solver penetration threshold")
    write("src/simulation.cpp", text)


def patch_crouch_stability() -> None:
    text = read("src/ppo.hpp")
    old = '''        const float pressure = environment.duck_press_completed()
            ? 0.0f : environment.duck_obstacle_weight();'''
    new = '''        float pressure = environment.duck_press_completed()
            ? 0.0f : environment.duck_obstacle_weight();
        if (environment.duck_active())
            pressure *= 0.42f;'''
    text = replace_once(text, old, new, "crouch pressure taper")
    text = replace_once(text,
        "compact.y *= 1.0f - pressure * 0.48f;",
        "compact.y *= 1.0f - pressure * 0.36f;",
        "bounded non-biped compression")
    text = replace_once(text,
        "const float knee = 0.68f * pressure;",
        "const float knee = 0.56f * pressure;",
        "bounded paired-leg compression")
    write("src/ppo.hpp", text)

    text = read("src/simulation.cpp")
    old_gate = '''        if (course_stage_ != CourseStage::balance
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node))
            return;'''
    new_gate = '''        const bool balance_stage = course_stage_ == CourseStage::balance;
        const bool crouch_stage = course_stage_ == CourseStage::duck_press;
        if ((!balance_stage && !crouch_stage)
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node))
            return;'''
    text = replace_once(text, old_gate, new_gate, "balance/crouch posture guide gate")
    old_correction = '''            Vec2 correction = target - particles_[node].position;
            const float magnitude = length(correction);'''
    new_correction = '''            Vec2 correction = target - particles_[node].position;
            if (crouch_stage)
                correction.y = 0.0f;
            const float magnitude = length(correction);'''
    text = replace_once(text, old_correction, new_correction,
        "crouch horizontal-only posture guide")
    old_guides = '''        guide(blueprint_.root_node, 0.16f, 0.035f);
        guide(blueprint_.torso_node, 0.12f, 0.030f);
        guide(blueprint_.head_node, 0.08f, 0.025f);'''
    new_guides = '''        const float guide_scale = crouch_stage ? 0.62f : 1.0f;
        guide(blueprint_.root_node, 0.16f * guide_scale, 0.035f);
        guide(blueprint_.torso_node, 0.12f * guide_scale, 0.030f);
        guide(blueprint_.head_node, 0.08f * guide_scale, 0.025f);'''
    text = replace_once(text, old_guides, new_guides,
        "crouch posture guide strength")
    write("src/simulation.cpp", text)


def patch_hexapod_motors() -> None:
    text = read("src/simulation.cpp")
    old = '''        result.motors = {
            MotorConstraint{ 0, 9, 3 }, MotorConstraint{ 0, 9, 4 },
            MotorConstraint{ 1, 10, 5 }, MotorConstraint{ 0, 1, 11 }
        };'''
    new = '''        result.motors = {
            MotorConstraint{ 0, 9, 3 }, MotorConstraint{ 0, 9, 4 },
            MotorConstraint{ 0, 10, 5 }, MotorConstraint{ 1, 11, 7 }
        };'''
    text = replace_once(text, old, new, "enabled hexapod support motors")
    write("src/simulation.cpp", text)

    text = read("src/acceptance.cpp")
    old = '''        for (const NamedBlueprint& preset : presets)
        {
            if (preset.blueprint.valid())
                continue;
            blueprints_valid = false;'''
    new = '''        for (const NamedBlueprint& preset : presets)
        {
            const bool active_motors_enabled = std::ranges::all_of(
                std::span{ preset.blueprint.motors }.first(
                    preset.blueprint.active_motor_count),
                [](const sim::MotorConstraint& motor) { return motor.enabled; });
            if (preset.blueprint.valid() && active_motors_enabled)
                continue;
            blueprints_valid = false;'''
    text = replace_once(text, old, new, "authored active-motor integrity")
    write("src/acceptance.cpp", text)


def main() -> None:
    patch_press_geometry()
    patch_crouch_stability()
    patch_hexapod_motors()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
