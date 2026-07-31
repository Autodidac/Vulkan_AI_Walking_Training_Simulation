from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


header = Path("src/simulation.hpp")
text = header.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    [[nodiscard]] inline bool qualifies_alternating_step(int previous_side, int strike_side,
        float seconds_since_previous, float root_displacement) noexcept
""",
    """    [[nodiscard]] inline bool recovery_terminal_fall(bool geometric_fall,
        bool hard_fall, bool recovery_active) noexcept
    {
        return hard_fall || (geometric_fall && !recovery_active);
    }

    [[nodiscard]] inline bool qualifies_alternating_step(int previous_side, int strike_side,
        float seconds_since_previous, float root_displacement) noexcept
""",
    "recovery terminal gate",
)
header.write_text(text, encoding="utf-8")

source = Path("src/simulation.cpp")
text = source.read_text(encoding="utf-8")
old = """        const float head_floor = local_ground + 0.34f;
        fallen_ = particles_[blueprint_.torso_node].position.y < torso_floor
            || particles_[blueprint_.head_node].position.y < head_floor;

        float recovery_reward = 0.0f;
        const bool supported = particles_[blueprint_.left_contact_node].grounded
            || particles_[blueprint_.right_contact_node].grounded;
        if (!recovery_active_ && !fallen_ && (collided_this_step_ || upright < 0.72f))
        {
            recovery_active_ = true;
            recovery_started_seconds_ = elapsed_seconds_;
            recovery_best_upright_ = upright;
            ++recovery_events_;
        }
        if (recovery_active_)
        {
            const float improvement = upright - recovery_best_upright_;
            if (improvement > 0.0f)
                recovery_reward += improvement * 0.10f;
            recovery_best_upright_ = std::max(recovery_best_upright_, upright);
            const float recovery_time = elapsed_seconds_ - recovery_started_seconds_;
            if (upright >= 0.90f && supported && recovery_time >= 0.12f)
            {
                recovery_active_ = false;
                ++recovery_successes_;
                recovery_reward += 0.14f;
            }
            else if (fallen_ || recovery_time > 3.0f)
            {
                recovery_active_ = false;
                recovery_reward -= 0.10f;
            }
        }

        const float allowed_airtime = course_stage_ == CourseStage::hurdles ? 1.30f
            : course_stage_ == CourseStage::moving_hazards ? 1.05f
            : course_stage_ >= CourseStage::ramps ? 0.90f : 0.72f;
        const float gated_upright = elapsed_seconds_ > 0.25f ? upright : 1.0f;
        invalidate(classify_motion_gate(gated_upright, maximum_speed_kmh_, pelvis_position,
            airborne_seconds_, allowed_airtime, micro_motion_seconds_, fallen_));
"""
new = """        const float head_floor = local_ground + 0.34f;
        const float torso_y = particles_[blueprint_.torso_node].position.y;
        const float head_y = particles_[blueprint_.head_node].position.y;
        const bool geometric_fall = torso_y < torso_floor || head_y < head_floor;
        const bool hard_fall = torso_y < local_ground + 0.18f || head_y < local_ground + 0.12f;
        fallen_ = geometric_fall;

        float recovery_reward = 0.0f;
        const bool supported = particles_[blueprint_.left_contact_node].grounded
            || particles_[blueprint_.right_contact_node].grounded;
        if (!recovery_active_ && !hard_fall
            && (collided_this_step_ || upright < 0.72f || geometric_fall))
        {
            recovery_active_ = true;
            recovery_started_seconds_ = elapsed_seconds_;
            recovery_best_upright_ = upright;
            ++recovery_events_;
        }
        if (recovery_active_)
        {
            const float improvement = upright - recovery_best_upright_;
            if (improvement > 0.0f)
                recovery_reward += improvement * 0.10f;
            recovery_best_upright_ = std::max(recovery_best_upright_, upright);
            const float recovery_time = elapsed_seconds_ - recovery_started_seconds_;
            if (upright >= 0.90f && supported && !geometric_fall && recovery_time >= 0.12f)
            {
                recovery_active_ = false;
                ++recovery_successes_;
                recovery_reward += 0.14f;
            }
            else if (hard_fall || recovery_time > 3.0f)
            {
                recovery_active_ = false;
                recovery_reward -= 0.10f;
            }
        }

        const float allowed_airtime = course_stage_ == CourseStage::hurdles ? 1.30f
            : course_stage_ == CourseStage::moving_hazards ? 1.05f
            : course_stage_ >= CourseStage::ramps ? 0.90f : 0.72f;
        const float gated_upright = elapsed_seconds_ > 0.25f ? upright : 1.0f;
        const bool terminal_fall = recovery_terminal_fall(
            geometric_fall, hard_fall, recovery_active_);
        invalidate(classify_motion_gate(gated_upright, maximum_speed_kmh_, pelvis_position,
            airborne_seconds_, allowed_airtime, micro_motion_seconds_, terminal_fall));
"""
text = replace_once(text, old, new, "near-fall recovery block")
source.write_text(text, encoding="utf-8")

tests = Path("tests/core_tests.cpp")
text = tests.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    require(!sim::qualifies_alternating_step(-1, 0, 0.30f, 0.10f),
        "simultaneous two-foot landing counted as a step");
""",
    """    require(!sim::recovery_terminal_fall(true, false, true),
        "recoverable near-fall terminated during its recovery window");
    require(sim::recovery_terminal_fall(true, false, false),
        "unrecovered geometric fall was not terminal");
    require(sim::recovery_terminal_fall(true, true, true),
        "hard ground impact incorrectly received recovery grace");

    require(!sim::qualifies_alternating_step(-1, 0, 0.30f, 0.10f),
        "simultaneous two-foot landing counted as a step");
""",
    "recovery grace tests",
)
tests.write_text(text, encoding="utf-8")
