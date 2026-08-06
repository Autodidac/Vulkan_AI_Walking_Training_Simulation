#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8", newline="\n")


def once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 match, found {count}")
    return text.replace(old, new, 1)


def main() -> int:
    # Curriculum: no update-10 policy destruction.
    text = read("src/autonomy_curriculum.cpp")
    text = once(text,
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
            }''', "nursery reset")
    write("src/autonomy_curriculum.cpp", text)

    # Training semantics and stronger paired-leg teacher authority.
    text = read("src/ppo.hpp")
    text = once(text,
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1700u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1800u;",
        "training semantics")
    text = once(text,
        '''            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.34f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.42f);''',
        '''            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.58f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.34f);''',
        "walking teacher authority")
    write("src/ppo.hpp", text)

    # Publish stage-local work alongside cumulative totals; bump state format.
    text = read("src/autonomy_persistence.cpp")
    text = text.replace('output << "RUNAUTONOMY 13\\n";', 'output << "RUNAUTONOMY 14\\n";')
    text = text.replace('version != 13', 'version != 14')
    text = once(text,
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
''', "stage telemetry publish")
    write("src/autonomy_persistence.cpp", text)

    text = read("src/autonomy_commands.cpp")
    text = text.replace("NO V0.7.6 AUTOSAVE FOUND", "NO V0.7.18 AUTOSAVE FOUND")
    text = text.replace("V0.7.6 AUTOSAVE RESUMED ASYNCHRONOUSLY", "V0.7.18 AUTOSAVE RESUMED ASYNCHRONOUSLY")
    write("src/autonomy_commands.cpp", text)

    # Reference markers: preserve terrain/world transforms, only restore readable references.
    text = read("src/ui_layout.hpp")
    text = once(text,
        "return units == DistanceUnits::metric ? 250.0f : 1609.344f * 0.25f;",
        "return units == DistanceUnits::metric ? 10.0f : 15.24f;",
        "marker spacing")
    write("src/ui_layout.hpp", text)

    # Keyboard inputs that README promised but runtime never exposed.
    text = read("src/app.hpp")
    text = once(text,
        '''        bool key_3_pressed{};
        bool save_pressed{};''',
        '''        bool key_3_pressed{};
        bool tab_pressed{};
        bool totals_pressed{};
        bool units_pressed{};
        bool art_pressed{};
        bool save_pressed{};''', "input fields")
    write("src/app.hpp", text)

    text = read("src/main.cpp")
    text = once(text,
        '''                    case SDL_SCANCODE_3: input.key_3_pressed = true; break;
                    case SDL_SCANCODE_S: input.save_pressed = true; break;''',
        '''                    case SDL_SCANCODE_3: input.key_3_pressed = true; break;
                    case SDL_SCANCODE_TAB: input.tab_pressed = true; break;
                    case SDL_SCANCODE_T: input.totals_pressed = true; break;
                    case SDL_SCANCODE_U: input.units_pressed = true; break;
                    case SDL_SCANCODE_A: input.art_pressed = true; break;
                    case SDL_SCANCODE_S: input.save_pressed = true; break;''', "SDL shortcuts")
    write("src/main.cpp", text)

    # Application UX and telemetry.
    text = read("src/app.cpp")
    text = text.replace('bool optional_art_enabled{ true };', 'bool optional_art_enabled{ false };', 1)
    text = text.replace('runner-v0717-gait-autosave.eppo', 'runner-v0718-runtime-autosave.eppo')
    text = text.replace('runner-v0717-gait-evolved.rig', 'runner-v0718-runtime-evolved.rig')
    text = text.replace('runner-v0717-gait-autonomy.state', 'runner-v0718-runtime-autonomy.state')
    text = once(text, 'if (optional_art_enabled && optional_foot_art.loaded())',
        'if (optional_foot_art.loaded())', "foot art independence")
    text = once(text,
        'add_text(canvas, { 20.0f, 50.0f }, "AUTONOMOUS PHYSICS LOCOMOTION LAB", 1.05f, muted);',
        'add_text(canvas, { 20.0f, 50.0f }, "TAB VIEW | SPACE TRAIN | 1/2/3 SPEED | T TOTALS | U UNITS | A ARMOR | R VIEW RESET", 0.92f, muted);',
        "control legend")
    text = once(text,
        '''                if (index <= 0)
                    continue;
                const float distance = static_cast<float>(index) * marker_spacing;''',
        '''                if (index < 0)
                    continue;
                const float distance = static_cast<float>(index) * marker_spacing;''',
        "start marker")
    text = once(text,
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
                            : std::format("{:.0f} FT", distance * 3.2808399f));''',
        "marker labels")
    text = text.replace('"METRIC / 0.25 KM"', '"METRIC / 10 M"')
    text = text.replace('"IMPERIAL / 0.25 MI"', '"IMPERIAL / 50 FT"')
    text = once(text,
        '''                add_text_fit(canvas, cursor, std::format("UPDATE {}   ENV STEPS {}",
                    metrics.update, metrics.environment_steps), 1.10f, white, usable_width);''',
        '''                add_text_fit(canvas, cursor,
                    std::format("TOTAL UPDATES {}   STAGE {}   EVAL {}   RESET {}",
                        metrics.total_updates, metrics.update,
                        metrics.evaluation_count, metrics.total_resets),
                    1.10f, white, usable_width);''', "results totals")
    text = once(text,
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
''', "stage progress UI")
    text = once(text,
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
''', "shortcut behavior")
    write("src/app.cpp", text)

    # Focused docs and tests.
    write("docs/RUNNER_V0718_RUNTIME_RECOVERY.md", '''# Runner v0.7.18 runtime recovery

Terrain rendering, collision, pressure, and treadmill coordinates are intentionally unchanged.

The v0.7.17 nursery evaluated at updates 1, 5, and 10 and could reset on the third invalid evaluation even though Stand required 120 fresh updates. v0.7.18 moves automatic nursery restart beyond the complete stage dwell budget and keeps cumulative totals visible.

The live UI shows all-time updates, local stage/policy updates, evaluations, resets, stage-work thresholds, pipeline state, and throughput. START and recurring 10 m / 50 ft reference markers are visible near launch. Tab/Space/1/2/3/T/U/A/R controls are advertised in-app. Optional body armor defaults off; foot sprites remain independent and visual-only. Early Walk receives stronger sagittal bootstrap guidance without weakening crab-walk rejection or mastery thresholds.
''')

    write("tests/v0718_runtime_recovery_tests.cpp", r'''#include "autonomy.hpp"
#include "ppo.hpp"
#include "ui_layout.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "Runner v0.7.18 regression failed: " << message << '\n';
            std::exit(1);
        }
    }
}

int main()
{
    using namespace runner;
    require(rl::training_semantics_version == 0x0007'1800u,
        "training semantics not isolated");
    require(rl::stage_minimum_fresh_updates(sim::CourseStage::balance) == 120u,
        "Stand dwell changed unexpectedly");
    require(!rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 10u, 3u),
        "update-10 reset still possible");
    require(!rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 120u, 12u),
        "reset allowed at the Stand dwell boundary");
    require(rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 240u, 12u),
        "extended nursery reset can never activate");
    require(ui_layout::course_reference_marker_spacing_m(
            ui_layout::DistanceUnits::metric) == 10.0f,
        "metric near-start marker spacing wrong");
    require(std::abs(ui_layout::course_reference_marker_spacing_m(
            ui_layout::DistanceUnits::imperial) - 15.24f) < 0.0001f,
        "imperial marker is not 50 feet");

    sim::Environment walking{ sim::CreatureBlueprint::biped(), 0x718u };
    walking.set_course(sim::CourseStage::uneven, 0.30f);
    const auto teacher = rl::walking_teacher_action(walking);
    require(teacher[0] * teacher[2] <= 0.0f,
        "walking teacher hips are not opposite-phase");
    const std::array<float, sim::action_count> neutral{};
    const auto assisted = rl::effective_policy_action(
        walking, neutral, sim::CourseStage::uneven);
    require(std::abs(assisted[0] - assisted[2]) > 0.08f,
        "early Walk assistance lacks sagittal leg separation");

    std::cout << "Runner v0.7.18 runtime recovery contracts passed\n";
    return 0;
}
''')

    readme = read("README.md")
    readme = readme.replace("Runner 0.7.16 is", "Runner 0.7.18 is", 1)
    if "`T`: Toggle Training Results" not in readme:
        readme = readme.replace("- `S`: Save the current rig\n",
            "- `T`: Toggle Training Results / Lifetime Totals\n- `U`: Toggle metric / imperial distance labels\n- `A`: Toggle optional body armor overlays\n- `S`: Save the current rig\n")
    if "RUNNER_V0718_RUNTIME_RECOVERY.md" not in readme:
        readme = readme.replace(
            "- [`docs/RUNNER_V0717_EYE_TEST_CORRECTION.md`](docs/RUNNER_V0717_EYE_TEST_CORRECTION.md) documents the released eye-test corrections for crouch recovery, sustained sagittal gait, terrain-conforming support stubs, and supplied optional sprites.\n",
            "- [`docs/RUNNER_V0717_EYE_TEST_CORRECTION.md`](docs/RUNNER_V0717_EYE_TEST_CORRECTION.md) documents the v0.7.17 crouch/gait/stub-foot correction.\n- [`docs/RUNNER_V0718_RUNTIME_RECOVERY.md`](docs/RUNNER_V0718_RUNTIME_RECOVERY.md) documents the update-loop, marker, controls, telemetry, and walking recovery.\n")
    write("README.md", readme)

    changelog = read("CHANGELOG.md")
    if not changelog.startswith("## 0.7.18"):
        changelog = '''## 0.7.18

- Removed the contradictory update-10 no-champion nursery reset while preserving cumulative totals.
- Added continuously visible total/stage updates, evaluations, resets, stage-work thresholds, pipeline state, and throughput.
- Restored START plus recurring 10 m / 50 ft reference markers without changing terrain simulation.
- Corrected keyboard controls and added visible in-app help.
- Disabled optional body skin overlays by default while retaining visual sprite feet.
- Strengthened early sagittal walking bootstrap without weakening crab-walk rejection.
- Isolated v0.7.18 training/autonomy state.

''' + changelog
    write("CHANGELOG.md", changelog)

    # Minimal repository audit update; keep existing art-hash and hygiene checks.
    audit = read("tools/repository_audit.cmake")
    audit = audit.replace("docs/RUNNER_V0717_EYE_TEST_CORRECTION.md\n",
        "docs/RUNNER_V0717_EYE_TEST_CORRECTION.md\n        docs/RUNNER_V0718_RUNTIME_RECOVERY.md\n", 1)
    audit = audit.replace("project(Runner VERSION 0.7.17 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.18 LANGUAGES CXX)")
    audit = audit.replace("CMake project version is not 0.7.17",
        "CMake project version is not 0.7.18")
    audit = audit.replace("training_semantics_version = 0x0007'1700u",
        "training_semantics_version = 0x0007'1800u")
    audit = audit.replace("Training semantics are not isolated for v0.7.17",
        "Training semantics are not isolated for v0.7.18")
    write("tools/repository_audit.cmake", audit)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
