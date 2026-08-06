#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def cache() -> None:
    path = ROOT / "missioncache.md"
    text = path.read_text(encoding="utf-8")
    anchor = "# Runner cache-first engineering policy and active release plan\n\n"
    section = '''# Runner v0.7.18 runtime recovery, controls, and observability release

**Release state:** CACHED BEFORE IMPLEMENTATION — released v0.7.17 eye-test regressions reopened. Terrain is explicitly preserved. The deferred equipment/carry/target curriculum moves intact to v0.7.19.

Direct packaged-runtime observations on v0.7.17 are authoritative: the terrain itself appears correct; reference/mile markers are absent from the initial live view; the visible update counter reaches approximately 10 and then appears to reset; all-time updates are hidden behind a secondary totals page; the trainer remains effectively stuck at the beginning and never reaches useful walking; the live controls and status information are difficult to discover or interpret; and the optional torso/helmet skin is visually unacceptable. Source audit confirms the update-loop cause: policy evaluation occurs at updates 1, 5, and 10, while `manage_curriculum_locked()` resets a no-champion policy on every third invalid evaluation. That reset therefore occurs at update 10 even though Stand requires 120 fresh updates before its dwell gate can complete. `reset_training_state()` preserves `total_updates`, so the cumulative work exists but the primary UI displays the resettable stage/update counter instead.

### WALK-RUNTIME-RESET-211 — Remove the update-10 nursery-reset contradiction
**Status:** OPEN — RELEASE BLOCKING

A no-champion controller may not be automatically discarded before the current stage has received a meaningful training budget. Stand must be able to accumulate at least its full 120-update dwell requirement without policy reset. Any later nursery reset requires an explicit large fresh-update/evaluation budget and must preserve cumulative totals.

### WALK-TOTAL-UPDATES-212 — Make cumulative updates impossible to miss
**Status:** OPEN — RELEASE BLOCKING

Live world and Training Results must show `total_updates` continuously, alongside the resettable stage/policy update count, evaluation count, reset count, and updates/second. The user must be able to distinguish training progress from a policy reset without opening another page.

### WALK-STAGE-PROGRESS-213 — Expose actual stage-work progress
**Status:** OPEN — RELEASE BLOCKING

Publish fresh updates, episodes, and evaluations since stage entry plus their required thresholds. The live panel must explain whether the trainer is waiting on work, strict evidence, or mastery confirmations instead of presenting an opaque starting state.

### WALK-MARKERS-214 — Restore visible reference markers near the start
**Status:** OPEN — RELEASE BLOCKING

Keep the current terrain/collision coordinate system unchanged. Restore a START marker and useful recurring distance markers inside the initial live viewport. Marker world positions must remain treadmill/course-progress correct.

### WALK-MARKER-LABELS-215 — Use readable meter/foot marker labels
**Status:** OPEN — RELEASE BLOCKING

Metric reference markers use practical meter labels near the start; imperial markers use practical foot labels before mile-scale distances. Do not display tiny `0.00 KM`/`0.00 MI` labels for nearby markers.

### WALK-CONTROLS-216 — Restore documented keyboard controls
**Status:** OPEN — RELEASE BLOCKING

`Tab` switches Live/Rig Lab; `Space` runs/pauses background training; `1/2/3` select Normal/Faster/Max CPU; `T` toggles results/totals; `U` toggles metric/imperial; `A` toggles optional armor; `R` resets only the live camera/preview. Runtime mappings must match README and visible help.

### WALK-CONTROL-UI-217 — Put control help in the application
**Status:** OPEN — RELEASE BLOCKING

The top bar and live panel must advertise the controls rather than requiring source/README knowledge. Training state, speed mode, pause state, and pipeline stage must remain visible.

### WALK-SKIN-218 — Disable the unacceptable body skin by default
**Status:** OPEN — RELEASE BLOCKING

Keep forward sprite feet available, but optional torso/helmet/weapon overlays default OFF and remain explicitly toggleable. Optional visuals never affect physics, training, observations, package startup, or the terrain.

### WALK-WALK-BOOTSTRAP-219 — Restore useful early walking guidance
**Status:** OPEN — RELEASE BLOCKING

Once the trainer reaches Walk, early paired-leg policies receive strong enough sagittal teacher guidance to produce visible fore/aft alternating leg motion and forward progress while still allowing PPO authority to grow. Do not loosen crab-walk rejection or stage mastery evidence.

### WALK-STATE-220 — Isolate corrected runtime state
**Status:** OPEN — RELEASE BLOCKING

Bump training/autonomy semantics and use `runner-v0718-*` autosave paths so the broken v0.7.17 reset loop or stale controller state cannot silently resume. Manual checkpoint transfer remains explicit.

### WALK-SOURCE-AUDIT-221 — Remove stale version/control assumptions
**Status:** OPEN — RELEASE BLOCKING

Audit application, autonomy, PPO, UI layout, persistence, tests, docs, CMake, package audit, and release automation for stale v0.7.6/v0.7.16/v0.7.17 runtime strings and contradictory control/update assumptions.

### WALK-REGRESSION-222 — Deterministically test the reset, marker, and gait recovery
**Status:** OPEN — RELEASE BLOCKING

Tests prove: update 10 cannot trigger nursery reset; the full Stand dwell can accumulate; a later bounded reset remains possible; initial reference-marker spacing is visible; v0.7.18 semantics are isolated; and the paired-leg walking teacher provides strong opposite-phase sagittal drive without changing terrain coordinates.

### WALK-DOC-223 — Document runtime recovery and controls
**Status:** OPEN — RELEASE BLOCKING

Update README, CHANGELOG, focused v0.7.18 documentation, missioncache, CMake install contents, and repository audit. Keep a single changelog and a single mission ledger.

### WALK-PACKAGE-224 — Audit the complete v0.7.18 package
**Status:** OPEN — RELEASE BLOCKING

Linux warnings-as-errors, full Windows SDL3/Vulkan build, all tests, acceptance/camera/package diagnostics, installed/extracted runs, optional-art fallback, ZIP/checksum/manifest, and unrelated-directory `run.bat` must all pass.

### WALK-RELEASE-225 — Publish and verify Runner v0.7.18
**Status:** OPEN — RELEASE BLOCKING

Merge only validated source, tag `v0.7.18`, publish audited assets, re-download and byte-verify them, record exact evidence, remove the release branch, and leave only `main`. User eye testing may reopen any matching mission.

'''
    if "# Runner v0.7.18 runtime recovery, controls, and observability release" not in text:
        if anchor not in text:
            raise RuntimeError("missioncache header missing")
        text = text.replace(anchor, anchor + section, 1)
    text = text.replace(
        "# Runner v0.7.18 equipment, carry, and target curriculum",
        "# Runner v0.7.19 equipment, carry, and target curriculum")
    text = text.replace(
        "**Release state:** VALIDATED — PR LINUX/WINDOWS/PACKAGE GATE PASSED; MERGE AND PUBLICATION PENDING. The equipment/carry/target curriculum remains intact for v0.7.18.",
        "**Release state:** PUBLISHED — v0.7.17 merged, packaged, published, re-downloaded, and branch cleanup verified; later user eye testing reopens only the matching v0.7.18 runtime-recovery missions above. The equipment/carry/target curriculum remains intact for v0.7.19.",
        1)
    text = text.replace(
        "### WALK-RELEASE-210 — Publish audited Runner v0.7.17\n**Status:** OPEN — RELEASE BLOCKING",
        "### WALK-RELEASE-210 — Publish audited Runner v0.7.17\n**Status:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED",
        1)
    path.write_text(text, encoding="utf-8", newline="\n")


def implement() -> None:
    # CMake/version/package docs/tests.
    cmake = read("CMakeLists.txt")
    cmake = replace_once(cmake,
        "project(Runner VERSION 0.7.17 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.18 LANGUAGES CXX)", "CMake version")
    cmake = replace_once(cmake,
        '''        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md"
            "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md"
''',
        '''        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md"
            "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0718_RUNTIME_RECOVERY.md"
            "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0718_RUNTIME_RECOVERY.md"
''', "post-build v0718 doc")
    cmake = replace_once(cmake,
        '''        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md"
        DESTINATION docs)''',
        '''        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0718_RUNTIME_RECOVERY.md"
        DESTINATION docs)''', "install v0718 doc")
    test_anchor = '''    add_executable(RunnerV0717EyeTestTests tests/v0717_eye_test_tests.cpp)
    target_link_libraries(RunnerV0717EyeTestTests PRIVATE Runner::Core)
    target_compile_features(RunnerV0717EyeTestTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0717EyeTestTests)
    add_test(NAME Runner.V0717EyeTest
        COMMAND RunnerV0717EyeTestTests "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    set_tests_properties(Runner.V0717EyeTest PROPERTIES TIMEOUT 180)
'''
    cmake = replace_once(cmake, test_anchor, test_anchor + '''
    add_executable(RunnerV0718RuntimeRecoveryTests tests/v0718_runtime_recovery_tests.cpp)
    target_link_libraries(RunnerV0718RuntimeRecoveryTests PRIVATE Runner::Core)
    target_compile_features(RunnerV0718RuntimeRecoveryTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0718RuntimeRecoveryTests)
    add_test(NAME Runner.V0718RuntimeRecovery COMMAND RunnerV0718RuntimeRecoveryTests)
    set_tests_properties(Runner.V0718RuntimeRecovery PROPERTIES TIMEOUT 90)
''', "v0718 test target")
    write("CMakeLists.txt", cmake)

    # Training semantics, no update-10 reset, and stronger early walking guidance.
    ppo = read("src/ppo.hpp")
    ppo = replace_once(ppo,
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1700u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1800u;",
        "training semantics")
    ppo = replace_once(ppo,
        '''            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.34f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.42f);''',
        '''            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.58f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.34f);''',
        "walking teacher authority")
    write("src/ppo.hpp", ppo)

    trainer = read("src/ppo_trainer.cpp")
    old_forward = '''            if (!sim::stage_requires_forward_gait(stage))
                return 0.0f;
            if (update < 400u)
                return 0.24f;
            if (update < 2200u)
                return lerp(0.24f, 0.10f,
                    static_cast<float>(update - 400u) / 1800.0f);
            if (update < 7000u)
                return lerp(0.10f, 0.02f,
                    static_cast<float>(update - 2200u) / 4800.0f);
            return 0.0f;'''
    new_forward = '''            if (!sim::stage_requires_forward_gait(stage))
                return 0.0f;
            if (update < 800u)
                return 0.50f;
            if (update < 3000u)
                return lerp(0.50f, 0.18f,
                    static_cast<float>(update - 800u) / 2200.0f);
            if (update < 8000u)
                return lerp(0.18f, 0.04f,
                    static_cast<float>(update - 3000u) / 5000.0f);
            return 0.02f;'''
    trainer = replace_once(trainer, old_forward, new_forward, "forward gait bootstrap")
    write("src/ppo_trainer.cpp", trainer)

    autonomy_h = read("src/autonomy.hpp")
    fresh_anchor = '''    [[nodiscard]] inline bool stage_fresh_work_complete(
        sim::CourseStage stage, std::uint64_t fresh_updates,
        std::uint64_t fresh_episodes, std::uint64_t fresh_evaluations) noexcept
    {
        return fresh_updates >= stage_minimum_fresh_updates(stage)
            && fresh_episodes >= stage_minimum_fresh_episodes(stage)
            && fresh_evaluations >= static_cast<std::uint64_t>(
                required_mastery_confirmations(stage));
    }
'''
    autonomy_h = replace_once(autonomy_h, fresh_anchor, fresh_anchor + '''
    [[nodiscard]] inline bool nursery_policy_reset_allowed(
        sim::CourseStage stage, std::uint64_t fresh_updates,
        std::uint64_t fresh_evaluations) noexcept
    {
        const std::uint64_t minimum_budget = stage_minimum_fresh_updates(stage) + 120u;
        return fresh_updates >= minimum_budget
            && fresh_evaluations >= 12u
            && (fresh_evaluations % 12u) == 0u;
    }
''', "nursery reset helper")
    status_anchor = '''        std::size_t pending_commands{};
        double updates_per_second{};
        int speed_mode{ 1 };
'''
    autonomy_h = replace_once(autonomy_h, status_anchor, '''        std::size_t pending_commands{};
        std::uint64_t stage_fresh_updates{};
        std::uint64_t stage_required_updates{};
        std::uint64_t stage_fresh_episodes{};
        std::uint64_t stage_required_episodes{};
        std::uint64_t stage_fresh_evaluations{};
        std::uint64_t stage_required_evaluations{};
        double updates_per_second{};
        int speed_mode{ 1 };
''', "autonomy stage work telemetry")
    write("src/autonomy.hpp", autonomy_h)

    curriculum = read("src/autonomy_curriculum.cpp")
    curriculum = replace_once(curriculum,
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
                stage_entry_total_updates_ = worker_.metrics().total_updates;
                stage_entry_total_episodes_ = worker_.metrics().total_episodes;
                stage_entry_evaluation_count_ = worker_.metrics().evaluation_count;
                worker_message_ = "EXTENDED NURSERY BUDGET EXHAUSTED - FRESH POLICY STARTED; TOTALS PRESERVED";
                queue_autosave();
                return;
            }''', "update-10 reset contradiction")
    write("src/autonomy_curriculum.cpp", curriculum)

    persistence = read("src/autonomy_persistence.cpp")
    persistence = persistence.replace('output << "RUNAUTONOMY 13\\n";', 'output << "RUNAUTONOMY 14\\n";')
    persistence = persistence.replace('version != 13', 'version != 14')
    # Publish actual current-stage work.
    publish_anchor = '''        snapshot.status.pending_commands = pending_command_count();
        snapshot.status.updates_per_second = worker_updates_per_second_;
'''
    publish_new = '''        snapshot.status.pending_commands = pending_command_count();
        const TrainingMetrics& status_metrics = worker_.metrics();
        snapshot.status.stage_fresh_updates = status_metrics.total_updates >= stage_entry_total_updates_
            ? status_metrics.total_updates - stage_entry_total_updates_ : 0u;
        snapshot.status.stage_required_updates = stage_minimum_fresh_updates(stage_);
        snapshot.status.stage_fresh_episodes = status_metrics.total_episodes >= stage_entry_total_episodes_
            ? status_metrics.total_episodes - stage_entry_total_episodes_ : 0u;
        snapshot.status.stage_required_episodes = stage_minimum_fresh_episodes(stage_);
        snapshot.status.stage_fresh_evaluations = status_metrics.evaluation_count >= stage_entry_evaluation_count_
            ? status_metrics.evaluation_count - stage_entry_evaluation_count_ : 0u;
        snapshot.status.stage_required_evaluations = static_cast<std::uint64_t>(
            required_mastery_confirmations(stage_));
        snapshot.status.updates_per_second = worker_updates_per_second_;
'''
    persistence = replace_once(persistence, publish_anchor, publish_new, "stage work publication")
    write("src/autonomy_persistence.cpp", persistence)

    commands = read("src/autonomy_commands.cpp")
    commands = commands.replace("NO V0.7.6 AUTOSAVE FOUND", "NO V0.7.18 AUTOSAVE FOUND")
    commands = commands.replace("V0.7.6 AUTOSAVE RESUMED ASYNCHRONOUSLY", "V0.7.18 AUTOSAVE RESUMED ASYNCHRONOUSLY")
    write("src/autonomy_commands.cpp", commands)

    # Visible reference markers near start, practical labels, and correct controls.
    layout = read("src/ui_layout.hpp")
    layout = replace_once(layout,
        'return units == DistanceUnits::metric ? 250.0f : 1609.344f * 0.25f;',
        'return units == DistanceUnits::metric ? 10.0f : 15.24f;',
        "reference marker spacing")
    write("src/ui_layout.hpp", layout)

    app_h = read("src/app.hpp")
    app_h = replace_once(app_h,
        '''        bool key_3_pressed{};
        bool save_pressed{};''',
        '''        bool key_3_pressed{};
        bool tab_pressed{};
        bool totals_pressed{};
        bool units_pressed{};
        bool art_pressed{};
        bool save_pressed{};''', "input shortcuts")
    write("src/app.hpp", app_h)

    main = read("src/main.cpp")
    main = replace_once(main,
        '''                    case SDL_SCANCODE_3: input.key_3_pressed = true; break;
                    case SDL_SCANCODE_S: input.save_pressed = true; break;''',
        '''                    case SDL_SCANCODE_3: input.key_3_pressed = true; break;
                    case SDL_SCANCODE_TAB: input.tab_pressed = true; break;
                    case SDL_SCANCODE_T: input.totals_pressed = true; break;
                    case SDL_SCANCODE_U: input.units_pressed = true; break;
                    case SDL_SCANCODE_A: input.art_pressed = true; break;
                    case SDL_SCANCODE_S: input.save_pressed = true; break;''', "SDL shortcut mapping")
    write("src/main.cpp", main)

    app = read("src/app.cpp")
    app = app.replace('bool optional_art_enabled{ true };', 'bool optional_art_enabled{ false };', 1)
    app = app.replace('std::filesystem::path autosave_policy_path{ "runner-v0717-gait-autosave.eppo" };',
        'std::filesystem::path autosave_policy_path{ "runner-v0718-runtime-autosave.eppo" };', 1)
    app = app.replace('std::filesystem::path autosave_rig_path{ "runner-v0717-gait-evolved.rig" };',
        'std::filesystem::path autosave_rig_path{ "runner-v0718-runtime-evolved.rig" };', 1)
    app = app.replace('std::filesystem::path autosave_state_path{ "runner-v0717-gait-autonomy.state" };',
        'std::filesystem::path autosave_state_path{ "runner-v0718-runtime-autonomy.state" };', 1)
    # Keep foot sprites independent from body skin toggle.
    app = replace_once(app,
        'if (optional_art_enabled && optional_foot_art.loaded())',
        'if (optional_foot_art.loaded())', "foot sprite independent from armor")
    # Visible top-bar control legend.
    app = replace_once(app,
        'add_text(canvas, { 20.0f, 50.0f }, "AUTONOMOUS PHYSICS LOCOMOTION LAB", 1.05f, muted);',
        'add_text(canvas, { 20.0f, 50.0f }, "TAB VIEW | SPACE TRAIN | 1/2/3 SPEED | T TOTALS | U UNITS | A ARMOR | R VIEW RESET", 0.92f, muted);',
        "top bar controls")
    # Restore marker zero and practical near-distance labels.
    app = replace_once(app,
        '''                if (index <= 0)
                    continue;
                const float distance = static_cast<float>(index) * marker_spacing;''',
        '''                if (index < 0)
                    continue;
                const float distance = static_cast<float>(index) * marker_spacing;''',
        "start marker")
    app = replace_once(app,
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
        "reference marker labels")
    app = app.replace('"METRIC / 0.25 KM"', '"METRIC / 10 M"')
    app = app.replace('"IMPERIAL / 0.25 MI"', '"IMPERIAL / 50 FT"')
    # Live armor/debug buttons are available without entering Rig Lab.
    unit_row = '''            cursor.y += 53.0f;

            if (button({ cursor, { half, 38.0f } }, "TRAINING RESULTS", input,'''
    unit_new = '''            cursor.y += 45.0f;
            if (button({ cursor, { half, 34.0f } },
                optional_art_enabled ? "ARMOR ART: ON" : "ARMOR ART: OFF",
                input, optional_art_enabled))
                optional_art_enabled = !optional_art_enabled;
            if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 34.0f } },
                debug_skeleton_overlay ? "SKELETON: ON" : "SKELETON: OFF",
                input, debug_skeleton_overlay))
                debug_skeleton_overlay = !debug_skeleton_overlay;
            cursor.y += 42.0f;

            if (button({ cursor, { half, 38.0f } }, "TRAINING RESULTS", input,'''
    app = replace_once(app, unit_row, unit_new, "live art controls")
    # Results show cumulative vs stage-local counters.
    app = replace_once(app,
        '''                add_text_fit(canvas, cursor, std::format("UPDATE {}   ENV STEPS {}",
                    metrics.update, metrics.environment_steps), 1.10f, white, usable_width);''',
        '''                add_text_fit(canvas, cursor,
                    std::format("TOTAL UPDATES {}   STAGE {}   EVAL {}",
                        metrics.total_updates, metrics.update, metrics.evaluation_count),
                    1.10f, white, usable_width);''', "results cumulative updates")
    # Add explicit stage work status under current lesson.
    progress_anchor = '''            add_text_fit(canvas, cursor, std::format("DIFFICULTY {:.0f}%   STRICT MASTERY {}/{}",
                autonomy.difficulty * 100.0f, autonomy.mastery_streak,
                rl::mastery_lock_confirmations), 1.12f, white, usable_width);
            cursor.y += 29.0f;
'''
    progress_new = '''            add_text_fit(canvas, cursor, std::format("DIFFICULTY {:.0f}%   STRICT MASTERY {}/{}",
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
'''
    app = replace_once(app, progress_anchor, progress_new, "stage progress UI")
    # Primary world overlay: cumulative progress first; no hidden reset ambiguity.
    old_overlay = '''            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 90.0f },
                std::format("COURSE {}   {}", format_distance(environment.course_progress()),
                    sim::invalid_motion_name(environment.invalid_reason())),
                1.16f, environment.valid_motion() ? green : danger, overlay_width);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 119.0f },
                std::format("STEPS {}  CROSS {}  HEEL {}  TOE {}  SLIP {:.2f}",
                    environment.alternating_steps(), environment.limb_crossings(),
                    environment.heel_strikes(), environment.toe_offs(),
                    environment.stance_slip_speed()),
                1.02f, environment.recovering() ? yellow : muted, overlay_width);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 147.0f },
                std::format("L {}  R {}  DUCK {:.1f} S  JUMP {}/{}  PASSED {}",
                    sim::foot_contact_phase_name(environment.left_foot_phase()),
                    sim::foot_contact_phase_name(environment.right_foot_phase()),
                    environment.duck_seconds(), environment.powered_jumps(),
                    environment.landed_jumps(), environment.obstacles_passed()),
                0.96f, muted, overlay_width);'''
    new_overlay = '''            const rl::TrainingMetrics& live_metrics = trainer.metrics();
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 90.0f },
                std::format("TOTAL UPDATES {}   STAGE {}   EVAL {}   RESET {}",
                    live_metrics.total_updates, live_metrics.update,
                    live_metrics.evaluation_count, live_metrics.total_resets),
                1.08f, accent, overlay_width);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 119.0f },
                std::format("COURSE {}   STEPS {}  CROSS {}  SLIP {:.2f}",
                    format_distance(environment.course_progress()),
                    environment.alternating_steps(), environment.limb_crossings(),
                    environment.stance_slip_speed()),
                1.02f, environment.recovering() ? yellow : muted, overlay_width);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 147.0f },
                std::format("{}   {:.2f} UPD/S   {}",
                    autonomy.pipeline_stage, autonomy.updates_per_second,
                    sim::invalid_motion_name(environment.invalid_reason())),
                0.96f, environment.valid_motion() ? muted : danger, overlay_width);'''
    app = replace_once(app, old_overlay, new_overlay, "live cumulative overlay")
    # PIP also distinguishes total from stage update.
    app = app.replace('std::format("UPDATE {}  STANCE', 'std::format("TOTAL {} STAGE {}  STANCE', 1)
    app = app.replace('trainer.metrics().update,\n                    environment.longest_stable_stance_seconds()',
        'trainer.metrics().total_updates, trainer.metrics().update,\n                    environment.longest_stable_stance_seconds()', 1)
    app = app.replace('std::format("UPDATE {}  BURIAL', 'std::format("TOTAL {} STAGE {}  BURIAL', 1)
    app = app.replace('trainer.metrics().update, environment.burial_depth()',
        'trainer.metrics().total_updates, trainer.metrics().update, environment.burial_depth()', 1)
    app = app.replace('std::format("UPDATE {}  CROUCH', 'std::format("TOTAL {} STAGE {}  CROUCH', 1)
    app = app.replace('trainer.metrics().update,\n                        environment.crouch_walk_seconds()',
        'trainer.metrics().total_updates, trainer.metrics().update,\n                        environment.crouch_walk_seconds()', 1)
    # Correct shortcut semantics.
    old_shortcuts = '''        void process_shortcuts(const InputState& input)
        {
            if (input.key_1_pressed) mode = Mode::live;
            if (input.key_2_pressed || input.key_3_pressed) mode = Mode::rig_lab;
            if (input.escape_pressed) quit = true;
            if (input.space_pressed)
                trainer.set_background_enabled(!trainer.background_enabled());
'''
    new_shortcuts = '''        void process_shortcuts(const InputState& input)
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
'''
    app = replace_once(app, old_shortcuts, new_shortcuts, "shortcut semantics")
    write("src/app.cpp", app)

    # README/changelog/focused doc.
    readme = read("README.md")
    readme = readme.replace("Runner 0.7.16 is", "Runner 0.7.18 is", 1)
    readme = readme.replace("- `S`: Save the current rig\n", "- `T`: Toggle Training Results / Lifetime Totals\n- `U`: Toggle metric / imperial distance labels\n- `A`: Toggle optional body armor overlays; sprite feet remain available\n- `S`: Save the current rig\n")
    readme = readme.replace(
        "- [`docs/RUNNER_V0717_EYE_TEST_CORRECTION.md`](docs/RUNNER_V0717_EYE_TEST_CORRECTION.md) documents the released eye-test corrections for crouch recovery, sustained sagittal gait, terrain-conforming support stubs, and supplied optional sprites.\n",
        "- [`docs/RUNNER_V0717_EYE_TEST_CORRECTION.md`](docs/RUNNER_V0717_EYE_TEST_CORRECTION.md) documents the v0.7.17 crouch/gait/stub-foot correction.\n- [`docs/RUNNER_V0718_RUNTIME_RECOVERY.md`](docs/RUNNER_V0718_RUNTIME_RECOVERY.md) documents the update-loop, marker, control, telemetry, walking-bootstrap, and optional-skin recovery.\n")
    readme += '''\n## v0.7.18 runtime recovery\n\n- The live overlay always shows cumulative `TOTAL UPDATES`, resettable stage updates, evaluations, resets, pipeline state, and update rate.\n- The no-champion nursery cannot reset at update 10; it receives an extended budget beyond the current stage's required dwell before any automatic restart.\n- START and recurring 10 m / 50 ft reference markers are visible from the initial course view without changing terrain coordinates.\n- Keyboard controls now match the documented behavior.\n- Optional torso/helmet/weapon overlays default OFF; forward foot sprites remain independent.\n- v0.7.18 uses isolated autosave and training semantics.\n'''
    write("README.md", readme)

    changelog = read("CHANGELOG.md")
    entry = '''## 0.7.18\n\n- Removed the contradictory update-10 no-champion nursery reset; cumulative training totals now remain visible while stage-local counters may restart only after an extended budget.\n- Added always-visible total/stage update, evaluation, reset, stage-work, pipeline, and throughput telemetry.\n- Restored a START marker plus recurring near-course 10 m / 50 ft reference markers without altering terrain simulation.\n- Corrected runtime keyboard controls and added visible in-app control help.\n- Disabled optional torso/helmet/weapon skin overlays by default while retaining non-physical sprite feet and explicit armor toggles.\n- Strengthened early sagittal walking bootstrap/teacher authority without weakening crab-gait rejection.\n- Isolated v0.7.18 autosaves and training/autonomy semantics.\n\n'''
    if not changelog.startswith("## 0.7.18"):
        changelog = entry + changelog
    write("CHANGELOG.md", changelog)

    doc = '''# Runner v0.7.18 runtime recovery\n\nThis release is a packaged-runtime recovery pass driven by direct v0.7.17 eye testing. Terrain rendering/collision is intentionally unchanged.\n\n## Confirmed reset contradiction\n\nPPO evaluates at updates 1, 5, and 10. v0.7.17 automatically reset a no-champion policy on every third invalid evaluation, so a fresh controller could be discarded at update 10 even though Stand requires 120 fresh updates before its dwell gate can complete. v0.7.18 gives the nursery an extended stage-aware budget and preserves cumulative totals.\n\n## Observability\n\nThe live viewport and results panel show total updates, stage-local updates, evaluations, reset count, stage-work progress, pipeline stage, and updates/second. A policy reset can no longer look like all training progress disappeared.\n\n## Reference markers\n\nThe treadmill/terrain coordinate system is unchanged. The initial view now includes START and recurring 10 m metric or 50 ft imperial reference markers with readable near-distance labels.\n\n## Controls\n\n- Tab: Live / Rig Lab\n- Space: run/pause background training\n- 1 / 2 / 3: Normal / Faster / Max CPU\n- T: Results / Lifetime Totals\n- U: Metric / Imperial\n- A: Optional body armor overlays\n- R: reset live preview/camera\n- Mouse wheel: viewport zoom\n\nOptional body armor defaults off. Foot sprites remain visual-only and independent.\n\n## Walking recovery\n\nEarly Walk training uses stronger opposite-phase sagittal teacher/bootstrap authority so useful fore/aft steps are demonstrated long enough for PPO to learn them. Existing crab-walk rejection and sustained-distance mastery requirements remain intact.\n'''
    write("docs/RUNNER_V0718_RUNTIME_RECOVERY.md", doc)

    # Focused regression suite.
    tests = r'''#include "autonomy.hpp"
#include "ppo.hpp"
#include "ui_layout.hpp"

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
        "training semantics were not isolated");
    require(rl::stage_minimum_fresh_updates(sim::CourseStage::balance) == 120u,
        "Stand dwell changed unexpectedly");
    require(!rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 10u, 3u),
        "the old update-10 reset is still possible");
    require(!rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 120u, 12u),
        "a policy can reset as soon as the Stand dwell is reached");
    require(rl::nursery_policy_reset_allowed(sim::CourseStage::balance, 240u, 12u),
        "the bounded extended nursery reset can never activate");
    require(ui_layout::course_reference_marker_spacing_m(
                ui_layout::DistanceUnits::metric) == 10.0f,
        "metric reference markers are not visible near start");
    require(std::abs(ui_layout::course_reference_marker_spacing_m(
                ui_layout::DistanceUnits::imperial) - 15.24f) < 0.0001f,
        "imperial reference marker spacing is not 50 feet");

    sim::Environment walking{ sim::CreatureBlueprint::biped(), 0x718u };
    walking.set_course(sim::CourseStage::uneven, 0.30f);
    const auto teacher = rl::walking_teacher_action(walking);
    require(teacher[0] * teacher[2] <= 0.0f,
        "walking teacher hips do not oppose each other sagittally");
    const auto effective = rl::effective_policy_action(
        walking, {}, sim::CourseStage::uneven);
    require(std::abs(effective[0] - effective[2]) > 0.08f,
        "early Walk assistance does not create meaningful opposite-phase leg drive");

    std::cout << "Runner v0.7.18 runtime recovery contracts passed\n";
    return 0;
}
'''
    write("tests/v0718_runtime_recovery_tests.cpp", tests)

    # Repository audit follows the new version and requires focused recovery evidence.
    audit = read("tools/repository_audit.cmake")
    audit = audit.replace("docs/RUNNER_V0717_EYE_TEST_CORRECTION.md\n", "docs/RUNNER_V0717_EYE_TEST_CORRECTION.md\n        docs/RUNNER_V0718_RUNTIME_RECOVERY.md\n", 1)
    audit = audit.replace('project(Runner VERSION 0.7.17 LANGUAGES CXX)', 'project(Runner VERSION 0.7.18 LANGUAGES CXX)')
    audit = audit.replace('CMake project version is not 0.7.17', 'CMake project version is not 0.7.18')
    audit = audit.replace("training_semantics_version = 0x0007'1700u", "training_semantics_version = 0x0007'1800u")
    audit = audit.replace('Training semantics are not isolated for v0.7.17', 'Training semantics are not isolated for v0.7.18')
    audit = audit.replace('        "RUNNER_V0717_EYE_TEST_CORRECTION.md"\n', '        "RUNNER_V0717_EYE_TEST_CORRECTION.md"\n        "RUNNER_V0718_RUNTIME_RECOVERY.md"\n')
    audit = audit.replace('foreach(reference IN ITEMS "WALK-QUAD-CROUCH-181" "WALK-RELEASE-210"\n        "# Runner v0.7.18 equipment, carry, and target curriculum")',
        'foreach(reference IN ITEMS "WALK-RUNTIME-RESET-211" "WALK-RELEASE-225"\n        "# Runner v0.7.19 equipment, carry, and target curriculum")')
    audit = audit.replace('        .github/workflows/runner-v0716-release.yml)',
        '        .github/workflows/runner-v0716-release.yml\n        .github/workflows/runner-v0717-release.yml\n        tools/apply_v0718_runtime_recovery.py\n        .github/workflows/apply-v0718-runtime-recovery.yml)')
    write("tools/repository_audit.cmake", audit)


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {"cache", "implement"}:
        print("usage: apply_v0718_runtime_recovery.py cache|implement", file=sys.stderr)
        return 2
    cache() if sys.argv[1] == "cache" else implement()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
