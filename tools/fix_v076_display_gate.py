from pathlib import Path

root = Path(__file__).resolve().parents[1]

# Prevent the raised central shoulder triangle from reflecting through the
# upper spine. Head-to-shoulder braces preserve the intended central-above-
# lateral geometry while retaining articulated shoulder and elbow motors.
simulation_path = root / 'src/simulation.cpp'
simulation = simulation_path.read_text(encoding='utf-8')
old_bones = '''            { 1, 7, 0.0f, 0.98f }, { 7, 8, 0.0f, 0.98f }, { 8, 9, 0.0f, 0.96f },
            { 1, 10, 0.0f, 0.98f }, { 10, 11, 0.0f, 0.98f }, { 11, 12, 0.0f, 0.96f },
            { 7, 10, 0.0f, 0.72f }
        };'''
new_bones = '''            { 1, 7, 0.0f, 0.98f }, { 7, 8, 0.0f, 0.98f }, { 8, 9, 0.0f, 0.96f },
            { 1, 10, 0.0f, 0.98f }, { 10, 11, 0.0f, 0.98f }, { 11, 12, 0.0f, 0.96f },
            { 7, 10, 0.0f, 0.72f },
            { 2, 7, 0.0f, 0.94f }, { 2, 10, 0.0f, 0.94f }
        };'''
if new_bones not in simulation:
    if old_bones not in simulation:
        raise SystemExit('humanoid shoulder-girdle bone block was not found')
    simulation = simulation.replace(old_bones, new_bones, 1)
simulation_path.write_text(simulation, encoding='utf-8', newline='\n')

# Align display priority with the honest PIP contract. Qualified environments
# remain visible through a momentary contact flicker; finite rejected attempts
# are still published at a lower priority rather than producing a blank PIP.
ppo_path = root / 'src/ppo.hpp'
ppo = ppo_path.read_text(encoding='utf-8')
old_gate = '''        if (!qualification.valid
            || !environment.current_display_posture_valid())
            return false;'''
new_gate = '''        if (!qualification.valid || !environment.body_integrity_valid())
            return false;'''
if new_gate not in ppo:
    if old_gate not in ppo:
        raise SystemExit('stage display gate target was not found')
    ppo = ppo.replace(old_gate, new_gate, 1)
old_balance = '''        if (stage == sim::CourseStage::balance)
        {
            return environment.stable_stance_seconds() >= 1.0f
                && environment.uprightness() >= 0.82f
                && environment.maximum_upper_body_motor_deviation()
                    <= standing_neutral_arm_limit
                && environment.uncontrolled_spin_turns()
                    <= standing_qualification_spin_limit
                && (environment.left_supported() || environment.right_supported());
        }'''
new_balance = '''        if (stage == sim::CourseStage::balance)
        {
            // Qualification proves sustained support. Keep the current sample
            // visible through one solver-frame contact flicker while still
            // rejecting collapsed, arms-up, spinning, or broken frames.
            return environment.uprightness() >= 0.60f
                && environment.maximum_upper_body_motor_deviation()
                    <= standing_neutral_arm_limit
                && environment.uncontrolled_spin_turns()
                    <= standing_qualification_spin_limit;
        }'''
if new_balance not in ppo:
    if old_balance not in ppo:
        raise SystemExit('balance PIP block was not found')
    ppo = ppo.replace(old_balance, new_balance, 1)
ppo_path.write_text(ppo, encoding='utf-8', newline='\n')

# Keep acceptance coverage focused on both priorities: a current neutral frame
# earns top priority, while a historically qualified finite environment can
# never disappear from the PIP. The robust seed gate also checks full skeleton
# integrity, matching the real PPO evaluator.
test_path = root / 'tests/core_tests.cpp'
tests = test_path.read_text(encoding='utf-8')
old_expectation = '''        require(rl::stage_display_sample_eligible(
                sim::CourseStage::balance, assisted_stance),
            "valid current stance is hidden from the training sample");'''
new_expectation = '''        require(rl::training_preview_priority(
                sim::CourseStage::balance, assisted_stance) > 0,
            "stage-qualified standing environment disappears from the training PIP");'''
if new_expectation not in tests:
    if old_expectation not in tests:
        raise SystemExit('stale standing PIP test expectation was not found')
    tests = tests.replace(old_expectation, new_expectation, 1)

old_neutral = '''        require(rl::stage_motion_qualification(
                sim::CourseStage::balance, neutral_stance).valid,
            "neutral strict standing evidence is rejected");'''
new_neutral = '''        require(rl::stage_motion_qualification(
                sim::CourseStage::balance, neutral_stance).valid,
            "neutral strict standing evidence is rejected");
        require(rl::stage_display_sample_eligible(
                sim::CourseStage::balance, neutral_stance),
            "current neutral strict stance is not eligible for top-priority PIP display");'''
if new_neutral not in tests:
    if old_neutral not in tests:
        raise SystemExit('neutral stance test anchor was not found')
    tests = tests.replace(old_neutral, new_neutral, 1)

old_seed = '''            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(sim::CourseStage::balance, environment);
            valid_agents += qualification.valid ? 1u : 0u;
            if (!qualification.valid)
            {
                std::cerr << "evaluation seed " << seed
                    << " rejection=" << qualification.rejection_mask
                    << " invalid=" << static_cast<int>(environment.invalid_reason())
                    << " stance=" << environment.stable_stance_seconds()
                    << " longest=" << environment.longest_stable_stance_seconds()
                    << " max_joint=" << environment.maximum_joint_speed()
                    << " survival=" << environment.elapsed_seconds() << std::endl;
            }'''
new_seed = '''            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(sim::CourseStage::balance, environment);
            const bool integrity = environment.body_integrity_valid();
            valid_agents += qualification.valid && integrity ? 1u : 0u;
            if (!qualification.valid || !integrity)
            {
                std::cerr << "evaluation seed " << seed
                    << " rejection=" << qualification.rejection_mask
                    << " invalid=" << static_cast<int>(environment.invalid_reason())
                    << " integrity=" << integrity
                    << " stance=" << environment.stable_stance_seconds()
                    << " longest=" << environment.longest_stable_stance_seconds()
                    << " max_joint=" << environment.maximum_joint_speed()
                    << " survival=" << environment.elapsed_seconds() << std::endl;
            }'''
if new_seed not in tests:
    if old_seed not in tests:
        raise SystemExit('evaluation seed gate was not found')
    tests = tests.replace(old_seed, new_seed, 1)

brace_anchor = '''    require(humanoid.nodes[8].y < humanoid.nodes[7].y
            && humanoid.nodes[11].y < humanoid.nodes[10].y,
        "humanoid rest arms do not hang below the shoulder pivots");'''
brace_test = brace_anchor + '''
    require(std::ranges::any_of(humanoid.bones, [](const sim::DistanceConstraint& bone)
            { return (bone.a == 2u && bone.b == 7u) || (bone.a == 7u && bone.b == 2u); })
            && std::ranges::any_of(humanoid.bones, [](const sim::DistanceConstraint& bone)
            { return (bone.a == 2u && bone.b == 10u) || (bone.a == 10u && bone.b == 2u); }),
        "raised humanoid shoulder girdle can still invert through the upper spine");'''
if brace_test not in tests:
    if brace_anchor not in tests:
        raise SystemExit('humanoid geometry test anchor was not found')
    tests = tests.replace(brace_anchor, brace_test, 1)

test_path.write_text(tests, encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('braced raised humanoid shoulder girdle and finalized standing PIP gates')
