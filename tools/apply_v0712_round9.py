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


def rollback_broad_stance_changes() -> None:
    text = read("src/simulation.cpp")
    duck_function = text.index("    void Environment::stabilize_duck_posture() noexcept\n")
    balance_prefix = text[:duck_function]
    old_velocity = "            particles_[node].previous += applied;"
    if balance_prefix.count(old_velocity) != 1:
        raise RuntimeError(
            f"expected one balance-guide velocity update, found {balance_prefix.count(old_velocity)}")
    balance_prefix = balance_prefix.replace(old_velocity,
        "            particles_[node].previous += applied * 0.94f;", 1)
    text = balance_prefix + text[duck_function:]

    old = '''        float rest_support_height = 0.0f;
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
    new = '''        const float head_height_ratio = rest_head_clearance > 1.0e-5f
            ? head_clearance / rest_head_clearance : 0.0f;
        const float support_span_ratio = primary_support_span_ratio();'''
    text = replace_once(text, old, new,
        "original head-clearance stance metric")
    text = replace_once(text,
        "            && (horizontal_body\n"
        "                || std::abs(root_vertical_speed) <= vertical_speed_threshold);",
        "            && std::abs(root_vertical_speed) <= vertical_speed_threshold;",
        "original stable vertical-speed condition")
    text = replace_once(text,
        "            && (horizontal_body || std::abs(root_vertical_speed) <= 2.25f);",
        "            && std::abs(root_vertical_speed) <= 2.25f;",
        "original recoverable vertical-speed condition")

    old_stance = '''        const bool stable_stance_frame = feet_supported
            && support_layout_valid
            && current_uprightness >= upright_threshold
            && head_height_ratio >= head_threshold
            && stance_slip_speed_ <= slip_threshold
            && std::abs(torso_turn_speed_) <= 2.00f
            && current_joint_speed <= 12.0f
            && std::abs(root_vertical_speed) <= vertical_speed_threshold;'''
    new_stance = '''        const bool stable_horizontal_stance_frame = horizontal_body
            && feet_supported && support_layout_valid && !non_foot_grounded_
            && current_uprightness >= upright_threshold
            && stance_slip_speed_ <= slip_threshold
            && std::abs(torso_turn_speed_) <= 2.00f
            && current_joint_speed <= 12.0f;
        const bool stable_stance_frame = stable_horizontal_stance_frame
            || (feet_supported
                && support_layout_valid
                && current_uprightness >= upright_threshold
                && head_height_ratio >= head_threshold
                && stance_slip_speed_ <= slip_threshold
                && std::abs(torso_turn_speed_) <= 2.00f
                && current_joint_speed <= 12.0f
                && std::abs(root_vertical_speed) <= vertical_speed_threshold);'''
    text = replace_once(text, old_stance, new_stance,
        "horizontal authored Stand frame")
    write("src/simulation.cpp", text)


def keep_static_press_recoverable() -> None:
    text = read("src/simulation.cpp")
    old = '''        if (airborne_seconds_ > allowed_airtime)
            invalidate(InvalidMotion::sustained_flight);'''
    count = text.count(old)
    if count < 1:
        raise RuntimeError("missing sustained-flight terminator")
    new = '''        if (course_stage_ != CourseStage::duck_press
            && airborne_seconds_ > allowed_airtime)
            invalidate(InvalidMotion::sustained_flight);'''
    text = text.replace(old, new)
    write("src/simulation.cpp", text)


def main() -> None:
    rollback_broad_stance_changes()
    keep_static_press_recoverable()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
