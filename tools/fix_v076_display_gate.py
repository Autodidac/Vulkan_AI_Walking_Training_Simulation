from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / 'src/ppo.hpp'
text = path.read_text(encoding='utf-8')

old_gate = '''        if (!qualification.valid
            || !environment.current_display_posture_valid())
            return false;'''
new_gate = '''        if (!qualification.valid || !environment.body_integrity_valid())
            return false;'''
if new_gate not in text:
    if old_gate not in text:
        raise SystemExit('stage display gate target was not found')
    text = text.replace(old_gate, new_gate, 1)

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
            // Qualification already proves a sustained neutral supported stance.
            // Keep the current training frame visible through a single solver-frame
            // foot-contact flicker or small raised-shoulder posture oscillation.
            return environment.uprightness() >= 0.60f
                && environment.maximum_upper_body_motor_deviation()
                    <= standing_neutral_arm_limit
                && environment.uncontrolled_spin_turns()
                    <= standing_qualification_spin_limit;
        }'''
if new_balance not in text:
    if old_balance not in text:
        raise SystemExit('balance PIP block was not found')
    text = text.replace(old_balance, new_balance, 1)

path.write_text(text, encoding='utf-8', newline='\n')

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

test_path.write_text(tests, encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('aligned standing PIP tests with current-frame priority and fallback publication')
