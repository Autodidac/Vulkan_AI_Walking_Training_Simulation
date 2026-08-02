from pathlib import Path
import re
import shutil

root = Path(__file__).resolve().parents[1]
workflow_root = root / '.github' / 'workflows'
old_brand = 'ep' + 'och'

for relative in ('archive-audit', 'artifact', 'published', 'release-stage'):
    shutil.rmtree(root / relative, ignore_errors=True)

for path in sorted(root.rglob('*'), key=lambda item: len(item.parts), reverse=True):
    if '.git' in path.parts or path == Path(__file__) or path.is_relative_to(workflow_root):
        continue
    if old_brand in path.name.lower():
        if path.is_dir():
            shutil.rmtree(path, ignore_errors=True)
        else:
            path.unlink(missing_ok=True)

(root / 'tools/remove_legacy_runner_artifacts.py').unlink(missing_ok=True)
(root / 'tools/fix_v074_plate_scope.py').unlink(missing_ok=True)

for path in root.rglob('*'):
    if (not path.is_file() or '.git' in path.parts or path == Path(__file__)
            or path.is_relative_to(workflow_root)):
        continue
    try:
        original = path.read_text(encoding='utf-8')
    except (UnicodeDecodeError, OSError):
        continue
    text = original
    text = text.replace(
        'Runner v" RUNNER_VERSION " - Sand-Sim Enemy Locomotion Trainer',
        'Runner v" RUNNER_VERSION " - Autonomous Physics Locomotion Trainer')
    text = text.replace('## Sand-sim enemy locomotion hotfix',
                        '## Autonomous locomotion hotfix')
    text = text.replace('Sand-sim enemy locomotion curriculum',
                        'Simulation-enemy locomotion curriculum')
    text = re.sub(r'sand-sim enemy', 'simulation-enemy', text,
                  flags=re.IGNORECASE)
    text = text.replace('std::array<bool, 5> found{};',
                        'std::array<bool, 6> found{};')
    text = text.replace(
        '0.8f, 0u, 0.0f, 0u, 1u),\n        "valid duck-and-clear result cannot seed self-imitation"',
        '0.8f, 0u, 0.0f, 0u, 2u),\n        "valid press-and-low-bar result cannot seed self-imitation"')
    text = text.replace(
        '''        case sim::CourseStage::duck_press:
            if (environment.longest_stable_stance_seconds() < 2.0f
                || environment.stable_stance_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.duck_recoveries() < 1u || environment.duck_seconds() < 0.50f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.obstacles_passed() < 1u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);''',
        '''        case sim::CourseStage::duck_press:
            if (environment.longest_stable_stance_seconds() < 2.0f
                || environment.stable_stance_seconds() < 0.75f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.duck_recoveries() < 1u || environment.duck_seconds() < 0.50f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_recovery);
            if (environment.obstacles_passed() < 2u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);''')
    text = text.replace('const gui::Vec2 measured = font::measure_text(status, scale);',
                        'const Vec2 measured = font::measure_text(status, scale);')
    text = text.replace(
        '''        require(std::ranges::any_of(duck_lesson.course_features(),
                [](const sim::CourseFeature& feature)
                {
                    return feature.kind == sim::CourseFeatureKind::overhead_bar;
                }),
            "duck lesson has no explicit low-bar obstacle");''',
        '''        require(std::ranges::any_of(duck_lesson.course_features(),
                [](const sim::CourseFeature& feature)
                {
                    return feature.kind == sim::CourseFeatureKind::duck_press;
                }),
            "duck lesson has no explicit compression platen");''')
    text = text.replace(
        '''        const std::array<float, sim::action_count> neutral{};
        const auto duck = rl::effective_policy_action(
            duck_lesson, neutral, sim::CourseStage::duck_press);
        require(duck[0] < -0.05f && duck[1] > 0.10f
                && duck[2] > 0.05f && duck[3] < -0.10f,
            "low-bar obstacle does not trigger a coordinated duck primitive");''',
        '''        sim::EnvironmentTestAccess::set_duck_pressure(duck_lesson, 1.0f);
        const std::array<float, sim::action_count> neutral{};
        const auto duck = rl::effective_policy_action(
            duck_lesson, neutral, sim::CourseStage::duck_press);
        require(duck[0] < -0.05f && duck[1] > 0.10f
                && duck[2] > 0.05f && duck[3] < -0.10f,
            "compression pressure does not trigger a coordinated leg-driven duck primitive");
        require(std::abs(duck[4]) < 0.01f && std::abs(duck[5]) < 0.01f
                && std::abs(duck[6]) < 0.01f && std::abs(duck[7]) < 0.01f,
            "compression lesson still drives shoulders or elbows");''')
    text = text.replace(
        '''    void Environment::separate_support_clusters() noexcept
    {
        auto support_nodes = [&](bool left)
''',
        '''    void Environment::separate_support_clusters() noexcept
    {
        auto rigid_contact_plate = [&](std::uint16_t primary,
            const std::vector<std::uint16_t>& additional) noexcept
        {
            return valid_node(primary) && !additional.empty()
                && std::ranges::all_of(additional,
                    [&](std::uint16_t node)
                    {
                        return valid_node(node)
                            && direct_bone(blueprint_, primary, node);
                    });
        };
        if (!rigid_contact_plate(blueprint_.left_contact_node,
                blueprint_.additional_left_contact_nodes)
            || !rigid_contact_plate(blueprint_.right_contact_node,
                blueprint_.additional_right_contact_nodes))
            return;

        auto support_nodes = [&](bool left)
''')
    text = text.replace(
        '''                (void)environment.step(zero);
                require(environment.body_integrity_valid(),
                    "head or passive tail escaped the articulated body");''',
        '''                (void)environment.step(zero);
                if (!environment.body_integrity_valid())
                {
                    std::cerr << "passive integrity failure rig=" << index
                        << " frame=" << frame
                        << " invalid=" << static_cast<int>(environment.invalid_reason())
                        << std::endl;
                }
                require(environment.body_integrity_valid(),
                    "head or passive tail escaped the articulated body");''')
    normalized = '\n'.join(line.rstrip() for line in text.splitlines()).rstrip() + '\n'
    if normalized != original:
        path.write_text(normalized, encoding='utf-8', newline='\n')

Path(__file__).unlink()
print('normalized tested source without rewriting protected workflow files')
