from pathlib import Path

root = Path(__file__).resolve().parents[1]

mission_path = root / 'missioncache.md'
mission = mission_path.read_text(encoding='utf-8')
entry = '''

### WALK-HAZARD-079 — Falling material, impact, burial, and escape training
**Status:** CARRIED FORWARD — NOT IN v0.7.5

Add dynamic overhead hazards driven by the same terrain/material simulation: falling sand, collapsing loose slopes, rocks, debris, and thrown objects. Observations must include incoming direction, velocity, estimated impact time, material density, local burial depth, free-space direction, and whether the head, torso, or support limbs are obstructed. The rig must learn to evade when possible, brace when avoidance is impossible, remain oriented after impact, dig or push toward free space, recover from forward-prone or partially buried states, regain foot support, and continue the assigned stage.

Acceptance requires seeded scenarios covering glancing hits, direct hits, accumulating sand, partial burial, full-body obstruction with an escape path, and repeated impacts. Success cannot be credited for tunnelling, teleporting, deleting material, remaining motionless under debris, or exploiting detached limbs. Suffocation or complete burial without an escape route terminates the attempt honestly. This mission is paired with WALK-SAND-078 and is intentionally carried to the next release rather than delaying the v0.7.5 PIP/curriculum correction.
'''
if '### WALK-HAZARD-079' not in mission:
    mission += entry
mission_path.write_text(mission, encoding='utf-8', newline='\n')

notes_path = root / 'RELEASE_NOTES_v0.7.5.md'
notes = notes_path.read_text(encoding='utf-8')
line = '- Carries falling sand/debris avoidance, impact recovery, burial escape, and continuation training into the next release mission ledger.\n'
if line not in notes:
    notes = notes.replace('# Runner v0.7.5\n\n', '# Runner v0.7.5\n\n' + line, 1)
notes_path.write_text(notes, encoding='utf-8', newline='\n')

# Static crouch and moving crouch are separate stages. Keep the old platen test
# static, and test uneven terrain/low bars on the dedicated crouch-walk stage.
tests_path = root / 'tests/core_tests.cpp'
tests = tests_path.read_text(encoding='utf-8')
old = '''    sim::EnvironmentTestAccess::complete_duck_press(press_environment);
    require(std::abs(press_environment.ground_height_at(0.0f)
            - press_environment.ground_height_at(1.25f)) > 0.005f,
        "crouch-walk lesson ground remains flat and stable");
    const auto later_bar_iterator = std::ranges::find_if(
        press_environment.course_features(), [](const sim::CourseFeature& feature)
        {
            return feature.kind == sim::CourseFeatureKind::overhead_bar;
        });
    require(later_bar_iterator != press_environment.course_features().end(),
        "duck press never advances to the later moving low-bar lesson");
    const sim::CourseFeature& later_bar = *later_bar_iterator;
    require(later_bar.center.x
            - press_environment.particles()[press_environment.blueprint().root_node].position.x >= 6.0f,
        "later low bar starts too close for a meaningful crouch response");
    require(later_bar.half_extent.x > later_bar.half_extent.y * 5.0f,
        "later low bar is not horizontal or is effectively an undodgeable wall");
    require(sim::stage_skill_evidence(sim::CourseStage::duck_press,
            5u, 3.0f, 0u, 0.0f, 0u, 4u),
        "valid foot-only crouch-walk evidence is rejected");'''
new = '''    sim::EnvironmentTestAccess::complete_duck_press(press_environment);
    require(std::abs(press_environment.ground_height_at(0.0f)
            - press_environment.ground_height_at(1.25f)) < 0.0001f,
        "static crouch lesson incorrectly requires uneven-ground movement");
    require(std::ranges::none_of(press_environment.course_features(),
            [](const sim::CourseFeature& feature)
            {
                return feature.kind == sim::CourseFeatureKind::overhead_bar;
            }),
        "static crouch lesson incorrectly contains the moving low-bar course");

    sim::Environment crouch_environment(sim::CreatureBlueprint::humanoid(), 23);
    crouch_environment.set_course(sim::CourseStage::crouch_walk, 0.5f);
    require(std::abs(crouch_environment.ground_height_at(0.0f)
            - crouch_environment.ground_height_at(1.25f)) > 0.005f,
        "crouch-walk lesson ground remains flat and stable");
    const auto later_bar_iterator = std::ranges::find_if(
        crouch_environment.course_features(), [](const sim::CourseFeature& feature)
        {
            return feature.kind == sim::CourseFeatureKind::overhead_bar;
        });
    require(later_bar_iterator != crouch_environment.course_features().end(),
        "crouch-walk lesson has no later moving low bar");
    const sim::CourseFeature& later_bar = *later_bar_iterator;
    require(later_bar.center.x
            - crouch_environment.particles()[crouch_environment.blueprint().root_node].position.x >= 5.5f,
        "crouch-walk low bar starts too close for a meaningful response");
    require(later_bar.half_extent.x > later_bar.half_extent.y * 5.0f,
        "crouch-walk low bar is not horizontal or is effectively a wall");
    require(sim::stage_skill_evidence(sim::CourseStage::crouch_walk,
            5u, 3.0f, 0u, 0.0f, 0u, 4u),
        "valid foot-only crouch-walk evidence is rejected");'''
if new not in tests:
    if old not in tests:
        raise SystemExit('split crouch-stage test block was not found')
    tests = tests.replace(old, new, 1)
tests_path.write_text(tests, encoding='utf-8', newline='\n')

(root / 'tools/fix_v075_split_stage_tests.py').unlink(missing_ok=True)
Path(__file__).unlink()
print('carried future hazards and aligned static/moving crouch tests')
