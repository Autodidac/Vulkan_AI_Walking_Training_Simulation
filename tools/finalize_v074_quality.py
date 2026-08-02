from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"missing target in {path}: {old[:120]!r}")
    write(path, text.replace(old, new, 1))


for path in ROOT.rglob("*"):
    if not path.is_file() or ".git" in path.parts or path == Path(__file__):
        continue
    try:
        original = path.read_text(encoding="utf-8")
    except (UnicodeDecodeError, OSError):
        continue
    updated = original.replace("RunnerGui-backed", "built-in")
    updated = updated.replace("RunnerGui", "built-in bitmap UI")
    updated = updated.replace("RUNNERGUI", "BUILT-IN UI")
    updated = updated.replace("fewer runners", "fewer optimization passes")
    if updated != original:
        path.write_text(updated, encoding="utf-8", newline="\n")

replace_once(
    "README.md",
    "Runner is a C++23 Vulkan locomotion laboratory built with SDL3, built-in bitmap UI, vcpkg manifest mode, and a compact PPO controller. Version 0.6.5 completes the guided sand-simulation enemy pass:",
    "Runner is a C++23 Vulkan locomotion laboratory built with SDL3, Vulkan, vcpkg manifest mode, a built-in bitmap UI, and a compact PPO controller. Version 0.7.4 completes the compression-first ducking, rig-integrity, telemetry, and packaging pass while preserving the guided simulation-enemy curriculum:",
)
replace_once(
    "vcpkg.json",
    '"description": "Vulkan 1.3 autonomous articulated locomotion trainer with background curriculum learning and built-in bitmap UI."',
    '"description": "Vulkan 1.3 autonomous articulated locomotion trainer with background curriculum learning and a built-in bitmap UI."',
)

replace_once(
    "src/ppo_trainer.cpp",
    """            constexpr std::size_t runners = 2;""",
    """            constexpr std::size_t optimization_passes = 2;""",
)
replace_once(
    "src/ppo_trainer.cpp",
    """            for (std::size_t runner = 0; runner < runners; ++runner)""",
    """            for (std::size_t optimization_pass = 0;
                optimization_pass < optimization_passes; ++optimization_pass)""",
)

replace_once(
    "src/app.cpp",
    """        void add_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color fill,
            Color outline = {}, float border_width = 0.0f)
        {
            static_cast<void>(radius);
            canvas.quad(rect.position, rect.position + rect.size, fill);
            if (border_width <= 0.0f)
                return;
            const Vec2 minimum = rect.position;
            const Vec2 maximum = rect.position + rect.size;
            canvas.quad(minimum, { maximum.x, minimum.y + border_width }, outline);
            canvas.quad({ minimum.x, maximum.y - border_width }, maximum, outline);
            canvas.quad(minimum, { minimum.x + border_width, maximum.y }, outline);
            canvas.quad({ maximum.x - border_width, minimum.y }, maximum, outline);
        }""",
    """        void fill_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color color)
        {
            if (rect.size.x <= 0.0f || rect.size.y <= 0.0f)
                return;
            radius = std::clamp(radius, 0.0f,
                std::min(rect.size.x, rect.size.y) * 0.5f);
            if (radius <= 0.0f)
            {
                canvas.quad(rect.position, rect.position + rect.size, color);
                return;
            }
            const Vec2 minimum = rect.position;
            const Vec2 maximum = rect.position + rect.size;
            canvas.quad({ minimum.x + radius, minimum.y },
                { maximum.x - radius, maximum.y }, color);
            canvas.quad({ minimum.x, minimum.y + radius },
                { maximum.x, maximum.y - radius }, color);
            canvas.circle({ minimum.x + radius, minimum.y + radius }, radius, color, 12);
            canvas.circle({ maximum.x - radius, minimum.y + radius }, radius, color, 12);
            canvas.circle({ minimum.x + radius, maximum.y - radius }, radius, color, 12);
            canvas.circle({ maximum.x - radius, maximum.y - radius }, radius, color, 12);
        }

        void add_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color fill,
            Color outline = {}, float border_width = 0.0f)
        {
            if (border_width <= 0.0f)
            {
                fill_rounded_rect(canvas, rect, radius, fill);
                return;
            }
            fill_rounded_rect(canvas, rect, radius, outline);
            const float inset = std::clamp(border_width, 0.0f,
                std::min(rect.size.x, rect.size.y) * 0.5f);
            fill_rounded_rect(canvas,
                { rect.position + Vec2{ inset, inset },
                  rect.size - Vec2{ inset * 2.0f, inset * 2.0f } },
                std::max(0.0f, radius - inset), fill);
        }""",
)

replace_once(
    "src/ppo.hpp",
    """        std::uint64_t update{};
        std::uint64_t environment_steps{};
        std::uint64_t total_episodes{};""",
    """        std::uint64_t update{};
        std::uint64_t environment_steps{};
        std::uint64_t total_updates{};
        std::uint64_t total_environment_steps{};
        std::uint64_t total_episodes{};""",
)
replace_once(
    "src/ppo.hpp",
    """        double total_distance{};
        float mean_reward{};""",
    """        double total_distance{};
        double total_training_seconds{};
        float mean_reward{};""",
)
replace_once(
    "src/training_checkpoint.cpp",
    """            return write_value(output, value.update)
                && write_value(output, value.environment_steps)
                && write_value(output, value.total_episodes)""",
    """            return write_value(output, value.update)
                && write_value(output, value.environment_steps)
                && write_value(output, value.total_updates)
                && write_value(output, value.total_environment_steps)
                && write_value(output, value.total_episodes)""",
)
replace_once(
    "src/training_checkpoint.cpp",
    """                && write_value(output, value.total_distance)
                && write_value(output, value.mean_reward)""",
    """                && write_value(output, value.total_distance)
                && write_value(output, value.total_training_seconds)
                && write_value(output, value.mean_reward)""",
)
replace_once(
    "src/training_checkpoint.cpp",
    """            return read_value(input, value.update)
                && read_value(input, value.environment_steps)
                && read_value(input, value.total_episodes)""",
    """            return read_value(input, value.update)
                && read_value(input, value.environment_steps)
                && read_value(input, value.total_updates)
                && read_value(input, value.total_environment_steps)
                && read_value(input, value.total_episodes)""",
)
replace_once(
    "src/training_checkpoint.cpp",
    """                && read_value(input, value.total_distance)
                && read_value(input, value.mean_reward)""",
    """                && read_value(input, value.total_distance)
                && read_value(input, value.total_training_seconds)
                && read_value(input, value.mean_reward)""",
)
replace_once(
    "src/ppo_trainer.cpp",
    """        metrics_ = {};
        metrics_.total_episodes = previous_metrics.total_episodes;""",
    """        metrics_ = {};
        metrics_.total_updates = previous_metrics.total_updates;
        metrics_.total_environment_steps = previous_metrics.total_environment_steps;
        metrics_.total_episodes = previous_metrics.total_episodes;""",
)
replace_once(
    "src/ppo_trainer.cpp",
    """        metrics_.total_distance = previous_metrics.total_distance;
        reward_history_.clear();""",
    """        metrics_.total_distance = previous_metrics.total_distance;
        metrics_.total_training_seconds = previous_metrics.total_training_seconds;
        reward_history_.clear();""",
)
replace_once(
    "src/ppo_trainer.cpp",
    """        ++metrics_.update;
        metrics_.environment_steps += rollout_.size();
        metrics_.total_episodes += staged_totals_.completed_episodes;""",
    """        ++metrics_.update;
        metrics_.environment_steps += rollout_.size();
        ++metrics_.total_updates;
        metrics_.total_environment_steps += rollout_.size();
        metrics_.total_episodes += staged_totals_.completed_episodes;""",
)
replace_once(
    "src/ppo_trainer.cpp",
    """        metrics_.total_distance += staged_totals_.total_distance;
        metrics_.mean_speed = staged_totals_.accumulated_speed""",
    """        metrics_.total_distance += staged_totals_.total_distance;
        if (!environments_.empty())
        {
            metrics_.total_training_seconds += static_cast<double>(rollout_.size())
                / static_cast<double>(environments_.size()) / 60.0;
        }
        metrics_.mean_speed = staged_totals_.accumulated_speed""",
)

replace_once(
    "src/app.cpp",
    """        double rig_start_distance{};
        std::uint64_t session_start_episodes{};""",
    """        double rig_start_distance{};
        double rig_start_training_seconds{};
        std::uint64_t rig_start_accepted_rigs{};
        std::uint64_t rig_start_rejected_rigs{};
        std::uint64_t rig_start_rollbacks{};
        std::uint64_t session_start_environment_steps{};
        std::uint64_t session_start_episodes{};""",
)
replace_once(
    "src/app.cpp",
    """        double session_start_distance{};
        std::uint8_t rig_best_stage{};""",
    """        double session_start_distance{};
        double session_start_training_seconds{};
        std::uint64_t session_start_accepted_rigs{};
        std::uint64_t session_start_rejected_rigs{};
        std::uint64_t session_start_rollbacks{};
        std::uint8_t rig_best_stage{};""",
)
replace_once(
    "src/app.cpp",
    """            const rl::TrainingMetrics& current_metrics = trainer.metrics();
            if (!session_stats_initialized)""",
    """            const rl::TrainingMetrics& current_metrics = trainer.metrics();
            const rl::AutonomyStatus& current_autonomy = trainer.autonomy_status();
            if (!session_stats_initialized)""",
)
replace_once(
    "src/app.cpp",
    """                session_stats_initialized = true;
                session_start_episodes = current_metrics.total_episodes;""",
    """                session_stats_initialized = true;
                session_start_environment_steps = current_metrics.total_environment_steps;
                session_start_episodes = current_metrics.total_episodes;""",
)
replace_once(
    "src/app.cpp",
    """                session_start_distance = current_metrics.total_distance;
            }""",
    """                session_start_distance = current_metrics.total_distance;
                session_start_training_seconds = current_metrics.total_training_seconds;
                session_start_accepted_rigs = current_autonomy.accepted_rig_changes;
                session_start_rejected_rigs = current_autonomy.rejected_rig_changes;
                session_start_rollbacks = current_autonomy.rollback_count;
            }""",
)
replace_once(
    "src/app.cpp",
    """                rig_start_distance = current_metrics.total_distance;
                rig_best_stage = static_cast<std::uint8_t>(trainer.autonomy_status().stage);""",
    """                rig_start_distance = current_metrics.total_distance;
                rig_start_training_seconds = current_metrics.total_training_seconds;
                rig_start_accepted_rigs = current_autonomy.accepted_rig_changes;
                rig_start_rejected_rigs = current_autonomy.rejected_rig_changes;
                rig_start_rollbacks = current_autonomy.rollback_count;
                rig_best_stage = static_cast<std::uint8_t>(current_autonomy.stage);""",
)
replace_once(
    "src/app.cpp",
    """                    static_cast<std::uint8_t>(trainer.autonomy_status().stage));""",
    """                    static_cast<std::uint8_t>(current_autonomy.stage));""",
)

replace_once(
    "src/app.cpp",
    """        enum class RigPanelPage : std::uint8_t { body, motor };
        enum class JointTestGroup : std::uint8_t { selected, pair_a, pair_b, all };""",
    """        enum class RigPanelPage : std::uint8_t { body, motor };
        enum class LivePanelPage : std::uint8_t { results, totals };
        enum class JointTestGroup : std::uint8_t { selected, pair_a, pair_b, all };""",
)
replace_once(
    "src/app.cpp",
    """        RigPanelPage rig_panel_page{ RigPanelPage::body };
        int selected_node{ -1 };""",
    """        RigPanelPage rig_panel_page{ RigPanelPage::body };
        LivePanelPage live_panel_page{ LivePanelPage::results };
        int selected_node{ -1 };""",
)
app_path = ROOT / "src/app.cpp"
app_text = app_path.read_text(encoding="utf-8")
start_marker = "            add_rounded_rect(canvas, { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 247.0f } },"
end_marker = "        void draw_live_world(Rect viewport, float dt)"
start = app_text.index(start_marker)
end = app_text.index(end_marker, start)
new_tail = r'''            if (button({ cursor, { half, 38.0f } }, "TRAINING RESULTS", input,
                live_panel_page == LivePanelPage::results))
                live_panel_page = LivePanelPage::results;
            if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 38.0f } },
                "LIFETIME TOTALS", input, live_panel_page == LivePanelPage::totals))
                live_panel_page = LivePanelPage::totals;
            cursor.y += 47.0f;

            if (live_panel_page == LivePanelPage::results)
            {
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 247.0f } },
                    8.0f, panel_alt, border, 1.0f);
                add_text(canvas, cursor, "TRAINING RESULTS", 1.18f, accent);
                cursor.y += 31.0f;
                add_text_fit(canvas, cursor, std::format("UPDATE {}   ENV STEPS {}",
                    metrics.update, metrics.environment_steps), 1.10f, white, usable_width);
                cursor.y += 29.0f;
                add_text_fit(canvas, cursor, std::format("EVAL {:+.2f}   DIST {}",
                    metrics.evaluation_score, format_distance(metrics.evaluation_distance)),
                    1.10f, metrics.evaluation_valid ? green : danger, usable_width);
                cursor.y += 29.0f;
                add_text_fit(canvas, cursor, std::format("BEST {:+.2f} @ UPDATE {}",
                    metrics.best_evaluation_score, metrics.best_update),
                    1.10f, accent, usable_width);
                cursor.y += 29.0f;
                add_text_fit(canvas, cursor, std::format("SURVIVAL {:.1f} S   STRIDES {:.1f}",
                    metrics.evaluation_survival, metrics.evaluation_stride_events),
                    1.08f, white, usable_width);
                cursor.y += 29.0f;
                add_text_fit(canvas, cursor,
                    std::format("STANCE {:.1f}/{:.1f} S   DUCK REC {:.1f}",
                        metrics.evaluation_stable_stance,
                        metrics.evaluation_longest_stance,
                        metrics.evaluation_duck_recoveries),
                    1.05f, metrics.evaluation_valid ? green : danger, usable_width);
                cursor.y += 29.0f;
                add_text_fit(canvas, cursor, std::format("QUALITY {:016X}   {}",
                    metrics.evaluation_quality_key,
                    rl::primary_motion_rejection_name(metrics.evaluation_rejection_mask)),
                    0.98f, metrics.evaluation_valid ? accent : danger,
                    usable_width, 0.82f);
            }
            else
            {
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 318.0f } },
                    8.0f, panel_alt, border, 1.0f);
                add_text(canvas, cursor, "RIG / SESSION / ALL-TIME TOTALS", 1.08f, accent);
                cursor.y += 27.0f;
                add_text_fit(canvas, cursor, std::format("{} WORKERS {} ENV {:.2f} UPDATES/S {}",
                    autonomy.rollout_threads, autonomy.environment_count,
                    autonomy.updates_per_second, autonomy.speed_mode),
                    0.76f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("RIG {} UPD {} ENV {} BEST STAGE {}",
                    format_duration(rig_lifetime_seconds),
                    ui_layout::lifetime_delta(metrics.update, rig_start_update),
                    ui_layout::lifetime_delta(metrics.environment_steps,
                        rig_start_environment_steps),
                    static_cast<unsigned>(rig_best_stage) + 1u),
                    0.78f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("RIG EPS {} VALID {} BAD {} DIST {}",
                    ui_layout::lifetime_delta(metrics.total_episodes, rig_start_episodes),
                    ui_layout::lifetime_delta(metrics.total_valid_episodes,
                        rig_start_valid_episodes),
                    ui_layout::lifetime_delta(metrics.total_invalid_episodes,
                        rig_start_invalid_episodes),
                    format_distance(static_cast<float>(std::max(0.0,
                        metrics.total_distance - rig_start_distance)))),
                    0.76f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("RIG STEP {} FALL {} COLL {} OBS {}",
                    ui_layout::lifetime_delta(metrics.total_alternating_steps, rig_start_steps),
                    ui_layout::lifetime_delta(metrics.total_falls, rig_start_falls),
                    ui_layout::lifetime_delta(metrics.total_collisions, rig_start_collisions),
                    ui_layout::lifetime_delta(metrics.total_obstacles_passed,
                        rig_start_obstacles)), 0.76f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("RIG JUMP {} LAND {} FLIP {} TRAIN {}",
                    ui_layout::lifetime_delta(metrics.total_powered_jumps, rig_start_jumps),
                    ui_layout::lifetime_delta(metrics.total_landed_jumps, rig_start_landings),
                    ui_layout::lifetime_delta(metrics.total_landed_flips, rig_start_flips),
                    format_duration(static_cast<float>(std::max(0.0,
                        metrics.total_training_seconds - rig_start_training_seconds)))),
                    0.74f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("RIG ACCEPT {} REJECT {} ROLLBACK {}",
                    ui_layout::lifetime_delta(autonomy.accepted_rig_changes,
                        rig_start_accepted_rigs),
                    ui_layout::lifetime_delta(autonomy.rejected_rig_changes,
                        rig_start_rejected_rigs),
                    ui_layout::lifetime_delta(autonomy.rollback_count, rig_start_rollbacks)),
                    0.74f, white, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("SESSION {} TRAIN {} ENV {} EPS {} BAD {}",
                    format_duration(session_runtime_seconds),
                    format_duration(static_cast<float>(std::max(0.0,
                        metrics.total_training_seconds - session_start_training_seconds))),
                    ui_layout::lifetime_delta(metrics.total_environment_steps,
                        session_start_environment_steps),
                    ui_layout::lifetime_delta(metrics.total_episodes, session_start_episodes),
                    ui_layout::lifetime_delta(metrics.total_invalid_episodes,
                        session_start_invalid_episodes)), 0.70f, muted, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("SESSION DIST {} COLL {} JUMP {} FLIP {} OBS {}",
                    format_distance(static_cast<float>(std::max(0.0,
                        metrics.total_distance - session_start_distance))),
                    ui_layout::lifetime_delta(metrics.total_collisions,
                        session_start_collisions),
                    ui_layout::lifetime_delta(metrics.total_powered_jumps, session_start_jumps),
                    ui_layout::lifetime_delta(metrics.total_landed_flips, session_start_flips),
                    ui_layout::lifetime_delta(metrics.total_obstacles_passed,
                        session_start_obstacles)), 0.70f, muted, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("SESSION ACCEPT {} REJECT {} RESET {} RB {}",
                    ui_layout::lifetime_delta(autonomy.accepted_rig_changes,
                        session_start_accepted_rigs),
                    ui_layout::lifetime_delta(autonomy.rejected_rig_changes,
                        session_start_rejected_rigs),
                    ui_layout::lifetime_delta(metrics.total_resets, session_start_resets),
                    ui_layout::lifetime_delta(autonomy.rollback_count,
                        session_start_rollbacks)), 0.70f, muted, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("ALL TRAIN {} UPD {} ENV {} EPS {} RESET {}",
                    format_duration(static_cast<float>(metrics.total_training_seconds)),
                    metrics.total_updates, metrics.total_environment_steps,
                    metrics.total_episodes, metrics.total_resets),
                    0.70f, muted, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("ALL DIST {} COLL {} JUMP {} FLIP {} OBS {}",
                    format_distance(static_cast<float>(metrics.total_distance)),
                    metrics.total_collisions, metrics.total_powered_jumps,
                    metrics.total_landed_flips, metrics.total_obstacles_passed),
                    0.70f, muted, usable_width);
                cursor.y += 22.0f;
                add_text_fit(canvas, cursor, std::format("ALL ACCEPT {} REJECT {} ROLLBACK {}",
                    autonomy.accepted_rig_changes, autonomy.rejected_rig_changes,
                    autonomy.rollback_count), 0.70f, muted, usable_width);
            }
        }

'''
app_path.write_text(app_text[:start] + new_tail + app_text[end:],
                    encoding="utf-8", newline="\n")

replace_once(
    "tests/core_tests.cpp",
    """        require(std::isfinite(metrics.value_loss), "PPO value loss is not finite");
    }""",
    """        require(std::isfinite(metrics.value_loss), "PPO value loss is not finite");
        require(metrics.total_updates == metrics.update,
            "fresh cumulative update count does not track policy updates");
        require(metrics.total_environment_steps == metrics.environment_steps,
            "fresh cumulative environment count does not track policy steps");
        require(metrics.total_training_seconds > 0.0,
            "cumulative training time did not advance");
    }""",
)
replace_once(
    "tests/core_tests.cpp",
    """    require(resumed.metrics().update == trainer.metrics().update, "checkpoint update count was not restored");""",
    """    require(resumed.metrics().update == trainer.metrics().update, "checkpoint update count was not restored");
    require(resumed.metrics().total_updates == trainer.metrics().total_updates
            && resumed.metrics().total_environment_steps
                == trainer.metrics().total_environment_steps,
        "checkpoint cumulative update/environment totals were not restored");
    require(resumed.metrics().total_training_seconds == trainer.metrics().total_training_seconds,
        "checkpoint cumulative training time was not restored");""",
)
replace_once(
    "tests/core_tests.cpp",
    "Runner v0.6.2 obstacle observation, recovery, concurrency, gait, and rig-edit tests passed",
    "Runner v0.7.4 obstacle, duck-press, integrity, telemetry, concurrency, gait, and rig-edit tests passed",
)

notes = read("RELEASE_NOTES_v0.7.4.md")
extra = """
- Restored genuinely rounded local UI panels after removing the external GUI dependency.
- Corrected PPO optimization-pass terminology that was accidentally changed during rebranding.
- Split training results and complete lifetime totals into readable panel pages.
- Added persisted cumulative training time plus complete per-rig, session, and all-time environment, episode, distance, step, fall, collision, jump, flip, obstacle, rig-change, reset, and rollback telemetry.
"""
if "Restored genuinely rounded local UI panels" not in notes:
    write("RELEASE_NOTES_v0.7.4.md", notes.rstrip() + "\n" + extra)

Path(__file__).unlink()
print("completed final Runner v0.7.4 quality, UI, and telemetry pass")
