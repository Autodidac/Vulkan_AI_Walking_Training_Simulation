from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def save(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str) -> None:
    text = load(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"missing slide-semantics target in {path}: {old[:180]!r}")
    save(path, text.replace(old, new, 1))


# Core math types live in runner, not runner::sim.
ppo = load("src/ppo.hpp")
ppo = ppo.replace("const sim::Vec2 reference =", "const Vec2 reference =")
ppo = ppo.replace("const sim::Vec2 driven =", "const Vec2 driven =")
ppo = ppo.replace("sim::Vec2 compact =", "Vec2 compact =")
ppo = ppo.replace("sim::length(reference)", "length(reference)")
ppo = ppo.replace("sim::length(driven)", "length(driven)")
ppo = ppo.replace("sim::signed_angle(reference, compact)",
                  "signed_angle(reference, compact)")
save("src/ppo.hpp", ppo)

replace_once("src/simulation.hpp",
'''    [[nodiscard]] inline bool wheel_sliding_motion(float root_speed, bool left_supported,
        bool right_supported, float stance_slip_speed) noexcept
    {
        return left_supported && right_supported
            && std::abs(root_speed) > 0.22f && stance_slip_speed > 0.18f;
    }
''',
'''    [[nodiscard]] inline bool friction_driven_shuffle(float root_speed,
        bool left_supported, bool right_supported, float stance_slip_speed,
        std::uint32_t gait_cycles, float swing_clearance) noexcept
    {
        return left_supported && right_supported
            && gait_cycles == 0u && swing_clearance < 0.06f
            && std::abs(root_speed) > 0.35f && stance_slip_speed > 0.24f;
    }
''')

replace_once("src/simulation.cpp",
'''        const bool locomotion_required = stage_requires_forward_gait(course_stage_);
        if (locomotion_required
            && wheel_sliding_motion(root_speed, left, right, stance_slip_speed_))
            wheel_sliding_seconds_ += dt;
        else
            wheel_sliding_seconds_ = std::max(0.0f, wheel_sliding_seconds_ - dt * 1.5f);
        if (wheel_sliding_seconds_ > 0.90f)
            invalidate(InvalidMotion::wheel_sliding);
''',
'''        const bool locomotion_required = stage_requires_forward_gait(course_stage_);
        const float recent_swing_clearance = std::max(left_clearance, right_clearance);
        if (locomotion_required && friction_driven_shuffle(root_speed,
                left, right, stance_slip_speed_, gait_cycles(), recent_swing_clearance))
            wheel_sliding_seconds_ = std::min(3.0f, wheel_sliding_seconds_ + dt);
        else
            wheel_sliding_seconds_ = std::max(0.0f, wheel_sliding_seconds_ - dt * 1.5f);
        // Sliding is a normal part of stance adjustment, crouching, and gait.
        // It is never a hard invalidation. Pure friction-driven shuffling simply
        // receives no gait credit and a mild shaping penalty until a real cycle occurs.
''')

replace_once("src/simulation.cpp",
'''        const float stance_slip_penalty = clamp(stance_slip_speed_ - 0.08f, 0.0f, 4.0f) * 0.012f;
        const float wheel_penalty = wheel_sliding_motion(raw_speed,
            left_supported, right_supported, stance_slip_speed_) ? 0.055f : 0.0f;
''',
'''        const float stance_slip_penalty = course_stage_ == CourseStage::balance
            ? clamp(stance_slip_speed_ - 0.08f, 0.0f, 4.0f) * 0.012f
            : 0.0f;
        const float wheel_penalty = friction_driven_shuffle(raw_speed,
            left_supported, right_supported, stance_slip_speed_, gait_cycles(),
            swing_clearance) ? 0.028f : 0.0f;
''')

replace_once("src/simulation.cpp",
'''        const float double_support_shuffle_penalty = left_supported && right_supported
            && std::abs(raw_speed) > 0.08f && obstacle_lift_clearance_ < 0.085f
            ? 0.028f : 0.0f;
''',
'''        const float double_support_shuffle_penalty = friction_driven_shuffle(raw_speed,
            left_supported, right_supported, stance_slip_speed_, gait_cycles(),
            swing_clearance) ? 0.018f : 0.0f;
''')

# Keep cumulative evaluation accounting aligned with cumulative PPO updates when
# optimizer-local state is reset by transfer, autosave, or recalibration.
replace_once("src/ppo_trainer.cpp",
'''        metrics_.total_training_seconds = previous_metrics.total_training_seconds;
        reward_history_.clear();
''',
'''        metrics_.total_training_seconds = previous_metrics.total_training_seconds;
        metrics_.evaluation_count = previous_metrics.evaluation_count;
        if (!clear_best)
        {
            metrics_.best_evaluation_distance = previous_metrics.best_evaluation_distance;
            metrics_.best_evaluation_score = previous_metrics.best_evaluation_score;
            metrics_.best_quality_key = previous_metrics.best_quality_key;
            metrics_.best_update = previous_metrics.best_update;
        }
        reward_history_.clear();
''')

# Regression: sliding and foot repositioning are legal; only friction-driven
# motion with no gait evidence is recognized as a shaping condition.
tests = load("tests/core_tests.cpp")
tests = tests.replace(
    "sim::wheel_sliding_motion(0.45f, true, true, 0.50f)",
    "sim::friction_driven_shuffle(0.45f, true, true, 0.50f, 0u, 0.0f)")
tests = tests.replace(
    "sim::wheel_sliding_motion(0.45f, true, false, 0.50f)",
    "sim::friction_driven_shuffle(0.45f, true, false, 0.50f, 0u, 0.0f)")
anchor = '''    require(sim::classify_motion_gate(1.0f, 0.0f, { 301.0f, 3.0f }, 0.0f, 0.7f, 0.0f, false)
        == sim::InvalidMotion::out_of_bounds, "course bounds gate missing");'''
addition = anchor + '''
    require(!sim::friction_driven_shuffle(0.42f, true, false, 0.30f, 0u, 0.0f)
            && !sim::friction_driven_shuffle(0.42f, true, true, 0.30f, 1u, 0.0f)
            && !sim::friction_driven_shuffle(0.42f, true, true, 0.30f, 0u, 0.10f),
        "normal single-support, established-gait, or foot-repositioning slide is penalized");
    require(sim::friction_driven_shuffle(0.42f, true, true, 0.30f, 0u, 0.0f),
        "friction-driven double-support shuffling is not recognized");'''
if addition not in tests:
    if anchor not in tests:
        raise RuntimeError("motion-gate regression anchor missing")
    tests = tests.replace(anchor, addition, 1)
save("tests/core_tests.cpp", tests)

mission = load("missioncache.md")
slide_entry = '''

### WALK-SLIDE-089 — Allow natural foot sliding without friction-drive exploits
**Status:** IN PROGRESS

Foot sliding is permitted during crouch entry, stance adjustment, walking, running, and unstable-terrain recovery. Sliding itself is not an invalid-motion gate. Pure double-support translation with no gait cycle, no swing clearance, and sustained planted-foot slip is recognized only as a friction-driven shuffle: it receives no gait credit and a mild shaping penalty, but does not terminate the attempt. Standing retains a low-slip stability requirement because its task is stationary support.
'''
if "### WALK-SLIDE-089" not in mission:
    mission = mission.rstrip() + slide_entry
update_entry = '''

### WALK-UPDATES-090 — Keep evaluations synchronized with PPO updates
**Status:** IN PROGRESS

Evaluation count is cumulative and survives optimizer, transferred-rig, autosave, and recalibration resets just as PPO update count does. With evaluation scheduled on update 1 and every fifth update, update 240 must report 49 evaluations unless the stage itself has just changed. The PIP publication and mastery streak consume each new evaluation exactly once; they may not remain at one evaluation after hundreds of updates.
'''
if "### WALK-UPDATES-090" not in mission:
    mission = mission.rstrip() + update_entry
save("missioncache.md", mission.rstrip() + "\n")

notes = load("RELEASE_NOTES_v0.7.7.md")
slide_line = "- Allows natural foot sliding during crouch and locomotion; only no-step planted-foot friction shuffling loses gait credit and receives a mild shaping penalty.\n"
update_line = "- Preserves cumulative evaluation accounting across recalibration and autosave state so evaluations, mastery, PPO updates, and the PIP remain synchronized.\n"
if slide_line not in notes:
    notes = notes.rstrip() + "\n" + slide_line
if update_line not in notes:
    notes = notes.rstrip() + "\n" + update_line
save("RELEASE_NOTES_v0.7.7.md", notes.rstrip() + "\n")

(ROOT / "tools/fix_v077_eof.py").unlink(missing_ok=True)
(ROOT / "tools/fix_v077_namespace.py").unlink(missing_ok=True)
(ROOT / "tools/fix_v077_updates.py").unlink(missing_ok=True)
Path(__file__).unlink()
print("materialized sliding, friction-drive semantics, math namespace, and evaluation accounting")
