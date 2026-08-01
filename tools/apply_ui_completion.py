from pathlib import Path

root = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str) -> None:
    target = root / path
    text = target.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise RuntimeError(f"{path}: expected one match, found {text.count(old)}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "src/app.cpp",
    '#include "simulation.hpp"\n',
    '#include "simulation.hpp"\n#include "ui_layout.hpp"\n',
)
replace_once(
    "src/app.cpp",
    '''            add_rounded_rect(canvas, { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 230.0f } },
                8.0f, panel_alt, border, 1.0f);''',
    '''            add_rounded_rect(canvas, { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 259.0f } },
                8.0f, panel_alt, border, 1.0f);''',
)
replace_once(
    "src/app.cpp",
    '''            add_text_fit(canvas, cursor, std::format("COMMANDS {}   {}",
                autonomy.pending_commands, autonomy.worker_busy ? "TRAINER BUSY" : "TRAINER IDLE"),
                1.06f, autonomy.worker_busy ? yellow : green, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("RIG GEN {}   ACCEPTED {}   REJECTED {}",''',
    '''            add_text_fit(canvas, cursor, std::format("COMMANDS {}   {}",
                autonomy.pending_commands, autonomy.worker_busy ? "TRAINER BUSY" : "TRAINER IDLE"),
                1.06f, autonomy.worker_busy ? yellow : green, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("PIPELINE {}   MASK {:02X}",
                autonomy.pipeline_stage, autonomy.pipeline_stage_mask),
                1.02f, autonomy.worker_busy ? yellow : green, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("RIG GEN {}   ACCEPTED {}   REJECTED {}",''',
)
replace_once(
    "src/app.cpp",
    '''            const float pip_width = std::clamp(viewport.size.x * 0.34f, 300.0f, 390.0f);
            const float pip_height = std::clamp(viewport.size.y * 0.27f, 190.0f, 245.0f);
            draw_training_pip({
                { viewport.position.x + viewport.size.x - pip_width - 18.0f,
                  viewport.position.y + 18.0f },
                { pip_width, pip_height }
            });''',
    '''            const ui_layout::Box pip = ui_layout::training_pip_box({
                viewport.position.x, viewport.position.y, viewport.size.x, viewport.size.y });
            draw_training_pip({ { pip.x, pip.y }, { pip.width, pip.height } });''',
)
replace_once(
    "src/app.cpp",
    '''            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, 31.0f } }, "PAIR 1+2", input,
                joint_test_group == JointTestGroup::pair_a))''',
    '''            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, 31.0f } }, "LEGS 1-4", input,
                joint_test_group == JointTestGroup::pair_a))''',
)
replace_once(
    "src/app.cpp",
    '''            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, 31.0f } }, "PAIR 3+4", input,
                joint_test_group == JointTestGroup::pair_b))''',
    '''            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, 31.0f } }, "ARMS 5-8", input,
                joint_test_group == JointTestGroup::pair_b))''',
)
replace_once(
    "src/app.cpp",
    '''            const Rect content{ { 10.0f, 92.0f },
                { static_cast<float>(width) - 20.0f, static_cast<float>(height) - 102.0f } };
            if (content.size.x < 1080.0f || content.size.y < 640.0f)
            {
                add_text(canvas, { 24.0f, 100.0f }, "WINDOW TOO SMALL", 2.0f, danger);
                return;
            }

            if (mode == Mode::live)
            {
                const float panel_width = std::clamp(content.size.x * 0.42f, 650.0f, 720.0f);
                const Rect world{ content.position, { content.size.x - panel_width - 10.0f, content.size.y } };
                const Rect side{ { content.position.x + world.size.x + 10.0f, content.position.y },
                    { panel_width, content.size.y } };
                draw_live_world(world, dt);
                draw_live_panel(side, input);
            }''',
    '''            const ui_layout::Box layout_content = ui_layout::content_box(
                static_cast<float>(width), static_cast<float>(height));
            const Rect content{ { layout_content.x, layout_content.y },
                { layout_content.width, layout_content.height } };
            if (!ui_layout::supported_window(static_cast<float>(width), static_cast<float>(height)))
            {
                add_text(canvas, { 24.0f, 100.0f },
                    "WINDOW TOO SMALL - MINIMUM CONTENT 1080 X 800", 2.0f, danger);
                return;
            }

            if (mode == Mode::live)
            {
                const ui_layout::Box layout_world = ui_layout::live_world_box(layout_content);
                const ui_layout::Box layout_side = ui_layout::live_panel_box(layout_content);
                const Rect world{ { layout_world.x, layout_world.y },
                    { layout_world.width, layout_world.height } };
                const Rect side{ { layout_side.x, layout_side.y },
                    { layout_side.width, layout_side.height } };
                draw_live_world(world, dt);
                draw_live_panel(side, input);
            }''',
)
replace_once(
    "tests/core_tests.cpp",
    '#include "simulation.hpp"\n',
    '#include "simulation.hpp"\n#include "ui_layout.hpp"\n',
)
replace_once(
    "tests/core_tests.cpp",
    '''    using namespace epochrunner;

    require(sim::classify_motion_gate''',
    '''    using namespace epochrunner;

    require(ui_layout::live_layout_valid(1100.0f, 902.0f),
        "supported minimum live layout overlaps its panel, telemetry, or PIP");
    require(!ui_layout::supported_window(1099.0f, 902.0f)
            && !ui_layout::supported_window(1100.0f, 901.0f),
        "undersized windows are incorrectly treated as fully supported");
    const ui_layout::Box minimum_content = ui_layout::content_box(1100.0f, 902.0f);
    const ui_layout::Box minimum_world = ui_layout::live_world_box(minimum_content);
    const ui_layout::Box minimum_pip = ui_layout::training_pip_box(minimum_world);
    require(ui_layout::contains(minimum_world, minimum_pip),
        "training PIP escapes the world viewport");
    require(!ui_layout::overlaps(minimum_pip,
                ui_layout::primary_telemetry_box(minimum_world))
            && !ui_layout::overlaps(minimum_pip,
                ui_layout::bottom_telemetry_box(minimum_world)),
        "training PIP overlaps primary telemetry at the supported minimum window");

    require(sim::classify_motion_gate''',
)

print("Applied deterministic UI and PIP completion patch.")
