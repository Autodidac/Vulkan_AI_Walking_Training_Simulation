from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / 'tests/core_tests.cpp'
text = path.read_text(encoding='utf-8')
replacements = [
('''    require(!press_environment.course_features().empty()
            && press_environment.course_features().front().kind == sim::CourseFeatureKind::overhead_bar,
        "duck press never advances to the later moving low-bar lesson");
    const sim::CourseFeature& later_bar = press_environment.course_features().front();
    require(later_bar.center.x
            - press_environment.particles()[press_environment.blueprint().root_node].position.x >= 6.0f,
        "later low bar starts too close for a meaningful crouch response");
    require(later_bar.half_extent.x > later_bar.half_extent.y * 5.0f,
        "later low bar is not horizontal or is effectively an undodgeable wall");''',
'''    const auto later_bar_iterator = std::ranges::find_if(
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
        "later low bar is not horizontal or is effectively an undodgeable wall");'''),
('''        && sim::course_stage_name(sim::CourseStage::duck_press) == "2. PRESS DUCK / HOLD / RECOVER"''',
'''        && sim::course_stage_name(sim::CourseStage::duck_press) == "2. CROUCH WALK / UNEVEN AVOID"'''),
('''    require(!sim::stage_skill_evidence(sim::CourseStage::duck_press, 0u, 0.6f, 0u, 0.0f, 0u, 0u),
        "duck lesson completes without holding and recovering from the press");
    require(sim::stage_skill_evidence(sim::CourseStage::duck_press, 0u, 0.6f, 0u, 0.0f, 0u, 2u),
        "press hold and recovery evidence cannot complete the duck lesson");''',
'''    require(!sim::stage_skill_evidence(sim::CourseStage::duck_press, 0u, 0.6f, 0u, 0.0f, 0u, 0u),
        "duck lesson completes without moving crouch evidence");
    require(sim::stage_skill_evidence(sim::CourseStage::duck_press, 5u, 3.0f, 0u, 0.0f, 0u, 4u),
        "foot-only crouch walk and obstacle evidence cannot complete the duck lesson");'''),
('''    require(rl::elite_motion_eligible(sim::CourseStage::duck_press, true, 0, 0.0f, 4.0f,
            0.8f, 0u, 0.0f, 0u, 2u),
        "valid press-and-low-bar result cannot seed self-imitation");''',
'''    require(rl::elite_motion_eligible(sim::CourseStage::duck_press, true, 5, 1.2f, 12.0f,
            3.0f, 0u, 0.0f, 0u, 4u),
        "valid foot-only crouch-walk result cannot seed self-imitation");'''),
]
for old, new in replacements:
    if new in text:
        continue
    if old not in text:
        raise SystemExit(f'legacy duck test target not found: {old[:80]}')
    text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8', newline='\n')

pip_fix = Path(__file__).with_name('fix_v075_pip_live.py')
if pip_fix.exists():
    code = compile(pip_fix.read_text(encoding='utf-8'), str(pip_fix), 'exec')
    exec(code, {'__name__': '__main__', '__file__': str(pip_fix)})

Path(__file__).unlink()
print('aligned duck tests and applied the live readable training PIP')
