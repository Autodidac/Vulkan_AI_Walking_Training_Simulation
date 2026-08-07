#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def replace_between(text: str, start: str, end: str, replacement: str, label: str) -> str:
    first = text.find(start)
    if first < 0:
        raise RuntimeError(f"{label}: start marker missing")
    last = text.find(end, first + len(start))
    if last < 0:
        raise RuntimeError(f"{label}: end marker missing")
    return text[:first] + replacement + text[last:]


def patch_simulation_header() -> None:
    path = "src/simulation.hpp"
    text = read(path)
    text = replace_once(text,
'''        [[nodiscard]] bool paired_leg_chains() const noexcept
        {
            return !monopedal_gait() && active_motor_count >= 4u
                && motors[0].enabled && motors[1].enabled
                && motors[2].enabled && motors[3].enabled
                && motors[0].pivot == motors[2].pivot
                && motors[1].a == motors[0].pivot
                && motors[3].a == motors[2].pivot;
        }
''',
'''        [[nodiscard]] bool paired_leg_chains() const noexcept
        {
            return support_seed_count() == 2u
                && !monopedal_gait() && active_motor_count >= 4u
                && motors[0].enabled && motors[1].enabled
                && motors[2].enabled && motors[3].enabled
                && motors[0].pivot == motors[2].pivot
                && motors[1].a == motors[0].pivot
                && motors[3].a == motors[2].pivot;
        }
''', "paired leg classification")

    crossing_helper = '''    [[nodiscard]] inline bool completes_side_view_crossing(
        bool began_behind, float swing_center_x, float stance_center_x,
        float swing_clearance) noexcept
    {
        return began_behind
            && swing_clearance >= 0.065f
            && swing_center_x >= stance_center_x + 0.035f;
    }

'''
    marker = '''    [[nodiscard]] inline bool qualifies_crossing_step(int previous_side,
'''
    text = replace_once(text, marker, crossing_helper + marker,
        "side-view crossing helper")

    text = replace_once(text,
'''        bool left_swing_crossed_{};
        bool right_swing_crossed_{};
        std::uint32_t limb_crossings_{};
''',
'''        bool left_swing_started_behind_{};
        bool right_swing_started_behind_{};
        bool left_swing_crossed_{};
        bool right_swing_crossed_{};
        std::uint32_t limb_crossings_{};
''', "swing order state")
    write(path, text)


def preset_scaffold() -> str:
    return '''    CreatureBlueprint CreatureBlueprint::scaffold()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.62f }, { 0.02f, 3.68f }, { 0.10f, 4.38f },
            { -0.08f, 1.46f }, { -0.14f, 0.25f },
            { 0.08f, 1.46f }, { 0.14f, 0.25f }
        };
        result.radii = { 0.24f, 0.27f, 0.23f, 0.17f, 0.15f, 0.17f, 0.15f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 }
        };
        result.active_motor_count = 4u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.right_contact_node = 6u;
        add_passive_feet(result, 0.16f, 0.28f);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 38.0f, 58.0f, 0.042f, 0.048f);
        return result;
    }

'''


def preset_chicken() -> str:
    return '''    CreatureBlueprint CreatureBlueprint::chicken()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.40f }, { 0.72f, 2.48f },
            { 0.98f, 3.04f }, { 1.18f, 3.50f }, { 1.54f, 3.46f },
            { -0.92f, 2.64f }, { -1.36f, 2.84f },
            { -0.10f, 1.42f }, { -0.15f, 0.28f },
            { 0.10f, 1.42f }, { 0.15f, 0.28f },
            { 0.02f, 3.12f }
        };
        result.radii = {
            0.42f, 0.38f, 0.23f, 0.28f, 0.11f,
            0.24f, 0.13f, 0.18f, 0.14f, 0.18f, 0.14f, 0.27f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.98f },
            { 2, 3, 0.0f, 0.98f }, { 3, 4, 0.0f, 0.94f },
            { 0, 2, 0.0f, 0.94f }, { 1, 3, 0.0f, 0.94f },
            { 0, 5, 0.0f, 0.92f }, { 5, 6, 0.0f, 0.88f },
            { 0, 6, 0.0f, 0.86f }, { 1, 5, 0.0f, 0.82f },
            { 0, 11, 0.0f, 0.96f }, { 1, 11, 0.0f, 0.92f },
            { 11, 2, 0.0f, 0.92f }, { 11, 3, 0.0f, 0.90f },
            { 11, 7, 0.0f, 0.84f }, { 11, 9, 0.0f, 0.84f },
            { 0, 7, 0.0f, 1.0f }, { 7, 8, 0.0f, 1.0f },
            { 0, 9, 0.0f, 1.0f }, { 9, 10, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 11, 0, 7 }, MotorConstraint{ 0, 7, 8 },
            MotorConstraint{ 11, 0, 9 }, MotorConstraint{ 0, 9, 10 }
        };
        result.active_motor_count = 4u;
        result.root_node = 0u;
        result.torso_node = 11u;
        result.head_node = 3u;
        result.left_contact_node = 8u;
        result.right_contact_node = 10u;
        add_passive_feet(result, 0.17f, 0.29f);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 38.0f, 60.0f, 0.038f, 0.044f);
        return result;
    }

'''


def preset_biped() -> str:
    return '''    CreatureBlueprint CreatureBlueprint::biped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.74f }, { 0.02f, 3.84f }, { 0.10f, 4.64f },
            { -0.09f, 1.52f }, { -0.14f, 0.25f },
            { 0.09f, 1.52f }, { 0.14f, 0.25f }
        };
        result.radii = { 0.25f, 0.29f, 0.25f, 0.18f, 0.17f, 0.18f, 0.17f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 }
        };
        result.active_motor_count = 4u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.right_contact_node = 6u;
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 40.0f, 60.0f, 0.045f, 0.051f);
        return result;
    }

'''


def preset_humanoid() -> str:
    return '''    CreatureBlueprint CreatureBlueprint::humanoid()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.82f }, { 0.02f, 4.20f }, { 0.12f, 4.98f },
            { -0.09f, 1.56f }, { -0.14f, 0.25f },
            { 0.09f, 1.56f }, { 0.14f, 0.25f },
            { -0.04f, 4.04f }, { -0.34f, 3.46f }, { -0.18f, 2.78f },
            { 0.04f, 4.04f }, { 0.34f, 3.46f }, { 0.18f, 2.78f }
        };
        result.radii = {
            0.26f, 0.31f, 0.27f, 0.19f, 0.17f, 0.19f, 0.17f,
            0.16f, 0.15f, 0.14f, 0.16f, 0.15f, 0.14f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f },
            { 1, 7, 0.0f, 0.98f }, { 7, 8, 0.0f, 0.98f },
            { 8, 9, 0.0f, 0.96f },
            { 1, 10, 0.0f, 0.98f }, { 10, 11, 0.0f, 0.98f },
            { 11, 12, 0.0f, 0.96f },
            { 7, 10, 0.0f, 0.48f },
            { 2, 7, 0.0f, 0.90f }, { 2, 10, 0.0f, 0.90f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 },
            MotorConstraint{ 1, 7, 8 }, MotorConstraint{ 7, 8, 9 },
            MotorConstraint{ 1, 10, 11 }, MotorConstraint{ 10, 11, 12 }
        };
        result.active_motor_count = 8u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.right_contact_node = 6u;
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        for (std::size_t index = 0; index < 4u; ++index)
        {
            const bool knee = (index & 1u) != 0u;
            const MotorConstraint& motor = result.motors[index];
            const float driven_length = length(
                result.nodes[motor.c] - result.nodes[motor.pivot]);
            const float linear_gain = knee ? 0.051f : 0.045f;
            const float strength = linear_gain / std::max(0.75f, driven_length);
            result.calibrate_motor(index, knee ? 60.0f : 40.0f,
                knee ? 60.0f : 40.0f, strength);
        }
        result.calibrate_motor(4, 88.0f, 88.0f, 0.032f);
        result.calibrate_motor(5, 100.0f, 100.0f, 0.030f);
        result.calibrate_motor(6, 88.0f, 88.0f, 0.032f);
        result.calibrate_motor(7, 100.0f, 100.0f, 0.030f);
        return result;
    }

'''


def preset_quadruped() -> str:
    return '''    CreatureBlueprint CreatureBlueprint::quadruped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { -0.65f, 2.12f }, { 0.65f, 2.16f }, { 1.38f, 2.46f },
            { -0.82f, 1.18f }, { -0.94f, 0.24f },
            { -0.48f, 1.14f }, { -0.40f, 0.24f },
            { 0.48f, 1.16f }, { 0.38f, 0.24f },
            { 0.82f, 1.14f }, { 0.92f, 0.24f }
        };
        result.radii = {
            0.31f, 0.32f, 0.25f,
            0.16f, 0.14f, 0.16f, 0.14f,
            0.16f, 0.14f, 0.16f, 0.14f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.96f },
            { 0, 2, 0.0f, 0.74f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f },
            { 1, 7, 0.0f, 1.0f }, { 7, 8, 0.0f, 1.0f },
            { 1, 9, 0.0f, 1.0f }, { 9, 10, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 },
            MotorConstraint{ 0, 1, 7 }, MotorConstraint{ 1, 7, 8 },
            MotorConstraint{ 0, 1, 9 }, MotorConstraint{ 1, 9, 10 }
        };
        result.active_motor_count = 8u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.additional_left_contact_nodes = { 10u };
        result.right_contact_node = 6u;
        result.additional_right_contact_nodes = { 8u };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 48.0f);
        return result;
    }

'''


def preset_crawler4() -> str:
    return '''    CreatureBlueprint CreatureBlueprint::crawler4()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { -0.70f, 1.64f }, { 0.55f, 1.68f }, { 1.26f, 1.86f },
            { -0.90f, 0.92f }, { -1.00f, 0.23f },
            { -0.50f, 0.88f }, { -0.42f, 0.23f },
            { 0.35f, 0.90f }, { 0.25f, 0.23f },
            { 0.75f, 0.88f }, { 0.85f, 0.23f }
        };
        result.radii = {
            0.29f, 0.30f, 0.23f,
            0.15f, 0.13f, 0.15f, 0.13f,
            0.15f, 0.13f, 0.15f, 0.13f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.94f },
            { 0, 2, 0.0f, 0.66f },
            { 0, 3, 0.0f, 0.98f }, { 3, 4, 0.0f, 0.98f },
            { 0, 5, 0.0f, 0.98f }, { 5, 6, 0.0f, 0.98f },
            { 1, 7, 0.0f, 0.98f }, { 7, 8, 0.0f, 0.98f },
            { 1, 9, 0.0f, 0.98f }, { 9, 10, 0.0f, 0.98f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 },
            MotorConstraint{ 0, 1, 7 }, MotorConstraint{ 1, 7, 8 },
            MotorConstraint{ 0, 1, 9 }, MotorConstraint{ 1, 9, 10 }
        };
        result.active_motor_count = 8u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.additional_left_contact_nodes = { 10u };
        result.right_contact_node = 6u;
        result.additional_right_contact_nodes = { 8u };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 50.0f);
        return result;
    }

'''


def preset_hexapod() -> str:
    return '''    CreatureBlueprint CreatureBlueprint::hexapod()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { -0.82f, 1.66f }, { 0.00f, 1.70f },
            { 0.82f, 1.68f }, { 1.46f, 1.92f },
            { -1.02f, 0.24f }, { -0.62f, 0.24f },
            { -0.20f, 0.24f }, { 0.20f, 0.24f },
            { 0.62f, 0.24f }, { 1.02f, 0.24f }
        };
        result.radii = {
            0.27f, 0.28f, 0.27f, 0.22f,
            0.13f, 0.13f, 0.13f, 0.13f, 0.13f, 0.13f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 2, 3, 0.0f, 0.94f }, { 0, 2, 0.0f, 0.68f },
            { 0, 4, 0.0f, 0.98f }, { 0, 5, 0.0f, 0.98f },
            { 1, 6, 0.0f, 0.98f }, { 1, 7, 0.0f, 0.98f },
            { 2, 8, 0.0f, 0.98f }, { 2, 9, 0.0f, 0.98f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 4 }, MotorConstraint{ 1, 0, 5 },
            MotorConstraint{ 0, 1, 6 }, MotorConstraint{ 0, 1, 7 },
            MotorConstraint{ 1, 2, 8 }, MotorConstraint{ 1, 2, 9 }
        };
        result.active_motor_count = 6u;
        result.root_node = 1u;
        result.torso_node = 2u;
        result.head_node = 3u;
        result.left_contact_node = 4u;
        result.additional_left_contact_nodes = { 7u, 8u };
        result.right_contact_node = 5u;
        result.additional_right_contact_nodes = { 6u, 9u };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 44.0f);
        return result;
    }

'''


def preset_monoped() -> str:
    return '''    CreatureBlueprint CreatureBlueprint::monoped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.85f }, { 0.04f, 3.86f }, { 0.12f, 4.62f },
            { 0.00f, 1.62f }, { -0.18f, 0.22f },
            { 0.00f, 0.68f }, { 0.18f, 0.22f }
        };
        result.radii = { 0.27f, 0.29f, 0.25f, 0.19f, 0.15f, 0.18f, 0.15f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 5, 0.0f, 1.0f },
            { 5, 4, 0.0f, 0.92f }, { 5, 6, 0.0f, 0.92f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 5 },
            MotorConstraint{ 3, 5, 4 }, MotorConstraint{ 3, 5, 6 }
        };
        result.active_motor_count = 4u;
        result.root_node = 0u;
        result.torso_node = 1u;
        result.head_node = 2u;
        result.left_contact_node = 4u;
        result.right_contact_node = 6u;
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 34.0f, 50.0f, 0.043f, 0.049f);
        return result;
    }

'''


def patch_simulation_source() -> None:
    path = "src/simulation.cpp"
    text = read(path)
    functions = [
        ("scaffold", "chicken", preset_scaffold()),
        ("chicken", "biped", preset_chicken()),
        ("biped", "humanoid", preset_biped()),
        ("humanoid", "quadruped", preset_humanoid()),
        ("quadruped", "crawler4", preset_quadruped()),
        ("crawler4", "hexapod", preset_crawler4()),
        ("hexapod", "monoped", preset_hexapod()),
    ]
    for current, next_name, replacement in functions:
        text = replace_between(text,
            f"    CreatureBlueprint CreatureBlueprint::{current}()\n",
            f"    CreatureBlueprint CreatureBlueprint::{next_name}()\n",
            replacement,
            f"{current} preset")
    text = replace_between(text,
        "    CreatureBlueprint CreatureBlueprint::monoped()\n",
        "    void CreatureBlueprint::rebuild_rest_lengths() noexcept\n",
        preset_monoped(),
        "monoped preset")

    text = replace_once(text,
'''        const float rest_span = std::abs(
            blueprint_.nodes[blueprint_.right_contact_node].x
            - blueprint_.nodes[blueprint_.left_contact_node].x);
        if (rest_span < 0.08f)
            return 1.0f;
''',
'''        const float rest_span = std::max(0.42f, std::abs(
            blueprint_.nodes[blueprint_.right_contact_node].x
            - blueprint_.nodes[blueprint_.left_contact_node].x));
''', "support-span reference")

    text = replace_once(text,
'''        if (!left && previous_left_grounded_)
            left_swing_crossed_ = false;
        if (!right && previous_right_grounded_)
            right_swing_crossed_ = false;
''',
'''        if (!left && previous_left_grounded_)
        {
            left_swing_started_behind_ = left_center < right_center - 0.035f;
            left_swing_crossed_ = false;
        }
        if (!right && previous_right_grounded_)
        {
            right_swing_started_behind_ = right_center < left_center - 0.035f;
            right_swing_crossed_ = false;
        }
''', "swing lift ordering")

    text = replace_once(text,
'''            left_swing_crossed_ = left_swing_crossed_
                || (left_clearance >= 0.065f && left_center > right_center + 0.035f);
''',
'''            left_swing_crossed_ = left_swing_crossed_
                || completes_side_view_crossing(left_swing_started_behind_,
                    left_center, right_center, left_clearance);
''', "left crossing")
    text = replace_once(text,
'''            right_swing_crossed_ = right_swing_crossed_
                || (right_clearance >= 0.065f && right_center > left_center + 0.035f);
''',
'''            right_swing_crossed_ = right_swing_crossed_
                || completes_side_view_crossing(right_swing_started_behind_,
                    right_center, left_center, right_clearance);
''', "right crossing")

    text = replace_once(text,
'''                const bool swing_crossed = new_left
                    ? left_swing_crossed_ : right_swing_crossed_;
''',
'''                const bool swing_crossed = new_left
                    ? left_swing_started_behind_ && left_swing_crossed_
                    : right_swing_started_behind_ && right_swing_crossed_;
''', "credited crossing order")

    text = replace_once(text,
'''        if (left)
        {
            left_swing_seconds_ = 0.0f;
            left_swing_clearance_ = 0.0f;
        }
        if (right)
        {
            right_swing_seconds_ = 0.0f;
            right_swing_clearance_ = 0.0f;
        }
''',
'''        if (left)
        {
            left_swing_seconds_ = 0.0f;
            left_swing_clearance_ = 0.0f;
            left_swing_started_behind_ = false;
        }
        if (right)
        {
            right_swing_seconds_ = 0.0f;
            right_swing_clearance_ = 0.0f;
            right_swing_started_behind_ = false;
        }
''', "swing state landing reset")

    text = replace_once(text,
'''        left_swing_crossed_ = false;
        right_swing_crossed_ = false;
        limb_crossings_ = 0u;
''',
'''        left_swing_started_behind_ = false;
        right_swing_started_behind_ = false;
        left_swing_crossed_ = false;
        right_swing_crossed_ = false;
        limb_crossings_ = 0u;
''', "swing state episode reset")
    write(path, text)


def patch_ppo() -> None:
    path = "src/ppo.hpp"
    text = read(path)
    text = replace_once(text,
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'2001u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'2101u;",
        "training semantics")

    old = '''        auto action = balance_teacher_action(environment);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        if (!rig.paired_leg_chains())
            return action;

        const locomotion::Plan movement = current_locomotion_plan(environment);
'''
    new = '''        auto action = balance_teacher_action(environment);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const locomotion::Plan movement = current_locomotion_plan(environment);
        if (!rig.paired_leg_chains())
        {
            if (rig.support_seed_count() < 4u)
                return action;
            const float phase = environment.elapsed_seconds() * 2.0f * pi
                * movement.cadence_hz;
            const float swing = std::sin(phase) * movement.direction;
            const float amplitude = 0.28f + movement.stride_scale * 0.34f;
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
            {
                const std::uint8_t mask = motor_support_mask(rig, rig.motors[index]);
                if (mask == 0u)
                    continue;
                const float phase_drive = mask == 0x1u ? swing
                    : mask == 0x2u ? -swing
                    : ((index & 1u) == 0u ? swing : -swing);
                action[index] = clamp(action[index] + phase_drive * amplitude,
                    -0.90f, 0.90f);
            }
            return bilateral_joint_synergy_action(environment, action,
                environment.course_stage());
        }

'''
    text = replace_once(text, old, new, "multi-support walking teacher")
    write(path, text)


def patch_autonomy_header() -> None:
    path = "src/autonomy.hpp"
    text = read(path)
    marker = '''    [[nodiscard]] RigMutationCandidate evolve_rig_candidate(
        const sim::CreatureBlueprint& source, std::uint64_t generation) noexcept;
'''
    replacement = marker + '''    [[nodiscard]] RigMutationCandidate automatic_rig_tuning_candidate(
        const sim::CreatureBlueprint& source, std::uint64_t generation) noexcept;
'''
    text = replace_once(text, marker, replacement, "automatic tuning declaration")
    write(path, text)


def patch_autonomy_curriculum() -> None:
    path = "src/autonomy_curriculum.cpp"
    text = read(path)
    implementation = '''    RigMutationCandidate automatic_rig_tuning_candidate(
        const sim::CreatureBlueprint& source, std::uint64_t generation) noexcept
    {
        RigMutationCandidate result{};
        result.blueprint = source;
        sim::CreatureBlueprint& candidate = result.blueprint;
        const std::uint64_t original_signature = source.signature();
        const float direction = ((generation / 3u) & 1u) == 0u ? 1.0f : -1.0f;
        switch (generation % 3u)
        {
        case 0u:
        {
            result.kind = RigMutationKind::motor_strength;
            if (candidate.active_motor_count > 0u)
            {
                const std::size_t index = static_cast<std::size_t>(
                    generation % candidate.active_motor_count);
                candidate.motors[index].strength = clamp(
                    candidate.motors[index].strength + direction * 0.0020f,
                    0.020f, 0.11f);
            }
            break;
        }
        case 1u:
        {
            result.kind = RigMutationKind::joint_range;
            if (candidate.active_motor_count > 0u)
            {
                const std::size_t index = static_cast<std::size_t>(
                    generation % candidate.active_motor_count);
                sim::MotorConstraint& motor = candidate.motors[index];
                const float delta = direction * 1.5f * pi / 180.0f;
                const float negative = clamp(
                    motor.neutral_angle - motor.minimum_angle + delta,
                    2.0f * pi / 180.0f, 120.0f * pi / 180.0f);
                const float positive = clamp(
                    motor.maximum_angle - motor.neutral_angle + delta,
                    2.0f * pi / 180.0f, 120.0f * pi / 180.0f);
                motor.minimum_angle = motor.neutral_angle - negative;
                motor.maximum_angle = motor.neutral_angle + positive;
            }
            break;
        }
        default:
        {
            result.kind = RigMutationKind::bone_stiffness;
            if (!candidate.bones.empty())
            {
                const std::size_t index = static_cast<std::size_t>(
                    generation % candidate.bones.size());
                candidate.bones[index].stiffness = clamp(
                    candidate.bones[index].stiffness + direction * 0.025f,
                    0.20f, 1.0f);
            }
            break;
        }
        }
        result.changed = candidate.valid()
            && candidate.signature() != original_signature;
        result.topology_changed = false;
        result.activated_motor_mask = 0u;
        if (!result.changed)
            result.blueprint = source;
        return result;
    }

'''
    marker = '''    float AutonomousTrainer::evaluate_rig_locked(
'''
    text = replace_once(text, marker, implementation + marker,
        "automatic tuning implementation")

    text = replace_once(text,
'''    RigMutationCandidate AutonomousTrainer::mutate_rig_locked() noexcept
    {
        return evolve_rig_candidate(worker_.blueprint(), rig_generation_);
    }
''',
'''    RigMutationCandidate AutonomousTrainer::mutate_rig_locked() noexcept
    {
        return automatic_rig_tuning_candidate(
            worker_.blueprint(), rig_generation_);
    }
''', "automatic tuning route")

    substitutions = {
        '"RIG GENERATION {} REJECTED - INVALID/EMPTY {} MUTATION"':
            '"CONTROLLER TUNING {} SKIPPED - INVALID/EMPTY {} CHANGE"',
        '"TOPOLOGY NURSERY {} REJECTED - POLICY TRANSFER FAILED"':
            '"CONTROLLER TUNING {} REJECTED - POLICY TRANSFER FAILED"',
        '"TOPOLOGY NURSERY {} ROLLED BACK - ADAPTED POLICY APPLY FAILED"':
            '"CONTROLLER TUNING {} ROLLED BACK - ADAPTED POLICY APPLY FAILED"',
        '"TOPOLOGY NURSERY {} ACCEPTED {}{}  {:+.3f} VALID SCORE"':
            '"CONTROLLER TUNING {} ACCEPTED {}{}  {:+.3f} VALID SCORE"',
        '"TOPOLOGY NURSERY {} REJECTED {} - NO VALID IMPROVEMENT"':
            '"CONTROLLER TUNING {} REJECTED {} - NO VALID IMPROVEMENT"',
    }
    for old, new in substitutions.items():
        text = text.replace(old, new)
    write(path, text)


def patch_state_paths() -> None:
    path = "src/autonomy_persistence.cpp"
    text = read(path)
    text = text.replace('output << "RUNAUTONOMY 15\\n";',
                        'output << "RUNAUTONOMY 16\\n";')
    text = text.replace('version != 15', 'version != 16')
    write(path, text)


def patch_ui_layout() -> None:
    path = "src/ui_layout.hpp"
    text = read(path)
    block = '''    [[nodiscard]] constexpr float rig_lab_panel_width(float content_width) noexcept
    {
        return std::clamp(content_width * 0.31f, 420.0f, 560.0f);
    }

    [[nodiscard]] constexpr Box rig_lab_panel_box(Box content) noexcept
    {
        return { content.x, content.y,
            rig_lab_panel_width(content.width), content.height };
    }

    [[nodiscard]] constexpr Box rig_lab_world_box(Box content) noexcept
    {
        const Box panel = rig_lab_panel_box(content);
        return { panel.x + panel.width + panel_gap, content.y,
            std::max(0.0f, content.width - panel.width - panel_gap),
            content.height };
    }

    [[nodiscard]] constexpr bool rig_lab_layout_valid(
        float width, float height) noexcept
    {
        if (!supported_window(width, height))
            return false;
        const Box content = content_box(width, height);
        const Box panel = rig_lab_panel_box(content);
        const Box world = rig_lab_world_box(content);
        return panel.width >= 420.0f && world.width >= 680.0f
            && contains(content, panel) && contains(content, world)
            && !overlaps(panel, world);
    }

'''
    marker = '''    inline constexpr std::array<std::array<float, 2>, 5> validation_sizes{
'''
    text = replace_once(text, marker, block + marker, "Rig Lab layout helpers")
    write(path, text)


def new_rig_panel() -> str:
    return '''        void draw_rig_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            canvas.push_clip(rect.position + Vec2{ 1.0f, 1.0f },
                rect.position + rect.size - Vec2{ 1.0f, 1.0f });
            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const float usable = rect.size.x - 36.0f;
            Vec2 cursor = rect.position + Vec2{ 18.0f, 16.0f };
            add_text_fit(canvas, cursor, "RIG LAB", 1.70f, white, usable, 1.10f);
            cursor.y += 38.0f;
            const float tab = (usable - 18.0f) * 0.25f;
            auto page_button = [&](int slot, std::string_view label, RigPanelPage page)
            {
                if (button({ cursor + Vec2{ static_cast<float>(slot) * (tab + 6.0f), 0.0f },
                    { tab, 35.0f } }, label, input, rig_panel_page == page))
                    rig_panel_page = page;
            };
            page_button(0, "PRESETS", RigPanelPage::presets);
            page_button(1, "STRUCTURE", RigPanelPage::structure);
            page_button(2, "MOTORS", RigPanelPage::motors);
            page_button(3, "TEST", RigPanelPage::test);
            cursor.y += 48.0f;

            if (rig_panel_page == RigPanelPage::presets)
            {
                add_text(canvas, cursor, "CANONICAL SIDE-VIEW RIGS", 1.02f, accent);
                cursor.y += 25.0f;
                const float half = (usable - 6.0f) * 0.5f;
                auto preset = [&](int row, int column, std::string_view label,
                    RigPreset value)
                {
                    const Vec2 position = cursor + Vec2{
                        static_cast<float>(column) * (half + 6.0f),
                        static_cast<float>(row) * 41.0f };
                    if (button({ position, { half, 35.0f } }, label, input,
                        rig_preset == value))
                        use_preset(value);
                };
                preset(0, 0, "HUMANOID", RigPreset::humanoid);
                preset(0, 1, "BIPED", RigPreset::biped);
                preset(1, 0, "SCAFFOLD", RigPreset::scaffold);
                preset(1, 1, "CHICKEN", RigPreset::chicken);
                preset(2, 0, "QUADRUPED", RigPreset::quadruped);
                preset(2, 1, "FOUR-LEG CRAWLER", RigPreset::crawler4);
                preset(3, 0, "HEXAPOD", RigPreset::hexapod);
                preset(3, 1, "MONOPED", RigPreset::monoped);
                cursor.y += 176.0f;
                add_wrapped_text(canvas, cursor,
                    "Selecting a preset restores its authored anatomy. Automatic training tunes control parameters only; it never changes limb length or adds body parts.",
                    0.73f, muted, usable, 2.0f);
                cursor.y += 55.0f;

                const float third = (usable - 12.0f) / 3.0f;
                if (button({ cursor, { third, 35.0f } }, "SAVE RIG", input)
                    || input.save_pressed)
                {
                    std::string error{};
                    set_status(blueprint.save(rig_path, error) ? "RIG SAVED" : error);
                }
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f },
                    { third, 35.0f } }, "LOAD RIG", input) || input.load_pressed)
                {
                    std::string error{};
                    blueprint = sim::CreatureBlueprint::load(rig_path, error);
                    rig_preset = RigPreset::custom;
                    trainer.set_blueprint(blueprint, false);
                    set_status(error.empty()
                        ? "CUSTOM RIG LOADED - FRESH BALANCE LESSON STARTED"
                        : error);
                }
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f },
                    { third, 35.0f } }, "COPY TRAINING RIG", input))
                {
                    blueprint = trainer.blueprint();
                    rig_preset = RigPreset::custom;
                    set_status("CURRENT TRAINING RIG COPIED INTO STRUCTURE EDITOR");
                }
                cursor.y += 48.0f;
                if (button({ cursor, { half, 35.0f } }, "RESTORE RETAINED CONTROLLER",
                    input, trainer.has_best_policy(), trainer.has_best_policy()))
                {
                    set_status(trainer.restore_best_policy()
                        ? "RETAINED CONTROLLER RESTORE QUEUED"
                        : "NO RETAINED CONTROLLER AVAILABLE");
                }
                if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 35.0f } },
                    "START FRESH CONTROLLER", input))
                {
                    trainer.reset_policy(0x721300u
                        + autonomy.rig_generation * 0x9E3779B97F4A7C15ULL);
                    set_status("FRESH CONTROLLER QUEUED FOR UNCHANGED ANATOMY");
                }
                cursor.y += 48.0f;
                const float visual_third = (usable - 12.0f) / 3.0f;
                if (button({ cursor, { visual_third, 35.0f } },
                    right_leg_near ? "NEAR LEG: RIGHT" : "NEAR LEG: LEFT",
                    input, right_leg_near))
                    right_leg_near = !right_leg_near;
                if (button({ cursor + Vec2{ visual_third + 6.0f, 0.0f },
                    { visual_third, 35.0f } },
                    optional_art_enabled ? "ART: ON" : "ART: OFF",
                    input, optional_art_enabled))
                    optional_art_enabled = !optional_art_enabled;
                if (button({ cursor + Vec2{ (visual_third + 6.0f) * 2.0f, 0.0f },
                    { visual_third, 35.0f } },
                    debug_skeleton_overlay ? "SKELETON: ON" : "SKELETON: OFF",
                    input, debug_skeleton_overlay))
                    debug_skeleton_overlay = !debug_skeleton_overlay;
                cursor.y += 50.0f;
                add_text_fit(canvas, cursor,
                    std::format("CONTROL TUNING {}   ACCEPTED {}   REJECTED {}   ROLLBACKS {}",
                        autonomy.rig_generation, autonomy.accepted_rig_changes,
                        autonomy.rejected_rig_changes, autonomy.rollback_count),
                    0.74f, accent, usable, 0.60f);
            }
            else if (rig_panel_page == RigPanelPage::structure)
            {
                add_text(canvas, cursor, "MANUAL STRUCTURE EDITING", 1.02f, accent);
                cursor.y += 25.0f;
                add_wrapped_text(canvas, cursor,
                    "Only manual edits change anatomy. Select and drag nodes in the viewport. Shift adds a node, Ctrl connects it, Alt selects a bone.",
                    0.72f, muted, usable, 2.0f);
                cursor.y += 58.0f;
                add_text_fit(canvas, cursor,
                    std::format("SELECTED NODE: {}", selected_node),
                    1.02f, white, usable - 120.0f);
                if (button({ cursor + Vec2{ usable - 110.0f, -5.0f },
                    { 110.0f, 32.0f } }, "DELETE NODE", input,
                    false, selected_node >= 0))
                    delete_selected_node();
                cursor.y += 36.0f;
                if (selected_node >= 0
                    && static_cast<std::size_t>(selected_node) < blueprint.radii.size())
                {
                    float& radius = blueprint.radii[static_cast<std::size_t>(selected_node)];
                    const float updated = slider({ cursor, { usable, 38.0f } },
                        "NODE SIZE", radius, 0.08f, 0.60f, input);
                    if (updated != radius)
                    {
                        radius = updated;
                        queue_rig_change("NODE SIZE UPDATED");
                    }
                    cursor.y += 52.0f;
                    add_text(canvas, cursor, "SEMANTIC ROLE", 0.92f, muted);
                    cursor.y += 22.0f;
                    const float role = (usable - 16.0f) * 0.20f;
                    auto set_role = [&](int slot, std::string_view label,
                        std::uint16_t& target)
                    {
                        if (button({ cursor + Vec2{ static_cast<float>(slot) * (role + 4.0f), 0.0f },
                            { role, 31.0f } }, label, input,
                            target == selected_node))
                        {
                            target = static_cast<std::uint16_t>(selected_node);
                            apply_small_rig_change("NODE ROLE UPDATED");
                        }
                    };
                    set_role(0, "ROOT", blueprint.root_node);
                    set_role(1, "TORSO", blueprint.torso_node);
                    set_role(2, "HEAD", blueprint.head_node);
                    set_role(3, "PHASE A", blueprint.left_contact_node);
                    set_role(4, "PHASE B", blueprint.right_contact_node);
                    cursor.y += 45.0f;
                }
                add_text_fit(canvas, cursor,
                    std::format("SELECTED BONE: {}", selected_bone),
                    1.00f, white, usable - 120.0f);
                cursor.y += 34.0f;
                if (selected_bone >= 0
                    && static_cast<std::size_t>(selected_bone) < blueprint.bones.size())
                {
                    sim::DistanceConstraint& selected = blueprint.bones[
                        static_cast<std::size_t>(selected_bone)];
                    const float stiffness = slider({ cursor, { usable, 38.0f } },
                        "BONE STIFFNESS", selected.stiffness, 0.20f, 1.0f, input);
                    if (stiffness != selected.stiffness)
                    {
                        selected.stiffness = stiffness;
                        queue_rig_change("BONE STIFFNESS UPDATED");
                    }
                    cursor.y += 52.0f;
                    if (button({ cursor, { usable, 34.0f } }, "DELETE SELECTED BONE", input))
                    {
                        sim::CreatureBlueprint candidate = blueprint;
                        candidate.bones.erase(candidate.bones.begin() + selected_bone);
                        if (candidate.valid() && blueprint_connected(candidate))
                        {
                            blueprint = std::move(candidate);
                            selected_bone = -1;
                            apply_small_rig_change("BONE DELETED");
                        }
                        else
                            set_status("BONE DELETE REJECTED - RIG WOULD DISCONNECT");
                    }
                }
            }
            else if (rig_panel_page == RigPanelPage::motors)
            {
                add_text(canvas, cursor, "MOTOR CHAINS", 1.02f, accent);
                cursor.y += 25.0f;
                const float quarter = (usable - 18.0f) * 0.25f;
                for (int index = 0; index < static_cast<int>(sim::action_count); ++index)
                {
                    const int column = index % 4;
                    const int row = index / 4;
                    const bool available = static_cast<std::size_t>(index)
                        < blueprint.active_motor_count;
                    if (button({ cursor + Vec2{
                            static_cast<float>(column) * (quarter + 6.0f),
                            static_cast<float>(row) * 40.0f },
                        { quarter, 34.0f } }, std::format("MOTOR {}", index + 1),
                        input, selected_motor == index, available))
                    {
                        selected_motor = index;
                        joint_test_group = JointTestGroup::selected;
                    }
                }
                cursor.y += 88.0f;
                const auto names = motor_names();
                add_text_fit(canvas, cursor,
                    names[static_cast<std::size_t>(selected_motor)],
                    1.28f, white, usable, 0.90f);
                cursor.y += 31.0f;
                sim::MotorConstraint& motor = blueprint.motors[
                    static_cast<std::size_t>(selected_motor)];
                const float third = (usable - 12.0f) / 3.0f;
                auto endpoint = [&](int slot, std::string_view label,
                    std::uint16_t& value)
                {
                    if (!button({ cursor + Vec2{ static_cast<float>(slot) * (third + 6.0f), 0.0f },
                        { third, 34.0f } }, label, input, false, selected_node >= 0))
                        return;
                    value = static_cast<std::uint16_t>(selected_node);
                    const bool connected = motor.a != motor.pivot
                        && motor.pivot != motor.c && motor.a != motor.c
                        && has_direct_bone(motor.a, motor.pivot)
                        && has_direct_bone(motor.pivot, motor.c);
                    motor.enabled = connected;
                    if (connected)
                    {
                        blueprint.calibrate_motor(
                            static_cast<std::size_t>(selected_motor), 30.0f,
                            30.0f, motor.strength);
                        apply_small_rig_change("MOTOR ENDPOINT UPDATED");
                    }
                    else
                        set_status("MOTOR NEEDS REAL A-PIVOT AND PIVOT-C BONES");
                };
                endpoint(0, "SET PARENT", motor.a);
                endpoint(1, "SET PIVOT", motor.pivot);
                endpoint(2, "SET DRIVEN", motor.c);
                cursor.y += 45.0f;
                const bool connected = motor.a < blueprint.nodes.size()
                    && motor.pivot < blueprint.nodes.size()
                    && motor.c < blueprint.nodes.size()
                    && motor.a != motor.pivot && motor.pivot != motor.c
                    && motor.a != motor.c
                    && has_direct_bone(motor.a, motor.pivot)
                    && has_direct_bone(motor.pivot, motor.c);
                add_text_fit(canvas, cursor,
                    std::format("PARENT {}   PIVOT {}   DRIVEN {}   {}",
                        motor.a, motor.pivot, motor.c,
                        connected ? (motor.enabled ? "READY" : "DISABLED")
                            : "NOT CONNECTED"),
                    0.78f, connected ? green : yellow, usable, 0.62f);
                cursor.y += 29.0f;
                float negative = (motor.neutral_angle - motor.minimum_angle)
                    * 180.0f / pi;
                float positive = (motor.maximum_angle - motor.neutral_angle)
                    * 180.0f / pi;
                const float updated_negative = slider({ cursor, { usable, 38.0f } },
                    "NEGATIVE RANGE", negative, 2.0f, 120.0f, input, " DEG");
                if (updated_negative != negative)
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        updated_negative, positive, motor.strength);
                cursor.y += 50.0f;
                const float updated_positive = slider({ cursor, { usable, 38.0f } },
                    "POSITIVE RANGE", positive, 2.0f, 120.0f, input, " DEG");
                if (updated_positive != positive)
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        updated_negative, updated_positive, motor.strength);
                cursor.y += 50.0f;
                const float power = slider({ cursor, { usable, 38.0f } },
                    "MOTOR POWER", motor.strength, 0.0f, 0.20f, input);
                if (power != motor.strength)
                {
                    motor.strength = power;
                    queue_rig_change("MOTOR POWER UPDATED");
                }
                cursor.y += 53.0f;
                if (button({ cursor, { third, 34.0f } }, "SET REST", input,
                    false, connected))
                {
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        updated_negative, updated_positive, motor.strength);
                    apply_small_rig_change("REST POSE RECALIBRATED");
                }
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 34.0f } },
                    "SAFE RANGE", input, false, connected))
                {
                    blueprint.calibrate_motor(static_cast<std::size_t>(selected_motor),
                        18.0f, 20.0f, 0.050f);
                    apply_small_rig_change("SAFE MOTOR DEFAULT APPLIED");
                }
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f },
                    { third, 34.0f } }, motor.enabled ? "DISABLE" : "ENABLE",
                    input, motor.enabled, connected))
                {
                    motor.enabled = !motor.enabled;
                    apply_small_rig_change(motor.enabled
                        ? "MOTOR ENABLED" : "MOTOR DISABLED");
                }
            }
            else
            {
                add_text(canvas, cursor, "JOINT AND TRACTION TESTS", 1.02f, accent);
                cursor.y += 28.0f;
                const Rect test_card{ cursor, { usable, 205.0f } };
                draw_joint_lab(test_card, input);
                cursor.y += 220.0f;
                add_wrapped_text(canvas, cursor,
                    "GAIT CYCLE is a side-view fore/aft check. A credited walking step must lift from behind, pass the stance support, and land ahead. Test controls never change the saved training policy.",
                    0.72f, muted, usable, 2.0f);
            }
            canvas.pop_clip();
            add_rounded_rect(canvas, rect, 11.0f, Color{}, border, 1.0f);
        }

'''


def patch_app() -> None:
    path = "src/app.cpp"
    text = read(path)
    text = replace_once(text,
        "enum class RigPanelPage : std::uint8_t { body, motor };",
        "enum class RigPanelPage : std::uint8_t { presets, structure, motors, test };",
        "Rig Lab pages")
    text = replace_once(text,
        "RigPanelPage rig_panel_page{ RigPanelPage::body };",
        "RigPanelPage rig_panel_page{ RigPanelPage::presets };",
        "Rig Lab default page")
    text = text.replace('runner-v0720-ui-autosave.eppo',
                        'runner-v0721-rig-autosave.eppo')
    text = text.replace('runner-v0720-ui-evolved.rig',
                        'runner-v0721-rig-evolved.rig')
    text = text.replace('runner-v0720-ui-autonomy.state',
                        'runner-v0721-rig-autonomy.state')

    text = replace_between(text,
        "        void draw_rig_panel(Rect rect, const InputState& input)\n",
        "        void process_shortcuts(const InputState& input)\n",
        new_rig_panel(),
        "focused Rig Lab panel")

    start = text.find("        void draw_blueprint(Rect viewport, const InputState& input)\n")
    end = text.find("        bool delete_selected_node()\n", start)
    if start < 0 or end < 0:
        raise RuntimeError("draw_blueprint boundaries missing")
    segment = text[start:end]
    old_begin = '''        void draw_blueprint(Rect viewport, const InputState& input)
        {
            constexpr float scale = 86.0f;
            const float ground_y = world_to_screen({ 0.0f, 0.0f }, viewport, 0.0f, scale).y;
'''
    new_begin = '''        void draw_blueprint(Rect viewport, const InputState& input)
        {
            float minimum_x = std::numeric_limits<float>::infinity();
            float maximum_x = -std::numeric_limits<float>::infinity();
            float maximum_y = 0.0f;
            for (std::size_t index = 0; index < blueprint.nodes.size(); ++index)
            {
                const float radius = index < blueprint.radii.size()
                    ? blueprint.radii[index] : 0.15f;
                minimum_x = std::min(minimum_x, blueprint.nodes[index].x - radius);
                maximum_x = std::max(maximum_x, blueprint.nodes[index].x + radius);
                maximum_y = std::max(maximum_y, blueprint.nodes[index].y + radius);
            }
            if (!std::isfinite(minimum_x) || !std::isfinite(maximum_x))
            {
                minimum_x = -1.0f;
                maximum_x = 1.0f;
            }
            const float blueprint_camera = 0.5f * (minimum_x + maximum_x);
            const float horizontal_scale = (viewport.size.x - 90.0f)
                / std::max(1.0f, maximum_x - minimum_x + 0.50f);
            const float vertical_scale = (viewport.size.y * 0.70f - 45.0f)
                / std::max(1.0f, maximum_y + 0.30f);
            const float scale = std::clamp(
                std::min(horizontal_scale, vertical_scale), 42.0f, 106.0f);
            const float ground_y = world_to_screen({ 0.0f, 0.0f }, viewport,
                blueprint_camera, scale).y;
'''
    segment = replace_once(segment, old_begin, new_begin, "blueprint auto-fit")
    segment = segment.replace("viewport, 0.0f, scale", "viewport, blueprint_camera, scale")
    old_joint = '''            const Rect joint_rect{ { viewport.position.x + 20.0f, viewport.position.y + viewport.size.y - 220.0f },
                { std::min(850.0f, viewport.size.x - 40.0f), 200.0f } };
            const bool over_joint_lab = contains(joint_rect, input.mouse);
'''
    segment = replace_once(segment, old_joint,
        "            const bool over_joint_lab = false;\n",
        "remove world joint-test overlay")
    segment = replace_once(segment,
        "            draw_joint_lab(joint_rect, input);\n",
        "", "remove joint-test draw call")
    text = text[:start] + segment + text[end:]

    old_layout = '''            else
            {
                const float panel_width = std::clamp(content.size.x * 0.42f, 680.0f, 760.0f);
                const Rect side{ content.position, { panel_width, content.size.y } };
                const Rect world{ { content.position.x + panel_width + 10.0f, content.position.y },
                    { content.size.x - panel_width - 10.0f, content.size.y } };
                draw_rig_panel(side, input);
                add_rounded_rect(canvas, world, 11.0f, rgb(0x0a131d), border, 1.0f);
                draw_blueprint(world, input);
                add_text(canvas, world.position + Vec2{ 20.0f, 18.0f },
                    "CLICK/DRAG NODE / SHIFT ADD / CTRL CONNECT / ALT SELECT BONE / DELETE NODE", 1.02f, muted);
            }
'''
    new_layout = '''            else
            {
                const ui_layout::Box layout_side =
                    ui_layout::rig_lab_panel_box(layout_content);
                const ui_layout::Box layout_world =
                    ui_layout::rig_lab_world_box(layout_content);
                const Rect side{ { layout_side.x, layout_side.y },
                    { layout_side.width, layout_side.height } };
                const Rect world{ { layout_world.x, layout_world.y },
                    { layout_world.width, layout_world.height } };
                draw_rig_panel(side, input);
                add_rounded_rect(canvas, world, 11.0f, rgb(0x0a131d), border, 1.0f);
                canvas.push_clip(world.position + Vec2{ 1.0f, 1.0f },
                    world.position + world.size - Vec2{ 1.0f, 1.0f });
                draw_blueprint(world, input);
                canvas.pop_clip();
                add_text_fit(canvas, world.position + Vec2{ 18.0f, 16.0f },
                    "SIDE VIEW  |  DRAG NODE  |  SHIFT ADD  |  CTRL CONNECT  |  ALT SELECT BONE",
                    0.80f, muted, world.size.x - 36.0f, 0.68f);
                add_rounded_rect(canvas, world, 11.0f, Color{}, border, 1.0f);
            }
'''
    text = replace_once(text, old_layout, new_layout, "responsive Rig Lab layout")
    write(path, text)


def patch_cmake() -> None:
    path = "CMakeLists.txt"
    text = read(path)
    marker = '''    add_executable(RunnerCoreTests tests/core_tests.cpp)
'''
    target = '''    add_executable(RunnerV0721RigGaitTests
        tests/v0721_rig_gait_tests.cpp)
    target_link_libraries(RunnerV0721RigGaitTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0721RigGaitTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0721RigGaitTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0721RigGaitTests)
    add_test(NAME Runner.V0721RigGait COMMAND RunnerV0721RigGaitTests)
    set_tests_properties(Runner.V0721RigGait PROPERTIES TIMEOUT 90)

'''
    text = replace_once(text, marker, target + marker, "rig/gait test target")
    write(path, text)


def patch_docs() -> None:
    changelog = read("CHANGELOG.md")
    anchor = '''- Preserved v0.7.20 policy, checkpoint, curriculum, terrain, autosave, preview, DPI, clipping, and icon semantics.
'''
    replacement = '''- Stopped automatic training from changing limb length, support width, topology, or semantic contacts; automatic refinement now tunes controller parameters only.
- Rebuilt bipedal presets as compact side-view rigs and rebuilt quadruped, crawler, and hexapod support branches.
- Tightened gait credit to require a swing support to begin behind, clear, pass, and land ahead of the stance support.
- Split Rig Lab into Presets, Structure, Motors, and Test pages with automatic viewport fitting.
- Bumped rig/gait training semantics and isolated v0.7.21 autosave state while preserving explicit checkpoint transfer.
'''
    if anchor in changelog:
        changelog = changelog.replace(anchor, replacement, 1)
    write("CHANGELOG.md", changelog)

    readme = read("README.md")
    anchor = '''- Preserves v0.7.20 learned-state, checkpoint, terrain, curriculum, preview, DPI, clipping, and icon behavior unchanged.
'''
    replacement = '''- Automatic training tunes motor strength, joint range, and stiffness without changing the character's anatomy.
- Shipped bipeds use compact side-view rest poses and gait credit requires a real behind-to-ahead support crossing.
- Quadruped and crawler presets use four articulated two-segment legs; the hexapod uses six independent tripod-phase supports.
- Rig Lab is split into Presets, Structure, Motors, and Test pages and auto-fits every preset in the editor viewport.
- v0.7.21 uses isolated autosave/training semantics; older checkpoints remain explicit transfer inputs.
'''
    if anchor in readme:
        readme = readme.replace(anchor, replacement, 1)
    write("README.md", readme)

    doc_path = "docs/RUNNER_V0721_READABLE_TELEMETRY.md"
    doc = read(doc_path)
    doc = doc.replace(
        "Runner v0.7.21 changes presentation only. Policy dimensions, checkpoints, curriculum thresholds, terrain physics, learned parameters, and the v0.7.20 autosave paths remain compatible.",
        "Runner v0.7.21 makes the dashboard understandable and corrects the rig/gait defects visible in v0.7.20. Policy dimensions and terrain physics remain stable, but corrected anatomy and gait semantics use isolated v0.7.21 autosave state; older checkpoints remain explicit transfer inputs.")
    doc += '''

## Fixed-anatomy training

Automatic curriculum refinement may adjust motor strength, joint range, and bone stiffness. It cannot move nodes, change limb length, widen feet, add or delete branches, or reassign supports. Anatomy changes are manual Rig Lab operations only.

## Side-view gait truth

A biped step is credited only when the swing support begins behind the stance support, leaves the ground, achieves useful clearance, passes ahead, and lands on the opposite contact phase. Permanent split stance, shuffling, treadmill-only progress, and a foot that remains ahead do not count.

## Preset and Rig Lab contract

Bipedal presets use compact side-view silhouettes. Quadruped and four-leg crawler presets have four independent articulated two-segment legs. The hexapod has six independent supports split into alternating tripod phases and no support-to-support brace. Rig Lab separates Presets, Structure, Motors, and Test controls, clips each page, and automatically centers/fits the complete rig in the viewport.
'''
    write(doc_path, doc)


def patch_repository_audit() -> None:
    path = "tools/repository_audit.cmake"
    text = read(path)
    text = replace_once(text,
        "        tests/v0720_ui_tests.cpp\n",
        "        tests/v0720_ui_tests.cpp\n"
        "        tests/v0721_rig_gait_tests.cpp\n",
        "audit rig/gait test file")
    text = replace_once(text,
        '''        "WALK-HUMAN-STATUS-263"
        "WALK-RELEASE-275")
''',
        '''        "WALK-HUMAN-STATUS-263"
        "WALK-AUTO-TUNING-275"
        "WALK-SIDE-GAIT-276"
        "WALK-RIG-LAB-279"
        "WALK-RELEASE-283")
''', "audit current missions")
    text = replace_once(text,
        "        .github/workflows/apply-v0721-readable-telemetry.yml)\n",
        "        .github/workflows/apply-v0721-readable-telemetry.yml\n"
        "        tools/cache_v0721_rig_repair.py\n"
        "        tools/apply_v0721_rig_gait_repair.py\n"
        "        .github/workflows/cache-v0721-rig-repair.yml\n"
        "        .github/workflows/apply-v0721-rig-gait-repair.yml)\n",
        "audit temporary rig repair tools")
    write(path, text)


def main() -> int:
    patch_simulation_header()
    patch_simulation_source()
    patch_ppo()
    patch_autonomy_header()
    patch_autonomy_curriculum()
    patch_state_paths()
    patch_ui_layout()
    patch_app()
    patch_cmake()
    patch_docs()
    patch_repository_audit()
    print("Runner v0.7.21 rig, gait, and Rig Lab repair applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
