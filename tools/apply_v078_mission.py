from pathlib import Path

root = Path(__file__).resolve().parents[1]

def replace_once(path: str, old: str, new: str) -> None:
    file = root / path
    text = file.read_text(encoding='utf-8')
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: expected one match, found {count}')
    file.write_text(text.replace(old, new, 1), encoding='utf-8')

replace_once('src/simulation.hpp', '''        [[nodiscard]] bool paired_leg_chains() const noexcept
        {
            return !monopedal_gait() && active_motor_count >= 4u
                && motors[0].enabled && motors[1].enabled
                && motors[2].enabled && motors[3].enabled
                && motors[0].pivot == motors[2].pivot
                && motors[1].a == motors[0].pivot
                && motors[3].a == motors[2].pivot;
        }

''', '''        [[nodiscard]] bool paired_leg_chains() const noexcept
        {
            return !monopedal_gait() && active_motor_count >= 4u
                && motors[0].enabled && motors[1].enabled
                && motors[2].enabled && motors[3].enabled
                && motors[0].pivot == motors[2].pivot
                && motors[1].a == motors[0].pivot
                && motors[3].a == motors[2].pivot;
        }
        [[nodiscard]] bool horizontal_body_plan() const noexcept
        {
            if (root_node >= nodes.size() || head_node >= nodes.size())
                return false;
            const Vec2 head_offset = nodes[head_node] - nodes[root_node];
            return active_motor_count <= 4u
                && std::abs(head_offset.x) >= std::abs(head_offset.y) * 0.72f;
        }

''')

replace_once('src/simulation.cpp', '''        const bool stable_stance_frame = feet_supported
            && support_layout_valid
            && current_uprightness >= 0.84f
            && head_height_ratio >= 0.62f
            && stance_slip_speed_ <= 0.10f
            && std::abs(torso_turn_speed_) <= 2.00f
            && current_joint_speed <= 12.0f
            && std::abs(root_vertical_speed) <= 1.50f;
        if (stable_stance_frame)
        {
            stance_failure_grace_seconds_ = std::max(
                0.0f, stance_failure_grace_seconds_ - dt * 2.0f);
            stable_stance_seconds_ += dt;
        }
        else if (!catastrophic_stance_failure
            && stance_failure_grace_seconds_ < 0.60f)
        {
            stance_failure_grace_seconds_ += dt;
            stable_stance_seconds_ = std::max(
                0.0f, stable_stance_seconds_ - dt * 0.10f);
        }
''', '''        const bool horizontal_body = blueprint_.horizontal_body_plan();
        const float upright_threshold = horizontal_body ? 0.78f : 0.84f;
        const float head_threshold = horizontal_body ? 0.58f : 0.62f;
        const float slip_threshold = horizontal_body ? 0.18f : 0.10f;
        const float vertical_speed_threshold = horizontal_body ? 1.85f : 1.50f;
        const float stance_grace_limit = horizontal_body ? 1.40f : 0.60f;
        const bool stable_stance_frame = feet_supported
            && support_layout_valid
            && current_uprightness >= upright_threshold
            && head_height_ratio >= head_threshold
            && stance_slip_speed_ <= slip_threshold
            && std::abs(torso_turn_speed_) <= 2.00f
            && current_joint_speed <= 12.0f
            && std::abs(root_vertical_speed) <= vertical_speed_threshold;
        if (stable_stance_frame)
        {
            stance_failure_grace_seconds_ = std::max(
                0.0f, stance_failure_grace_seconds_ - dt * 2.0f);
            stable_stance_seconds_ += dt;
        }
        else if (!catastrophic_stance_failure
            && stance_failure_grace_seconds_ < stance_grace_limit)
        {
            stance_failure_grace_seconds_ += dt;
            stable_stance_seconds_ = std::max(
                0.0f, stable_stance_seconds_ - dt * (horizontal_body ? 0.035f : 0.10f));
        }
''')

replace_once('src/simulation.cpp', '''        const float center_ground = ground_height_at(root.position.x);
        const float left_cost = left_density + std::max(0.0f,
            ground_height_at(root.position.x - 1.25f) - center_ground) * 3.0f;
        const float right_cost = right_density + std::max(0.0f,
            ground_height_at(root.position.x + 1.25f) - center_ground) * 3.0f;
        const float delta_cost = left_cost - right_cost;
        free_space_direction_ = std::abs(delta_cost) < 0.08f
            ? 0.0f : (delta_cost > 0.0f ? 1.0f : -1.0f);
''', '''        const float left_surface = std::min(
            ground_height_at(root.position.x - 0.85f),
            ground_height_at(root.position.x - 1.35f));
        const float right_surface = std::min(
            ground_height_at(root.position.x + 0.85f),
            ground_height_at(root.position.x + 1.35f));
        const float left_space = root.position.y + root.radius - left_surface
            - left_density * 0.18f;
        const float right_space = root.position.y + root.radius - right_surface
            - right_density * 0.18f;
        const float space_delta = right_space - left_space;
        free_space_direction_ = std::abs(space_delta) < 0.06f
            ? 0.0f : (space_delta > 0.0f ? 1.0f : -1.0f);
''')

replace_once('tests/deformable_terrain_tests.cpp', '''    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.80f,12.0f,0.22f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.25f,10.0f,0.18f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x+0.55f,1.0f,0.25f);
''', '''    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-1.05f,14.0f,0.22f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.35f,10.0f,0.18f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x+0.10f,7.0f,0.20f);
''')

Path(__file__).unlink()
