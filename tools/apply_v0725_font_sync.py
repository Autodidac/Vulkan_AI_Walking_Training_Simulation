#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def patch_application() -> None:
    path = "src/app.cpp"
    text = read(path)

    text = replace_once(text,
        "        constexpr float ui_font_scale = 2.05f;\n",
        '''        [[nodiscard]] constexpr font::FontSize font_size(
            float style_scale) noexcept
        {
            return {
                .logical_height = font::default_logical_height
                    * (style_scale > 0.0f ? style_scale : 1.0f),
                .dpi_scale = 1.0f
            };
        }
''',
        "EpochGui logical font-size helper")

    old_add_text = '''        void add_text(render::Canvas& canvas, Vec2 position, std::string_view text, float scale, Color color)
        {
            scale *= ui_font_scale;
            Vec2 cursor = position;
            const float start_x = position.x;
            for (const char character : text)
            {
                if (character == '\\n')
                {
                    cursor.x = start_x;
                    cursor.y += static_cast<float>(font::line_advance) * scale;
                    continue;
                }
                const font::BitmapGlyph glyph = font::default_glyph(character);
                for (std::uint32_t row = 0; row < font::glyph_height; ++row)
                {
                    for (std::uint32_t column = 0; column < font::glyph_width; ++column)
                    {
                        if (!font::pixel_on(glyph, column, row))
                            continue;
                        const Vec2 minimum{
                            cursor.x + static_cast<float>(column) * scale,
                            cursor.y + static_cast<float>(row) * scale
                        };
                        canvas.quad(minimum, minimum + Vec2{ scale, scale }, color);
                    }
                }
                cursor.x += static_cast<float>(font::glyph_advance) * scale;
            }
        }
'''
    new_add_text = '''        void add_text(render::Canvas& canvas, Vec2 position,
            std::string_view text, float style_scale, Color color)
        {
            const font::BitmapFontMetrics metrics =
                font::make_bitmap_font_metrics(font_size(style_scale));
            Vec2 cursor = position;
            const float start_x = position.x;
            for (const char character : text)
            {
                if (character == '\\n')
                {
                    cursor.x = start_x;
                    cursor.y += metrics.line_advance;
                    continue;
                }
                const font::BitmapGlyph glyph = font::default_glyph(character);
                for (std::uint32_t row = 0; row < font::glyph_height; ++row)
                {
                    for (std::uint32_t column = 0;
                        column < font::glyph_width; ++column)
                    {
                        if (!font::pixel_on(glyph, column, row))
                            continue;
                        const Vec2 minimum{
                            cursor.x + static_cast<float>(column)
                                * metrics.cell_size,
                            cursor.y + static_cast<float>(row)
                                * metrics.cell_size
                        };
                        canvas.quad(minimum,
                            minimum + Vec2{ metrics.cell_size,
                                metrics.cell_size }, color);
                    }
                }
                cursor.x += metrics.advance;
            }
        }
'''
    text = replace_once(text, old_add_text, new_add_text,
        "logical-height text rendering")

    text = text.replace(
        "font::measure_text(text, scale * ui_font_scale)",
        "font::measure_text(text, font_size(scale))")
    text = text.replace(
        "font::measure_text(candidate, scale * ui_font_scale)",
        "font::measure_text(candidate, font_size(scale))")
    text = text.replace(
        "font::measure_text(label, scale * ui_font_scale)",
        "font::measure_text(label, font_size(scale))")
    text = replace_once(text,
        '''            const float advance = static_cast<float>(font::line_advance) * scale * ui_font_scale + line_gap;
''',
        '''            const float advance = font::make_bitmap_font_metrics(
                font_size(scale)).line_advance + line_gap;
''',
        "logical line advance")

    # Catch every remaining measurement expression, including calls whose first
    # argument is not named text/candidate/label. The old multiplier described
    # bitmap cell size; the new style scale describes logical glyph height.
    text = re.sub(r"\s*\*\s*ui_font_scale\b", "", text)
    text = re.sub(r"\bui_font_scale\s*\*\s*", "", text)

    work_helper = '''        [[nodiscard]] std::string format_work_counter(
            std::string_view label, std::uint64_t completed,
            std::uint64_t required)
        {
            if (required == 0u || completed >= required)
                return std::format("{} READY", label);
            return std::format("{} {}/{}", label, completed, required);
        }

'''
    text = replace_once(text,
        '''            flush();
            return y - position.y;
        }

        [[nodiscard]] Vec2 world_to_screen''',
        '''            flush();
            return y - position.y;
        }

''' + work_helper + '''        [[nodiscard]] Vec2 world_to_screen''',
        "human-readable work counter helper")

    old_progress = '''            add_text_fit(canvas, cursor,
                std::format("TRAINING WORK {}%   MASTERY PASSES {} / {}",
                    static_cast<int>(std::lround(progress.training_work * 100.0f)),
                    autonomy.mastery_streak,
                    rl::required_mastery_confirmations(autonomy.stage)),
                0.78f, human_color, usable_width, 0.64f);
            cursor.y += 20.0f;
            add_text_fit(canvas, cursor,
                std::format("UPDATES {}/{}   RUNS {}/{}   TESTS {}/{}",
                    autonomy.stage_fresh_updates, autonomy.stage_required_updates,
                    autonomy.stage_fresh_episodes, autonomy.stage_required_episodes,
                    autonomy.stage_fresh_evaluations, autonomy.stage_required_evaluations),
                0.74f, muted, usable_width, 0.60f);
'''
    new_progress = '''            const std::string training_work_label = progress.sample_budget_complete
                ? std::string("TRAINING SAMPLES READY")
                : std::format("TRAINING WORK {}%",
                    static_cast<int>(std::lround(
                        progress.training_work * 100.0f)));
            add_text_fit(canvas, cursor,
                std::format("{}   MASTERY PASSES {} / {}",
                    training_work_label, autonomy.mastery_streak,
                    rl::required_mastery_confirmations(autonomy.stage)),
                0.78f, human_color, usable_width, 0.64f);
            cursor.y += 20.0f;
            add_text_fit(canvas, cursor,
                std::format("{}   {}   {}",
                    format_work_counter("UPDATES",
                        autonomy.stage_fresh_updates,
                        autonomy.stage_required_updates),
                    format_work_counter("RUNS",
                        autonomy.stage_fresh_episodes,
                        autonomy.stage_required_episodes),
                    format_work_counter("TESTS",
                        autonomy.stage_fresh_evaluations,
                        autonomy.stage_required_evaluations)),
                0.74f, muted, usable_width, 0.60f);
'''
    text = replace_once(text, old_progress, new_progress,
        "noob-readable progress counters")

    if "ui_font_scale" in text:
        remaining = [
            f"{line_number}: {line}"
            for line_number, line in enumerate(text.splitlines(), 1)
            if "ui_font_scale" in line
        ]
        raise RuntimeError("legacy ui_font_scale remains after synchronization:\n"
            + "\n".join(remaining))
    write(path, text)


def patch_explainer() -> None:
    path = "src/training_explainer.hpp"
    text = read(path)
    text = replace_once(text,
        '''        if (progress.sample_budget_complete)
            return "Enough training samples exist; advancement now depends on repeat behavior tests.";
''',
        '''        if (progress.sample_budget_complete)
            return "Training samples are ready. The remaining 20% comes from repeat mastery tests.";
''',
        "mastery progress explanation")
    write(path, text)


def patch_docs() -> None:
    path = "README.md"
    text = read(path)
    needle = "- Isolates corrected v0.7.25 controller state and autosaves.\n"
    addition = (
        "- Synchronizes the EpochGui logical-pixel font sizing contract at commit "
        "`130f33fe31d73564a35a622f3bb5ddcc2b5105d5`.\n"
        "- Renders `%` correctly and replaces overflowing work fractions with "
        "`UPDATES/RUNS/TESTS READY` labels once each sample budget is met.\n")
    if addition not in text:
        text = replace_once(text, needle, needle + addition,
            "README font synchronization")
    write(path, text)

    path = "CHANGELOG.md"
    text = read(path)
    needle = "- Bumped training semantics and isolated v0.7.25 autosaves.\n"
    addition = (
        "- Synchronized EpochGui logical-pixel font sizing, added the missing percent glyph, "
        "and replaced overflowing work counters with clear READY states.\n")
    if addition not in text:
        text = replace_once(text, needle, needle + addition,
            "changelog font synchronization")
    write(path, text)

    path = "docs/RUNNER_V0725_ART_LEG_HOTFIX.md"
    text = read(path)
    if "## EpochGui font and progress synchronization\n" not in text:
        text += '''

## EpochGui font and progress synchronization

Runner's renderer-neutral bitmap font follows EpochGui commit `130f33fe31d73564a35a622f3bb5ddcc2b5105d5`: font sizes represent logical glyph height, and the renderer derives cell size, advance, measurement, and line advance from one shared metrics object. The application remains in logical SDL coordinates, so the font DPI multiplier is one while Vulkan maps the complete logical surface to the drawable surface.

The fallback glyph table includes `%`, preventing `30%`, `80%`, and `100%` from appearing as question marks. Once a lesson's sample budget is met, the compact header displays `UPDATES READY`, `RUNS READY`, and `TESTS READY` instead of misleading values such as `RUNS 17465/8`. Actual high-volume simulation totals remain available on the Totals page.
'''
    write(path, text)

    path = "missioncache.md"
    text = read(path)
    text = text.replace(
        "Direct packaged v0.7.24 eye testing confirms that the approved helmet and foot assets are usable, but the translucent torso sheet, circular shoulder masses, and duplicate ghost arms obscure the actual gait. Fixed segment lengths also remain insufficient: a two-link leg can preserve both bone lengths while folding until the knee appears to telescope into the pelvis.",
        "Direct packaged v0.7.24 eye testing confirms that the approved helmet and foot assets are usable, but the translucent torso sheet, circular shoulder masses, and duplicate ghost arms obscure the actual gait. Fixed segment lengths also remain insufficient: a two-link leg can preserve both bone lengths while folding until the knee appears to telescope into the pelvis. The same screenshots expose stale font-cell scaling, a missing percent glyph, and sample counters such as RUNS 17465/8 that are internally true but useless in the compact noob-facing header.")
    text = text.replace(
        "Add forced-compression recovery, exact segment-length, natural walking soak, compact-art source, approved helmet/foot retention, complete Linux, complete Windows SDL3/Vulkan, installed/extracted package, and runtime diagnostic tests.",
        "Add forced-compression recovery, exact segment-length, natural walking soak, compact-art source, approved helmet/foot retention, EpochGui logical font metrics, percent-glyph, READY-counter, complete Linux, complete Windows SDL3/Vulkan, installed/extracted package, and runtime diagnostic tests.")
    write(path, text)


def patch_repository_audit() -> None:
    path = "tools/repository_audit.cmake"
    text = read(path)
    marker = '''string(FIND "${app_text}" "Color{}" opaque_default_pos)
if(NOT opaque_default_pos EQUAL -1)
    message(FATAL_ERROR "Opaque default Color remains in application border rendering")
endif()
'''
    checks = marker + '''
foreach(reference IN ITEMS
        "font::make_bitmap_font_metrics"
        "TRAINING SAMPLES READY"
        "format_work_counter(\"RUNS\""
        "format_work_counter(\"TESTS\"")
    string(FIND "${app_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.25 readable font/progress contract missing: ${reference}")
    endif()
endforeach()
string(FIND "${app_text}" "ui_font_scale" legacy_font_scale_pos)
if(NOT legacy_font_scale_pos EQUAL -1)
    message(FATAL_ERROR "Legacy bitmap-cell font multiplier remains")
endif()

file(READ "${RUNNER_SOURCE_DIR}/src/ui_font.hpp" font_text)
foreach(reference IN ITEMS
        "130f33fe31d73564a35a622f3bb5ddcc2b5105d5"
        "default_logical_height = 16.0F"
        "make_bitmap_font_metrics"
        "case '%'")
    string(FIND "${font_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "EpochGui font synchronization missing: ${reference}")
    endif()
endforeach()
'''
    text = replace_once(text, marker, checks,
        "repository font/progress audit")
    text = replace_once(text,
        '''        tools/apply_v0725_art_leg_hotfix.py)
''',
        '''        tools/apply_v0725_art_leg_hotfix.py
        tools/apply_v0725_font_sync.py)
''',
        "stale font migration audit")
    write(path, text)


def main() -> int:
    patch_application()
    patch_explainer()
    patch_docs()
    patch_repository_audit()
    print("Runner v0.7.25 EpochGui font and progress synchronization applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
