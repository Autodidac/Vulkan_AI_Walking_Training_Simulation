#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def replace_between(text: str, start: str, end: str, replacement: str, label: str) -> str:
    first = text.find(start)
    if first < 0:
        raise RuntimeError(f"{label}: start marker not found")
    last = text.find(end, first)
    if last < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[:first] + replacement + text[last:]


def patch_app() -> None:
    path = "src/app.cpp"
    text = read(path)
    text = replace_once(text,
        '#include "simulation.hpp"\n#include "ui_layout.hpp"',
        '#include "simulation.hpp"\n#include "training_explainer.hpp"\n#include "ui_layout.hpp"',
        "training explainer include")

    color_helper = r'''        [[nodiscard]] constexpr Color telemetry_color(telemetry::Tone tone) noexcept
        {
            switch (tone)
            {
            case telemetry::Tone::information: return accent;
            case telemetry::Tone::caution: return yellow;
            case telemetry::Tone::success: return green;
            case telemetry::Tone::danger: return danger;
            }
            return accent;
        }

'''
    text = replace_once(text,
        '        constexpr Color leg{ 0.89f, 0.42f, 0.15f, 1.0f };\n\n',
        '        constexpr Color leg{ 0.89f, 0.42f, 0.15f, 1.0f };\n\n' + color_helper,
        "telemetry tone colors")

    text = replace_once(text,
        '        enum class LivePanelPage : std::uint8_t { results, totals };',
        '        enum class LivePanelPage : std::uint8_t { summary, totals, advanced };',
        "live panel pages")
    text = replace_once(text,
        '        LivePanelPage live_panel_page{ LivePanelPage::results };',
        '        LivePanelPage live_panel_page{ LivePanelPage::summary };',
        "default summary page")
    text = replace_once(text,
        '"TAB VIEW  SPACE TRAIN  1/2/3 SPEED  T TOTALS  U UNITS  A ART  R RESET"',
        '"TAB VIEW  SPACE TRAIN  1/2/3 SPEED  T DATA PAGE  U UNITS  A ART  R RESET"',
        "top bar data-page help")
    text = replace_once(text,
        '                "TOTAL {}  STAGE {}  {:.1f}M  STEPS {}",\n'
        '                trainer.metrics().total_updates, trainer.metrics().update,\n'
        '                environment.distance_travelled(), environment.gait_cycles());',
        '                "TOTAL UPDATES {}  LESSON UPDATE {}  DISTANCE {:.1f} M  STEPS {}",\n'
        '                trainer.metrics().total_updates, trainer.metrics().update,\n'
        '                environment.distance_travelled(), environment.gait_cycles());',
        "PIP plain labels")

    new_panel = r'''        void draw_live_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            canvas.push_clip(rect.position + Vec2{ 1.0f, 1.0f },
                rect.position + rect.size - Vec2{ 1.0f, 1.0f });
            const float usable_width = rect.size.x - 36.0f;
            Vec2 cursor = rect.position + Vec2{ 18.0f, 16.0f };
            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const rl::TrainingMetrics& metrics = trainer.metrics();
            const telemetry::LessonProgress progress = telemetry::lesson_progress(autonomy);
            const telemetry::StatusSummary human_status = telemetry::status_summary(
                autonomy, metrics, trainer.background_enabled(), trainer.has_best_policy(),
                trainer.controller_state_name());
            const Color human_color = telemetry_color(human_status.tone);
            const int progress_percent = static_cast<int>(std::lround(progress.overall * 100.0f));

            add_text_fit(canvas, cursor, "AUTONOMOUS RIG TRAINER", 1.54f,
                white, usable_width, 1.08f);
            cursor.y += 42.0f;
            if (button({ cursor, { usable_width, 48.0f } },
                trainer.background_enabled() ? "AUTOPILOT ON - CLICK TO PAUSE" : "AUTOPILOT PAUSED - CLICK TO RUN",
                input, trainer.background_enabled()))
            {
                trainer.set_background_enabled(!trainer.background_enabled());
                set_status(trainer.background_enabled() ? "BACKGROUND TRAINING RESUMED" : "BACKGROUND TRAINING PAUSED");
            }
            cursor.y += 64.0f;

            add_text(canvas, cursor, "CURRENT LESSON", 1.05f, muted);
            cursor.y += 23.0f;
            add_text_fit(canvas, cursor, sim::course_stage_name(autonomy.stage), 2.05f,
                accent, usable_width, 1.30f);
            cursor.y += 38.0f;
            add_text_fit(canvas, cursor,
                std::format("DIFFICULTY {:.0f}%   MASTERY TESTS {} / {}",
                    autonomy.difficulty * 100.0f, autonomy.mastery_streak,
                    rl::required_mastery_confirmations(autonomy.stage)),
                0.98f, white, usable_width, 0.80f);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor,
                std::format("LESSON PROGRESS {}%", progress_percent),
                1.02f, human_color, usable_width);
            cursor.y += 23.0f;
            const Rect progress_track{ cursor, { usable_width, 12.0f } };
            fill_rounded_rect(canvas, progress_track, 6.0f, rgb(0x101820));
            fill_rounded_rect(canvas,
                { progress_track.position,
                    { progress_track.size.x * progress.overall, progress_track.size.y } },
                6.0f, human_color);
            add_rounded_rect(canvas, progress_track, 6.0f, Color{}, border, 1.0f);
            cursor.y += 21.0f;
            add_text_fit(canvas, cursor,
                std::format("UPDATES {} / {}   ATTEMPTS {} / {}",
                    autonomy.stage_fresh_updates, autonomy.stage_required_updates,
                    autonomy.stage_fresh_episodes, autonomy.stage_required_episodes),
                0.78f, muted, usable_width, 0.68f);
            cursor.y += 20.0f;
            add_text_fit(canvas, cursor,
                std::format("REPEAT TESTS {} / {}   STATUS: {}",
                    autonomy.stage_fresh_evaluations, autonomy.stage_required_evaluations,
                    human_status.headline),
                0.78f, human_color, usable_width, 0.64f);
            cursor.y += 28.0f;

            const float third = (usable_width - 12.0f) / 3.0f;
            if (button({ cursor, { third, 40.0f } }, "NORMAL", input, trainer.updates_per_cycle() == 1))
                trainer.set_updates_per_cycle(1);
            if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 40.0f } },
                "FASTER", input, trainer.updates_per_cycle() == 2))
                trainer.set_updates_per_cycle(2);
            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 40.0f } },
                "MAX CPU", input, trainer.updates_per_cycle() == 4))
                trainer.set_updates_per_cycle(4);
            cursor.y += 48.0f;
            if (button({ cursor, { third, 38.0f } }, "ZOOM OUT", input))
            {
                live_zoom_factor = view_camera::apply_wheel_zoom(live_zoom_factor, -1.0f);
                live_zoom_auto = false;
            }
            if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 38.0f } },
                "AUTO VIEW", input, live_zoom_auto))
            {
                live_zoom_factor = 1.0f;
                live_zoom_auto = true;
            }
            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 38.0f } },
                "ZOOM IN", input))
            {
                live_zoom_factor = view_camera::apply_wheel_zoom(live_zoom_factor, 1.0f);
                live_zoom_auto = false;
            }
            cursor.y += 46.0f;
            const float half = (usable_width - 6.0f) * 0.5f;
            if (button({ cursor, { half, 36.0f } }, "METRIC / 10 M", input,
                distance_units == ui_layout::DistanceUnits::metric))
                distance_units = ui_layout::DistanceUnits::metric;
            if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 36.0f } },
                "IMPERIAL / 50 FT", input,
                distance_units == ui_layout::DistanceUnits::imperial))
                distance_units = ui_layout::DistanceUnits::imperial;
            cursor.y += 53.0f;

            const float page_third = (usable_width - 12.0f) / 3.0f;
            if (button({ cursor, { page_third, 38.0f } }, "SUMMARY", input,
                live_panel_page == LivePanelPage::summary))
                live_panel_page = LivePanelPage::summary;
            if (button({ cursor + Vec2{ page_third + 6.0f, 0.0f },
                    { page_third, 38.0f } }, "TOTALS", input,
                live_panel_page == LivePanelPage::totals))
                live_panel_page = LivePanelPage::totals;
            if (button({ cursor + Vec2{ (page_third + 6.0f) * 2.0f, 0.0f },
                    { page_third, 38.0f } }, "ADVANCED", input,
                live_panel_page == LivePanelPage::advanced))
                live_panel_page = LivePanelPage::advanced;
            cursor.y += 47.0f;

            if (live_panel_page == LivePanelPage::summary)
            {
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 365.0f } },
                    8.0f, panel_alt, border, 1.0f);
                add_text_fit(canvas, cursor, "WHAT THE TRAINER IS TELLING YOU",
                    1.12f, accent, usable_width, 0.92f);
                cursor.y += 29.0f;
                add_text_fit(canvas, cursor, human_status.headline,
                    1.32f, human_color, usable_width, 1.0f);
                cursor.y += 27.0f;
                cursor.y += add_wrapped_text(canvas, cursor, human_status.explanation,
                    0.78f, white, usable_width, 3.0f);
                cursor.y += 8.0f;
                add_text_fit(canvas, cursor,
                    std::format("TOTAL UPDATES {}   LESSON {}%",
                        metrics.total_updates, progress_percent),
                    1.10f, white, usable_width, 0.86f);
                cursor.y += 25.0f;
                cursor.y += add_wrapped_text(canvas, cursor,
                    telemetry::total_updates_help(), 0.68f, muted,
                    usable_width, 2.0f);
                cursor.y += 8.0f;
                const Color test_color = telemetry_color(
                    telemetry::latest_test_tone(metrics));
                add_text_fit(canvas, cursor, telemetry::latest_test_title(metrics),
                    1.02f, test_color, usable_width, 0.82f);
                cursor.y += 23.0f;
                cursor.y += add_wrapped_text(canvas, cursor,
                    telemetry::latest_test_explanation(metrics, autonomy.stage),
                    0.76f, metrics.evaluation_valid ? green : yellow,
                    usable_width, 2.0f);
                cursor.y += 7.0f;

                std::string evidence{};
                switch (autonomy.stage)
                {
                case sim::CourseStage::balance:
                {
                    const std::uint32_t valid_seeds = 6u
                        - std::min<std::uint32_t>(metrics.evaluation_invalid_runs, 6u);
                    evidence = std::format(
                        "CURRENT EVIDENCE: UPRIGHT {:.1f} / 6.0 S   VALID TEST SEEDS {} / 6",
                        metrics.evaluation_longest_stance, valid_seeds);
                    break;
                }
                case sim::CourseStage::duck_press:
                    evidence = std::format(
                        "CURRENT EVIDENCE: CROUCH {:.1f} S   RECOVERIES {:.0f}   SURVIVAL {:.1f} S",
                        metrics.evaluation_duck_seconds,
                        metrics.evaluation_duck_recoveries,
                        metrics.evaluation_survival);
                    break;
                case sim::CourseStage::uneven:
                    evidence = std::format(
                        "CURRENT EVIDENCE: DISTANCE {}   STRIDE EVENTS {:.0f}   SURVIVAL {:.1f} S",
                        format_distance(std::max(0.0f, metrics.evaluation_distance)),
                        metrics.evaluation_stride_events,
                        metrics.evaluation_survival);
                    break;
                case sim::CourseStage::crouch_walk:
                    evidence = std::format(
                        "CURRENT EVIDENCE: LOW {:.1f} S   STRIDES {:.0f}   OBSTACLES {:.0f}",
                        metrics.evaluation_duck_seconds,
                        metrics.evaluation_stride_events,
                        metrics.evaluation_obstacles_passed);
                    break;
                case sim::CourseStage::ramps:
                    evidence = std::format(
                        "CURRENT EVIDENCE: POWERED JUMPS {:.0f}   SAFE LANDINGS {:.0f}   DISTANCE {}",
                        metrics.evaluation_powered_jumps,
                        metrics.evaluation_jump_landings,
                        format_distance(std::max(0.0f, metrics.evaluation_distance)));
                    break;
                case sim::CourseStage::hurdles:
                    evidence = std::format(
                        "CURRENT EVIDENCE: OBSTACLES {:.0f}   LANDINGS {:.0f}   DISTANCE {}",
                        metrics.evaluation_obstacles_passed,
                        metrics.evaluation_jump_landings,
                        format_distance(std::max(0.0f, metrics.evaluation_distance)));
                    break;
                case sim::CourseStage::duck_bars:
                    evidence = std::format(
                        "CURRENT EVIDENCE: FLIP LANDINGS {:.0f}   MAX TURNS {:.2f}   JUMPS {:.0f}",
                        metrics.evaluation_spin_landings,
                        metrics.evaluation_spin_turns,
                        metrics.evaluation_powered_jumps);
                    break;
                case sim::CourseStage::moving_hazards:
                    evidence = std::format(
                        "CURRENT EVIDENCE: DISTANCE {}   STRIDES {:.0f}   HAZARDS PASSED {:.0f}",
                        format_distance(std::max(0.0f, metrics.evaluation_distance)),
                        metrics.evaluation_stride_events,
                        metrics.evaluation_obstacles_passed);
                    break;
                }
                add_text_fit(canvas, cursor, evidence, 0.76f,
                    metrics.evaluation_valid ? green : muted, usable_width, 0.60f);
                cursor.y += 23.0f;
                add_text_fit(canvas, cursor,
                    trainer.has_best_policy()
                        ? std::format("RETAINED BEST CONTROLLER: SAVED AT UPDATE {}",
                            metrics.best_update)
                        : std::string("RETAINED BEST CONTROLLER: NONE YET - STILL SEARCHING"),
                    0.76f, trainer.has_best_policy() ? green : yellow,
                    usable_width, 0.62f);
                cursor.y += 24.0f;
                add_text(canvas, cursor, "NEXT GOAL", 0.86f, accent);
                cursor.y += 20.0f;
                cursor.y += add_wrapped_text(canvas, cursor,
                    telemetry::stage_goal(autonomy.stage), 0.73f, white,
                    usable_width, 2.0f);
                cursor.y += 5.0f;
                add_wrapped_text(canvas, cursor,
                    telemetry::sample_budget_message(progress), 0.68f,
                    progress.sample_budget_complete ? green : muted,
                    usable_width, 2.0f);
            }
            else if (live_panel_page == LivePanelPage::totals)
            {
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 365.0f } },
                    8.0f, panel_alt, border, 1.0f);
                add_text(canvas, cursor, "THIS RIG", 1.05f, accent);
                cursor.y += 25.0f;
                add_text_fit(canvas, cursor,
                    std::format("RUNNING TIME {}   LEARNING UPDATES {}",
                        format_duration(rig_lifetime_seconds),
                        ui_layout::lifetime_delta(metrics.update, rig_start_update)),
                    0.76f, white, usable_width, 0.64f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("ATTEMPTS {}   VALID {}   FAILED CHECKS {}",
                        ui_layout::lifetime_delta(metrics.total_episodes, rig_start_episodes),
                        ui_layout::lifetime_delta(metrics.total_valid_episodes,
                            rig_start_valid_episodes),
                        ui_layout::lifetime_delta(metrics.total_invalid_episodes,
                            rig_start_invalid_episodes)),
                    0.74f, white, usable_width, 0.60f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("DISTANCE {}   STEPS {}   FALLS {}",
                        format_distance(static_cast<float>(std::max(0.0,
                            metrics.total_distance - rig_start_distance))),
                        ui_layout::lifetime_delta(metrics.total_alternating_steps,
                            rig_start_steps),
                        ui_layout::lifetime_delta(metrics.total_falls, rig_start_falls)),
                    0.74f, white, usable_width, 0.60f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("COLLISIONS {}   OBSTACLES PASSED {}   BEST LESSON {}",
                        ui_layout::lifetime_delta(metrics.total_collisions,
                            rig_start_collisions),
                        ui_layout::lifetime_delta(metrics.total_obstacles_passed,
                            rig_start_obstacles),
                        static_cast<unsigned>(rig_best_stage) + 1u),
                    0.72f, white, usable_width, 0.58f);
                cursor.y += 28.0f;

                add_text(canvas, cursor, "THIS SESSION", 1.05f, accent);
                cursor.y += 25.0f;
                add_text_fit(canvas, cursor,
                    std::format("RUN TIME {}   TRAINING TIME {}",
                        format_duration(session_runtime_seconds),
                        format_duration(static_cast<float>(std::max(0.0,
                            metrics.total_training_seconds - session_start_training_seconds)))),
                    0.74f, white, usable_width, 0.60f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("ATTEMPTS {}   RESETS {}   ROLLBACKS {}",
                        ui_layout::lifetime_delta(metrics.total_episodes,
                            session_start_episodes),
                        ui_layout::lifetime_delta(metrics.total_resets,
                            session_start_resets),
                        ui_layout::lifetime_delta(autonomy.rollback_count,
                            session_start_rollbacks)),
                    0.72f, white, usable_width, 0.58f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("DISTANCE {}   COLLISIONS {}   HAZARDS PASSED {}",
                        format_distance(static_cast<float>(std::max(0.0,
                            metrics.total_distance - session_start_distance))),
                        ui_layout::lifetime_delta(metrics.total_collisions,
                            session_start_collisions),
                        ui_layout::lifetime_delta(metrics.total_obstacles_passed,
                            session_start_obstacles)),
                    0.72f, white, usable_width, 0.58f);
                cursor.y += 28.0f;

                add_text(canvas, cursor, "ALL TIME", 1.05f, accent);
                cursor.y += 25.0f;
                add_text_fit(canvas, cursor,
                    std::format("TOTAL UPDATES {}   ATTEMPTS {}   VALID {}",
                        metrics.total_updates, metrics.total_episodes,
                        metrics.total_valid_episodes),
                    0.76f, white, usable_width, 0.62f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("DISTANCE {}   RESETS {}   ROLLBACKS {}",
                        format_distance(static_cast<float>(metrics.total_distance)),
                        metrics.total_resets, autonomy.rollback_count),
                    0.72f, white, usable_width, 0.58f);
                cursor.y += 26.0f;
                cursor.y += add_wrapped_text(canvas, cursor,
                    telemetry::attempts_help(), 0.66f, muted,
                    usable_width, 2.0f);
                cursor.y += 4.0f;
                add_wrapped_text(canvas, cursor,
                    telemetry::reset_help(), 0.66f, muted,
                    usable_width, 2.0f);
            }
            else
            {
                add_rounded_rect(canvas,
                    { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 365.0f } },
                    8.0f, panel_alt, border, 1.0f);
                add_text(canvas, cursor, "ADVANCED DIAGNOSTICS", 1.08f, accent);
                cursor.y += 27.0f;
                auto raw_number = [](float value)
                {
                    return std::isfinite(value)
                        ? std::format("{:+.4f}", value)
                        : std::string("NOT AVAILABLE");
                };
                const std::string quality = metrics.evaluation_quality_key == 0u
                    ? std::string("NOT AVAILABLE")
                    : std::format("{:016X}", metrics.evaluation_quality_key);
                add_text_fit(canvas, cursor,
                    std::format("RAW TEST SCORE {}   BEST RAW SCORE {}",
                        raw_number(metrics.evaluation_score),
                        raw_number(metrics.best_evaluation_score)),
                    0.72f, muted, usable_width, 0.58f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("BEST SAVED AT LOCAL UPDATE {}   QUALITY KEY {}",
                        metrics.best_update, quality),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("REJECTION MASK 0x{:08X}   {}",
                        metrics.evaluation_rejection_mask,
                        rl::primary_motion_rejection_name(
                            metrics.evaluation_rejection_mask)),
                    0.70f, metrics.evaluation_valid ? green : yellow,
                    usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("POLICY LOSS {}   VALUE LOSS {}",
                        raw_number(metrics.policy_loss), raw_number(metrics.value_loss)),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("ENTROPY {}   LEARNING RATE {:.7f}",
                        raw_number(metrics.entropy), metrics.learning_rate),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("MEAN REWARD {}   MEAN SPEED {}",
                        raw_number(metrics.mean_reward),
                        format_speed(metrics.mean_speed)),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("LOCAL UPDATE {}   ENVIRONMENT STEPS {}",
                        metrics.update, metrics.environment_steps),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("OPTIMIZER STEP {}   EXPLORATION {:.4f}",
                        trainer.optimizer_step(), trainer.exploration()),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("WORKERS {}   SIMULATIONS {}   {:.2f} UPDATES/SECOND",
                        autonomy.rollout_threads, autonomy.environment_count,
                        autonomy.updates_per_second),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("PIPELINE {}   CONTROLLER {}",
                        autonomy.pipeline_stage, trainer.controller_state_name()),
                    0.70f, muted, usable_width, 0.56f);
                cursor.y += 21.0f;
                add_text_fit(canvas, cursor,
                    std::format("RIG GENERATION {}   ACCEPTED {}   REJECTED {}   ROLLBACKS {}",
                        autonomy.rig_generation, autonomy.accepted_rig_changes,
                        autonomy.rejected_rig_changes, autonomy.rollback_count),
                    0.68f, muted, usable_width, 0.54f);
                cursor.y += 25.0f;
                add_wrapped_text(canvas, cursor,
                    "These raw values are for debugging. A negative score or loss does not mean the trainer can never learn.",
                    0.68f, accent, usable_width, 2.0f);
            }
            canvas.pop_clip();
            add_rounded_rect(canvas, rect, 11.0f, Color{}, border, 1.0f);
        }

'''
    text = replace_between(text,
        '        void draw_live_panel(Rect rect, const InputState& input)\n',
        '        void draw_live_world(Rect viewport, float dt, const InputState& input)\n',
        new_panel,
        "live panel replacement")

    text = replace_once(text,
        '                std::format("STEPS {}  CROSS {}  HEEL {}  TOE {}  SLIP {:.2f}",\n'
        '                    environment.alternating_steps(), environment.limb_crossings(),\n'
        '                    environment.heel_strikes(), environment.toe_offs(),\n'
        '                    environment.stance_slip_speed()),',
        '                std::format("REAL STEPS {}   LEG CROSSINGS {}   HEEL STRIKES {}   TOE LIFTS {}",\n'
        '                    environment.alternating_steps(), environment.limb_crossings(),\n'
        '                    environment.heel_strikes(), environment.toe_offs()),',
        "world gait labels")
    text = replace_once(text,
        '                std::format("LEFT {}   RIGHT {}   PASSED {}",\n'
        '                    sim::foot_contact_phase_name(environment.left_foot_phase()),\n'
        '                    sim::foot_contact_phase_name(environment.right_foot_phase()),\n'
        '                    environment.obstacles_passed()),',
        '                std::format("LEFT FOOT {}   RIGHT FOOT {}   OBSTACLES PASSED {}",\n'
        '                    sim::foot_contact_phase_name(environment.left_foot_phase()),\n'
        '                    sim::foot_contact_phase_name(environment.right_foot_phase()),\n'
        '                    environment.obstacles_passed()),',
        "world foot labels")
    text = replace_once(text,
        '                std::format("{}   TOTAL UPDATES {}   STAGE {}",\n'
        '                    sim::invalid_motion_name(environment.invalid_reason()),\n'
        '                    trainer.metrics().total_updates, trainer.metrics().update),',
        '                std::format("MOTION {}   TOTAL UPDATES {}   LESSON UPDATE {}",\n'
        '                    sim::invalid_motion_name(environment.invalid_reason()),\n'
        '                    trainer.metrics().total_updates, trainer.metrics().update),',
        "world update labels")

    text = replace_once(text,
        '            if (input.totals_pressed)\n'
        '                live_panel_page = live_panel_page == LivePanelPage::results\n'
        '                    ? LivePanelPage::totals : LivePanelPage::results;',
        '            if (input.totals_pressed)\n'
        '            {\n'
        '                switch (live_panel_page)\n'
        '                {\n'
        '                case LivePanelPage::summary:\n'
        '                    live_panel_page = LivePanelPage::totals;\n'
        '                    break;\n'
        '                case LivePanelPage::totals:\n'
        '                    live_panel_page = LivePanelPage::advanced;\n'
        '                    break;\n'
        '                case LivePanelPage::advanced:\n'
        '                    live_panel_page = LivePanelPage::summary;\n'
        '                    break;\n'
        '                }\n'
        '            }',
        "three-page shortcut")

    write(path, text)


def patch_cmake() -> None:
    path = "CMakeLists.txt"
    text = read(path)
    text = text.replace("project(Runner VERSION 0.7.20 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.21 LANGUAGES CXX)", 1)
    text = text.replace("Runner v0.7.20 icon generation failed",
        "Runner v0.7.21 icon generation failed", 1)
    text = replace_once(text,
        '        src/locomotion_strategy.hpp src/preview_sync.hpp)',
        '        src/locomotion_strategy.hpp src/preview_sync.hpp\n'
        '        src/training_explainer.hpp)',
        "app explainer header")
    text = replace_once(text,
        '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0720_UI_PREVIEW_ICON.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0720_UI_PREVIEW_ICON.md"\n',
        '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0720_UI_PREVIEW_ICON.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0720_UI_PREVIEW_ICON.md"\n'
        '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0721_READABLE_TELEMETRY.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0721_READABLE_TELEMETRY.md"\n',
        "post-build readable doc")
    text = replace_once(text,
        '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0720_UI_PREVIEW_ICON.md"\n'
        '        DESTINATION docs)',
        '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0720_UI_PREVIEW_ICON.md"\n'
        '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0721_READABLE_TELEMETRY.md"\n'
        '        DESTINATION docs)',
        "install readable doc")
    test_block = r'''    add_executable(RunnerV0721ReadableTelemetryTests
        tests/v0721_readable_telemetry_tests.cpp)
    target_include_directories(RunnerV0721ReadableTelemetryTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0721ReadableTelemetryTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0721ReadableTelemetryTests)
    add_test(NAME Runner.V0721ReadableTelemetry
        COMMAND RunnerV0721ReadableTelemetryTests)
    set_tests_properties(Runner.V0721ReadableTelemetry PROPERTIES TIMEOUT 30)

'''
    text = replace_once(text,
        '    add_executable(RunnerCoreTests tests/core_tests.cpp)\n',
        test_block + '    add_executable(RunnerCoreTests tests/core_tests.cpp)\n',
        "readable telemetry test target")
    write(path, text)


def patch_readme() -> None:
    path = "README.md"
    text = read(path)
    text = text.replace("Runner 0.7.20 is", "Runner 0.7.21 is", 1)
    text = text.replace("Python 3 for deterministic v0.7.19 source generation",
        "Python 3 for deterministic icon generation")
    text = text.replace("- `T`: toggle Training Results / Lifetime Totals",
        "- `T`: cycle Summary / Totals / Advanced Diagnostics")
    text = text.replace(
        "The application advertises the primary controls in the top bar. The live trainer distinguishes cumulative total updates from the resettable local policy/stage counter and exposes current stage work, evaluations, resets, pipeline state, and update throughput.",
        "The default Summary page explains learning health, lesson progress, the latest test, current useful evidence, the retained best controller, and the exact next goal. Raw scores, losses, quality keys, pipeline state, and throughput remain available on Advanced Diagnostics.")
    text = replace_once(text,
        '- [`docs/RUNNER_V0720_UI_PREVIEW_ICON.md`](docs/RUNNER_V0720_UI_PREVIEW_ICON.md) documents logical DPI, clipping, preview continuity, and application icon integration.\n',
        '- [`docs/RUNNER_V0720_UI_PREVIEW_ICON.md`](docs/RUNNER_V0720_UI_PREVIEW_ICON.md) documents logical DPI, clipping, preview continuity, and application icon integration.\n'
        '- [`docs/RUNNER_V0721_READABLE_TELEMETRY.md`](docs/RUNNER_V0721_READABLE_TELEMETRY.md) defines every plain-language training status, counter, goal, and color rule.\n',
        "README readable doc")
    section = r'''## v0.7.21 readable training dashboard

- Replaces the default raw negative-score display with a plain-language learning-health headline.
- Shows conservative lesson progress from required updates, attempts, and repeat tests.
- Translates the latest rejected test into one actionable reason without implying that saved training was lost.
- Reports stage-specific useful evidence and the exact current mastery goal.
- Explains total updates, attempts, valid attempts, resets, rollbacks, and retained champions on-screen.
- Keeps raw score, quality key, losses, optimizer state, throughput, and pipeline data on an explicit Advanced page.
- Preserves v0.7.20 learned-state, checkpoint, terrain, curriculum, preview, DPI, clipping, and icon behavior unchanged.

'''
    text = replace_once(text,
        '## v0.7.20 UI and preview continuity\n',
        section + '## v0.7.20 UI and preview continuity\n',
        "README v0721 section")
    write(path, text)


def patch_changelog() -> None:
    path = "CHANGELOG.md"
    text = read(path)
    if text.startswith("## 0.7.21"):
        return
    prefix = r'''## 0.7.21

- Replaced raw default evaluation scores with plain-language training health, test result, progress, evidence, and next-goal summaries.
- Added conservative lesson progress from updates, completed attempts, and repeat evaluations.
- Added stage-specific rejection explanations and mastery targets.
- Added friendly Rig / Session / All-Time totals and inline definitions for attempts, resets, rollbacks, and total updates.
- Moved score, quality, loss, optimizer, worker, and pipeline data to Advanced Diagnostics; non-finite values show as unavailable.
- Preserved v0.7.20 policy, checkpoint, curriculum, terrain, autosave, preview, DPI, clipping, and icon semantics.

'''
    write(path, prefix + text)


def patch_audit() -> None:
    path = "tools/repository_audit.cmake"
    text = read(path)
    text = replace_once(text,
        '        docs/RUNNER_V0720_UI_PREVIEW_ICON.md\n',
        '        docs/RUNNER_V0720_UI_PREVIEW_ICON.md\n'
        '        docs/RUNNER_V0721_READABLE_TELEMETRY.md\n',
        "audit readable doc")
    text = replace_once(text,
        '        tests/v0720_ui_tests.cpp\n',
        '        tests/v0720_ui_tests.cpp\n'
        '        tests/v0721_readable_telemetry_tests.cpp\n',
        "audit readable test")
    text = replace_once(text,
        '        src/preview_sync.hpp\n',
        '        src/preview_sync.hpp\n'
        '        src/training_explainer.hpp\n',
        "audit explainer header")
    text = text.replace('"project(Runner VERSION 0.7.20 LANGUAGES CXX)"',
        '"project(Runner VERSION 0.7.21 LANGUAGES CXX)"', 1)
    text = replace_once(text,
        '        "RunnerV0720UiTests"\n',
        '        "RunnerV0720UiTests"\n'
        '        "RunnerV0721ReadableTelemetryTests"\n',
        "audit readable target")
    text = replace_once(text,
        '        "RUNNER_V0720_UI_PREVIEW_ICON.md"\n',
        '        "RUNNER_V0720_UI_PREVIEW_ICON.md"\n'
        '        "RUNNER_V0721_READABLE_TELEMETRY.md"\n',
        "audit readable package")
    text = text.replace("CMake v0.7.20 contract missing",
        "CMake v0.7.21 contract missing")
    text = replace_once(text,
        '        "WALK-RELEASE-262")',
        '        "WALK-RELEASE-262"\n'
        '        "WALK-HUMAN-STATUS-263"\n'
        '        "WALK-ADVANCED-269"\n'
        '        "WALK-TELEMETRY-TEST-272"\n'
        '        "WALK-RELEASE-275")',
        "audit readable missions")
    text = text.replace("Mission cache v0.7.20 contract missing",
        "Mission cache v0.7.21 contract missing")
    text = replace_once(text,
        '        .github/workflows/fix-v0720-validation.yml)',
        '        .github/workflows/fix-v0720-validation.yml\n'
        '        tools/apply_v0721_readable_telemetry.py\n'
        '        .github/workflows/apply-v0721-readable-telemetry.yml)',
        "audit temporary v0721 files")
    text = text.replace("Runner v0.7.20 repository hygiene passed",
        "Runner v0.7.21 repository hygiene passed")
    write(path, text)


def main() -> int:
    patch_app()
    patch_cmake()
    patch_readme()
    patch_changelog()
    patch_audit()
    print("Runner v0.7.21 readable telemetry source migration applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
