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
Path(__file__).unlink()
print('aligned standing PIP with qualified raised-shoulder posture')
