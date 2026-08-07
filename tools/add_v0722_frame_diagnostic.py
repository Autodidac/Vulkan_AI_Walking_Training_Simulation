#!/usr/bin/env python3
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


path = Path('src/main.cpp')
text = path.read_text(encoding='utf-8')
text = replace_once(text,
    '#include "ui_layout.hpp"\n',
    '#include "ui_layout.hpp"\n#include "ui_frame_probe.hpp"\n',
    'visible-frame probe include')
helper = r'''    [[nodiscard]] bool visible_application_frames()
    {
        constexpr int width = 1600;
        constexpr int height = 900;
        constexpr float dt = 1.0f / 60.0f;
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(width), static_cast<float>(height));
        const runner::ui_layout::Box live_world =
            runner::ui_layout::live_world_box(content);
        const runner::ui_layout::Box live_panel =
            runner::ui_layout::live_panel_box(content);
        const runner::ui_layout::Box live_pip =
            runner::ui_layout::training_pip_box(live_world);
        const runner::ui_layout::Box rig_panel =
            runner::ui_layout::rig_lab_panel_box(content);
        const runner::ui_layout::Box rig_world =
            runner::ui_layout::rig_lab_world_box(content);

        runner::Application application{};
        auto visible = [&](runner::ui_layout::Box region)
        {
            return runner::ui_frame_probe::visibly_populated(
                runner::ui_frame_probe::analyze(application.vertices(), region));
        };

        runner::InputState input{};
        application.frame(input, dt, width, height);
        if (!visible(live_world) || !visible(live_panel) || !visible(live_pip))
            return false;

        runner::InputState switch_to_rig{};
        switch_to_rig.tab_pressed = true;
        application.frame(switch_to_rig, dt, width, height);
        if (!visible(rig_panel) || !visible(rig_world))
            return false;

        const float usable_width = rig_panel.width - 36.0f;
        const float tab_width = (usable_width - 18.0f) * 0.25f;
        const float tab_y = rig_panel.y + 71.5f;
        for (int slot = 1; slot < 4; ++slot)
        {
            runner::InputState click{};
            click.left_pressed = true;
            click.mouse = {
                rig_panel.x + 18.0f
                    + static_cast<float>(slot) * (tab_width + 6.0f)
                    + tab_width * 0.5f,
                tab_y
            };
            application.frame(click, dt, width, height);
            if (!visible(rig_panel) || !visible(rig_world))
                return false;
        }
        return true;
    }

'''
text = replace_once(text,
    '''    [[nodiscard]] bool is_headless_surface_error(std::string_view error) noexcept
    {
        return error.find("VK_KHR_surface") != std::string_view::npos
            || error.find("VK_KHR_win32_surface") != std::string_view::npos;
    }
}
''',
    '''    [[nodiscard]] bool is_headless_surface_error(std::string_view error) noexcept
    {
        return error.find("VK_KHR_surface") != std::string_view::npos
            || error.find("VK_KHR_win32_surface") != std::string_view::npos;
    }

''' + helper + '''}
''',
    'visible-frame diagnostic helper')
text = replace_once(text,
    '''        valid = valid && std::abs(dpi.x - 1.5f) < 1.0e-5f
            && std::abs(dpi.y - 1.5f) < 1.0e-5f;
        std::printf("Runner %s UI diagnostic: %s; layouts=%zu dpi=%.2fx%.2f\\\\n",
            RUNNER_VERSION, valid ? "passed" : "failed",
            runner::ui_layout::validation_sizes.size(), dpi.x, dpi.y);
''',
    '''        valid = valid && std::abs(dpi.x - 1.5f) < 1.0e-5f
            && std::abs(dpi.y - 1.5f) < 1.0e-5f
            && visible_application_frames();
        std::printf("Runner %s UI diagnostic: %s; layouts=%zu dpi=%.2fx%.2f frames=%s\\n",
            RUNNER_VERSION, valid ? "passed" : "failed",
            runner::ui_layout::validation_sizes.size(), dpi.x, dpi.y,
            valid ? "visible" : "invalid");
''',
    'packaged UI diagnostic')
path.write_text(text, encoding='utf-8', newline='\n')

readme = Path('README.md')
readme_text = readme.read_text(encoding='utf-8')
readme_text = readme_text.replace(
    '`--diagnose-acceptance` runs the deterministic rig/curriculum matrix used by package auditing. `--diagnose-camera` validates adaptive fit, clamps, wheel zoom, lookahead, dead-zone follow, and PIP scale without opening a window.',
    '`--diagnose-acceptance` runs the deterministic rig/curriculum matrix used by package auditing. `--diagnose-camera` validates adaptive fit, clamps, wheel zoom, lookahead, dead-zone follow, and PIP scale. `--diagnose-ui` CPU-composites representative Live and all four Rig Lab pages and fails if any content region is black or visually empty.')
readme.write_text(readme_text, encoding='utf-8', newline='\n')

audit = Path('tools/repository_audit.cmake')
audit_text = audit.read_text(encoding='utf-8')
audit_text = replace_once(audit_text,
    '''        "--diagnose-ui"
        "SDL_SetWindowIcon"''',
    '''        "--diagnose-ui"
        "visible_application_frames"
        "SDL_SetWindowIcon"''',
    'diagnostic repository audit')
audit_text = audit_text.replace(
    '        tools/apply_v0722_black_frame_hotfix.py\n',
    '        tools/apply_v0722_black_frame_hotfix.py\n        tools/add_v0722_frame_diagnostic.py\n')
audit.write_text(audit_text, encoding='utf-8', newline='\n')

print('Runner v0.7.22 packaged visible-frame diagnostic added')
