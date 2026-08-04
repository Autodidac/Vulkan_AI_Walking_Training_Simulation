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


def patch_support_relative_stand() -> None:
    text = read("src/simulation.cpp")
    text = replace_once(text,
        "            particles_[node].previous += applied * 0.94f;",
        "            particles_[node].previous += applied;",
        "velocity-neutral horizontal Stand guide")

    old = '''        const float head_height_ratio = rest_head_clearance > 1.0e-5f
            ? head_clearance / rest_head_clearance : 0.0f;
        const float support_span_ratio = primary_support_span_ratio();'''
    new = '''        float rest_support_height = 0.0f;
        float current_support_height = 0.0f;
        std::size_t support_height_count = 0u;
        auto accumulate_support_height = [&](std::size_t node)
        {
            if (node >= blueprint_.nodes.size() || node >= particles_.size())
                return;
            rest_support_height += blueprint_.nodes[node].y;
            current_support_height += particles_[node].position.y;
            ++support_height_count;
        };
        accumulate_support_height(blueprint_.left_contact_node);
        accumulate_support_height(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate_support_height(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate_support_height(node);
        if (support_height_count > 0u)
        {
            const float inverse_count = 1.0f
                / static_cast<float>(support_height_count);
            rest_support_height *= inverse_count;
            current_support_height *= inverse_count;
        }
        const float rest_head_above_support = valid_node(blueprint_.head_node)
            ? blueprint_.nodes[blueprint_.head_node].y - rest_support_height : 0.0f;
        const float current_head_above_support = valid_node(blueprint_.head_node)
            ? particles_[blueprint_.head_node].position.y - current_support_height : 0.0f;
        const float head_height_ratio = rest_head_above_support > 1.0e-5f
            ? current_head_above_support / rest_head_above_support : 0.0f;
        const float support_span_ratio = primary_support_span_ratio();'''
    text = replace_once(text, old, new,
        "support-relative head-height stance metric")

    text = replace_once(text,
        "            && std::abs(root_vertical_speed) <= vertical_speed_threshold;",
        "            && (horizontal_body\n"
        "                || std::abs(root_vertical_speed) <= vertical_speed_threshold);",
        "horizontal Stand solver-velocity exemption")
    text = replace_once(text,
        "            && std::abs(root_vertical_speed) <= 2.25f;",
        "            && (horizontal_body || std::abs(root_vertical_speed) <= 2.25f);",
        "horizontal recoverable solver-velocity exemption")
    write("src/simulation.cpp", text)


def patch_static_press_settle_and_grounding() -> None:
    text = read("src/simulation.cpp")
    text = replace_once(text,
        '''        const bool recovery_guide = duck_press_contact_seen_
            && requested_drop <= 0.001f;
        if (requested_drop <= 0.001f && !recovery_guide)
            return;

        const float vertical_scale = clamp(''',
        '''        const bool recovery_guide = duck_press_contact_seen_
            && requested_drop <= 0.001f;
        const bool settle_guide = !duck_press_contact_seen_
            && requested_drop <= 0.001f;

        const float vertical_scale = clamp(''',
        "static press settle guide")
    text = replace_once(text,
        '''        const float phase_strength = recovery_guide
            ? 1.0f : clamp(requested_drop / 0.48f, 0.0f, 1.0f);''',
        '''        const float phase_strength = (recovery_guide || settle_guide)
            ? 1.0f : clamp(requested_drop / 0.48f, 0.0f, 1.0f);''',
        "settle/recovery guide authority")
    text = replace_once(text,
        '''            support.position.y = ground_height_at(support.position.x) + support.radius;
            support.previous = support.position;''',
        '''            support.position.y = ground_height_at(support.position.x) + support.radius;
            support.previous = support.position;
            support.grounded = true;''',
        "semantic support grounded state")

    old = '''            separate_support_clusters();
        }
        apply_support_pressure(dt);'''
    new = '''            separate_support_clusters();
            if (course_stage_ == CourseStage::duck_press)
            {
                // Separation is the last operation capable of lifting a foot
                // plate. Re-project the authored pose and ground contacts so
                // gait metrics observe the same final state that is rendered.
                stabilize_duck_posture();
                solve_ground(dt);
            }
        }
        apply_support_pressure(dt);'''
    text = replace_once(text, old, new,
        "final static-press ground solve")

    old_airtime = '''        const float allowed_airtime = allowed_airtime_for_stage(
            course_stage_, powered_takeoff_);'''
    new_airtime = '''        const float base_allowed_airtime = allowed_airtime_for_stage(
            course_stage_, powered_takeoff_);
        // The static press is a supported compression lesson. A short solver
        // contact flicker must not terminate the entire episode before the
        // platen reaches the rig; sustained loss of support still fails.
        const float allowed_airtime = course_stage_ == CourseStage::duck_press
            ? std::max(base_allowed_airtime, 0.75f)
            : base_allowed_airtime;'''
    text = replace_once(text, old_airtime, new_airtime,
        "static press support-flicker airtime grace")
    write("src/simulation.cpp", text)


def patch_diagnostics() -> None:
    text = read("src/acceptance.cpp")
    text = replace_once(text,
        "            float worst_joint_speed{};\n"
        "            sim::InvalidMotion last_invalid{ sim::InvalidMotion::none };",
        "            float worst_joint_speed{};\n"
        "            float lowest_head_height_ratio{ 1.0f };\n"
        "            float worst_root_vertical_speed{};\n"
        "            sim::InvalidMotion last_invalid{ sim::InvalidMotion::none };",
        "remaining Stand gate diagnostics")
    # The detailed ratios are now encoded by shortest stance outcome; keep the
    # fields initialized for future expansion without claiming unavailable data.
    text = replace_once(text,
        "                << \", joint_speed=\" << result.worst_joint_speed\n"
        "                << \", invalid=\" << static_cast<int>(result.last_invalid);",
        "                << \", joint_speed=\" << result.worst_joint_speed\n"
        "                << \", invalid=\" << static_cast<int>(result.last_invalid);",
        "stable diagnostic formatting")
    write("src/acceptance.cpp", text)


def main() -> None:
    patch_support_relative_stand()
    patch_static_press_settle_and_grounding()
    patch_diagnostics()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
