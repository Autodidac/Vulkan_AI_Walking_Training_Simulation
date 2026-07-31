from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


def replace_section(text: str, start_marker: str, end_marker: str,
                    replacement: str, label: str) -> str:
    start = text.find(start_marker)
    if start < 0:
        raise SystemExit(f"{label}: start marker not found")
    end = text.find(end_marker, start)
    if end < 0:
        raise SystemExit(f"{label}: end marker not found")
    return text[:start] + replacement + text[end:]


app = Path("src/app.cpp")
text = app.read_text(encoding="utf-8")
text = replace_once(
    text,
    "        constexpr float ui_font_scale = 1.22f;",
    "        constexpr float ui_font_scale = 1.55f;",
    "global readable font scale",
)

helper_anchor = """        [[nodiscard]] Vec2 world_to_screen(Vec2 world, Rect viewport, float camera_x,
            float pixels_per_meter, float ground_fraction = 0.84f) noexcept
"""
helper = """        [[nodiscard]] float fit_text_scale(std::string_view text, float requested_scale,
            float maximum_width, float minimum_scale = 0.92f) noexcept
        {
            float scale = requested_scale;
            while (scale > minimum_scale
                && font::measure_text(text, scale * ui_font_scale).x > maximum_width)
                scale -= 0.05f;
            return std::max(scale, minimum_scale);
        }

        void add_text_fit(render::Canvas& canvas, Vec2 position, std::string_view text,
            float scale, Color color, float maximum_width, float minimum_scale = 0.92f)
        {
            add_text(canvas, position, text,
                fit_text_scale(text, scale, maximum_width, minimum_scale), color);
        }

        float add_wrapped_text(render::Canvas& canvas, Vec2 position, std::string_view text,
            float scale, Color color, float maximum_width, float line_gap = 5.0f)
        {
            const float advance = static_cast<float>(font::line_advance) * scale * ui_font_scale + line_gap;
            float y = position.y;
            std::string line{};
            std::size_t cursor = 0;
            auto flush = [&]()
            {
                if (line.empty())
                    return;
                add_text(canvas, { position.x, y }, line, scale, color);
                line.clear();
                y += advance;
            };

            while (cursor < text.size())
            {
                if (text[cursor] == '\n')
                {
                    flush();
                    ++cursor;
                    continue;
                }
                while (cursor < text.size() && text[cursor] == ' ')
                    ++cursor;
                if (cursor >= text.size())
                    break;
                const std::size_t end = text.find_first_of(" \n", cursor);
                const std::size_t word_end = end == std::string_view::npos ? text.size() : end;
                const std::string_view word = text.substr(cursor, word_end - cursor);
                std::string candidate = line;
                if (!candidate.empty())
                    candidate.push_back(' ');
                candidate.append(word);
                if (!line.empty()
                    && font::measure_text(candidate, scale * ui_font_scale).x > maximum_width)
                {
                    flush();
                    line.assign(word);
                }
                else
                {
                    line = std::move(candidate);
                }
                cursor = word_end;
            }
            flush();
            return y - position.y;
        }

""" + helper_anchor
text = replace_once(text, helper_anchor, helper, "responsive text helpers")

new_top_bar = """        void draw_top_bar(const InputState& input, int width)
        {
            constexpr float bar_height = 82.0f;
            canvas.quad({ 0.0f, 0.0f }, { static_cast<float>(width), bar_height }, rgb(0x0b1119));
            add_text(canvas, { 18.0f, 13.0f }, "EPOCH RUNNER v" EPOCHRUNNER_VERSION, 2.10f, white);
            if (width >= 1080)
                add_text(canvas, { 20.0f, 50.0f }, "AUTONOMOUS LOCOMOTION LAB", 1.05f, muted);

            const float tab_width = width >= 1080 ? 184.0f : 164.0f;
            const float start_x = static_cast<float>(width) - tab_width * 2.0f - 18.0f;
            if (button({ { start_x, 16.0f }, { tab_width - 7.0f, 50.0f } },
                "LIVE AUTOPILOT", input, mode == Mode::live))
                mode = Mode::live;
            if (button({ { start_x + tab_width, 16.0f }, { tab_width - 7.0f, 50.0f } },
                "RIG LAB", input, mode == Mode::rig_lab))
                mode = Mode::rig_lab;
        }

"""
text = replace_section(text, "        void draw_top_bar(",
    "        void draw_course_ground(", new_top_bar, "top bar layout")

new_live_panel = """        void draw_live_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            const float usable_width = rect.size.x - 36.0f;
            Vec2 cursor = rect.position + Vec2{ 18.0f, 16.0f };
            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const rl::TrainingMetrics& metrics = trainer.metrics();

            add_text(canvas, cursor, "AUTONOMOUS TRAINER", 1.72f, white);
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
            add_text_fit(canvas, cursor, std::format("DIFFICULTY {:.0f}%   MASTERY {}/3",
                autonomy.difficulty * 100.0f, autonomy.mastery_streak), 1.16f, white, usable_width);
            cursor.y += 29.0f;
            cursor.y += add_wrapped_text(canvas, cursor, autonomy.message, 1.00f,
                metrics.evaluation_valid || metrics.evaluation_count == 0 ? muted : danger,
                usable_width, 4.0f);
            cursor.y += 15.0f;

            const float third = (usable_width - 12.0f) / 3.0f;
            if (button({ cursor, { third, 40.0f } }, "NORMAL", input, trainer.updates_per_cycle() == 1))
                trainer.set_updates_per_cycle(1);
            if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 40.0f } },
                "FASTER", input, trainer.updates_per_cycle() == 2))
                trainer.set_updates_per_cycle(2);
            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 40.0f } },
                "MAX CPU", input, trainer.updates_per_cycle() == 4))
                trainer.set_updates_per_cycle(4);
            cursor.y += 57.0f;

            add_rounded_rect(canvas, { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 218.0f } },
                8.0f, panel_alt, border, 1.0f);
            add_text(canvas, cursor, "TRAINING RESULTS", 1.18f, accent);
            cursor.y += 31.0f;
            add_text_fit(canvas, cursor, std::format("UPDATE {}   ENV STEPS {}",
                metrics.update, metrics.environment_steps), 1.10f, white, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("EVAL {:+.2f}   DIST {:.2f} M",
                metrics.evaluation_score, metrics.evaluation_distance), 1.10f,
                metrics.evaluation_valid ? green : danger, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("BEST {:+.2f} @ UPDATE {}",
                metrics.best_evaluation_score, metrics.best_update), 1.10f, accent, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("SURVIVAL {:.1f} S   STRIDES {:.1f}",
                metrics.evaluation_survival, metrics.evaluation_stride_events), 1.08f, white, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("COLLISIONS {:.1f}   AIRBORNE {:.0f}%",
                metrics.evaluation_collisions, metrics.evaluation_airborne_ratio * 100.0f),
                1.08f, white, usable_width);
            cursor.y += 45.0f;

            add_rounded_rect(canvas, { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 230.0f } },
                8.0f, panel_alt, border, 1.0f);
            add_text(canvas, cursor, "RUNTIME", 1.18f, accent);
            cursor.y += 31.0f;
            add_text_fit(canvas, cursor, std::format("{} ROLLOUT THREADS   {} ENVIRONMENTS",
                autonomy.rollout_threads, autonomy.environment_count), 1.06f, white, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("{:.2f} UPDATES/S   MODE {}",
                autonomy.updates_per_second, autonomy.speed_mode), 1.06f, white, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("COMMANDS {}   {}",
                autonomy.pending_commands, autonomy.worker_busy ? "TRAINER BUSY" : "TRAINER IDLE"),
                1.06f, autonomy.worker_busy ? yellow : green, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("RIG GEN {}   ACCEPTED {}   REJECTED {}",
                autonomy.rig_generation, autonomy.accepted_rig_changes, autonomy.rejected_rig_changes),
                1.02f, white, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("ROLLBACKS {}   NO FLY / FLIP / >50 KM/H",
                autonomy.rollback_count), 1.00f, white, usable_width);
            cursor.y += 45.0f;

            cursor.y += add_wrapped_text(canvas, cursor,
                "REAL FEET / SOFT START / AUTOMATIC CHECKPOINTS AND RIG EVOLUTION",
                0.98f, muted, usable_width, 4.0f);
            cursor.y += 10.0f;
            add_wrapped_text(canvas, cursor,
                "A NEW VERIFIED BEST IS APPLIED AT THE NEXT LIVE RUN",
                0.98f, muted, usable_width, 4.0f);
        }

"""
text = replace_section(text, "        void draw_live_panel(",
    "        void draw_live_world(", new_live_panel, "live trainer panel")

new_live_world = """        void draw_live_world(Rect viewport, float dt)
        {
            if (!run_paused)
                trainer.step_preview(dt);
            const sim::Environment& environment = trainer.preview();
            if (!environment.particles().empty())
                camera_x = lerp(camera_x,
                    environment.particles()[environment.blueprint().root_node].position.x + 1.8f, 0.045f);
            add_rounded_rect(canvas, viewport, 11.0f, rgb(0x09101a), border, 1.0f);
            draw_course_ground(environment, viewport, camera_x, 90.0f);
            draw_course_reference(environment, viewport, camera_x, 90.0f);
            draw_course_features(environment, viewport, camera_x, 90.0f);
            draw_creature(environment, viewport, camera_x, 90.0f);

            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const float overlay_width = std::max(260.0f, viewport.size.x - 48.0f);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 22.0f },
                std::format("{}  /  {:.0f}%", sim::course_stage_name(autonomy.stage), autonomy.difficulty * 100.0f),
                1.95f, white, overlay_width, 1.25f);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 61.0f },
                std::format("{:.1f} KM/H   ACTUAL {:.2f} M",
                    environment.forward_speed() * 3.6f, environment.distance_travelled()),
                1.22f, environment.valid_motion() ? green : danger, overlay_width);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 90.0f },
                std::format("COURSE {:.1f} M   {}", environment.course_progress(),
                    sim::invalid_motion_name(environment.invalid_reason())),
                1.16f, environment.valid_motion() ? green : danger, overlay_width);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 119.0f },
                std::format("RECOVERY {}   {}/{}   FEET {}/{}",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.recovery_successes(), environment.recovery_events(),
                    environment.left_supported() ? "L" : "-",
                    environment.right_supported() ? "R" : "-"),
                1.06f, environment.recovering() ? yellow : muted, overlay_width);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, viewport.size.y - 38.0f },
                "LIVE BEST CONTROLLER   v" EPOCHRUNNER_VERSION "   BACKGROUND TRAINING ACTIVE",
                0.96f, muted, overlay_width, 0.82f);
        }

"""
text = replace_section(text, "        void draw_live_world(",
    "        void draw_joint_lab(", new_live_world, "live world telemetry")

text = replace_once(
    text,
    """            const Rect content{ { 10.0f, 74.0f },
                { static_cast<float>(width) - 20.0f, static_cast<float>(height) - 84.0f } };
            if (content.size.x < 700.0f || content.size.y < 500.0f)
""",
    """            const Rect content{ { 10.0f, 92.0f },
                { static_cast<float>(width) - 20.0f, static_cast<float>(height) - 102.0f } };
            if (content.size.x < 760.0f || content.size.y < 560.0f)
""",
    "content area below enlarged top bar",
)
text = replace_once(
    text,
    """                const float panel_width = std::clamp(content.size.x * 0.30f, 390.0f, 470.0f);
""",
    """                const float panel_width = std::clamp(content.size.x * 0.40f, 500.0f, 590.0f);
""",
    "readable live panel width",
)
text = replace_once(
    text,
    """                const float panel_width = std::clamp(content.size.x * 0.34f, 470.0f, 560.0f);
""",
    """                const float panel_width = std::clamp(content.size.x * 0.40f, 540.0f, 640.0f);
""",
    "readable rig panel width",
)
app.write_text(text, encoding="utf-8")

missions = Path("MISSIONS.md")
text = missions.read_text(encoding="utf-8")
mission = """## WALK-UI-002 — Readable responsive telemetry

**Status:** ACTIVE

The live and rig interfaces must remain readable at ordinary desktop sizes. Text may not overlap adjacent labels, buttons, cards, or the title bar. Long status messages must wrap or fit within their panel instead of being clipped into neighboring content.

**Acceptance:**

- Increase the default bitmap-font scale and minimum fitted scale.
- Use a taller responsive title bar with non-overlapping tabs and subtitle.
- Give live and rig side panels enough width for their controls.
- Group live metrics into readable cards with larger vertical spacing.
- Wrap long trainer/status lines and fit world telemetry to viewport width.
- Full Windows/Vulkan build and executable diagnostics pass with the responsive layout.

"""
if "## WALK-UI-002" not in text:
    marker = "## Current warning\n"
    if marker not in text:
        raise SystemExit("mission ledger: Current warning anchor not found")
    text = text.replace(marker, mission + marker, 1)
missions.write_text(text, encoding="utf-8")

readme = Path("README.md")
text = readme.read_text(encoding="utf-8")
needle = "Passive heel/toe triangles now count as the left or right support cluster used by observations, gait validation, airborne checks, rewards, and recovery. Only those designated foot clusters receive strong traction; incidental head, tail, or torso contact slides instead of acting like an unintended brake. Procedural rocks and hazards use stable sequence/world coordinates and no longer inherit root translation. Version-specific autosaves prevent incompatible v0.6.1 controllers from silently resuming under the corrected contact model.\n"
replacement = needle + "\nThe v0.6.2 interface also uses larger typography, wider responsive side panels, wrapped trainer messages, grouped runtime/result cards, and split world telemetry so labels remain readable instead of overlapping.\n"
text = replace_once(text, needle, replacement, "README responsive UI note")
readme.write_text(text, encoding="utf-8")
