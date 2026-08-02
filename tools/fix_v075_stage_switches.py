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
        raise SystemExit(f'missing stage-switch target in {path}: {old[:120]}')
    save(path, text.replace(old, new, 1))


# The earlier cleanup removed the duplicate old uneven case too broadly. Restore
# the complete qualification cases with stage-specific evidence.
replace_once('src/ppo.hpp',
'''        case sim::CourseStage::hurdles:
            if (environment.longest_stable_stance_seconds() < 1.0f)''',
'''        case sim::CourseStage::uneven:
            if (environment.longest_stable_stance_seconds() < 1.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.gait_cycles() < 4u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
        case sim::CourseStage::crouch_walk:
            if (environment.longest_stable_stance_seconds() < 1.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.gait_cycles() < 4u
                || environment.crouch_walk_seconds() < 2.0f
                || environment.crouch_walk_distance() < 0.75f
                || environment.obstacles_passed() < 3u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
        case sim::CourseStage::ramps:
            if (environment.longest_stable_stance_seconds() < 1.50f
                || environment.stable_stance_seconds() < 0.35f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.powered_jumps() < 1u || environment.landed_jumps() < 1u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            break;
        case sim::CourseStage::hurdles:
            if (environment.longest_stable_stance_seconds() < 1.0f)''')

# Evaluation score for moving crouch remains separate from static crouch.
replace_once('src/ppo_parallel.cpp',
'''            case sim::CourseStage::hurdles:
                metrics_.evaluation_score = metrics_.evaluation_reward''',
'''            case sim::CourseStage::crouch_walk:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_distance * 0.72f
                    + metrics_.evaluation_stride_events * 0.06f
                    + metrics_.evaluation_duck_seconds * 0.24f
                    + metrics_.evaluation_obstacles_passed * 0.42f
                    - metrics_.evaluation_collisions * 0.14f;
                break;
            case sim::CourseStage::hurdles:
                metrics_.evaluation_score = metrics_.evaluation_reward''')

# Static crouch never receives locomotion rewards. Moving crouch receives them
# only in its own stage and retains the foot-only/body-contact penalties.
replace_once('src/simulation.cpp',
'''        const bool reward_requires_locomotion = stage_requires_forward_gait(course_stage_)
            || (course_stage_ == CourseStage::duck_press && duck_press_completed_);''',
'''        const bool reward_requires_locomotion = stage_requires_forward_gait(course_stage_);''')
replace_once('src/simulation.cpp',
'''        const float torso_swing_penalty = course_stage_ == CourseStage::duck_press
            ? std::max(0.0f, std::abs(torso_turn_speed_) - 0.22f) * 0.030f : 0.0f;''',
'''        const float torso_swing_penalty = (course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::crouch_walk)
            ? std::max(0.0f, std::abs(torso_turn_speed_) - 0.22f) * 0.030f : 0.0f;''')
replace_once('src/simulation.cpp',
'''        case CourseStage::ramps:
            last_reward_ = std::max(0.0f, upright) * 0.010f''',
'''        case CourseStage::crouch_walk:
        {
            const float maintained_crouch = duck_active_ && !non_foot_grounded_
                ? 0.030f : -0.050f;
            last_reward_ = forward_gait_reward + maintained_crouch
                + std::max(0.0f, upright) * 0.010f
                + duck_reward * 1.25f + obstacle_duck_reward
                + swing_reward + run_reward + real_step_reward
                + obstacle_lift_reward + pass_reward
                - backward_penalty - unearned_progress_penalty
                - double_support_shuffle_penalty - action_energy * 0.0010f
                - action_change_penalty - collision_penalty - knee_first_penalty
                - stance_slip_penalty - wheel_penalty - hazard_stall_penalty
                - body_contact_penalty * 2.0f - torso_swing_penalty;
            break;
        }
        case CourseStage::ramps:
            last_reward_ = std::max(0.0f, upright) * 0.010f''')

Path(__file__).unlink()
print('completed all split-stage qualification, score, and reward switches')
