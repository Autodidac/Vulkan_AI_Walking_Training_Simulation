from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
path = root / 'src/simulation.cpp'
text = path.read_text(encoding='utf-8')
pattern = r"    CreatureBlueprint CreatureBlueprint::chicken\(\)\n    \{.*?\n    \}\n\n    CreatureBlueprint CreatureBlueprint::biped\(\)"
replacement = '''    CreatureBlueprint CreatureBlueprint::chicken()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.40f }, { 0.72f, 2.48f },
            { 0.98f, 3.04f }, { 1.18f, 3.50f }, { 1.54f, 3.46f },
            { -0.92f, 2.64f }, { -1.36f, 2.84f },
            { -0.42f, 1.42f }, { -0.58f, 0.28f },
            { 0.42f, 1.42f }, { 0.58f, 0.28f },
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
        result.active_motor_count = 4;
        result.root_node = 0;
        result.torso_node = 11;
        result.head_node = 3;
        result.left_contact_node = 8;
        result.right_contact_node = 10;
        add_passive_feet(result, 0.17f, 0.29f);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 34.0f, 58.0f, 0.038f, 0.044f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::biped()'''
text, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
if count != 1:
    raise RuntimeError(f'chicken replacement matched {count}')
old = '''        const bool trapped = burial_depth_ > 0.32f
            && (obstruction_mask_ & 0x3u) == 0x3u
            && (left_density + right_density) > 1.4f;
'''
new = '''        const float left_wall = ground_height_at(root.position.x - 0.70f)
            - (root.position.y - root.radius);
        const float right_wall = ground_height_at(root.position.x + 0.70f)
            - (root.position.y - root.radius);
        const bool trapped = burial_depth_ > 0.32f
            && (obstruction_mask_ & 0x3u) == 0x3u
            && left_wall > 0.18f && right_wall > 0.18f;
'''
if text.count(old) != 1:
    raise RuntimeError(f'burial trap replacement matched {text.count(old)}')
path.write_text(text.replace(old, new, 1), encoding='utf-8')
Path(__file__).unlink()
