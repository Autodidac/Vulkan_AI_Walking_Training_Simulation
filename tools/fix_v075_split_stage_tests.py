from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / 'tests/core_tests.cpp'
text = path.read_text(encoding='utf-8')
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
if new not in text:
    if old not in text:
        raise SystemExit('split crouch-stage test block was not found')
    text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('aligned static crouch and moving crouch terrain tests')
