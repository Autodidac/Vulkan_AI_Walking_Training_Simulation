#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def write(out: Path, name: str, text: str) -> None:
    out.mkdir(parents=True, exist_ok=True)
    (out / name).write_text(text, encoding="utf-8", newline="\n")


def patch_curriculum(out: Path) -> None:
    text = read("src/autonomy_curriculum.cpp")
    text = replace_once(text,
        '''            if (catastrophic_invalid && !worker_.has_best_policy()
                && metrics.evaluation_count % 3u == 0u)
            {
                worker_.reset_policy(0x715000u
                    + metrics.evaluation_count * 0x9E3779B97F4A7C15ULL);
                worker_.set_course(stage_, difficulty_, false);
                mastery_streak_ = 0;
                degradation_streak_ = 0;
                worker_message_ = "NO VALID CHAMPION AFTER THREE EVALUATIONS - RESET POLICY NURSERY";
                queue_autosave();
                return;
            }''',
        '''            if (catastrophic_invalid && !worker_.has_best_policy()
                && nursery_policy_reset_allowed(stage_, fresh_updates, fresh_evaluations))
            {
                worker_.reset_policy(0x718000u
                    + metrics.total_updates * 0x9E3779B97F4A7C15ULL);
                worker_.set_course(stage_, difficulty_, false);
                mastery_streak_ = 0;
                degradation_streak_ = 0;
                const TrainingMetrics& restarted = worker_.metrics();
                stage_entry_total_updates_ = restarted.total_updates;
                stage_entry_total_episodes_ = restarted.total_episodes;
                stage_entry_evaluation_count_ = restarted.evaluation_count;
                stage_entry_baseline_initialized_ = true;
                worker_message_ = "EXTENDED NURSERY BUDGET EXHAUSTED - FRESH POLICY STARTED; TOTALS PRESERVED";
                queue_autosave();
                return;
            }''', "update-10 nursery reset")
    write(out, "autonomy_curriculum.cpp", text)


def patch_persistence(out: Path) -> None:
    text = read("src/autonomy_persistence.cpp")
    text = text.replace('output << "RUNAUTONOMY 13\\n";', 'output << "RUNAUTONOMY 14\\n";')
    text = text.replace('version != 13', 'version != 14')
    text = replace_once(text,
        '''        snapshot.status.pending_commands = pending_command_count();
        snapshot.status.updates_per_second = worker_updates_per_second_;
''',
        '''        snapshot.status.pending_commands = pending_command_count();
        const TrainingMetrics& stage_metrics = worker_.metrics();
        snapshot.status.stage_fresh_updates = stage_metrics.total_updates >= stage_entry_total_updates_
            ? stage_metrics.total_updates - stage_entry_total_updates_ : 0u;
        snapshot.status.stage_required_updates = stage_minimum_fresh_updates(stage_);
        snapshot.status.stage_fresh_episodes = stage_metrics.total_episodes >= stage_entry_total_episodes_
            ? stage_metrics.total_episodes - stage_entry_total_episodes_ : 0u;
        snapshot.status.stage_required_episodes = stage_minimum_fresh_episodes(stage_);
        snapshot.status.stage_fresh_evaluations = stage_metrics.evaluation_count >= stage_entry_evaluation_count_
            ? stage_metrics.evaluation_count - stage_entry_evaluation_count_ : 0u;
        snapshot.status.stage_required_evaluations = static_cast<std::uint64_t>(
            required_mastery_confirmations(stage_));
        snapshot.status.updates_per_second = worker_updates_per_second_;
''', "stage progress publication")
    write(out, "autonomy_persistence.cpp", text)


def patch_commands(out: Path) -> None:
    text = read("src/autonomy_commands.cpp")
    text = text.replace("NO V0.7.6 AUTOSAVE FOUND", "NO V0.7.18 AUTOSAVE FOUND")
    text = text.replace("V0.7.6 AUTOSAVE RESUMED ASYNCHRONOUSLY", "V0.7.18 AUTOSAVE RESUMED ASYNCHRONOUSLY")
    write(out, "autonomy_commands.cpp", text)


def patch_main(out: Path) -> None:
    text = read("src/main.cpp")
    text = replace_once(text,
        '''                    case SDL_SCANCODE_3: input.key_3_pressed = true; break;
                    case SDL_SCANCODE_S: input.save_pressed = true; break;''',
        '''                    case SDL_SCANCODE_3: input.key_3_pressed = true; break;
                    case SDL_SCANCODE_TAB: input.tab_pressed = true; break;
                    case SDL_SCANCODE_T: input.totals_pressed = true; break;
                    case SDL_SCANCODE_U: input.units_pressed = true; break;
                    case SDL_SCANCODE_A: input.art_pressed = true; break;
                    case SDL_SCANCODE_S: input.save_pressed = true; break;''', "keyboard mapping")
    write(out, "main.cpp", text)


def patch_app(out: Path) -> None:
    text = read("src/app.cpp")
    text = replace_once(text, 'bool optional_art_enabled{ true };',
        'bool optional_art_enabled{ false };', "body art default")
    text = text.replace('runner-v0717-gait-autosave.eppo', 'runner-v0718-runtime-autosave.eppo')
    text = text.replace('runner-v0717-gait-evolved.rig', 'runner-v0718-runtime-evolved.rig')
    text = text.replace('runner-v0717-gait-autonomy.state', 'runner-v0718-runtime-autonomy.state')
    text = replace_once(text, 'if (optional_art_enabled && optional_foot_art.loaded())',
        'if (optional_foot_art.loaded())', "foot sprite independence")
    text = replace_once(text,
        'add_text(canvas, { 20.0f, 50.0f }, "AUTONOMOUS PHYSICS LOCOMOTION LAB", 1.05f, muted);',
        'add_text(canvas, { 20.0f, 50.0f }, "TAB VIEW | SPACE TRAIN | 1/2/3 SPEED | T TOTALS | U UNITS | A ARMOR | R VIEW RESET", 0.92f, muted);',
        "visible controls")
    text = replace_once(text,
        '''                if (index <= 0)
                    continue;
                const float distance = static_cast<float>(index) * marker_spacing;''',
        '''                if (index < 0)
                    continue;
                const float distance = static_cast<float>(index) * marker_spacing;''', "START marker")
    text = replace_once(text,
        '''                const std::string marker_label = distance_units == ui_layout::DistanceUnits::metric
                    ? std::format("{:.2f} KM", distance / 1000.0f)
                    : std::format("{:.2f} MI", distance / 1609.344f);''',
        '''                const std::string marker_label = index == 0 ? "START"
                    : distance_units == ui_layout::DistanceUnits::metric
                        ? (distance >= 1000.0f
                            ? std::format("{:.2f} KM", distance / 1000.0f)
                            : std::format("{:.0f} M", distance))
                        : (distance >= 1609.344f
                            ? std::format("{:.2f} MI", distance / 1609.344f)
                            : std::format("{:.0f} FT", distance * 3.2808399f));''', "marker labels")
    text = text.replace('"METRIC / 0.25 KM"', '"METRIC / 10 M"')
    text = text.replace('"IMPERIAL / 0.25 MI"', '"IMPERIAL / 50 FT"')
    text = replace_once(text,
        '''                add_text_fit(canvas, cursor, std::format("UPDATE {}   ENV STEPS {}",
                    metrics.update, metrics.environment_steps), 1.10f, white, usable_width);''',
        '''                add_text_fit(canvas, cursor,
                    std::format("TOTAL UPDATES {}   STAGE {}   EVAL {}   RESET {}",
                        metrics.total_updates, metrics.update,
                        metrics.evaluation_count, metrics.total_resets),
                    1.10f, white, usable_width);''', "visible cumulative updates")
    text = replace_once(text,
        '''            add_text_fit(canvas, cursor, std::format("DIFFICULTY {:.0f}%   STRICT MASTERY {}/{}",
                autonomy.difficulty * 100.0f, autonomy.mastery_streak,
                rl::mastery_lock_confirmations), 1.12f, white, usable_width);
            cursor.y += 29.0f;
''',
        '''            add_text_fit(canvas, cursor, std::format("DIFFICULTY {:.0f}%   STRICT MASTERY {}/{}",
                autonomy.difficulty * 100.0f, autonomy.mastery_streak,
                rl::required_mastery_confirmations(autonomy.stage)), 1.12f, white, usable_width);
            cursor.y += 27.0f;
            add_text_fit(canvas, cursor,
                std::format("STAGE WORK UPD {}/{}  EPS {}/{}  EVAL {}/{}",
                    autonomy.stage_fresh_updates, autonomy.stage_required_updates,
                    autonomy.stage_fresh_episodes, autonomy.stage_required_episodes,
                    autonomy.stage_fresh_evaluations, autonomy.stage_required_evaluations),
                0.86f, accent, usable_width, 0.72f);
            cursor.y += 25.0f;
''', "stage-work UI")
    text = replace_once(text,
        '''        void process_shortcuts(const InputState& input)
        {
            if (input.key_1_pressed) mode = Mode::live;
            if (input.key_2_pressed || input.key_3_pressed) mode = Mode::rig_lab;
            if (input.escape_pressed) quit = true;
            if (input.space_pressed)
                trainer.set_background_enabled(!trainer.background_enabled());
''',
        '''        void process_shortcuts(const InputState& input)
        {
            if (input.tab_pressed)
                mode = mode == Mode::live ? Mode::rig_lab : Mode::live;
            if (input.key_1_pressed) trainer.set_updates_per_cycle(1);
            if (input.key_2_pressed) trainer.set_updates_per_cycle(2);
            if (input.key_3_pressed) trainer.set_updates_per_cycle(4);
            if (input.totals_pressed)
                live_panel_page = live_panel_page == LivePanelPage::results
                    ? LivePanelPage::totals : LivePanelPage::results;
            if (input.units_pressed)
                distance_units = distance_units == ui_layout::DistanceUnits::metric
                    ? ui_layout::DistanceUnits::imperial : ui_layout::DistanceUnits::metric;
            if (input.art_pressed)
                optional_art_enabled = !optional_art_enabled;
            if (input.escape_pressed) quit = true;
            if (input.space_pressed)
                trainer.set_background_enabled(!trainer.background_enabled());
''', "shortcut semantics")
    # Make the training PIP explicitly distinguish cumulative progress from the local counter.
    text = text.replace('std::format("UPDATE {}  STANCE', 'std::format("TOTAL {} STAGE {}  STANCE', 1)
    text = text.replace('trainer.metrics().update,\n                    environment.longest_stable_stance_seconds()',
        'trainer.metrics().total_updates, trainer.metrics().update,\n                    environment.longest_stable_stance_seconds()', 1)
    text = text.replace('std::format("UPDATE {}  BURIAL', 'std::format("TOTAL {} STAGE {}  BURIAL', 1)
    text = text.replace('trainer.metrics().update, environment.burial_depth()',
        'trainer.metrics().total_updates, trainer.metrics().update, environment.burial_depth()', 1)
    text = text.replace('std::format("UPDATE {}  CROUCH', 'std::format("TOTAL {} STAGE {}  CROUCH', 1)
    text = text.replace('trainer.metrics().update,\n                        environment.crouch_walk_seconds()',
        'trainer.metrics().total_updates, trainer.metrics().update,\n                        environment.crouch_walk_seconds()', 1)
    write(out, "app.cpp", text)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: generate_v0718_sources.py <output-directory>", file=sys.stderr)
        return 2
    out = Path(sys.argv[1]).resolve()
    patch_curriculum(out)
    patch_persistence(out)
    patch_commands(out)
    patch_main(out)
    patch_app(out)
    print(f"Runner v0.7.18 generated sources: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
