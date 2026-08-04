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


def patch_state() -> None:
    text = read("src/simulation.hpp")
    text = replace_once(text,
        "        float duck_press_hold_seconds_{};\n"
        "        float duck_press_max_penetration_{};",
        "        float duck_press_hold_seconds_{};\n"
        "        float duck_body_contact_seconds_{};\n"
        "        float duck_press_max_penetration_{};",
        "duck body-contact grace state")
    write("src/simulation.hpp", text)

    text = read("src/simulation.cpp")
    text = replace_once(text,
        "        duck_press_hold_seconds_ = 0.0f;\n"
        "        duck_press_max_penetration_ = 0.0f;",
        "        duck_press_hold_seconds_ = 0.0f;\n"
        "        duck_body_contact_seconds_ = 0.0f;\n"
        "        duck_press_max_penetration_ = 0.0f;",
        "duck body-contact grace reset")
    write("src/simulation.cpp", text)


def patch_balance_pose() -> None:
    text = read("src/simulation.cpp")
    start = text.index("    void Environment::stabilize_balance_posture() noexcept\n")
    end = text.index("    void Environment::stabilize_duck_posture() noexcept\n", start)
    replacement = r'''    void Environment::stabilize_balance_posture() noexcept
    {
        if (course_stage_ != CourseStage::balance
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node))
            return;

        Vec2 rest_support{};
        Vec2 current_support{};
        std::size_t support_count = 0u;
        auto accumulate = [&](std::size_t index)
        {
            if (index >= blueprint_.nodes.size() || index >= particles_.size())
                return;
            rest_support += blueprint_.nodes[index];
            current_support += particles_[index].position;
            ++support_count;
        };
        accumulate(blueprint_.left_contact_node);
        accumulate(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate(node);
        if (support_count == 0u)
            return;

        rest_support /= static_cast<float>(support_count);
        current_support /= static_cast<float>(support_count);
        const bool horizontal = blueprint_.horizontal_body_plan();
        auto guide = [&](std::size_t node, float strength, float maximum_step)
        {
            if (node >= particles_.size() || node >= blueprint_.nodes.size()
                || blueprint_.is_support_seed(node))
                return;
            const Vec2 target = current_support + (blueprint_.nodes[node] - rest_support);
            Vec2 correction = target - particles_[node].position;
            const float magnitude = length(correction);
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const Vec2 applied = correction * strength;
            particles_[node].position += applied;
            particles_[node].previous += applied * 0.94f;
        };

        if (horizontal)
        {
            // Horizontal body plans need every articulated body link held near
            // its authored support-relative pose during the initial Stand
            // lesson. Guiding only root/torso/head leaves long multi-leg rigs
            // free to invert around their foot plates before PPO can learn.
            for (std::size_t node = 0; node < particles_.size(); ++node)
                guide(node, 0.22f, 0.060f);
        }
        else
        {
            guide(blueprint_.root_node, 0.16f, 0.035f);
            guide(blueprint_.torso_node, 0.12f, 0.030f);
            guide(blueprint_.head_node, 0.08f, 0.025f);
        }
    }

'''
    text = text[:start] + replacement + text[end:]
    write("src/simulation.cpp", text)


def patch_affine_crouch_pose() -> None:
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
        const float rest_height = std::max(0.65f, rest_head_top - rest_support.y);
        const float requested_drop = clamp(rest_head_top - profile.bottom_y,
            0.0f, rest_height * 0.48f);
        const bool recovery_guide = duck_press_contact_seen_
            && requested_drop <= 0.001f;
        if (requested_drop <= 0.001f && !recovery_guide)
            return;

        const float vertical_scale = clamp(
            (rest_height - requested_drop) / rest_height, 0.52f, 1.0f);
        const float horizontal_scale = 1.0f + (1.0f - vertical_scale) * 0.12f;
        const float phase_strength = recovery_guide
            ? 1.0f : clamp(requested_drop / 0.48f, 0.0f, 1.0f);

        for (std::size_t node = 0; node < particles_.size(); ++node)
        {
            if (node >= blueprint_.nodes.size() || blueprint_.is_support_seed(node))
                continue;
            const Vec2 rest_offset = blueprint_.nodes[node] - rest_support;
            Vec2 target = current_support + Vec2{
                rest_offset.x * horizontal_scale,
                rest_offset.y * vertical_scale
            };
            const float floor = ground_height_at(target.x)
                + particles_[node].radius + 0.14f;
            target.y = std::max(target.y, floor);
            if (!recovery_guide)
                target.y = std::min(target.y,
                    profile.bottom_y - particles_[node].radius - 0.035f);

            Vec2 correction = target - particles_[node].position;
            const float magnitude = length(correction);
            constexpr float maximum_step = 0.20f;
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const Vec2 applied = correction * (0.48f * phase_strength);
            particles_[node].position += applied;
            particles_[node].previous += applied;
        }
    }

'''
    text = text[:start] + replacement + text[end:]

    old_loop = '''            stabilize_balance_posture();
            stabilize_duck_posture();
            stabilize_passive_appendages();
            solve_ground(dt);
            solve_course();
            separate_support_clusters();'''
    new_loop = '''            stabilize_balance_posture();
            stabilize_duck_posture();
            stabilize_passive_appendages();
            solve_ground(dt);
            solve_course();
            // Re-apply the authored crouch after collision resolution so the
            // final solver state cannot leave an intermediate knee/body link
            // under the floor or inside the platen.
            stabilize_duck_posture();
            solve_ground(dt);
            solve_course();
            separate_support_clusters();'''
    text = replace_once(text, old_loop, new_loop,
        "post-collision crouch stabilization")
    write("src/simulation.cpp", text)


def patch_contact_grace_and_teacher() -> None:
    text = read("src/simulation.cpp")
    old = '''        duck_active_ = feet_supported && current_uprightness > 0.60f && duck_depth_ >= 0.48f;
        if (!duck_ground_contact_allowed(duck_active_, non_foot_grounded_))
            invalidate(InvalidMotion::duck_body_contact);
        if (duck_active_ && !non_foot_grounded_)
            duck_seconds_ += dt;'''
    new = '''        duck_active_ = feet_supported && current_uprightness > 0.60f && duck_depth_ >= 0.48f;
        const bool disallowed_duck_contact = !duck_ground_contact_allowed(
            duck_active_, non_foot_grounded_);
        if (course_stage_ == CourseStage::duck_press)
        {
            duck_body_contact_seconds_ = disallowed_duck_contact
                ? duck_body_contact_seconds_ + dt
                : std::max(0.0f, duck_body_contact_seconds_ - dt * 3.0f);
            if (duck_body_contact_seconds_ > 0.25f)
                invalidate(InvalidMotion::duck_body_contact);
        }
        else if (disallowed_duck_contact)
        {
            invalidate(InvalidMotion::duck_body_contact);
        }
        if (duck_active_ && !non_foot_grounded_)
            duck_seconds_ += dt;'''
    text = replace_once(text, old, new, "transient duck contact grace")
    write("src/simulation.cpp", text)

    text = read("src/ppo.hpp")
    text = replace_once(text,
        "const float hip = 0.040f * pressure + outward_help - inward_help;\n"
        "            const float knee = 0.30f * pressure;",
        "const float hip = 0.065f * pressure + outward_help - inward_help;\n"
        "            const float knee = 0.52f * pressure;",
        "restore coordinated leg-driven duck teacher")
    write("src/ppo.hpp", text)


def main() -> None:
    patch_state()
    patch_balance_pose()
    patch_affine_crouch_pose()
    patch_contact_grace_and_teacher()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
