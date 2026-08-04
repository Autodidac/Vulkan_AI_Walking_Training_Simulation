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


def patch_fixed_press_station() -> None:
    text = read("src/simulation.cpp")
    old = '''                const float authored_center_x = (minimum_x + maximum_x) * 0.5f;
                const float half_width = clamp(
                    (maximum_x - minimum_x) * 0.5f + 0.34f, 0.82f, 2.20f);
                const DuckPressProfile profile = duck_press_profile(
                    elapsed_seconds_, course_difficulty_, rest_head_top);
                // The station remains fixed in authored world space, but spans
                // the complete rig instead of crushing only the root end of
                // quadrupeds, crawlers, and hexapods.
                const float press_anchor_x = authored_center_x;'''
    new = '''                const float press_anchor_x = blueprint_.root_node < blueprint_.nodes.size()
                    ? blueprint_.nodes[blueprint_.root_node].x : 0.0f;
                const float authored_reach = std::max(
                    std::abs(minimum_x - press_anchor_x),
                    std::abs(maximum_x - press_anchor_x));
                const float half_width = clamp(authored_reach + 0.34f, 0.82f, 2.80f);
                const DuckPressProfile profile = duck_press_profile(
                    elapsed_seconds_, course_difficulty_, rest_head_top);
                // The press stays fixed over the authored station even if the
                // live rig slides, while still spanning the complete body plan.'''
    text = replace_once(text, old, new, "fixed full-rig press station")
    write("src/simulation.cpp", text)


def patch_full_chain_crouch_guide() -> None:
    text = read("src/simulation.cpp")
    start = text.index("    void Environment::stabilize_duck_posture() noexcept\n")
    end = text.index("    void Environment::solve_motor(const MotorConstraint& motor, float action) noexcept\n", start)
    replacement = r'''    void Environment::stabilize_duck_posture() noexcept
    {
        if (course_stage_ != CourseStage::duck_press
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node))
            return;

        Vec2 rest_support{};
        Vec2 current_support{};
        std::size_t support_count = 0u;
        auto accumulate_support = [&](std::size_t node)
        {
            if (node >= blueprint_.nodes.size() || node >= particles_.size())
                return;
            rest_support += blueprint_.nodes[node];
            current_support += particles_[node].position;
            ++support_count;
        };
        accumulate_support(blueprint_.left_contact_node);
        accumulate_support(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate_support(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate_support(node);
        if (support_count == 0u)
            return;
        rest_support /= static_cast<float>(support_count);
        current_support /= static_cast<float>(support_count);

        const float rest_head_top = blueprint_.nodes[blueprint_.head_node].y
            + particles_[blueprint_.head_node].radius;
        const DuckPressProfile profile = duck_press_profile(
            elapsed_seconds_, course_difficulty_, rest_head_top);
        const float requested_drop = clamp(rest_head_top - profile.bottom_y,
            0.0f, 1.05f);
        const bool recovery_guide = duck_press_contact_seen_
            && requested_drop <= 0.001f;
        if (requested_drop <= 0.001f && !recovery_guide)
            return;

        const float root_rest_y = blueprint_.nodes[blueprint_.root_node].y;
        const float leg_height = std::max(0.30f, root_rest_y - rest_support.y);
        const float phase_strength = recovery_guide
            ? 1.0f : clamp(requested_drop / 0.86f, 0.0f, 1.0f);

        auto guide = [&](std::size_t node)
        {
            if (node >= particles_.size() || node >= blueprint_.nodes.size()
                || blueprint_.is_support_seed(node))
                return;

            const Vec2 rest_offset = blueprint_.nodes[node] - rest_support;
            const float height_fraction = clamp(
                (blueprint_.nodes[node].y - rest_support.y) / leg_height,
                0.0f, 1.0f);
            const bool body_node = node == blueprint_.root_node
                || node == blueprint_.torso_node || node == blueprint_.head_node
                || blueprint_.nodes[node].y >= root_rest_y - 0.12f;
            const float drop_weight = body_node
                ? 1.0f : lerp(0.18f, 0.68f, height_fraction);

            Vec2 target = current_support + rest_offset;
            target.y -= requested_drop * drop_weight;
            if (!body_node && requested_drop > 0.001f)
            {
                float direction = 1.0f;
                const float relative_x = blueprint_.nodes[node].x
                    - blueprint_.nodes[blueprint_.root_node].x;
                if (relative_x < -0.04f)
                    direction = -1.0f;
                else if (relative_x > 0.04f)
                    direction = 1.0f;
                target.x += direction * requested_drop
                    * (1.0f - height_fraction) * 0.30f;
            }
            const float floor = ground_height_at(target.x)
                + particles_[node].radius + 0.13f;
            target.y = std::max(target.y, floor);

            Vec2 correction = target - particles_[node].position;
            const float maximum_step = body_node ? 0.055f : 0.040f;
            const float magnitude = length(correction);
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const float strength = body_node ? 0.20f : 0.14f;
            const Vec2 applied = correction * strength * phase_strength;
            particles_[node].position += applied;
            particles_[node].previous += applied;
        };

        // Every non-support link receives a support-relative target. Feet stay
        // authoritative, intermediate joints bend outward instead of dropping
        // into the floor, and the same guide returns the body to its authored
        // stance after the physical press retracts.
        for (std::size_t node = 0; node < particles_.size(); ++node)
            guide(node);
    }

'''
    text = text[:start] + replacement + text[end:]
    write("src/simulation.cpp", text)


def patch_hexapod_support_phases() -> None:
    text = read("src/simulation.cpp")
    old = '''        result.left_contact_node = 3;
        result.right_contact_node = 4;
        result.additional_left_contact_nodes = { 5, 7 };
        result.additional_right_contact_nodes = { 6, 8 };'''
    new = '''        // Each rigid foot plate belongs to one gait phase. Splitting the
        // endpoints of a single plate across left/right semantics made the
        // Stand teacher command the same plate in opposite directions.
        result.left_contact_node = 3;
        result.right_contact_node = 5;
        result.additional_left_contact_nodes = { 4, 7, 8 };
        result.additional_right_contact_nodes = { 6 };'''
    text = replace_once(text, old, new, "hexapod plate-phase semantics")
    write("src/simulation.cpp", text)


def patch_teacher_pressure() -> None:
    text = read("src/ppo.hpp")
    text = replace_once(text,
        "            : compact_support_teacher_action(environment, pressure);",
        "            : compact_support_teacher_action(environment, pressure * 0.35f);",
        "bounded compact crouch teacher")
    text = replace_once(text,
        "const float hip = 0.065f * pressure + outward_help - inward_help;\n"
        "            const float knee = 0.56f * pressure;",
        "const float hip = 0.040f * pressure + outward_help - inward_help;\n"
        "            const float knee = 0.30f * pressure;",
        "bounded paired crouch teacher")
    write("src/ppo.hpp", text)


def main() -> None:
    patch_fixed_press_station()
    patch_full_chain_crouch_guide()
    patch_hexapod_support_phases()
    patch_teacher_pressure()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
