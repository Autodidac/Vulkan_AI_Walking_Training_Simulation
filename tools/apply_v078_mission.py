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
        const bool recoverable_horizontal_stance = horizontal_body
            && feet_supported && support_layout_valid && !non_foot_grounded_
            && current_uprightness >= 0.70f && head_height_ratio >= 0.54f
            && stance_slip_speed_ <= 0.35f
            && std::abs(torso_turn_speed_) <= 2.50f
            && current_joint_speed <= 12.0f
            && std::abs(root_vertical_speed) <= 2.25f;
        if (stable_stance_frame)
        {
            stance_failure_grace_seconds_ = std::max(
                0.0f, stance_failure_grace_seconds_ - dt * 2.0f);
            stable_stance_seconds_ += dt;
        }
        else if (recoverable_horizontal_stance)
        {
            stance_failure_grace_seconds_ = std::min(
                stance_grace_limit, stance_failure_grace_seconds_ + dt);
            stable_stance_seconds_ += dt * 0.60f;
        }
        else if (!catastrophic_stance_failure
            && stance_failure_grace_seconds_ < stance_grace_limit)
        {
            stance_failure_grace_seconds_ += dt;
            stable_stance_seconds_ = std::max(
                0.0f, stable_stance_seconds_ - dt * 0.10f);
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

replace_once('src/simulation.cpp', '''        const float left_wall = ground_height_at(root.position.x - 0.70f)
            - (root.position.y - root.radius);
        const float right_wall = ground_height_at(root.position.x + 0.70f)
            - (root.position.y - root.radius);
        const bool trapped = burial_depth_ > 0.32f
            && (obstruction_mask_ & 0x3u) == 0x3u
            && left_wall > 0.18f && right_wall > 0.18f;
''', '''        auto node_burial_depth = [&](std::uint16_t node) noexcept
        {
            if (!valid_node(node))
                return 0.0f;
            const Particle& particle = particles_[node];
            return ground_height_at(particle.position.x)
                - (particle.position.y - particle.radius);
        };
        const float head_burial = node_burial_depth(blueprint_.head_node);
        const float torso_burial = node_burial_depth(blueprint_.torso_node);
        const float left_wall = ground_height_at(root.position.x - 0.70f)
            - (root.position.y - root.radius);
        const float right_wall = ground_height_at(root.position.x + 0.70f)
            - (root.position.y - root.radius);
        const bool trapped = burial_depth_ > 0.32f
            && head_burial > 0.18f && torso_burial > 0.18f
            && left_wall > 0.18f && right_wall > 0.18f;
''')

replace_once('tests/deformable_terrain_tests.cpp', '''        static Vec2 root_position(const Environment& environment) noexcept
        {
            return environment.particles_[environment.blueprint_.root_node].position;
        }

''', '''        static Vec2 root_position(const Environment& environment) noexcept
        {
            return environment.particles_[environment.blueprint_.root_node].position;
        }

        static Vec2 node_position(const Environment& environment,
            std::uint16_t node) noexcept
        {
            return environment.particles_[node].position;
        }

''')

replace_once('tests/deformable_terrain_tests.cpp', '''    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.80f,12.0f,0.22f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.25f,10.0f,0.18f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x+0.55f,1.0f,0.25f);
''', '''    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-1.05f,14.0f,0.22f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.35f,10.0f,0.18f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x+0.10f,7.0f,0.20f);
''')

replace_once('tests/deformable_terrain_tests.cpp', '''    const Vec2 trapped_root=sim::EnvironmentTestAccess::root_position(trapped);
    constexpr std::array<float,6> burial_offsets{-0.90f,-0.55f,-0.20f,0.20f,0.55f,0.90f};
    for(float offset:burial_offsets)
        sim::EnvironmentTestAccess::deposit_world(trapped,trapped_root.x+offset,12.0f,0.20f);
''', '''    const Vec2 trapped_root=sim::EnvironmentTestAccess::root_position(trapped);
    const Vec2 trapped_head=sim::EnvironmentTestAccess::node_position(
        trapped,trapped.blueprint().head_node);
    const Vec2 trapped_torso=sim::EnvironmentTestAccess::node_position(
        trapped,trapped.blueprint().torso_node);
    constexpr std::array<float,7> burial_offsets{-1.05f,-0.70f,-0.35f,0.0f,0.35f,0.70f,1.05f};
    for(float offset:burial_offsets)
        sim::EnvironmentTestAccess::deposit_world(trapped,trapped_root.x+offset,22.0f,0.20f);
    sim::EnvironmentTestAccess::deposit_world(trapped,trapped_head.x,26.0f,0.18f);
    sim::EnvironmentTestAccess::deposit_world(trapped,trapped_torso.x,26.0f,0.18f);
''')

Path(__file__).unlink()
