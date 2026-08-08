#!/usr/bin/env python3
from pathlib import Path

path = Path("src/autonomy_commands.cpp")
text = path.read_text(encoding="utf-8")
old = '''            const TrainingMetrics& reset_metrics = worker_.metrics();
            last_evaluation_count_ = reset_metrics.evaluation_count;
            last_saved_best_update_ = 0;
            stage_entry_total_updates_ = reset_metrics.total_updates;
            stage_entry_total_episodes_ = reset_metrics.total_episodes;
            stage_entry_evaluation_count_ = reset_metrics.evaluation_count;
            stage_entry_baseline_initialized_ = true;
'''
new = '''            last_evaluation_count_ = worker_.metrics().evaluation_count;
            last_saved_best_update_ = 0;
            stage_entry_total_updates_ = worker_.metrics().total_updates;
            stage_entry_total_episodes_ = worker_.metrics().total_episodes;
            stage_entry_evaluation_count_ = worker_.metrics().evaluation_count;
            stage_entry_baseline_initialized_ = true;
'''
count = text.count(old)
if count != 2:
    raise SystemExit(f"Expected two generated lesson-baseline declarations, found {count}")
path.write_text(text.replace(old, new), encoding="utf-8")
print("Fixed v0.7.26 switch-case baseline scoping.")
