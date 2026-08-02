from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / 'src/simulation.cpp'
text = path.read_text(encoding='utf-8')
old = '''        const float run_reward = locomotion_required
            ? clamp(forward_speed_ / target_speed, 0.0f, 1.0f) * 0.006f : 0.0f;'''
new = '''        const bool reward_requires_locomotion = stage_requires_forward_gait(course_stage_)
            || (course_stage_ == CourseStage::duck_press && duck_press_completed_);
        const float run_reward = reward_requires_locomotion
            ? clamp(forward_speed_ / target_speed, 0.0f, 1.0f) * 0.006f : 0.0f;'''
if new not in text:
    if old not in text:
        raise SystemExit('crouch-walk run reward target not found')
    path.write_text(text.replace(old, new, 1), encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('fixed crouch-walk reward scope')
