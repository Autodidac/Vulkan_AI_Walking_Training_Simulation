#!/usr/bin/env python3
from pathlib import Path


def replace_exact(path: str, old: str, new: str, expected: int = 1) -> None:
    file_path = Path(path)
    text = file_path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != expected:
        raise SystemExit(f"{path}: expected {expected} matches, found {count}")
    file_path.write_text(text.replace(old, new), encoding="utf-8")


replace_exact(
    "src/autonomy_commands.cpp",
    '''            const TrainingMetrics& reset_metrics = worker_.metrics();
            last_evaluation_count_ = reset_metrics.evaluation_count;
            last_saved_best_update_ = 0;
            stage_entry_total_updates_ = reset_metrics.total_updates;
            stage_entry_total_episodes_ = reset_metrics.total_episodes;
            stage_entry_evaluation_count_ = reset_metrics.evaluation_count;
            stage_entry_baseline_initialized_ = true;
''',
    '''            last_evaluation_count_ = worker_.metrics().evaluation_count;
            last_saved_best_update_ = 0;
            stage_entry_total_updates_ = worker_.metrics().total_updates;
            stage_entry_total_episodes_ = worker_.metrics().total_episodes;
            stage_entry_evaluation_count_ = worker_.metrics().evaluation_count;
            stage_entry_baseline_initialized_ = true;
''',
    expected=2,
)

replace_exact(
    "src/training_explainer.hpp",
    '''        return "TOTAL RIG UPDATES = completed learning cycles for the selected rig. It survives episode and policy retries, and resets only when a different rig is selected.";
''',
    '''        return "TOTAL RIG UPDATES = completed learning cycles for the selected rig. It never resets during episode or policy retries for the same rig; selecting a different rig starts at zero.";
''',
)

replace_exact(
    "src/training_explainer.hpp",
    '''        return "RESETS restart an episode or weak policy; ROLLBACKS restore a better retained controller. All-time totals stay.";
''',
    '''        return "RESETS restart an episode or weak policy; ROLLBACKS restore a better retained controller. Rig totals stay for the selected rig and clear when a different rig is selected.";
''',
)

replace_exact(
    "tests/v0721_readable_telemetry_tests.cpp",
    '''        require(runner::telemetry::reset_help().find("All-time totals stay")
                != std::string_view::npos,
            "reset help must explain that all-time work survives");
''',
    '''        require(runner::telemetry::reset_help().find("selected rig")
                != std::string_view::npos,
            "reset help must explain rig-scoped persistence");
''',
)

replace_exact(
    "tests/v0725_art_leg_hotfix_tests.cpp",
    '''        require(app.find("draw_pixel_art(canvas, optional_torso_art")
                == std::string::npos,
            "oversized torso bitmap overlay is still rendered");
''',
    '''        require(app.find("draw_pixel_art(canvas, optional_torso_art")
                != std::string::npos
                && app.find("User-supplied modular armor, bounded to the physical torso")
                    != std::string::npos
                && app.find("std::clamp(body_span * 0.72f, 42.0f, 76.0f)")
                    != std::string::npos,
            "bounded v0.7.26 torso component is missing or unbounded");
''',
)

print("Fixed v0.7.26 switch scoping and superseded telemetry/art regression contracts.")
