#!/usr/bin/env python3
from __future__ import annotations

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


def patch_app() -> None:
    path = "src/app.cpp"
    text = read(path)
    text = replace_once(text,
        '#include "training_explainer.hpp"\n',
        '#include "training_explainer.hpp"\n#include "ui_render_contract.hpp"\n',
        "UI render contract include")
    mask_count = text.count("Color{}")
    if mask_count < 5:
        raise RuntimeError(
            f"opaque post-content mask audit: expected at least five Color{{}} uses, found {mask_count}")
    text = text.replace("Color{}", "ui_render::transparent_fill")
    if "Color{}" in text:
        raise RuntimeError("opaque default Color remains in app rendering source")
    write(path, text)


def patch_ui_tests() -> None:
    path = "tests/v0720_ui_tests.cpp"
    text = read(path)
    text = replace_once(text,
        '#include "ui_layout.hpp"\n',
        '#include "ui_layout.hpp"\n#include "ui_render_contract.hpp"\n',
        "UI test transparency include")
    text = replace_once(text,
        '''    canvas.pop_clip();
    require(canvas.clip_depth() == 0u, "canvas clip stack did not unwind");
''',
        '''    canvas.pop_clip();
    require(!canvas.vertices().empty(),
        "clipped primitive emitted no triangles and made the clip test vacuous");
    require(canvas.vertices().size() % 3u == 0u,
        "clipped canvas output is not a complete triangle list");
    require(canvas.clip_depth() == 0u, "canvas clip stack did not unwind");
''',
        "non-vacuous clip assertion")
    marker = '''    for (const runner::render::Vertex& vertex : canvas.vertices())
    {
        require(vertex.position.x >= 10.0f && vertex.position.x <= 40.0f
                && vertex.position.y >= 20.0f && vertex.position.y <= 50.0f,
            "clipped canvas vertex escaped its viewport");
    }

'''
    nested = marker + '''    runner::render::Canvas nested{};
    nested.push_clip({ 0.0f, 0.0f }, { 80.0f, 80.0f });
    nested.push_clip({ 20.0f, 25.0f }, { 55.0f, 60.0f });
    nested.quad({ -20.0f, -20.0f }, { 100.0f, 100.0f },
        { 0.2f, 0.7f, 0.9f, 1.0f });
    nested.pop_clip();
    nested.pop_clip();
    require(!nested.vertices().empty(),
        "nested clip intersection emitted no visible geometry");
    require(nested.clip_depth() == 0u,
        "nested clip stack did not unwind to zero");
    for (const runner::render::Vertex& vertex : nested.vertices())
    {
        require(vertex.position.x >= 20.0f && vertex.position.x <= 55.0f
                && vertex.position.y >= 25.0f && vertex.position.y <= 60.0f,
            "nested clipped vertex escaped the intersected viewport");
    }
    require(runner::ui_render::is_explicitly_transparent(
            runner::ui_render::transparent_fill),
        "named border-only fill is not transparent");
    require(runner::Color{}.a == 1.0f,
        "default Color no longer demonstrates why explicit transparency is required");

'''
    text = replace_once(text, marker, nested, "nested clipping contract")
    write(path, text)


def patch_cmake() -> None:
    path = "CMakeLists.txt"
    text = read(path)
    text = replace_once(text,
        "project(Runner VERSION 0.7.21 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.22 LANGUAGES CXX)",
        "project version")
    text = text.replace("Runner v0.7.21 icon generation failed",
        "Runner v0.7.22 icon generation failed")
    text = replace_once(text,
        '''        src/locomotion_strategy.hpp src/preview_sync.hpp
        src/training_explainer.hpp)''',
        '''        src/locomotion_strategy.hpp src/preview_sync.hpp
        src/training_explainer.hpp src/ui_render_contract.hpp
        src/ui_frame_probe.hpp)''',
        "application UI contract headers")
    text = replace_once(text,
        '''        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0721_READABLE_TELEMETRY.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0721_READABLE_TELEMETRY.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${runner_sandhybrid_SOURCE_DIR}/missioncache.md"''',
        '''        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0721_READABLE_TELEMETRY.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0721_READABLE_TELEMETRY.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${runner_sandhybrid_SOURCE_DIR}/missioncache.md"''',
        "post-build hotfix document")
    text = replace_once(text,
        '''        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0721_READABLE_TELEMETRY.md"
        DESTINATION docs)''',
        '''        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0721_READABLE_TELEMETRY.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md"
        DESTINATION docs)''',
        "install hotfix document")
    test_marker = '''    add_executable(RunnerCoreTests tests/core_tests.cpp)
'''
    test_block = '''    add_executable(RunnerV0722VisibleFrameTests
        tests/v0722_visible_frame_tests.cpp src/app.cpp src/canvas.cpp)
    target_link_libraries(RunnerV0722VisibleFrameTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0722VisibleFrameTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0722VisibleFrameTests PRIVATE cxx_std_23)
    target_compile_definitions(RunnerV0722VisibleFrameTests PRIVATE
        RUNNER_VERSION="${PROJECT_VERSION}")
    runner_enable_warnings(RunnerV0722VisibleFrameTests)
    add_test(NAME Runner.V0722VisibleFrames COMMAND RunnerV0722VisibleFrameTests)
    set_tests_properties(Runner.V0722VisibleFrames PROPERTIES TIMEOUT 120)

'''
    text = replace_once(text, test_marker, test_block + test_marker,
        "visible frame test target")
    write(path, text)


def patch_readme() -> None:
    path = "README.md"
    text = read(path)
    text = replace_once(text,
        "Runner 0.7.21 is a combined autonomous physics locomotion trainer",
        "Runner 0.7.22 is a combined autonomous physics locomotion trainer",
        "README version")
    text = replace_once(text,
        '- [`docs/RUNNER_V0721_READABLE_TELEMETRY.md`](docs/RUNNER_V0721_READABLE_TELEMETRY.md) defines every plain-language training status, counter, goal, and color rule.\n',
        '- [`docs/RUNNER_V0721_READABLE_TELEMETRY.md`](docs/RUNNER_V0721_READABLE_TELEMETRY.md) defines every plain-language training status, counter, goal, and color rule.\n'
        '- [`docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md`](docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md) documents the opaque border-fill regression and visible-frame tests.\n',
        "README hotfix document")
    section = '''## v0.7.22 black-frame hotfix

- Restores the Live world, dashboard, training PIP, Rig Lab viewport, and all four Rig Lab pages after an opaque post-content border fill hid their interiors.
- Replaces ambiguous default-constructed UI fill colors with one explicit zero-alpha border-only fill contract.
- Makes Canvas clipping tests require real emitted triangles and validates nested clip intersections.
- Adds CPU final-frame compositing tests that fail when a later opaque rectangle hides otherwise-correct content.
- Preserves all v0.7.21 controller, rig, gait, terrain, checkpoint, and autosave semantics.

'''
    text = replace_once(text, "## v0.7.21 readable training dashboard\n",
        section + "## v0.7.21 readable training dashboard\n",
        "README hotfix section")
    write(path, text)


def patch_changelog() -> None:
    path = "CHANGELOG.md"
    text = read(path)
    if not text.startswith("## 0.7.22\n"):
        text = '''## 0.7.22

- Fixed the black-interior regression covering Live Autopilot, its dashboard and PIP, Rig Lab, and all four Rig Lab pages.
- Replaced opaque default `Color{}` border fills with the explicit zero-alpha UI transparency contract.
- Made clipped-geometry tests non-vacuous and added nested clip validation.
- Added CPU final-frame compositing tests for every Live and Rig Lab content region.
- Preserved v0.7.21 training, anatomy, gait, terrain, checkpoint, and autosave semantics.

''' + text
    write(path, text)


def patch_repository_audit() -> None:
    path = "tools/repository_audit.cmake"
    text = read(path)
    text = replace_once(text,
        '''        docs/RUNNER_V0721_READABLE_TELEMETRY.md
        assets/ui/runner_icon_concept.svg''',
        '''        docs/RUNNER_V0721_READABLE_TELEMETRY.md
        docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md
        assets/ui/runner_icon_concept.svg''',
        "audit hotfix document")
    text = replace_once(text,
        '''        tests/v0721_rig_gait_tests.cpp
        src/locomotion_strategy.hpp''',
        '''        tests/v0721_rig_gait_tests.cpp
        tests/v0722_visible_frame_tests.cpp
        src/locomotion_strategy.hpp''',
        "audit visible frame test")
    text = replace_once(text,
        '''        src/training_explainer.hpp
        src/runner_icon.rc.in''',
        '''        src/training_explainer.hpp
        src/ui_render_contract.hpp
        src/ui_frame_probe.hpp
        src/runner_icon.rc.in''',
        "audit UI contract headers")
    text = text.replace("project(Runner VERSION 0.7.21 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.22 LANGUAGES CXX)")
    text = replace_once(text,
        '''        "RunnerV0721ReadableTelemetryTests"
        "RUNNER_V0720_UI_PREVIEW_ICON.md"''',
        '''        "RunnerV0721ReadableTelemetryTests"
        "RunnerV0722VisibleFrameTests"
        "RUNNER_V0720_UI_PREVIEW_ICON.md"''',
        "audit visible frame target")
    text = replace_once(text,
        '''        "RUNNER_V0721_READABLE_TELEMETRY.md"
        "runner_icon.rc")''',
        '''        "RUNNER_V0721_READABLE_TELEMETRY.md"
        "RUNNER_V0722_BLACK_FRAME_HOTFIX.md"
        "runner_icon.rc")''',
        "audit hotfix package document")
    text = replace_once(text,
        '''        "WALK-RIG-LAB-279"
        "WALK-RELEASE-283")''',
        '''        "WALK-RIG-LAB-279"
        "WALK-RELEASE-283"
        "WALK-BLACK-FRAME-284"
        "WALK-FRAME-DIAGNOSTIC-287"
        "WALK-RELEASE-289")''',
        "audit hotfix missions")
    insertion = '''
file(READ "${RUNNER_SOURCE_DIR}/src/app.cpp" app_text)
string(FIND "${app_text}" "Color{}" opaque_default_pos)
if(NOT opaque_default_pos EQUAL -1)
    message(FATAL_ERROR "Opaque default Color remains in application border rendering")
endif()
string(FIND "${app_text}" "ui_render::transparent_fill" transparent_fill_pos)
if(transparent_fill_pos EQUAL -1)
    message(FATAL_ERROR "Explicit transparent border-fill contract is not used")
endif()

'''
    text = replace_once(text,
        'file(GLOB release_notes "${RUNNER_SOURCE_DIR}/RELEASE_NOTES*.md")\n',
        insertion + 'file(GLOB release_notes "${RUNNER_SOURCE_DIR}/RELEASE_NOTES*.md")\n',
        "opaque mask source audit")
    text = replace_once(text,
        '''        .github/workflows/apply-v0721-rig-gait-repair.yml)''',
        '''        .github/workflows/apply-v0721-rig-gait-repair.yml
        tools/cache_v0722_black_frame.py
        tools/apply_v0722_black_frame_hotfix.py
        tools/v0722.cache-trigger
        tools/v0722.cache-trigger2
        tools/v0722.cache-trigger3
        .github/workflows/cache-v0722-black-frame.yml
        .github/workflows/apply-v0722-black-frame.yml)''',
        "audit hotfix temporary files")
    text = text.replace("Runner v0.7.21 repository hygiene passed",
        "Runner v0.7.22 repository hygiene passed")
    write(path, text)


def main() -> int:
    patch_app()
    patch_ui_tests()
    patch_cmake()
    patch_readme()
    patch_changelog()
    patch_repository_audit()
    print("Runner v0.7.22 black-frame hotfix applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
