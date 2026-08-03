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
        raise RuntimeError(f'missing update-accounting target in {path}: {old[:160]!r}')
    save(path, text.replace(old, new, 1))


# Keep cumulative evaluation accounting aligned with cumulative PPO updates when
# a transferred rig, autosave, or recalibration resets optimizer-local state.
replace_once('src/ppo_trainer.cpp',
'''        metrics_.total_training_seconds = previous_metrics.total_training_seconds;
        reward_history_.clear();
''',
'''        metrics_.total_training_seconds = previous_metrics.total_training_seconds;
        metrics_.evaluation_count = previous_metrics.evaluation_count;
        if (!clear_best)
        {
            metrics_.best_evaluation_distance = previous_metrics.best_evaluation_distance;
            metrics_.best_evaluation_score = previous_metrics.best_evaluation_score;
            metrics_.best_quality_key = previous_metrics.best_quality_key;
            metrics_.best_update = previous_metrics.best_update;
        }
        reward_history_.clear();
''')

# Legacy tests referred to the removed hard sliding invalidation helper. Keep
# their intent but use the new friction-driven-shuffle classifier.
tests = load('tests/core_tests.cpp')
tests = tests.replace(
    'sim::wheel_sliding_motion(0.45f, true, true, 0.50f)',
    'sim::friction_driven_shuffle(0.45f, true, true, 0.50f, 0u, 0.0f)')
tests = tests.replace(
    'sim::wheel_sliding_motion(0.45f, true, false, 0.50f)',
    'sim::friction_driven_shuffle(0.45f, true, false, 0.50f, 0u, 0.0f)')
save('tests/core_tests.cpp', tests)

mission = load('missioncache.md')
entry = '''

### WALK-UPDATES-090 — Keep evaluations synchronized with PPO updates
**Status:** IN PROGRESS

Evaluation count is cumulative and survives optimizer, transferred-rig, autosave, and recalibration resets just as PPO update count does. With evaluation scheduled on update 1 and every fifth update, update 240 must report 49 evaluations unless the stage itself has just changed. The PIP publication and mastery streak consume each new evaluation exactly once; they may not remain at one evaluation after hundreds of updates.
'''
if '### WALK-UPDATES-090' not in mission:
    mission = mission.rstrip() + entry
save('missioncache.md', mission.rstrip() + '\n')

notes = load('RELEASE_NOTES_v0.7.7.md')
line = '- Preserves cumulative evaluation accounting across recalibration and autosave state so evaluations, mastery, PPO updates, and the PIP remain synchronized.\n'
if line not in notes:
    notes = notes.rstrip() + '\n' + line
save('RELEASE_NOTES_v0.7.7.md', notes.rstrip() + '\n')

Path(__file__).unlink()
print('fixed evaluation accounting and legacy sliding regressions')
