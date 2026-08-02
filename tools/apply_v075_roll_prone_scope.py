from pathlib import Path

root = Path(__file__).resolve().parents[1]


def load(path: str) -> str:
    return (root / path).read_text(encoding='utf-8')


def save(path: str, text: str) -> None:
    (root / path).write_text(text, encoding='utf-8', newline='\n')


def replace_once(path: str, old: str, new: str) -> None:
    text = load(path)
    if new in text:
        return
    if old not in text:
        raise SystemExit(f'missing roll/prone target in {path}: {old[:160]}')
    save(path, text.replace(old, new, 1))


replace_once('src/simulation.hpp',
'''    [[nodiscard]] inline bool controlled_flip_rolling_allowed(CourseStage stage,
        bool powered_flip, float spin_turns) noexcept
    {
        return stage_allows_controlled_flips(stage)
            && powered_flip && std::abs(spin_turns) <= 3.0f;
    }

    [[nodiscard]] inline bool rolling_body_motion(float root_speed, float torso_turn_speed,
''',
'''    [[nodiscard]] inline bool controlled_somersault_allowed(CourseStage stage,
        float spin_turns, float torso_turn_speed, bool airborne_or_landing) noexcept
    {
        return stage_allows_controlled_flips(stage)
            && airborne_or_landing
            && std::abs(torso_turn_speed) >= 0.45f
            && std::abs(spin_turns) <= 3.0f;
    }

    [[nodiscard]] inline bool forward_prone_allowed(CourseStage stage,
        bool non_foot_grounded, bool head_faces_forward, float uprightness,
        float forward_speed) noexcept
    {
        const bool recovery_stage = stage == CourseStage::uneven
            || stage == CourseStage::ramps
            || stage == CourseStage::hurdles
            || stage == CourseStage::duck_bars
            || stage == CourseStage::moving_hazards;
        return recovery_stage && non_foot_grounded && head_faces_forward
            && uprightness <= 0.42f && forward_speed >= -0.15f;
    }

    [[nodiscard]] inline bool rolling_body_motion(float root_speed, float torso_turn_speed,
''')

replace_once('src/simulation.cpp',
'''        const float active_flip_turns = std::abs(current_airborne_rotation_) / (2.0f * pi);
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
''',
'''        const float active_flip_turns = std::abs(current_airborne_rotation_) / (2.0f * pi);
        const float evaluated_turns = spin_landing_this_step_
            ? maximum_spin_turns_ : active_flip_turns;
        const bool airborne_or_landing = !feet_supported || spin_landing_this_step_;
        const bool controlled_somersault = controlled_somersault_allowed(
            course_stage_, evaluated_turns, torso_turn_speed_, airborne_or_landing);
        const bool head_faces_forward = valid_node(blueprint_.head_node)
            && valid_node(blueprint_.torso_node)
            && particles_[blueprint_.head_node].position.x
                >= particles_[blueprint_.torso_node].position.x - 0.05f;
        const bool controlled_prone = forward_prone_allowed(course_stage_,
            non_foot_grounded_, head_faces_forward, torso_uprightness(), root_speed);
        if (controlled_somersault || controlled_prone)
        {
            body_rolling_seconds_ = std::max(0.0f, body_rolling_seconds_ - dt * 3.0f);
            head_contact_seconds_ = std::max(0.0f, head_contact_seconds_ - dt * 3.0f);
        }
        else
        {
''')

# Replace the powered-only regression assertions added earlier.
tests = load('tests/core_tests.cpp')
old = '''    require(sim::controlled_flip_rolling_allowed(
            sim::CourseStage::duck_bars, true, 2.75f),
        "recognized powered flip cannot roll its body");
    require(!sim::controlled_flip_rolling_allowed(
            sim::CourseStage::duck_bars, false, 2.0f)
            && !sim::controlled_flip_rolling_allowed(
                sim::CourseStage::uneven, true, 2.0f)
            && !sim::controlled_flip_rolling_allowed(
                sim::CourseStage::duck_bars, true, 3.01f),
        "unpowered, wrong-stage, or over-three-turn rolling is allowed");'''
new = '''    require(sim::controlled_somersault_allowed(
            sim::CourseStage::duck_bars, 2.75f, 1.2f, true),
        "controlled somersault is rejected without a powered-launch flag");
    require(!sim::controlled_somersault_allowed(
            sim::CourseStage::uneven, 2.0f, 1.2f, true)
            && !sim::controlled_somersault_allowed(
                sim::CourseStage::duck_bars, 3.01f, 1.2f, true)
            && !sim::controlled_somersault_allowed(
                sim::CourseStage::duck_bars, 2.0f, 0.10f, true),
        "wrong-stage, over-three-turn, or non-rotating tumbling is accepted");
    require(sim::forward_prone_allowed(sim::CourseStage::uneven,
            true, true, 0.25f, 0.10f)
            && !sim::forward_prone_allowed(sim::CourseStage::duck_press,
                true, true, 0.25f, 0.10f)
            && !sim::forward_prone_allowed(sim::CourseStage::crouch_walk,
                true, true, 0.25f, 0.10f)
            && !sim::forward_prone_allowed(sim::CourseStage::uneven,
                true, false, 0.25f, 0.10f),
        "forward-prone recovery rules do not preserve crouch foot-only contact");'''
if old not in tests:
    raise SystemExit('powered-only rolling test block was not found')
tests = tests.replace(old, new, 1)
save('tests/core_tests.cpp', tests)

mission = load('missioncache.md')
mission = mission.replace(
'''### WALK-FLIP-075 — Rolling permitted only as a controlled flip
**Status:** IN PROGRESS

Body rotation is permitted during a recognized powered flip and its landing frame. The exemption applies only in controlled-flip stages, never to ground rolling or unpowered tumbling, and ends at three turns. More than three rotations remains an immediate invalidation, and flip credit still requires a controlled landing.''',
'''### WALK-FLIP-075 — Controlled somersault and prone recovery rules
**Status:** IN PROGRESS

A recognized somersault may rotate without requiring a separate powered-launch flag, but it must occur in a flip-capable stage, maintain meaningful directed rotation, and remain at or below three turns. Forward-facing prone posture is permitted as a recoverable state during locomotion, jump, hurdle, flip, and mixed stages. Backward-facing collapse and uncontrolled tumbling remain invalid. Static crouch and crouch-walk retain the stricter rule that only feet may touch terrain.''')

sand_entry = '''

### WALK-SAND-078 — Deformable sand-cell uneven terrain
**Status:** CARRIED FORWARD — NOT IN v0.7.5

Replace the current fixed analytic uneven-ground waves with a deterministic deformable sand-cell terrain layer. Foot pressure must compact, displace, mound, and locally collapse the terrain; loose slopes must shift under load; contacts and observations must expose changing support height, firmness, slip, and nearby surface shape. The same terrain state must drive physics, PIP rendering, evaluation, and replay. Acceptance requires repeatable seeded tests, bounded runtime cost across the training pool, no terrain/body tunnelling, and successful gait, prone recovery, crouch-walk, and obstacle traversal on terrain that changes under the rig. This is intentionally carried to the next release rather than delaying the v0.7.5 correction package.
'''
if '### WALK-SAND-078' not in mission:
    mission += sand_entry
save('missioncache.md', mission)

notes = load('RELEASE_NOTES_v0.7.5.md')
notes = notes.replace(
'- Allows body rotation during a recognized powered flip and landing frame while retaining the hard three-rotation limit and rejecting ordinary ground rolling.\n',
'- Allows controlled somersaulting without requiring a separate powered-launch flag, permits forward-facing prone recovery outside crouch lessons, and retains the hard three-rotation limit.\n')
carry = '- Carries full deformable sand-cell terrain integration into the next release mission ledger rather than delaying this correction package.\n'
if carry not in notes:
    notes = notes.replace('# Runner v0.7.5\n\n', '# Runner v0.7.5\n\n' + carry, 1)
save('RELEASE_NOTES_v0.7.5.md', notes)

Path(__file__).unlink()
print('corrected somersault/prone rules and carried sand-cell terrain forward')
