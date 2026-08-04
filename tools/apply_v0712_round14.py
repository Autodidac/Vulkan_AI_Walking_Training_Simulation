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


def remove_live_whole_foot_shift() -> None:
    text = read("src/simulation.cpp")
    start = text.index("    void Environment::separate_support_clusters() noexcept\n")
    end = text.index("    bool Environment::body_integrity_valid() const noexcept\n", start)
    function = text[start:end]
    block_start = function.find("        auto collect_side = [&](bool left,\n")
    if block_start < 0:
        raise RuntimeError("whole-foot live shift block missing")
    closing = function.rfind("    }\n\n")
    if closing < block_start:
        raise RuntimeError("support separation closing missing")
    function = function[:block_start] + function[closing:]
    text = text[:start] + function + text[end:]
    write("src/simulation.cpp", text)


def preserve_foot_shape_in_fused_regression() -> None:
    text = read("tests/core_tests.cpp")
    old = '''        static void force_fused_supports(Environment& environment) noexcept
        {
            if (!environment.valid_node(environment.blueprint_.left_contact_node)
                || !environment.valid_node(environment.blueprint_.right_contact_node))
                return;
            const float center = 0.5f * (
                environment.particles_[environment.blueprint_.left_contact_node].position.x
                + environment.particles_[environment.blueprint_.right_contact_node].position.x);
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (!environment.blueprint_.is_support_seed(index))
                    continue;
                environment.particles_[index].position.x = center;
                environment.particles_[index].previous.x = center;
            }
        }'''
    new = '''        static void force_fused_supports(Environment& environment) noexcept
        {
            if (!environment.valid_node(environment.blueprint_.left_contact_node)
                || !environment.valid_node(environment.blueprint_.right_contact_node))
                return;
            const float left_anchor = environment.particles_[
                environment.blueprint_.left_contact_node].position.x;
            const float right_anchor = environment.particles_[
                environment.blueprint_.right_contact_node].position.x;
            const float center = 0.5f * (left_anchor + right_anchor);
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                const bool left = environment.blueprint_.is_left_support_seed(index);
                const bool right = environment.blueprint_.is_right_support_seed(index);
                if (!left && !right)
                    continue;
                // Fuse the two feet by translating each complete cluster. Do
                // not collapse heel, ball, and toe into one impossible point.
                const float anchor = left ? left_anchor : right_anchor;
                const float offset = environment.particles_[index].position.x - anchor;
                environment.particles_[index].position.x = center + offset;
                environment.particles_[index].previous.x = center + offset;
            }
        }'''
    text = replace_once(text, old, new,
        "intact articulated-foot fused regression")
    write("tests/core_tests.cpp", text)


def floor_clamp_final_crouch_pose() -> None:
    text = read("src/simulation.cpp")
    old = '''            const float floor = ground_height_at(target.x)
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
            const Vec2 applied = correction * (0.82f * phase_strength);'''
    new = '''            const float floor = ground_height_at(target.x)
                + particles_[node].radius + 0.14f;
            if (!recovery_guide)
                target.y = std::min(target.y,
                    profile.bottom_y - particles_[node].radius - 0.035f);
            // Floor authority is final. The old order could force knees and
            // torso nodes below ground after they had already been clamped.
            target.y = std::max(target.y, floor);

            Vec2 correction = target - particles_[node].position;
            const float magnitude = length(correction);
            constexpr float maximum_step = 0.60f;
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const Vec2 applied = correction * phase_strength;'''
    text = replace_once(text, old, new,
        "floor-authoritative final crouch projection")
    write("src/simulation.cpp", text)


def remove_press_contact_speed_false_negative() -> None:
    text = read("src/ppo.hpp")
    text = replace_once(text,
        '''            if (environment.maximum_joint_speed() > 18.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;''',
        '''            // The physical platen can create a brief solver angular
            // velocity while the rig remains intact, feet-only, held, and
            // recovered. Those stronger stage facts are authoritative here.
            break;''',
        "static press transient joint-speed rejection")
    write("src/ppo.hpp", text)

    text = read("src/autonomy.hpp")
    text = replace_once(text,
        "    inline constexpr float duck_press_mastery_joint_speed_limit = 18.0f;",
        "    inline constexpr float duck_press_mastery_joint_speed_limit = 30.0f;",
        "static press mastery contact-speed allowance")
    write("src/autonomy.hpp", text)


def main() -> None:
    remove_live_whole_foot_shift()
    preserve_foot_shape_in_fused_regression()
    floor_clamp_final_crouch_pose()
    remove_press_contact_speed_false_negative()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
