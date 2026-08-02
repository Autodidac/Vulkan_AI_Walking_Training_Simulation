from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def save(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace(path: str, old: str, new: str) -> None:
    text = load(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"missing target in {path}: {old[:160]!r}")
    save(path, text.replace(old, new, 1))


# Strict staged mastery: no random mixed replay. A skill is locked only after
# repeated strict evaluations, its best controller is restored, and that same
# controller becomes the starting point for the next prerequisite stage.
replace(
    "src/autonomy.hpp",
    "namespace runner::rl\n{\n",
    "namespace runner::rl\n{\n    inline constexpr int mastery_lock_confirmations = 8;\n",
)

replace(
    "src/autonomy_curriculum.cpp",
    '''        case sim::CourseStage::balance:
            return metrics.evaluation_longest_stance >= 3.0f
                && metrics.evaluation_survival >= 3.0f
                && metrics.evaluation_max_joint_speed <= 12.0f;
        case sim::CourseStage::duck_press:
            return metrics.evaluation_duck_recoveries >= 1.0f
                && metrics.evaluation_stride_events >= 4.0f
                && metrics.evaluation_duck_seconds >= 2.0f
                && metrics.evaluation_distance >= 0.75f
                && metrics.evaluation_obstacles_passed >= 3.0f
                && metrics.evaluation_survival >= 12.0f;
        case sim::CourseStage::ramps:
            return metrics.evaluation_jump_landings >= 2.0f
                && metrics.evaluation_powered_jumps >= 2.0f;
        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 5.0f
                && metrics.evaluation_stride_events >= 4.0f
                && metrics.evaluation_speed >= 0.65f;
        case sim::CourseStage::hurdles:
            return metrics.evaluation_distance >= 7.0f
                && metrics.evaluation_obstacles_passed >= 2.0f
                && (metrics.evaluation_jump_landings >= 1.0f
                    || metrics.evaluation_duck_seconds >= 0.75f);
        case sim::CourseStage::duck_bars:
            return metrics.evaluation_spin_landings >= 1.0f
                && metrics.evaluation_spin_turns >= 0.85f
                && metrics.evaluation_spin_turns <= 3.05f;
        case sim::CourseStage::moving_hazards:
            return metrics.evaluation_distance >= 9.0f
                && metrics.evaluation_obstacles_passed >= 2.0f
                && metrics.evaluation_collisions <= 4.0f;''',
    '''        case sim::CourseStage::balance:
            return metrics.evaluation_longest_stance >= 5.0f
                && metrics.evaluation_survival >= 6.0f
                && metrics.evaluation_max_joint_speed <= 10.0f;
        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 7.0f
                && metrics.evaluation_stride_events >= 8.0f
                && metrics.evaluation_speed >= 0.70f
                && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::duck_press:
            return metrics.evaluation_duck_recoveries >= 2.0f
                && metrics.evaluation_stride_events >= 8.0f
                && metrics.evaluation_duck_seconds >= 3.5f
                && metrics.evaluation_distance >= 1.50f
                && metrics.evaluation_obstacles_passed >= 4.0f
                && metrics.evaluation_collisions <= 1.0f
                && metrics.evaluation_survival >= 14.0f;
        case sim::CourseStage::ramps:
            return metrics.evaluation_jump_landings >= 3.0f
                && metrics.evaluation_powered_jumps >= 3.0f
                && metrics.evaluation_stride_events >= 4.0f
                && metrics.evaluation_distance >= 3.0f;
        case sim::CourseStage::hurdles:
            return metrics.evaluation_distance >= 8.0f
                && metrics.evaluation_stride_events >= 6.0f
                && metrics.evaluation_obstacles_passed >= 3.0f
                && metrics.evaluation_collisions <= 2.0f
                && (metrics.evaluation_jump_landings >= 1.0f
                    || metrics.evaluation_duck_seconds >= 1.0f);
        case sim::CourseStage::duck_bars:
            return metrics.evaluation_spin_landings >= 2.0f
                && metrics.evaluation_powered_jumps >= 2.0f
                && metrics.evaluation_spin_turns >= 0.85f
                && metrics.evaluation_spin_turns <= 3.00f;
        case sim::CourseStage::moving_hazards:
            return metrics.evaluation_distance >= 11.0f
                && metrics.evaluation_stride_events >= 8.0f
                && metrics.evaluation_obstacles_passed >= 4.0f
                && metrics.evaluation_collisions <= 3.0f;''',
)

replace(
    "src/autonomy_curriculum.cpp",
    "else if (mastery_streak_ >= 3)",
    "else if (mastery_streak_ >= mastery_lock_confirmations)",
)
replace(
    "src/autonomy_curriculum.cpp",
    '''            worker_message_ = std::format("{} - MASTERY {}/3", sim::course_stage_name(stage_), mastery_streak_);''',
    '''            worker_message_ = std::format("{} - STRICT MASTERY {}/{}",
                sim::course_stage_name(stage_), mastery_streak_, mastery_lock_confirmations);''',
)
replace(
    "src/autonomy_curriculum.cpp",
    '''        mastery_streak_ = 0;
        degradation_streak_ = 0;
        if (stage_ != sim::CourseStage::moving_hazards)''',
    '''        if (worker_.has_best_policy())
            (void)worker_.restore_best_policy();
        mastery_streak_ = 0;
        degradation_streak_ = 0;
        if (stage_ != sim::CourseStage::moving_hazards)''',
)
replace(
    "src/autonomy_curriculum.cpp",
    '''            worker_message_ = std::format("LESSON COMPLETE - ADVANCING TO {}", sim::course_stage_name(stage_));''',
    '''            worker_message_ = std::format("SKILL LOCKED - ADVANCING TO {}",
                sim::course_stage_name(stage_));''',
)

replace(
    "src/app.cpp",
    '''            add_text_fit(canvas, cursor, std::format("DIFFICULTY {:.0f}%   MASTERY {}/3",
                autonomy.difficulty * 100.0f, autonomy.mastery_streak), 1.16f, white, usable_width);''',
    '''            add_text_fit(canvas, cursor, std::format("DIFFICULTY {:.0f}%   STRICT MASTERY {}/{}",
                autonomy.difficulty * 100.0f, autonomy.mastery_streak,
                rl::mastery_lock_confirmations), 1.12f, white, usable_width);''',
)

# Body orientation can roll during a recognized powered flip. Ground rolling,
# unpowered tumbling, wrong-stage spinning, and any rotation above three turns
# remain invalid. The landing frame is included so a valid somersault is not
# rejected at the instant the feet reconnect.
replace(
    "src/simulation.hpp",
    '''    [[nodiscard]] inline bool rolling_body_motion(float root_speed, float torso_turn_speed,
        float uprightness, bool feet_supported, bool non_foot_grounded) noexcept
    {
        return non_foot_grounded
            && (std::abs(torso_turn_speed) > 0.45f || uprightness < 0.55f)
            && (!feet_supported || std::abs(root_speed) > 0.08f);
    }
''',
    '''    [[nodiscard]] inline bool controlled_flip_rolling_allowed(CourseStage stage,
        bool powered_flip, float spin_turns) noexcept
    {
        return stage_allows_controlled_flips(stage)
            && powered_flip && std::abs(spin_turns) <= 3.0f;
    }

    [[nodiscard]] inline bool rolling_body_motion(float root_speed, float torso_turn_speed,
        float uprightness, bool feet_supported, bool non_foot_grounded) noexcept
    {
        return non_foot_grounded
            && (std::abs(torso_turn_speed) > 0.45f || uprightness < 0.55f)
            && (!feet_supported || std::abs(root_speed) > 0.08f);
    }
''',
)

replace(
    "src/simulation.cpp",
    '''        previous_left_grounded_ = left;
        previous_right_grounded_ = right;
        if (rolling_body_motion(root_speed, torso_turn_speed_, torso_uprightness(),
            feet_supported, non_foot_grounded_))
            body_rolling_seconds_ += dt;
        else
            body_rolling_seconds_ = std::max(0.0f, body_rolling_seconds_ - dt * 2.0f);
        if (head_ground_contact())
            head_contact_seconds_ += dt;
        else
            head_contact_seconds_ = std::max(0.0f, head_contact_seconds_ - dt * 3.0f);''',
    '''        previous_left_grounded_ = left;
        previous_right_grounded_ = right;
        const float active_flip_turns = std::abs(current_airborne_rotation_) / (2.0f * pi);
        const bool controlled_flip_motion = controlled_flip_rolling_allowed(
            course_stage_, powered_takeoff_ || spin_landing_this_step_,
            spin_landing_this_step_ ? maximum_spin_turns_ : active_flip_turns);
        if (controlled_flip_motion)
        {
            body_rolling_seconds_ = std::max(0.0f, body_rolling_seconds_ - dt * 3.0f);
            head_contact_seconds_ = std::max(0.0f, head_contact_seconds_ - dt * 3.0f);
        }
        else
        {
            if (rolling_body_motion(root_speed, torso_turn_speed_, torso_uprightness(),
                feet_supported, non_foot_grounded_))
                body_rolling_seconds_ += dt;
            else
                body_rolling_seconds_ = std::max(0.0f, body_rolling_seconds_ - dt * 2.0f);
            if (head_ground_contact())
                head_contact_seconds_ += dt;
            else
                head_contact_seconds_ = std::max(0.0f, head_contact_seconds_ - dt * 3.0f);
        }''',
)

# Preserve the landing rotation before current_airborne_rotation_ is cleared so
# the landing frame can be recognized by the rolling exemption.
replace(
    "src/simulation.cpp",
    '''                    if (landed_turns >= 0.75f)
                    {
                        spin_landing_this_step_ = true;
                        ++spin_landing_count_;
                    }''',
    '''                    if (landed_turns >= 0.75f && landed_turns <= 3.0f)
                    {
                        spin_landing_this_step_ = true;
                        ++spin_landing_count_;
                    }''',
)

# Deterministic proof of strict staged locking and flip-only rolling.
tests = load("tests/core_tests.cpp")
anchor = '''    require(sim::classify_motion_gate(0.4f, 0.0f, { 0.0f, 4.0f }, 1.2f, 2.7f, 0.0f,
            false, sim::CourseStage::duck_bars, 3.21f)
        == sim::InvalidMotion::excessive_spins, "more than three spins is not rejected");'''
addition = anchor + '''
    require(rl::mastery_lock_confirmations >= 8,
        "staged mastery locks after too few repeated evaluations");
    require(sim::controlled_flip_rolling_allowed(
            sim::CourseStage::duck_bars, true, 2.75f),
        "recognized powered flip cannot roll its body");
    require(!sim::controlled_flip_rolling_allowed(
            sim::CourseStage::duck_bars, false, 2.0f)
            && !sim::controlled_flip_rolling_allowed(
                sim::CourseStage::uneven, true, 2.0f)
            && !sim::controlled_flip_rolling_allowed(
                sim::CourseStage::duck_bars, true, 3.01f),
        "unpowered, wrong-stage, or over-three-turn rolling is allowed");'''
if addition not in tests:
    if anchor not in tests:
        raise RuntimeError("flip gate test anchor missing")
    tests = tests.replace(anchor, addition, 1)
save("tests/core_tests.cpp", tests)

mission = load("missioncache.md")
entry = '''

### WALK-MASTERY-074 — Strict staged skill locking
**Status:** IN PROGRESS

Training remains sequential rather than mixed replay. A lesson must pass eight consecutive stricter evaluations before its best verified controller is restored and locked as the starting point for the next lesson. Later lessons reinforce earlier skills by requiring them as prerequisites; they do not randomly switch back to old lesson types. Repeatedly solved tasks therefore stop consuming the main training focus while remaining embedded in the succeeding skill.

### WALK-FLIP-075 — Rolling permitted only as a controlled flip
**Status:** IN PROGRESS

Body rotation is permitted during a recognized powered flip and its landing frame. The exemption applies only in controlled-flip stages, never to ground rolling or unpowered tumbling, and ends at three turns. More than three rotations remains an immediate invalidation, and flip credit still requires a controlled landing.
'''
if "### WALK-MASTERY-074" not in mission:
    mission += entry
save("missioncache.md", mission)

notes = load("RELEASE_NOTES_v0.7.5.md")
lines = (
    "- Keeps training strictly staged: eight consecutive strict successes lock the best controller, then the next lesson builds on it without random mixed replay.\n"
    "- Allows body rotation during a recognized powered flip and landing frame while retaining the hard three-rotation limit and rejecting ordinary ground rolling.\n"
)
if "eight consecutive strict successes" not in notes:
    notes = notes.replace("# Runner v0.7.5\n\n", "# Runner v0.7.5\n\n" + lines, 1)
save("RELEASE_NOTES_v0.7.5.md", notes)

Path(__file__).unlink()
print("implemented strict staged mastery and flip-only rolling")
