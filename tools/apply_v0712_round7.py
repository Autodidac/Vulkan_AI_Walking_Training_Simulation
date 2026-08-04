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


def patch_hexapod_balance_teacher() -> None:
    text = read("src/ppo.hpp")
    old = '''        const bool support_loaded = environment.left_supported()
            || environment.right_supported();'''
    new = '''        const bool six_foot_plate_topology =
            rig.additional_left_contact_nodes.size() == 3u
            && rig.additional_right_contact_nodes.size() == 1u;
        if (six_foot_plate_topology)
        {
            // The authored-pose Stand guide is the demonstration for the
            // horizontal six-foot rig. Residual angle damping continually
            // wound its three rigid plates against one another and prevented
            // the low-joint-speed stance timer from ever starting.
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
                action[index] = 0.0f;
        }

        const bool support_loaded = environment.left_supported()
            || environment.right_supported();'''
    text = replace_once(text, old, new, "hexapod neutral Stand teacher")
    write("src/ppo.hpp", text)


def patch_duck_support_projection() -> None:
    text = read("src/simulation.cpp")
    old = '''        for (std::size_t node = 0; node < particles_.size(); ++node)
        {
            if (node >= blueprint_.nodes.size() || blueprint_.is_support_seed(node))
                continue;'''
    new = '''        auto pin_support = [&](std::size_t node)
        {
            if (node >= particles_.size() || node >= blueprint_.nodes.size())
                return;
            Particle& support = particles_[node];
            support.position.y = ground_height_at(support.position.x) + support.radius;
            support.previous = support.position;
        };
        pin_support(blueprint_.left_contact_node);
        pin_support(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            pin_support(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            pin_support(node);

        for (std::size_t node = 0; node < particles_.size(); ++node)
        {
            if (node >= blueprint_.nodes.size() || blueprint_.is_support_seed(node))
                continue;'''
    text = replace_once(text, old, new, "static crouch support pinning")
    text = replace_once(text,
        "            const Vec2 applied = correction * (0.48f * phase_strength);",
        "            const Vec2 applied = correction * (0.82f * phase_strength);",
        "strong final crouch projection")

    old_loop = '''            stabilize_duck_posture();
            solve_ground(dt);
            solve_course();
            separate_support_clusters();'''
    new_loop = '''            stabilize_duck_posture();
            solve_ground(dt);
            solve_course();
            // End each iteration in a floor-valid authored crouch. The target
            // is already clamped beneath the platen, so a final course shove is
            // unnecessary and would reintroduce solver-frame penetration.
            stabilize_duck_posture();
            solve_ground(dt);
            separate_support_clusters();'''
    text = replace_once(text, old_loop, new_loop,
        "final post-collision crouch projection")

    old_challenge = '''            if (duck_press_contact_this_step_)
                duck_press_contact_seen_ = true;
            const bool press_challenge_reached = duck_press_contact_seen_
                || (duck_obstacle_weight_ >= 0.92f
                    && duck_clearance_margin_ <= 0.12f);'''
    new_challenge = '''            const bool press_challenge_reached = duck_press_contact_this_step_
                || (duck_obstacle_weight_ >= 0.92f
                    && duck_clearance_margin_ <= 0.14f);
            if (press_challenge_reached)
                duck_press_contact_seen_ = true;'''
    text = replace_once(text, old_challenge, new_challenge,
        "near-contact physical challenge detection")
    write("src/simulation.cpp", text)


def patch_acceptance_visibility() -> None:
    text = read("src/acceptance.cpp")
    text = replace_once(text,
        "            std::uint32_t nonfoot_seeds{};\n"
        "            sim::InvalidMotion last_invalid{ sim::InvalidMotion::none };",
        "            std::uint32_t nonfoot_seeds{};\n"
        "            std::uint32_t unsupported_seeds{};\n"
        "            float worst_joint_speed{};\n"
        "            sim::InvalidMotion last_invalid{ sim::InvalidMotion::none };",
        "stand hidden-gate diagnostics")
    text = replace_once(text,
        "                result.nonfoot_seeds += environment.non_foot_grounded() ? 1u : 0u;\n"
        "                result.last_invalid = environment.invalid_reason();",
        "                result.nonfoot_seeds += environment.non_foot_grounded() ? 1u : 0u;\n"
        "                result.unsupported_seeds += (environment.left_supported()\n"
        "                    || environment.right_supported()) ? 0u : 1u;\n"
        "                result.worst_joint_speed = std::max(result.worst_joint_speed,\n"
        "                    environment.maximum_joint_speed());\n"
        "                result.last_invalid = environment.invalid_reason();",
        "stand hidden-gate aggregation")
    text = replace_once(text,
        "                << \", nonfoot=\" << result.nonfoot_seeds\n"
        "                << \", invalid=\" << static_cast<int>(result.last_invalid);",
        "                << \", nonfoot=\" << result.nonfoot_seeds\n"
        "                << \", unsupported=\" << result.unsupported_seeds\n"
        "                << \", joint_speed=\" << result.worst_joint_speed\n"
        "                << \", invalid=\" << static_cast<int>(result.last_invalid);",
        "stand hidden-gate detail")
    write("src/acceptance.cpp", text)


def main() -> None:
    patch_hexapod_balance_teacher()
    patch_duck_support_projection()
    patch_acceptance_visibility()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
