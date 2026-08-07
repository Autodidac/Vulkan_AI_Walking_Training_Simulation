#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}: {old[:100]!r}")
    write(path, text.replace(old, new, 1))


# Centralized fill + true rounded-outline geometry. A transparent fill is never
# used as an eraser; border-only cards emit only the perimeter ring.
write("src/ui_render_contract.hpp", r'''#pragma once

#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace runner::ui_render
{
    inline constexpr Color transparent_fill{ 0.0f, 0.0f, 0.0f, 0.0f };

    [[nodiscard]] constexpr bool is_explicitly_transparent(Color color) noexcept
    {
        return color.a == 0.0f;
    }

    inline void fill_rounded_rect(render::Canvas& canvas, Vec2 position,
        Vec2 size, float radius, Color color)
    {
        if (size.x <= 0.0f || size.y <= 0.0f || color.a <= 0.0f)
            return;
        radius = std::clamp(radius, 0.0f, std::min(size.x, size.y) * 0.5f);
        const Vec2 minimum = position;
        const Vec2 maximum = position + size;
        if (radius <= 0.0f)
        {
            canvas.quad(minimum, maximum, color);
            return;
        }
        canvas.quad({ minimum.x + radius, minimum.y },
            { maximum.x - radius, maximum.y }, color);
        canvas.quad({ minimum.x, minimum.y + radius },
            { maximum.x, maximum.y - radius }, color);
        canvas.circle({ minimum.x + radius, minimum.y + radius }, radius, color, 12u);
        canvas.circle({ maximum.x - radius, minimum.y + radius }, radius, color, 12u);
        canvas.circle({ minimum.x + radius, maximum.y - radius }, radius, color, 12u);
        canvas.circle({ maximum.x - radius, maximum.y - radius }, radius, color, 12u);
    }

    namespace detail
    {
        inline void rounded_corner_stroke(render::Canvas& canvas, Vec2 center,
            float outer_radius, float inner_radius, float start_angle, Color color)
        {
            constexpr std::uint32_t segments = 8u;
            Vec2 previous_outer = center + Vec2{
                std::cos(start_angle) * outer_radius,
                std::sin(start_angle) * outer_radius };
            Vec2 previous_inner = center + Vec2{
                std::cos(start_angle) * inner_radius,
                std::sin(start_angle) * inner_radius };
            for (std::uint32_t segment = 1u; segment <= segments; ++segment)
            {
                const float angle = start_angle + (pi * 0.5f)
                    * static_cast<float>(segment) / static_cast<float>(segments);
                const Vec2 current_outer = center + Vec2{
                    std::cos(angle) * outer_radius,
                    std::sin(angle) * outer_radius };
                if (inner_radius > 0.0f)
                {
                    const Vec2 current_inner = center + Vec2{
                        std::cos(angle) * inner_radius,
                        std::sin(angle) * inner_radius };
                    canvas.triangle(previous_outer, current_outer, current_inner, color);
                    canvas.triangle(previous_outer, current_inner, previous_inner, color);
                    previous_inner = current_inner;
                }
                else
                {
                    canvas.triangle(center, previous_outer, current_outer, color);
                }
                previous_outer = current_outer;
            }
        }
    }

    inline void stroke_rounded_rect(render::Canvas& canvas, Vec2 position,
        Vec2 size, float radius, float border_width, Color color)
    {
        if (size.x <= 0.0f || size.y <= 0.0f
            || border_width <= 0.0f || color.a <= 0.0f)
            return;

        const float maximum_border = std::min(size.x, size.y) * 0.5f;
        border_width = std::clamp(border_width, 0.0f, maximum_border);
        radius = std::clamp(radius, 0.0f, std::min(size.x, size.y) * 0.5f);
        const Vec2 minimum = position;
        const Vec2 maximum = position + size;

        if (radius <= 0.0f)
        {
            canvas.quad(minimum, { maximum.x, minimum.y + border_width }, color);
            canvas.quad({ minimum.x, maximum.y - border_width }, maximum, color);
            canvas.quad({ minimum.x, minimum.y + border_width },
                { minimum.x + border_width, maximum.y - border_width }, color);
            canvas.quad({ maximum.x - border_width, minimum.y + border_width },
                { maximum.x, maximum.y - border_width }, color);
            return;
        }

        const float inner_radius = std::max(0.0f, radius - border_width);
        if (maximum.x - minimum.x > radius * 2.0f)
        {
            canvas.quad({ minimum.x + radius, minimum.y },
                { maximum.x - radius, minimum.y + border_width }, color);
            canvas.quad({ minimum.x + radius, maximum.y - border_width },
                { maximum.x - radius, maximum.y }, color);
        }
        if (maximum.y - minimum.y > radius * 2.0f)
        {
            canvas.quad({ minimum.x, minimum.y + radius },
                { minimum.x + border_width, maximum.y - radius }, color);
            canvas.quad({ maximum.x - border_width, minimum.y + radius },
                { maximum.x, maximum.y - radius }, color);
        }

        detail::rounded_corner_stroke(canvas,
            { minimum.x + radius, minimum.y + radius },
            radius, inner_radius, pi, color);
        detail::rounded_corner_stroke(canvas,
            { maximum.x - radius, minimum.y + radius },
            radius, inner_radius, -pi * 0.5f, color);
        detail::rounded_corner_stroke(canvas,
            { maximum.x - radius, maximum.y - radius },
            radius, inner_radius, 0.0f, color);
        detail::rounded_corner_stroke(canvas,
            { minimum.x + radius, maximum.y - radius },
            radius, inner_radius, pi * 0.5f, color);
    }

    inline void rounded_rect(render::Canvas& canvas, Vec2 position, Vec2 size,
        float radius, Color fill, Color outline = transparent_fill,
        float border_width = 0.0f)
    {
        fill_rounded_rect(canvas, position, size, radius, fill);
        stroke_rounded_rect(canvas, position, size, radius, border_width, outline);
    }
}
''')

old_helpers = r'''        void fill_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color color)
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
        }
'''
new_helpers = r'''        void fill_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color color)
        {
            ui_render::fill_rounded_rect(canvas, rect.position, rect.size, radius, color);
        }

        void add_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color fill,
            Color outline = {}, float border_width = 0.0f)
        {
            ui_render::rounded_rect(canvas, rect.position, rect.size,
                radius, fill, outline, border_width);
        }
'''
replace_once("src/app.cpp", old_helpers, new_helpers)

replace_once(
    "src/ui_frame_probe.hpp",
    "        return stats.sample_count > 0u\n"
    "            && stats.non_black_samples >= 3u\n"
    "            && stats.source_vertex_count >= 6u\n"
    "            && stats.source_color_buckets >= 3u;\n",
    "        return stats.sample_count > 0u\n"
    "            && stats.non_black_samples >= 3u\n"
    "            && stats.distinct_color_buckets >= 3u\n"
    "            && stats.source_vertex_count >= 6u\n"
    "            && stats.source_color_buckets >= 3u;\n")

old_test_path = ROOT / "tests/v0722_visible_frame_tests.cpp"
new_test_path = ROOT / "tests/v0723_gray_frame_tests.cpp"
if old_test_path.exists():
    old_test_path.rename(new_test_path)
test_text = new_test_path.read_text(encoding="utf-8")
test_text = test_text.replace("#include <cstdlib>\n", "#include <cmath>\n#include <cstdlib>\n", 1)
anchor = r'''    require(Color{}.a == 1.0f,
        "test fixture no longer proves that default Color is opaque");
'''
addition = anchor + r'''
    {
        render::Canvas border_canvas{};
        constexpr Color background{ 0.12f, 0.31f, 0.55f, 1.0f };
        constexpr Color outline{ 0.82f, 0.24f, 0.10f, 1.0f };
        border_canvas.quad({ 0.0f, 0.0f }, { 200.0f, 120.0f }, background);
        ui_render::rounded_rect(border_canvas, { 0.0f, 0.0f }, { 200.0f, 120.0f },
            18.0f, ui_render::transparent_fill, outline, 4.0f);
        const Color center = ui_frame_probe::sample_color(
            border_canvas.vertices(), { 100.0f, 60.0f });
        const Color edge = ui_frame_probe::sample_color(
            border_canvas.vertices(), { 2.0f, 60.0f });
        const auto close = [](float lhs, float rhs) noexcept
        {
            return std::abs(lhs - rhs) <= 1.0e-4f;
        };
        require(close(center.r, background.r) && close(center.g, background.g)
                && close(center.b, background.b),
            "border-only rounded rectangle changed the center composite");
        require(std::abs(edge.r - background.r) + std::abs(edge.g - background.g)
                + std::abs(edge.b - background.b) > 0.20f,
            "rounded outline did not reach the perimeter sample");
    }
'''
if test_text.count(anchor) != 1:
    raise RuntimeError("v0.7.23 test insertion anchor mismatch")
test_text = test_text.replace(anchor, addition, 1)
test_text = test_text.replace(
    "Runner v0.7.22 visible Live and Rig Lab frame tests passed",
    "Runner v0.7.23 rounded-outline and final-frame tests passed")
new_test_path.write_text(test_text, encoding="utf-8", newline="\n")

cmake = read("CMakeLists.txt")
cmake = cmake.replace("project(Runner VERSION 0.7.22 LANGUAGES CXX)",
                      "project(Runner VERSION 0.7.23 LANGUAGES CXX)", 1)
cmake = cmake.replace("Runner v0.7.22 icon generation failed",
                      "Runner v0.7.23 icon generation failed", 1)
post_doc = '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md"\n'
if cmake.count(post_doc) != 1:
    raise RuntimeError("CMake post-build v0.7.22 doc anchor mismatch")
cmake = cmake.replace(post_doc, post_doc
    + '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md"\n', 1)
install_doc = '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md"\n'
if cmake.count(install_doc) != 1:
    raise RuntimeError("CMake install v0.7.22 doc anchor mismatch")
cmake = cmake.replace(install_doc, install_doc
    + '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md"\n', 1)
old_test_block = r'''    add_executable(RunnerV0722VisibleFrameTests
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
new_test_block = r'''    add_executable(RunnerV0723GrayFrameTests
        tests/v0723_gray_frame_tests.cpp src/app.cpp src/canvas.cpp)
    target_link_libraries(RunnerV0723GrayFrameTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0723GrayFrameTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0723GrayFrameTests PRIVATE cxx_std_23)
    target_compile_definitions(RunnerV0723GrayFrameTests PRIVATE
        RUNNER_VERSION="${PROJECT_VERSION}")
    runner_enable_warnings(RunnerV0723GrayFrameTests)
    add_test(NAME Runner.V0723GrayFrames COMMAND RunnerV0723GrayFrameTests)
    set_tests_properties(Runner.V0723GrayFrames PROPERTIES TIMEOUT 120)
'''
if cmake.count(old_test_block) != 1:
    raise RuntimeError("CMake v0.7.22 test block mismatch")
cmake = cmake.replace(old_test_block, new_test_block, 1)
write("CMakeLists.txt", cmake)

write("docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md", r'''# Runner v0.7.23 true rounded-outline rendering hotfix

## Observed package failure

Runner v0.7.22 replaced an opaque black inset with a zero-alpha inset, but the helper still drew the complete outer card using the outline color first. Transparent blending cannot erase that prior write. On the sRGB Windows swapchain, the linear border color appeared as the uniform gray shown in the packaged eye test.

## Rendering contract

A filled rounded rectangle now renders its fill once and then adds an independent perimeter ring. A border-only rounded rectangle emits only four bounded straight strips and four tessellated quarter-ring corners. No transparent primitive is treated as an eraser, and no border path writes center geometry.

## Regression contract

The deterministic test renders a known colored background, overlays a border-only rounded rectangle, and samples the final composite. The center must remain exactly the background color and the perimeter must contain the outline. The complete Live world, dashboard, PIP, Rig Lab viewport, and all four Rig Lab pages must also retain multiple final visible colors after the entire draw order.

## Compatibility

This release changes presentation only. Training semantics remain `0x0007'2101`; policy dimensions, fixed rig anatomy, gait qualification, terrain, checkpoints, and `runner-v0721-*` autosaves are unchanged.
''')

readme = read("README.md")
readme = readme.replace("Runner 0.7.22 is", "Runner 0.7.23 is", 1)
doc_anchor = "- [`docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md`](docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md) documents the explicit transparent-border and final-frame visibility contract.\n"
if doc_anchor not in readme:
    raise RuntimeError("README v0.7.22 doc anchor missing")
readme = readme.replace(doc_anchor, doc_anchor
    + "- [`docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md`](docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md) documents the true rounded-outline and center-preservation contract.\n", 1)
section_anchor = "## v0.7.22 black-frame rendering hotfix\n"
new_section = r'''## v0.7.23 true rounded-outline rendering hotfix

- Replaces the fake outer-fill/transparent-inset border with actual bounded rounded perimeter geometry.
- Keeps the center of every border-only card untouched instead of covering it with the linear-space border color.
- Restores final-composite color-diversity checks so hidden source geometry cannot certify a flat black or gray frame.
- Tests the center and edge pixels directly, then checks Live, dashboard, PIP, Rig Lab viewport, and all four Rig Lab pages.
- Preserves v0.7.21 training, rig, gait, terrain, checkpoint, and autosave semantics.

'''
if readme.count(section_anchor) != 1:
    raise RuntimeError("README v0.7.22 section anchor mismatch")
readme = readme.replace(section_anchor, new_section + section_anchor, 1)
write("README.md", readme)

changelog = read("CHANGELOG.md")
if not changelog.startswith("## 0.7.23\n"):
    changelog = r'''## 0.7.23

- Replaced destructive rounded-card border overdraw with a true inset perimeter ring.
- Prevented border-only cards from writing any center geometry.
- Added direct final-composite center and edge sampling.
- Restored final color-diversity requirements for Live, dashboard, PIP, and all Rig Lab pages.
- Preserved v0.7.21 training/checkpoint semantics and moved equipment work to v0.7.24.

''' + changelog
write("CHANGELOG.md", changelog)

audit = read("tools/repository_audit.cmake")
audit = audit.replace(
    "        docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md\n",
    "        docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md\n"
    "        docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md\n", 1)
audit = audit.replace(
    "        tests/v0722_visible_frame_tests.cpp\n",
    "        tests/v0723_gray_frame_tests.cpp\n", 1)
audit = audit.replace(
    '        "project(Runner VERSION 0.7.22 LANGUAGES CXX)"\n',
    '        "project(Runner VERSION 0.7.23 LANGUAGES CXX)"\n', 1)
audit = audit.replace(
    '        "RunnerV0722VisibleFrameTests"\n',
    '        "RunnerV0723GrayFrameTests"\n', 1)
audit = audit.replace(
    '        "RUNNER_V0722_BLACK_FRAME_HOTFIX.md"\n',
    '        "RUNNER_V0722_BLACK_FRAME_HOTFIX.md"\n'
    '        "RUNNER_V0723_GRAY_FRAME_HOTFIX.md"\n', 1)
audit = audit.replace("CMake v0.7.21 contract missing", "CMake v0.7.23 contract missing")
mission_anchor = '        "WALK-RELEASE-289")\n'
if mission_anchor not in audit:
    raise RuntimeError("audit mission anchor missing")
audit = audit.replace(mission_anchor,
    '        "WALK-RELEASE-289"\n'
    '        "WALK-TRUE-OUTLINE-290"\n'
    '        "WALK-COMPOSITE-292"\n'
    '        "WALK-ALL-VIEWS-293"\n'
    '        "WALK-RELEASE-295")\n', 1)
audit = audit.replace("Mission cache v0.7.21 contract missing",
                      "Mission cache v0.7.23 contract missing")
app_check = r'''string(FIND "${app_text}" "ui_render::transparent_fill" transparent_fill_pos)
if(transparent_fill_pos EQUAL -1)
    message(FATAL_ERROR "Explicit transparent border-fill contract is not used")
endif()
'''
app_check_new = app_check + r'''string(FIND "${app_text}" "ui_render::rounded_rect" rounded_rect_pos)
if(rounded_rect_pos EQUAL -1)
    message(FATAL_ERROR "Application is not using the true rounded-outline helper")
endif()
file(READ "${RUNNER_SOURCE_DIR}/src/ui_render_contract.hpp" render_contract_text)
foreach(reference IN ITEMS "stroke_rounded_rect" "rounded_corner_stroke" "border_width")
    string(FIND "${render_contract_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "True rounded-outline contract missing: ${reference}")
    endif()
endforeach()
'''
if audit.count(app_check) != 1:
    raise RuntimeError("audit app check anchor mismatch")
audit = audit.replace(app_check, app_check_new, 1)
stale_anchor = "        .github/workflows/apply-v0722-black-frame.yml)\n"
if stale_anchor not in audit:
    raise RuntimeError("audit stale anchor missing")
audit = audit.replace(stale_anchor,
    "        .github/workflows/apply-v0722-black-frame.yml\n"
    "        tools/cache_v0723_gray_frame.py\n"
    "        tools/apply_v0723_gray_frame_hotfix.py\n"
    "        .github/workflows/cache-v0723-gray-frame.yml\n"
    "        .github/workflows/apply-v0723-gray-frame.yml)\n", 1)
audit = audit.replace("Runner v0.7.22 repository hygiene passed",
                      "Runner v0.7.23 repository hygiene passed", 1)
write("tools/repository_audit.cmake", audit)

# PR validation is rewritten rather than incrementally inheriting stale release checks.
write(".github/workflows/runner-pr-validation.yml", r'''name: Runner pull-request validation

on:
  pull_request:
    branches: [main]

permissions:
  contents: read

jobs:
  linux-gcc14:
    runs-on: ubuntu-24.04
    timeout-minutes: 120
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
      - name: Install deterministic build tools
        run: |
          sudo apt-get update
          sudo apt-get install -y g++-14 ninja-build python3
      - name: Source audit
        run: |
          set -euo pipefail
          git diff --check
          grep -F "project(Runner VERSION 0.7.23 LANGUAGES CXX)" CMakeLists.txt
          grep -F "WALK-TRUE-OUTLINE-290" missioncache.md
          grep -F "WALK-ROUNDING-291" missioncache.md
          grep -F "WALK-COMPOSITE-292" missioncache.md
          grep -F "WALK-ALL-VIEWS-293" missioncache.md
          grep -F "WALK-HOTFIX-COMPAT-294" missioncache.md
          grep -F "WALK-RELEASE-295" missioncache.md
          grep -F "training_semantics_version = 0x0007'2101u" src/ppo.hpp
          grep -F "ui_render::rounded_rect" src/app.cpp
          grep -F "stroke_rounded_rect" src/ui_render_contract.hpp
          grep -F "rounded_corner_stroke" src/ui_render_contract.hpp
          grep -F "distinct_color_buckets >= 3u" src/ui_frame_probe.hpp
          grep -F "RunnerV0723GrayFrameTests" CMakeLists.txt
          test -f tests/v0723_gray_frame_tests.cpp
          test -f docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md
          test -f .github/workflows/runner-v0723-release.yml
          test ! -e tests/v0722_visible_frame_tests.cpp
          test ! -e tools/cache_v0723_gray_frame.py
          test ! -e tools/apply_v0723_gray_frame_hotfix.py
          test ! -e .github/workflows/cache-v0723-gray-frame.yml
          test ! -e .github/workflows/apply-v0723-gray-frame.yml
      - name: Configure
        env:
          CC: gcc-14
          CXX: g++-14
        run: >-
          cmake -S . -B build/pr-linux -G Ninja
          -DRUNNER_BUILD_APP=OFF
          -DRUNNER_BUILD_TESTS=ON
          -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build/pr-linux --parallel
      - name: Test
        run: ctest --test-dir build/pr-linux --output-on-failure -V --timeout 1800

  windows-sdl3-vulkan:
    runs-on: windows-2025
    timeout-minutes: 210
    env:
      VCPKG_ROOT: ${{ github.workspace }}\vcpkg
      VCPKG_DEFAULT_BINARY_CACHE: ${{ github.workspace }}\vcpkg-bincache
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
      - uses: actions/checkout@v4
        with:
          repository: microsoft/vcpkg
          ref: d015e31e90838a4c9dfa3eed45979bc70d9357fc
          path: vcpkg
      - uses: actions/cache@v4
        with:
          path: vcpkg-bincache
          key: runner-v0723-pr-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}
          restore-keys: |
            runner-v0723-pr-${{ runner.os }}-
            runner-v0722-pr-${{ runner.os }}-
            runner-v0721-pr-${{ runner.os }}-
      - name: Bootstrap vcpkg
        shell: cmd
        run: call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics
      - name: Configure and build
        shell: pwsh
        run: |
          New-Item -ItemType Directory -Force $env:VCPKG_DEFAULT_BINARY_CACHE | Out-Null
          cmake --preset windows-release --fresh
          if ($LASTEXITCODE -ne 0) { throw 'Windows configure failed' }
          cmake --build --preset windows-release --parallel
          if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
      - name: Test
        shell: pwsh
        run: |
          ctest --preset windows-release --output-on-failure -V --timeout 1800
          if ($LASTEXITCODE -ne 0) { throw 'Windows tests failed' }
          & 'build/windows-release/Release/RunnerV0723GrayFrameTests.exe'
          if ($LASTEXITCODE -ne 0) { throw 'Rounded-outline regression test failed' }
      - name: Runtime diagnostics
        shell: pwsh
        run: |
          $exe = (Resolve-Path 'build/windows-release/Release/Runner.exe').Path
          $version = (& $exe --version | Out-String).Trim()
          if ($version -ne 'Runner 0.7.23') { throw "Version mismatch: $version" }
          & $exe --diagnose-ui
          if ($LASTEXITCODE -ne 0) { throw 'Final-composite UI diagnostic failed' }
          & $exe --diagnose-camera
          if ($LASTEXITCODE -ne 0) { throw 'Camera diagnostic failed' }
          & $exe --diagnose-package
          if ($LASTEXITCODE -ne 0) { throw 'Package diagnostic failed' }
          & $exe --diagnose-acceptance
          if ($LASTEXITCODE -ne 0) { throw 'Acceptance diagnostic failed' }
''')

# Start from the already-proven v0.7.22 publisher and update only the release contract.
old_workflow = subprocess.check_output([
    "git", "show",
    "5186652c709afe726f3e648a82bc04907670e0f7:.github/workflows/runner-v0722-release.yml"
], cwd=ROOT, text=True)
workflow = old_workflow.replace("0.7.22", "0.7.23")
workflow = workflow.replace("v0722", "v0723").replace("V0722", "V0723")
for old, new in {
    "WALK-BLACK-FRAME-284": "WALK-TRUE-OUTLINE-290",
    "WALK-ALL-VIEWS-285": "WALK-ROUNDING-291",
    "WALK-CLIP-TEST-286": "WALK-COMPOSITE-292",
    "WALK-FRAME-DIAGNOSTIC-287": "WALK-ALL-VIEWS-293",
    "WALK-HOTFIX-COMPAT-288": "WALK-HOTFIX-COMPAT-294",
    "WALK-RELEASE-289": "WALK-RELEASE-295",
    "RunnerV0723VisibleFrameTests": "RunnerV0723GrayFrameTests",
    "tests/v0723_visible_frame_tests.cpp": "tests/v0723_gray_frame_tests.cpp",
    "RUNNER_V0723_BLACK_FRAME_HOTFIX.md": "RUNNER_V0723_GRAY_FRAME_HOTFIX.md",
    "# Runner v0.7.23 black-frame rendering hotfix": "# Runner v0.7.23 true rounded-outline rendering hotfix",
    "for number in range(284, 290):": "for number in range(290, 296):",
    "gh pr close 75": "gh pr close 76",
    "agent%2Fv0723-black-frame-hotfix": "agent%2Fv0723-gray-frame-hotfix",
}.items():
    workflow = workflow.replace(old, new)
workflow = workflow.replace(
    "          grep -F 'ui_render::transparent_fill' src/app.cpp\n",
    "          grep -F 'ui_render::rounded_rect' src/app.cpp\n"
    "          grep -F 'stroke_rounded_rect' src/ui_render_contract.hpp\n"
    "          grep -F 'distinct_color_buckets >= 3u' src/ui_frame_probe.hpp\n")
workflow = workflow.replace(
    "          Runner v0.7.23 is the black-frame rendering hotfix.\n\n"
    "          - Restores the large Live world, right-side dashboard, training PIP, Rig Lab viewport, and all four Rig Lab pages.\n"
    "          - Fixes the exact opaque border-fill defect that drew correct content and then covered it with a black rectangle.\n"
    "          - Replaces ambiguous default UI colors with an explicit zero-alpha border-only fill contract.\n"
    "          - Makes clipping tests require real emitted triangles and validates nested clip intersections.\n"
    "          - Adds CPU final-frame compositing to the test suite and packaged `--diagnose-ui` path.\n"
    "          - Preserves every v0.7.21 controller, anatomy, gait, terrain, checkpoint, and autosave semantic.\n",
    "          Runner v0.7.23 is the true rounded-outline rendering hotfix.\n\n"
    "          - Replaces full-card outline overdraw with an actual inset rounded perimeter ring.\n"
    "          - Proves a border-only card leaves its center composite unchanged while drawing the requested edge.\n"
    "          - Restores final-composite color-diversity checks for Live, dashboard, PIP, Rig Lab viewport, and all four Rig Lab pages.\n"
    "          - Keeps clipping, DPI, readable telemetry, fixed rigs, and true side-view gait intact.\n"
    "          - Preserves v0.7.21 policy dimensions, training semantics, checkpoints, terrain, and autosave compatibility.\n")
write(".github/workflows/runner-v0723-release.yml", workflow)

print("Runner v0.7.23 true rounded-outline hotfix applied")
