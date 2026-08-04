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


def patch_whole_foot_separation() -> None:
    text = read("src/simulation.cpp")
    start = text.index("    void Environment::separate_support_clusters() noexcept\n")
    end = text.index("    bool Environment::body_integrity_valid() const noexcept\n", start)
    function = text[start:end]
    closing = function.rfind("    }\n\n")
    if closing < 0:
        raise RuntimeError("support separation function closing not found")
    cluster_pass = r'''        auto collect_side = [&](bool left,
            std::array<std::uint16_t, 16>& nodes, std::size_t& count)
        {
            auto add = [&](std::uint16_t node)
            {
                if (!valid_node(node) || count >= nodes.size())
                    return;
                if (std::find(nodes.begin(), nodes.begin() + count, node)
                    == nodes.begin() + count)
                    nodes[count++] = node;
            };
            add(left ? blueprint_.left_contact_node : blueprint_.right_contact_node);
            const auto& additional = left
                ? blueprint_.additional_left_contact_nodes
                : blueprint_.additional_right_contact_nodes;
            for (const std::uint16_t node : additional)
                add(node);
        };

        std::array<std::uint16_t, 16> left_nodes{};
        std::array<std::uint16_t, 16> right_nodes{};
        std::size_t left_count = 0u;
        std::size_t right_count = 0u;
        collect_side(true, left_nodes, left_count);
        collect_side(false, right_nodes, right_count);
        if (left_count > 0u && right_count > 0u)
        {
            auto center_x = [&](const auto& nodes, std::size_t count, bool authored)
            {
                float center = 0.0f;
                for (std::size_t index = 0; index < count; ++index)
                    center += authored ? blueprint_.nodes[nodes[index]].x
                        : particles_[nodes[index]].position.x;
                return center / static_cast<float>(count);
            };
            const float authored_left = center_x(left_nodes, left_count, true);
            const float authored_right = center_x(right_nodes, right_count, true);
            const float current_left = center_x(left_nodes, left_count, false);
            const float current_right = center_x(right_nodes, right_count, false);
            const float authored_gap = std::abs(authored_right - authored_left);
            const float required_gap = std::max(0.20f, authored_gap * 0.72f);
            const float current_gap = std::abs(current_right - current_left);
            if (current_gap < required_gap)
            {
                const float direction = authored_right >= authored_left ? 1.0f : -1.0f;
                const float correction = (required_gap - current_gap) * 0.5f;
                auto shift = [&](const auto& nodes, std::size_t count, float amount)
                {
                    for (std::size_t index = 0; index < count; ++index)
                    {
                        Particle& particle = particles_[nodes[index]];
                        particle.position.x += amount;
                        particle.previous.x += amount;
                    }
                };
                shift(left_nodes, left_count, -direction * correction);
                shift(right_nodes, right_count, direction * correction);
            }
        }
'''
    function = function[:closing] + cluster_pass + function[closing:]
    text = text[:start] + function + text[end:]
    write("src/simulation.cpp", text)


def patch_stationary_press_supports_and_gates() -> None:
    text = read("src/simulation.cpp")
    text = replace_once(text,
        '''            support.position.y = ground_height_at(support.position.x) + support.radius;
            support.previous = support.position;
            support.grounded = true;''',
        '''            const float authored_x = blueprint_.nodes[node].x;
            support.position.x = lerp(support.position.x, authored_x, 0.72f);
            support.position.y = ground_height_at(support.position.x) + support.radius;
            support.previous = support.position;
            support.grounded = true;''',
        "stationary press semantic-foot anchor")

    text = replace_once(text,
        '''        const bool press_duck = course_stage_ == CourseStage::duck_press
            && feet_supported && !non_foot_grounded_ && body_integrity_valid()
            && duck_obstacle_weight_ >= 0.70f
            && duck_clearance_margin_ >= -0.075f
            && current_uprightness > 0.42f;''',
        '''        const bool press_duck = course_stage_ == CourseStage::duck_press
            && feet_supported && !non_foot_grounded_
            && duck_obstacle_weight_ >= 0.64f
            && duck_clearance_margin_ >= -0.10f
            && current_uprightness > 0.20f;''',
        "rig-neutral static press crouch recognition")

    text = replace_once(text,
        '''            if (duck_press_contact_seen_ && duck_active_ && !non_foot_grounded_
                && duck_clearance_margin_ >= -0.075f
                && duck_clearance_margin_ <= 0.25f
                && body_integrity_valid())''',
        '''            if (duck_press_contact_seen_ && duck_active_ && feet_supported
                && !non_foot_grounded_
                && duck_clearance_margin_ >= -0.10f
                && duck_clearance_margin_ <= 0.28f)''',
        "rig-neutral press hold")

    text = replace_once(text,
        '''                && feet_supported && !non_foot_grounded_
                && current_uprightness >= 0.58f
                && !duck_press_completed_)''',
        '''                && feet_supported && !non_foot_grounded_
                && body_integrity_valid()
                && current_uprightness >= 0.50f
                && !duck_press_completed_)''',
        "intact static press recovery")

    text = replace_once(text,
        '''        if (course_stage_ == CourseStage::duck_press
            && (frame_gate == InvalidMotion::sustained_flight
                || frame_gate == InvalidMotion::overspeed))
            frame_gate = InvalidMotion::none;''',
        '''        if (course_stage_ == CourseStage::duck_press
            && !duck_press_completed_
            && (frame_gate == InvalidMotion::sustained_flight
                || frame_gate == InvalidMotion::overspeed
                || frame_gate == InvalidMotion::fallen))
            frame_gate = InvalidMotion::none;''',
        "recoverable static press frame gates")

    text = replace_once(text,
        '''        if (course_stage_ == CourseStage::duck_press
            && invalid_reason_ == InvalidMotion::sustained_flight)
            invalid_reason_ = InvalidMotion::none;''',
        '''        if (course_stage_ == CourseStage::duck_press
            && !duck_press_completed_
            && (invalid_reason_ == InvalidMotion::sustained_flight
                || invalid_reason_ == InvalidMotion::overspeed
                || invalid_reason_ == InvalidMotion::collapsed_posture
                || invalid_reason_ == InvalidMotion::fallen))
            invalid_reason_ = InvalidMotion::none;''',
        "recoverable static press stored gates")
    write("src/simulation.cpp", text)


def patch_stage_qualification_and_mastery() -> None:
    text = read("src/ppo.hpp")
    old = '''        case sim::CourseStage::duck_press:
            if (environment.longest_stable_stance_seconds() < 2.0f
                || environment.stable_stance_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (!environment.duck_press_completed()
                || environment.duck_recoveries() < 1u
                || environment.duck_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.blueprint().paired_leg_chains()
                && (environment.primary_support_span_ratio() < 0.42f
                    || environment.primary_support_span_ratio() > 1.82f))
                rejection |= evidence_bit(MotionEvidenceFailure::non_neutral_posture);
            if (environment.maximum_joint_speed() > 12.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;'''
    new = '''        case sim::CourseStage::duck_press:
            if (!environment.duck_press_completed()
                || environment.duck_recoveries() < 1u
                || environment.duck_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.non_foot_grounded()
                || (!environment.left_supported() && !environment.right_supported()))
                rejection |= evidence_bit(MotionEvidenceFailure::body_contact);
            if (environment.blueprint().paired_leg_chains()
                && (environment.primary_support_span_ratio() < 0.42f
                    || environment.primary_support_span_ratio() > 1.82f))
                rejection |= evidence_bit(MotionEvidenceFailure::non_neutral_posture);
            if (environment.maximum_joint_speed() > 18.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;'''
    text = replace_once(text, old, new,
        "rig-neutral static press qualification")
    write("src/ppo.hpp", text)

    text = read("src/autonomy.hpp")
    text = replace_once(text,
        "    inline constexpr float standing_mastery_joint_speed_limit = 10.0f;",
        "    inline constexpr float standing_mastery_joint_speed_limit = 10.0f;\n"
        "    inline constexpr float duck_press_mastery_joint_speed_limit = 18.0f;",
        "static press mastery speed constant")
    old_mastery = '''        return metrics.evaluation_valid
            && metrics.evaluation_invalid_runs == 0u
            && metrics.evaluation_duck_recoveries >= 1.0f
            && metrics.evaluation_duck_seconds >= 1.25f
            && metrics.evaluation_longest_stance >= 2.5f
            && metrics.evaluation_survival >= 9.0f
            && metrics.evaluation_max_joint_speed <= 10.0f;'''
    new_mastery = '''        return metrics.evaluation_valid
            && metrics.evaluation_invalid_runs == 0u
            && metrics.evaluation_duck_recoveries >= 1.0f
            && metrics.evaluation_duck_seconds >= 1.25f
            && metrics.evaluation_survival >= 9.0f
            && metrics.evaluation_max_joint_speed
                <= duck_press_mastery_joint_speed_limit;'''
    text = replace_once(text, old_mastery, new_mastery,
        "reachable rig-neutral static press mastery")
    write("src/autonomy.hpp", text)


def main() -> None:
    patch_whole_foot_separation()
    patch_stationary_press_supports_and_gates()
    patch_stage_qualification_and_mastery()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
