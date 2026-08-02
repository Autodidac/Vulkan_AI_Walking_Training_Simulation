from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str, marker: str | None = None) -> None:
    text = read(path)
    if marker is not None and marker in text:
        return
    if old not in text:
        raise RuntimeError(f"missing replacement target in {path}: {old[:100]!r}")
    write(path, text.replace(old, new, 1))


def replace_regex(path: str, pattern: str, replacement: str, marker: str | None = None) -> None:
    text = read(path)
    if marker is not None and marker in text:
        return
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(f"regex matched {count} times in {path}: {pattern[:100]!r}")
    write(path, updated)


# Remove the former project brand from every owned text file first. The external
# GUI dependency is removed below, so the repository can enforce a literal
# case-insensitive zero-match branding gate.
for path in ROOT.rglob("*"):
    if not path.is_file() or ".git" in path.parts or path == Path(__file__):
        continue
    try:
        text = path.read_text(encoding="utf-8")
    except (UnicodeDecodeError, OSError):
        continue
    updated = text
    updated = updated.replace("EPOCHRUNNER", "RUNNER")
    updated = updated.replace("EpochRunner", "Runner")
    updated = updated.replace("epochrunner", "runner")
    updated = updated.replace("EPOCHAUTONOMY", "RUNAUTONOMY")
    updated = updated.replace("EPOCHRIG", "RUNRIG")
    updated = updated.replace("EPOCHGUI", "RUNNERGUI")
    updated = updated.replace("EpochGui", "RunnerGui")
    updated = updated.replace("epoch.gui", "runner.gui")
    updated = updated.replace("epochengine", "runnerengine")
    updated = updated.replace("Epoch", "Runner")
    updated = updated.replace("epoch", "runner")
    updated = updated.replace("EPOCH", "RUNNER")
    updated = updated.replace(".runnerrig", ".rig")
    updated = updated.replace("RUNNERRIG", "RUNRIG")
    if updated != text:
        path.write_text(updated, encoding="utf-8", newline="\n")

# Local bitmap font replaces the branded external GUI dependency.
write("src/ui_font.hpp", r'''#pragma once

#include "math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace runner::ui_font
{
    inline constexpr std::uint32_t glyph_width = 5;
    inline constexpr std::uint32_t glyph_height = 7;
    inline constexpr std::uint32_t glyph_advance = 6;
    inline constexpr std::uint32_t line_advance = 9;

    struct BitmapGlyph
    {
        std::array<std::uint8_t, glyph_height> rows{};
    };

    [[nodiscard]] constexpr BitmapGlyph default_glyph(char character) noexcept
    {
        char c = character;
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
        switch (c)
        {
        case 'A': return { { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } };
        case 'B': return { { 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e } };
        case 'C': return { { 0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f } };
        case 'D': return { { 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e } };
        case 'E': return { { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f } };
        case 'F': return { { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 } };
        case 'G': return { { 0x0f, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0f } };
        case 'H': return { { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } };
        case 'I': return { { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f } };
        case 'J': return { { 0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c } };
        case 'K': return { { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } };
        case 'L': return { { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f } };
        case 'M': return { { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 } };
        case 'N': return { { 0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11 } };
        case 'O': return { { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e } };
        case 'P': return { { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 } };
        case 'Q': return { { 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d } };
        case 'R': return { { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 } };
        case 'S': return { { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e } };
        case 'T': return { { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } };
        case 'U': return { { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e } };
        case 'V': return { { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 } };
        case 'W': return { { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a } };
        case 'X': return { { 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 } };
        case 'Y': return { { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 } };
        case 'Z': return { { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f } };
        case '0': return { { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e } };
        case '1': return { { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e } };
        case '2': return { { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f } };
        case '3': return { { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e } };
        case '4': return { { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 } };
        case '5': return { { 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e } };
        case '6': return { { 0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e } };
        case '7': return { { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } };
        case '8': return { { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e } };
        case '9': return { { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e } };
        case '-': return { { 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00 } };
        case '.': return { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06 } };
        case ':': return { { 0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00 } };
        case '/': return { { 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10 } };
        case '_': return { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f } };
        case '+': return { { 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00 } };
        case '!': return { { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 } };
        case '?': return { { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 } };
        case ',': return { { 0x00, 0x00, 0x00, 0x00, 0x06, 0x04, 0x08 } };
        case ';': return { { 0x00, 0x06, 0x06, 0x00, 0x06, 0x04, 0x08 } };
        case '=': return { { 0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00, 0x00 } };
        case '(': return { { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 } };
        case ')': return { { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 } };
        case '[': return { { 0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e } };
        case ']': return { { 0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e } };
        case ' ': return {};
        default: return { { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 } };
        }
    }

    [[nodiscard]] constexpr bool pixel_on(const BitmapGlyph& glyph,
        std::uint32_t column, std::uint32_t row) noexcept
    {
        if (column >= glyph_width || row >= glyph_height)
            return false;
        const std::uint8_t mask = static_cast<std::uint8_t>(1U << (glyph_width - 1U - column));
        return (glyph.rows[row] & mask) != 0;
    }

    [[nodiscard]] constexpr Vec2 measure_text(std::string_view text, float scale = 1.0f) noexcept
    {
        if (scale <= 0.0f)
            return {};
        std::size_t column{};
        std::size_t maximum_column{};
        std::size_t line_count{ 1 };
        for (const char character : text)
        {
            if (character == '\n')
            {
                maximum_column = maximum_column < column ? column : maximum_column;
                column = 0;
                ++line_count;
            }
            else
                ++column;
        }
        maximum_column = maximum_column < column ? column : maximum_column;
        const float width = maximum_column == 0 ? 0.0f
            : static_cast<float>((maximum_column - 1U) * glyph_advance + glyph_width) * scale;
        const float height = static_cast<float>((line_count - 1U) * line_advance + glyph_height) * scale;
        return { width, height };
    }
}
''')

write("CMakeLists.txt", r'''cmake_minimum_required(VERSION 3.28)

if(MSVC)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
else()
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

project(Runner VERSION 0.7.4 LANGUAGES CXX)

option(RUNNER_BUILD_APP "Build the SDL3/Vulkan Runner application." ON)
option(RUNNER_BUILD_TESTS "Build deterministic core tests." ON)
option(RUNNER_ENABLE_TSAN "Enable ThreadSanitizer for CPU-only tests." OFF)

find_package(Threads REQUIRED)

add_library(RunnerCore STATIC
    src/simulation.cpp
    src/ppo_network.cpp
    src/ppo_trainer.cpp
    src/ppo_parallel.cpp
    src/self_imitation.cpp
    src/training_checkpoint.cpp
    src/autonomy_runtime.cpp
    src/autonomy_commands.cpp
    src/autonomy_curriculum.cpp
    src/autonomy_persistence.cpp
)
add_library(Runner::Core ALIAS RunnerCore)
target_include_directories(RunnerCore PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")
target_link_libraries(RunnerCore PUBLIC Threads::Threads)
target_compile_features(RunnerCore PUBLIC cxx_std_23)
set_target_properties(RunnerCore PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

function(runner_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
    endif()
endfunction()
runner_enable_warnings(RunnerCore)

if(RUNNER_ENABLE_TSAN AND NOT MSVC)
    target_compile_options(RunnerCore PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(RunnerCore PRIVATE -fsanitize=thread)
endif()

if(RUNNER_BUILD_APP)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(Vulkan 1.3 REQUIRED)
    find_package(unofficial-shaderc CONFIG REQUIRED)

    add_executable(RunnerShaderCompiler tools/shader_compiler.cpp)
    target_link_libraries(RunnerShaderCompiler PRIVATE unofficial::shaderc::shaderc)
    target_compile_features(RunnerShaderCompiler PRIVATE cxx_std_23)
    set_target_properties(RunnerShaderCompiler PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    runner_enable_warnings(RunnerShaderCompiler)

    set(RUNNER_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    set(RUNNER_SHADER_OUTPUTS)
    foreach(shader IN ITEMS flat.vert flat.frag)
        set(source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}")
        set(output "${RUNNER_SHADER_OUTPUT_DIR}/${shader}.spv")
        add_custom_command(
            OUTPUT "${output}"
            COMMAND "$<TARGET_FILE:RunnerShaderCompiler>" "${source}" "${output}"
            DEPENDS "${source}" RunnerShaderCompiler
            VERBATIM
            COMMENT "Compiling ${shader} with vcpkg shaderc")
        list(APPEND RUNNER_SHADER_OUTPUTS "${output}")
    endforeach()
    add_custom_target(RunnerShaders DEPENDS ${RUNNER_SHADER_OUTPUTS})

    add_executable(Runner
        src/main.cpp src/app.cpp src/canvas.cpp src/renderer.cpp
        src/app.hpp src/renderer.hpp src/math.hpp src/ui_font.hpp)
    add_dependencies(Runner RunnerShaders)
    target_link_libraries(Runner PRIVATE Runner::Core SDL3::SDL3 Vulkan::Vulkan)
    target_compile_features(Runner PRIVATE cxx_std_23)
    target_compile_definitions(Runner PRIVATE
        RUNNER_VERSION="${PROJECT_VERSION}"
        RUNNER_SHADER_DIRECTORY="shaders"
        RUNNER_ASSET_DIRECTORY="assets")
    set_target_properties(Runner PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Runner>")
    runner_enable_warnings(Runner)

    add_custom_command(TARGET Runner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:Runner>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${RUNNER_SHADER_OUTPUT_DIR}" "$<TARGET_FILE_DIR:Runner>/shaders"
        VERBATIM)

    install(TARGETS Runner RUNTIME DESTINATION .)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/run.bat" DESTINATION .)
    install(DIRECTORY "${RUNNER_SHADER_OUTPUT_DIR}/" DESTINATION shaders)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION assets)
endif()

if(RUNNER_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_executable(RunnerCoreTests tests/core_tests.cpp)
    target_link_libraries(RunnerCoreTests PRIVATE Runner::Core)
    target_compile_features(RunnerCoreTests PRIVATE cxx_std_23)
    set_target_properties(RunnerCoreTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    runner_enable_warnings(RunnerCoreTests)
    add_test(NAME Runner.Core COMMAND RunnerCoreTests)

    add_executable(RunnerConcurrencyBenchmark tests/concurrency_benchmark.cpp)
    target_link_libraries(RunnerConcurrencyBenchmark PRIVATE Runner::Core)
    target_compile_features(RunnerConcurrencyBenchmark PRIVATE cxx_std_23)
    set_target_properties(RunnerConcurrencyBenchmark PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    runner_enable_warnings(RunnerConcurrencyBenchmark)
    add_test(NAME Runner.ConcurrencyBenchmark COMMAND RunnerConcurrencyBenchmark)
    set_tests_properties(Runner.ConcurrencyBenchmark PROPERTIES TIMEOUT 45)

    add_executable(RunnerRuntimePipelineTests tests/runtime_pipeline_tests.cpp)
    target_link_libraries(RunnerRuntimePipelineTests PRIVATE Runner::Core)
    target_compile_features(RunnerRuntimePipelineTests PRIVATE cxx_std_23)
    set_target_properties(RunnerRuntimePipelineTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    runner_enable_warnings(RunnerRuntimePipelineTests)
    if(RUNNER_ENABLE_TSAN AND NOT MSVC)
        target_compile_options(RunnerRuntimePipelineTests PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(RunnerRuntimePipelineTests PRIVATE -fsanitize=thread)
    endif()
    add_test(NAME Runner.RuntimePipeline COMMAND RunnerRuntimePipelineTests)
    set_tests_properties(Runner.RuntimePipeline PROPERTIES TIMEOUT 90)

    if(TARGET Runner)
        add_test(NAME Runner.PackageLayout COMMAND $<TARGET_FILE:Runner> --diagnose-package)
        set_tests_properties(Runner.PackageLayout PROPERTIES TIMEOUT 30)
    endif()
endif()
''')

# App now uses the local font and simple robust panel geometry.
app = read("src/app.cpp")
app = app.replace('#include "ui_layout.hpp"', '#include "ui_layout.hpp"\n#include "ui_font.hpp"')
app = re.sub(r'\nimport runner\.gui;\nimport runner\.gui\.font;\nimport runner\.gui\.rounded_rect;\n', '\n', app)
app = re.sub(r'\n    namespace gui = runnerengine::gui_lib;\n    namespace font = runnerengine::gui_lib::font;\n    namespace rounded = runnerengine::gui_lib::rounded_rect;\n', '\n    namespace font = ui_font;\n', app)
app = re.sub(r'\n        \[\[nodiscard\]\] gui::Rect to_gui\(Rect rect\) noexcept\n        \{.*?\n        \}\n', '\n', app, count=1, flags=re.DOTALL)
app = re.sub(
    r'        void add_rounded_rect\(render::Canvas& canvas, Rect rect, float radius, Color fill,\n            Color outline = \{\}, float border_width = 0\.0f\)\n        \{.*?\n        \}\n\n        void add_text',
    '''        void add_rounded_rect(render::Canvas& canvas, Rect rect, float radius, Color fill,
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
        }

        void add_text''',
    app, count=1, flags=re.DOTALL)
app = app.replace('"RUNNER RUNNER v" RUNNER_VERSION', '"RUNNER v" RUNNER_VERSION')
app = app.replace('"SAND-SIM ENEMY LOCOMOTION LAB"', '"AUTONOMOUS PHYSICS LOCOMOTION LAB"')
app = app.replace('"SAND-SIM ENEMY TRAINER"', '"AUTONOMOUS RIG TRAINER"')
app = app.replace('"HAZARD: {}", sim::course_feature_name(feature.kind)',
                  'feature.kind == sim::CourseFeatureKind::duck_press\n                        ? std::format("TRAINER: {}", sim::course_feature_name(feature.kind))\n                        : std::format("HAZARD: {}", sim::course_feature_name(feature.kind))')
write("src/app.cpp", app)

# Version and persistence formats are intentionally incompatible with v0.7.3.
for path in ("vcpkg.json", "src/main.cpp", "src/app.cpp", "src/autonomy_commands.cpp",
             "src/autonomy_persistence.cpp", "src/ppo.hpp", "README.md", "run.bat"):
    text = read(path)
    text = text.replace("0.7.3", "0.7.4").replace("v073", "v074")
    text = text.replace("0x0007'0300u", "0x0007'0400u")
    text = text.replace("RUNAUTONOMY 6", "RUNAUTONOMY 7")
    text = text.replace("version != 6", "version != 7")
    write(path, text)

# Stage two is a stationary compression lesson, not a moving low bar.
for path in ("src/simulation.hpp", "src/simulation.cpp", "src/ppo.hpp", "src/ppo_trainer.cpp",
             "src/ppo_parallel.cpp", "src/autonomy_curriculum.cpp", "src/app.cpp",
             "tests/core_tests.cpp", "tests/runtime_pipeline_tests.cpp"):
    if not (ROOT / path).exists():
        continue
    text = read(path).replace("CourseStage::walk", "CourseStage::duck_press")
    write(path, text)
replace_once("src/simulation.hpp", "        walk,", "        duck_press,")
replace_once("src/simulation.hpp", 'case CourseStage::duck_press: return "2. LOW BAR DUCK / RECOVER";',
             'case CourseStage::duck_press: return "2. PRESS DUCK / HOLD / RECOVER";')
replace_once("src/simulation.hpp", 'case CourseStage::hurdles: return "5. MOVING DUCK / JUMP";',
             'case CourseStage::hurdles: return "5. MOVING LOW BAR / HURDLE";')

# Add a deterministic press profile and feature type.
replace_once("src/simulation.hpp",
'''        hurdle,
        overhead_bar,
        moving_hazard,''',
'''        hurdle,
        overhead_bar,
        duck_press,
        moving_hazard,''')
replace_once("src/simulation.hpp",
'''        case CourseFeatureKind::overhead_bar: return "LOW BAR";
        case CourseFeatureKind::moving_hazard:''',
'''        case CourseFeatureKind::overhead_bar: return "LOW BAR";
        case CourseFeatureKind::duck_press: return "DUCK PRESS";
        case CourseFeatureKind::moving_hazard:''')
text = read("src/simulation.hpp")
text = text.replace('''        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
            return feature.half_extent.x;''',
'''        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
        case CourseFeatureKind::duck_press:
            return feature.half_extent.x;''')
text = text.replace('''        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
            return feature.center.y + feature.half_extent.y;''',
'''        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
        case CourseFeatureKind::duck_press:
            return feature.center.y + feature.half_extent.y;''')
text = text.replace('''        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
            return std::max(feature.half_extent.x, feature.half_extent.y);''',
'''        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
        case CourseFeatureKind::duck_press:
            return std::max(feature.half_extent.x, feature.half_extent.y);''')
write("src/simulation.hpp", text)
replace_once("src/simulation.hpp",
'''    [[nodiscard]] inline bool hazard_quiver_motion(float distance_ahead, float root_speed,''',
'''    struct DuckPressProfile
    {
        float bottom_y{};
        float vertical_velocity{};
        bool descending{};
        bool holding{};
        bool retracting{};
    };

    [[nodiscard]] inline DuckPressProfile duck_press_profile(float elapsed_seconds,
        float difficulty, float standing_head_top) noexcept
    {
        constexpr float settle_end = 2.50f;
        constexpr float descend_end = 5.00f;
        constexpr float hold_end = 7.00f;
        constexpr float retract_end = 9.50f;
        constexpr float cycle = 11.0f;
        float local = std::fmod(std::max(0.0f, elapsed_seconds), cycle);
        if (local < 0.0f)
            local += cycle;
        const float start = standing_head_top + 1.10f;
        const float target = standing_head_top - (0.78f + clamp(difficulty, 0.0f, 1.0f) * 0.20f);
        if (local < settle_end)
            return { start, 0.0f, false, false, false };
        if (local < descend_end)
        {
            const float t = (local - settle_end) / (descend_end - settle_end);
            const float smooth = t * t * (3.0f - 2.0f * t);
            const float derivative = 6.0f * t * (1.0f - t) / (descend_end - settle_end);
            return { lerp(start, target, smooth), (target - start) * derivative, true, false, false };
        }
        if (local < hold_end)
            return { target, 0.0f, false, true, false };
        if (local < retract_end)
        {
            const float t = (local - hold_end) / (retract_end - hold_end);
            const float smooth = t * t * (3.0f - 2.0f * t);
            const float derivative = 6.0f * t * (1.0f - t) / (retract_end - hold_end);
            return { lerp(target, start, smooth), (start - target) * derivative, false, false, true };
        }
        return { start, 0.0f, false, false, false };
    }

    [[nodiscard]] inline bool hazard_quiver_motion(float distance_ahead, float root_speed,''')

# Scheduled stage and switch completeness.
replace_once("src/simulation.hpp",
'''        if (stage == CourseStage::duck_press)
            return CourseFeatureKind::overhead_bar;''',
'''        if (stage == CourseStage::duck_press)
            return CourseFeatureKind::duck_press;''')
replace_once("src/simulation.hpp",
'''        hazard_quiver
    };''',
'''        hazard_quiver,
        robotic_torso_swing,
        press_penetration
    };''')
replace_once("src/simulation.hpp",
'''        case InvalidMotion::hazard_quiver: return "HAZARD QUIVER / NO LEG LIFT";
        }''',
'''        case InvalidMotion::hazard_quiver: return "HAZARD QUIVER / NO LEG LIFT";
        case InvalidMotion::robotic_torso_swing: return "ROBOTIC TORSO / SHOULDER SWING";
        case InvalidMotion::press_penetration: return "DUCK PRESS PENETRATION";
        }''')

# Public telemetry and state for the press lesson.
replace_once("src/simulation.hpp",
'''        [[nodiscard]] float duck_clearance_margin() const noexcept
        {
            return duck_clearance_margin_;
        }''',
'''        [[nodiscard]] float duck_clearance_margin() const noexcept
        {
            return duck_clearance_margin_;
        }
        [[nodiscard]] bool duck_press_contact() const noexcept { return duck_press_contact_this_step_; }
        [[nodiscard]] bool duck_press_completed() const noexcept { return duck_press_completed_; }
        [[nodiscard]] float duck_press_penetration() const noexcept { return duck_press_max_penetration_; }
        [[nodiscard]] float torso_swing_seconds() const noexcept { return torso_swing_seconds_; }''')
replace_once("src/simulation.hpp",
'''        float duck_clearance_margin_{};
        float current_duck_hold_seconds_{};''',
'''        float duck_clearance_margin_{};
        float duck_press_hold_seconds_{};
        float duck_press_max_penetration_{};
        float torso_swing_seconds_{};
        float current_duck_hold_seconds_{};''')
replace_once("src/simulation.hpp",
'''        bool duck_cycle_qualified_{};
        float current_airborne_rotation_{};''',
'''        bool duck_cycle_qualified_{};
        bool duck_press_contact_this_step_{};
        bool duck_press_contact_seen_{};
        bool duck_press_hold_qualified_{};
        bool duck_press_completed_{};
        float current_airborne_rotation_{};''')

# Replace the moving stage-two obstacle with the overhead platen.
replace_regex("src/simulation.cpp",
    r'''        if \(course_stage_ == CourseStage::duck_press\)\n        \{.*?\n            return;\n        \}\n        const int first_sequence''',
'''        if (course_stage_ == CourseStage::duck_press)
        {
            float minimum_x = 0.0f;
            float maximum_x = 0.0f;
            if (!blueprint_.nodes.empty())
            {
                minimum_x = blueprint_.nodes.front().x;
                maximum_x = minimum_x;
                for (const Vec2 node : blueprint_.nodes)
                {
                    minimum_x = std::min(minimum_x, node.x);
                    maximum_x = std::max(maximum_x, node.x);
                }
            }
            const float half_width = std::max(1.45f,
                (maximum_x - minimum_x) * 0.5f + 0.65f);
            const float rest_head_top = valid_node(blueprint_.head_node)
                ? blueprint_.nodes[blueprint_.head_node].y
                    + particles_[blueprint_.head_node].radius
                : 4.30f;
            const DuckPressProfile profile = duck_press_profile(
                elapsed_seconds_, course_difficulty_, rest_head_top);
            constexpr float half_height = 0.16f;
            course_features_.push_back({
                CourseFeatureKind::duck_press,
                { root_x, profile.bottom_y + half_height },
                { half_width, half_height }, 0.0f,
                { 0.0f, profile.vertical_velocity }, -2
            });
            return;
        }
        const int first_sequence''')

# Give later moving hazards enough observation and preparation distance.
replace_once("src/simulation.cpp",
'''            const float x = course_feature_world_x(sequence, progress);
            const float ground = ground_height_at(x);''',
'''            const float x = course_feature_world_x(sequence, progress);
            const float minimum_approach = course_stage_ == CourseStage::hurdles ? 6.5f : 5.0f;
            if ((course_stage_ == CourseStage::hurdles
                    || course_stage_ == CourseStage::moving_hazards)
                && x < root_x + minimum_approach)
                continue;
            const float ground = ground_height_at(x);''')
replace_once("src/simulation.cpp",
'''            case CourseFeatureKind::overhead_bar:
            {
                const float clearance''',
'''            case CourseFeatureKind::duck_press:
                break;
            case CourseFeatureKind::overhead_bar:
            {
                const float clearance''')

# One-way underside collision: the platen can push down but can never pass through the body.
replace_once("src/simulation.cpp",
'''            for (const CourseFeature& feature : course_features_)
            {
                if (feature.kind == CourseFeatureKind::moving_hazard''',
'''            for (const CourseFeature& feature : course_features_)
            {
                if (feature.kind == CourseFeatureKind::duck_press)
                {
                    const float left = feature.center.x - feature.half_extent.x;
                    const float right = feature.center.x + feature.half_extent.x;
                    const float bottom = feature.center.y - feature.half_extent.y;
                    const float top = feature.center.y + feature.half_extent.y;
                    const bool horizontal_overlap = particle.position.x + particle.radius > left
                        && particle.position.x - particle.radius < right;
                    const bool vertical_overlap = particle.position.y + particle.radius > bottom
                        && particle.position.y - particle.radius < top;
                    if (!horizontal_overlap || !vertical_overlap)
                        continue;
                    const float penetration = particle.position.y + particle.radius - bottom;
                    duck_press_max_penetration_ = std::max(duck_press_max_penetration_, penetration);
                    if (penetration > 0.0f)
                    {
                        const Vec2 correction{ 0.0f, -penetration };
                        particle.position += correction;
                        particle.previous += correction * 0.18f;
                        duck_press_contact_this_step_ = true;
                    }
                    continue;
                }
                if (feature.kind == CourseFeatureKind::moving_hazard''')

# Reset and frame-local press state.
replace_once("src/simulation.cpp",
'''        duck_clearance_margin_ = 0.0f;
        current_duck_hold_seconds_ = 0.0f;''',
'''        duck_clearance_margin_ = 0.0f;
        duck_press_hold_seconds_ = 0.0f;
        duck_press_max_penetration_ = 0.0f;
        torso_swing_seconds_ = 0.0f;
        current_duck_hold_seconds_ = 0.0f;''')
replace_once("src/simulation.cpp",
'''        duck_cycle_qualified_ = false;
        current_airborne_rotation_ = 0.0f;''',
'''        duck_cycle_qualified_ = false;
        duck_press_contact_this_step_ = false;
        duck_press_contact_seen_ = false;
        duck_press_hold_qualified_ = false;
        duck_press_completed_ = false;
        current_airborne_rotation_ = 0.0f;''')
replace_once("src/simulation.cpp",
'''        collided_this_step_ = false;
        rebuild_course_features();''',
'''        collided_this_step_ = false;
        duck_press_contact_this_step_ = false;
        duck_press_max_penetration_ = 0.0f;
        rebuild_course_features();''')
replace_once("src/simulation.cpp",
'''        if (elapsed_seconds_ >= 8.00f && !body_integrity_valid())
            invalidate(InvalidMotion::collapsed_posture);''',
'''        if (elapsed_seconds_ >= 8.00f && !body_integrity_valid())
            invalidate(InvalidMotion::collapsed_posture);
        if (duck_press_max_penetration_ > 0.18f)
            invalidate(InvalidMotion::press_penetration);''')

# Press proximity is vertical, while later low bars remain horizontally observed.
replace_once("src/simulation.cpp",
'''            if (feature.kind != CourseFeatureKind::overhead_bar)
                continue;
            const float dx = feature.center.x - root_x;
            const float weight = duck_obstacle_approach_weight(dx);
            if (weight <= duck_obstacle_weight_)
                continue;
            duck_obstacle_weight_ = weight;
            const float bar_bottom = feature.center.y - feature.half_extent.y;
            const float head_top = valid_node(blueprint_.head_node)
                ? particles_[blueprint_.head_node].position.y
                    + particles_[blueprint_.head_node].radius
                : bar_bottom;
            duck_clearance_margin_ = bar_bottom - head_top;''',
'''            if (feature.kind != CourseFeatureKind::overhead_bar
                && feature.kind != CourseFeatureKind::duck_press)
                continue;
            const float bar_bottom = feature.center.y - feature.half_extent.y;
            const float head_top = valid_node(blueprint_.head_node)
                ? particles_[blueprint_.head_node].position.y
                    + particles_[blueprint_.head_node].radius
                : bar_bottom;
            const float clearance = bar_bottom - head_top;
            const float weight = feature.kind == CourseFeatureKind::duck_press
                ? clamp((1.10f - clearance) / 1.10f, 0.0f, 1.0f)
                : duck_obstacle_approach_weight(feature.center.x - root_x);
            if (weight <= duck_obstacle_weight_)
                continue;
            duck_obstacle_weight_ = weight;
            duck_clearance_margin_ = clearance;''')

# A press cycle qualifies only after real contact, a held crouch, retraction, and stable recovery.
replace_regex("src/simulation.cpp",
    r'''        if \(duck_active_\)\n        \{\n            current_duck_hold_seconds_ \+= dt;.*?\n        \}\n\n        const bool collapsed_balance_posture''',
'''        if (course_stage_ == CourseStage::duck_press)
        {
            if (duck_press_contact_this_step_)
                duck_press_contact_seen_ = true;
            if (duck_press_contact_this_step_ && duck_active_
                && duck_clearance_margin_ >= -0.025f && body_integrity_valid())
            {
                duck_press_hold_seconds_ += dt;
                if (duck_press_hold_seconds_ >= 0.55f)
                    duck_press_hold_qualified_ = true;
            }
            else if (!duck_press_hold_qualified_)
            {
                duck_press_hold_seconds_ = std::max(0.0f, duck_press_hold_seconds_ - dt * 0.5f);
            }
            if (duck_press_hold_qualified_ && !duck_press_contact_this_step_
                && duck_obstacle_weight_ < 0.15f && stable_stance_seconds_ >= 0.50f
                && !duck_press_completed_)
            {
                duck_press_completed_ = true;
                ++duck_recovery_count_;
                ++obstacles_passed_;
                passed_obstacle_this_step_ = true;
            }
        }
        else if (duck_active_)
        {
            current_duck_hold_seconds_ += dt;
            duck_cycle_qualified_ = duck_cycle_qualified_
                || current_duck_hold_seconds_ >= 0.30f;
        }
        else if (duck_cycle_qualified_ && stable_stance_seconds_ >= 0.40f)
        {
            ++duck_recovery_count_;
            current_duck_hold_seconds_ = 0.0f;
            duck_cycle_qualified_ = false;
        }
        else if (!duck_cycle_qualified_)
        {
            current_duck_hold_seconds_ = 0.0f;
        }

        if (course_stage_ == CourseStage::duck_press && duck_obstacle_weight_ > 0.10f
            && std::abs(torso_turn_speed_) > 1.10f)
            torso_swing_seconds_ += dt;
        else
            torso_swing_seconds_ = std::max(0.0f, torso_swing_seconds_ - dt * 2.0f);
        if (torso_swing_seconds_ > 0.75f)
            invalidate(InvalidMotion::robotic_torso_swing);

        const bool collapsed_balance_posture''')

# Do not let the stationary press enter the horizontal passed-feature path.
replace_once("src/simulation.cpp",
'''        for (const CourseFeature& feature : course_features_)
        {
            const float trailing_edge = feature.center.x + course_feature_half_width(feature);''',
'''        for (const CourseFeature& feature : course_features_)
        {
            if (feature.kind == CourseFeatureKind::duck_press)
                continue;
            const float trailing_edge = feature.center.x + course_feature_half_width(feature);''')

# Reward leg-driven compression, penalize the robot-like torso/shoulder swing, and do not
# punish expected contact with the trainer platen.
replace_once("src/simulation.cpp",
'''        const float action_change_penalty = action_change_energy_ * 0.0025f;

        switch (course_stage_)''',
'''        const float action_change_penalty = action_change_energy_ * 0.0025f;
        const float press_contact_reward = course_stage_ == CourseStage::duck_press
            && duck_press_contact_this_step_ && duck_active_ ? 0.045f : 0.0f;
        const float torso_swing_penalty = course_stage_ == CourseStage::duck_press
            ? std::max(0.0f, std::abs(torso_turn_speed_) - 0.35f) * 0.018f : 0.0f;

        switch (course_stage_)''')
replace_once("src/simulation.cpp",
'''        case CourseStage::duck_press:
            last_reward_ = std::max(0.0f, upright) * 0.016f
                + contact * 0.0015f + duck_reward + obstacle_duck_reward
                + pass_reward - std::abs(forward_speed_) * 0.0030f
                - action_energy * 0.0009f - collision_penalty
                - premature_duck_penalty - body_contact_penalty;''',
'''        case CourseStage::duck_press:
            last_reward_ = std::max(0.0f, upright) * 0.016f
                + contact * 0.0015f + duck_reward + obstacle_duck_reward
                + press_contact_reward + pass_reward
                - std::abs(forward_speed_) * 0.0030f
                - action_energy * 0.0009f - torso_swing_penalty
                - premature_duck_penalty - body_contact_penalty;''')

# Observation switch completeness.
replace_once("src/simulation.cpp",
'''            case CourseFeatureKind::overhead_bar: result[30] = 0.0f; break;
            case CourseFeatureKind::moving_hazard:''',
'''            case CourseFeatureKind::overhead_bar: result[30] = 0.0f; break;
            case CourseFeatureKind::duck_press: result[30] = 0.0f; break;
            case CourseFeatureKind::moving_hazard:''')

# Duck teacher: legs compress; arms are fully neutral until the press lesson is complete.
replace_regex("src/ppo.hpp",
    r'''    \[\[nodiscard\]\] inline std::array<float, sim::action_count> duck_teacher_action\(.*?\n    \}\n\n    \[\[nodiscard\]\] inline std::array<float, sim::action_count> effective_policy_action''',
'''    [[nodiscard]] inline std::array<float, sim::action_count> duck_teacher_action(
        const sim::Environment& environment) noexcept
    {
        auto action = balance_teacher_action(environment);
        const float pressure = environment.duck_obstacle_weight();
        action[0] = clamp(action[0] - 0.30f * pressure, -0.70f, 0.70f);
        action[1] = clamp(action[1] + 0.62f * pressure, -0.82f, 0.82f);
        action[2] = clamp(action[2] + 0.30f * pressure, -0.70f, 0.70f);
        action[3] = clamp(action[3] - 0.62f * pressure, -0.82f, 0.82f);
        for (std::size_t index = 4; index < environment.blueprint().active_motor_count; ++index)
            action[index] = 0.0f;
        return bilateral_joint_synergy_action(environment, action, sim::CourseStage::duck_press);
    }

    [[nodiscard]] inline std::array<float, sim::action_count> effective_policy_action''')
replace_regex("src/ppo.hpp",
    r'''        else if \(stage == sim::CourseStage::duck_press\)\n        \{.*?\n        \}\n        else if \(stage == sim::CourseStage::ramps\)''',
'''        else if (stage == sim::CourseStage::duck_press)
        {
            const auto teacher = duck_teacher_action(environment);
            const float pressure = environment.duck_obstacle_weight();
            const float leg_assist = 0.72f + pressure * 0.24f;
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], leg_assist);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], 0.0f, 0.995f);
        }
        else if (stage == sim::CourseStage::ramps)''')
replace_once("src/ppo.hpp",
'''        const float leg_pair_strength = stage == sim::CourseStage::duck_press
            ? 0.28f :''',
'''        const float leg_pair_strength = stage == sim::CourseStage::duck_press
            ? 0.12f :''')

# Test access for deterministic press and teacher checks.
replace_once("tests/core_tests.cpp",
'''        static void qualify_stable_stance(Environment& environment) noexcept
        {''',
'''        static void set_duck_pressure(Environment& environment, float pressure) noexcept
        {
            environment.duck_obstacle_weight_ = pressure;
        }

        static bool press_collision_resolves_below(Environment& environment) noexcept
        {
            if (!environment.valid_node(environment.blueprint_.head_node))
                return false;
            Particle& head = environment.particles_[environment.blueprint_.head_node];
            const float bottom = head.position.y + head.radius * 0.45f;
            environment.course_features_.clear();
            environment.course_features_.push_back({
                CourseFeatureKind::duck_press,
                { head.position.x, bottom + 0.16f }, { 1.5f, 0.16f }, 0.0f, {}, -2
            });
            environment.duck_press_contact_this_step_ = false;
            environment.duck_press_max_penetration_ = 0.0f;
            environment.solve_course();
            return environment.duck_press_contact_this_step_
                && head.position.y + head.radius <= bottom + 0.0001f;
        }

        static void qualify_stable_stance(Environment& environment) noexcept
        {''')
replace_once("tests/core_tests.cpp",
'''    require(ui_layout::top_bar_box(1970.0f).width == 1970.0f,''',
'''    const sim::DuckPressProfile press_clear = sim::duck_press_profile(1.0f, 0.5f, 5.0f);
    const sim::DuckPressProfile press_descend = sim::duck_press_profile(3.5f, 0.5f, 5.0f);
    const sim::DuckPressProfile press_hold = sim::duck_press_profile(5.5f, 0.5f, 5.0f);
    const sim::DuckPressProfile press_retract = sim::duck_press_profile(8.0f, 0.5f, 5.0f);
    require(press_clear.bottom_y > 6.0f && press_descend.descending
            && press_descend.vertical_velocity < 0.0f,
        "duck press does not begin clear and descend gradually");
    require(press_hold.holding && press_hold.bottom_y < 4.2f,
        "duck press does not hold a meaningful crouch target");
    require(press_retract.retracting && press_retract.vertical_velocity > 0.0f,
        "duck press does not retract after the hold");

    require(ui_layout::top_bar_box(1970.0f).width == 1970.0f,''')
replace_once("tests/core_tests.cpp",
'''    require(sim::ground_velocity_retention(true, 0.0f)''',
'''    sim::Environment press_environment(sim::CreatureBlueprint::humanoid(), 17);
    press_environment.set_course(sim::CourseStage::duck_press, 0.5f);
    sim::EnvironmentTestAccess::set_duck_pressure(press_environment, 1.0f);
    const auto press_teacher = rl::duck_teacher_action(press_environment);
    require(std::abs(press_teacher[4]) < 0.0001f
            && std::abs(press_teacher[5]) < 0.0001f
            && std::abs(press_teacher[6]) < 0.0001f
            && std::abs(press_teacher[7]) < 0.0001f,
        "duck teacher still prefers shoulder or arm swing over leg compression");
    require(std::abs(press_teacher[1]) + std::abs(press_teacher[3]) > 0.60f,
        "duck teacher does not apply meaningful leg compression");
    require(sim::EnvironmentTestAccess::press_collision_resolves_below(press_environment),
        "duck press clips through the model instead of resolving below the platen");

    require(sim::ground_velocity_retention(true, 0.0f)''')

# Update curriculum assertions and feature aggregates for the new enum member.
tests = read("tests/core_tests.cpp")
tests = tests.replace('"2. LOW BAR DUCK / RECOVER"', '"2. PRESS DUCK / HOLD / RECOVER"')
tests = tests.replace('"5. MOVING DUCK / JUMP"', '"5. MOVING LOW BAR / HURDLE"')
tests = tests.replace('"duck lesson completes without clearing its low bar"',
                      '"duck lesson completes without holding and recovering from the press"')
tests = tests.replace('"duck-and-clear evidence cannot complete the duck lesson"',
                      '"press hold and recovery evidence cannot complete the duck lesson"')
write("tests/core_tests.cpp", tests)

# New release ledger. Earlier package evidence remains historical and is carried forward.
mission = read("missioncache.md")
mission = re.sub(r'\*\*Target:\*\* Runner v0\.7\.3', '**Target:** Runner v0.7.4', mission, count=1)
mission = re.sub(r'\*\*Release state:\*\*.*',
                 '**Release state:** IN PROGRESS — rebrand, duck-press collision, control-priority, and carried mission pass',
                 mission, count=1)
section = r'''

## v0.7.4 rebrand and duck-training correction

### WALK-BRAND-040 — Remove the former project brand completely
**Status:** IN PROGRESS

The application title, executable, CMake targets, namespaces, macros, autosaves, rig/state magic, package names, documentation, tests, and release notes use Runner naming. A case-insensitive repository search for the former word must return zero matches. The external GUI dependency is removed and the required bitmap font is local.

### WALK-TITLE-041 — Replace the sand-sim enemy trainer title
**Status:** IN PROGRESS

The visible title is `AUTONOMOUS RIG TRAINER`, with `AUTONOMOUS PHYSICS LOCOMOTION LAB` as the project subtitle. Sand-simulation hazards may remain curriculum inputs without defining the whole trainer.

### WALK-DUCK-042 — Compression-first duck curriculum
**Status:** IN PROGRESS

Stage two begins with a broad stationary overhead platen. It waits for settling, descends gradually, holds at a safe crouch target, retracts, and requires stable recovery before completion. Moving low bars remain a later lesson.

### WALK-COLLIDE-043 — Non-clipping duck press
**Status:** IN PROGRESS

The platen is a one-way underside collider. It applies downward contact pressure, never passes through a particle, records penetration, and invalidates excessive penetration rather than treating clipping as duck evidence.

### WALK-CONTROL-044 — Legs before shoulders during ducking
**Status:** IN PROGRESS

The duck teacher uses hips and knees while arm outputs remain neutral. Repeated torso/shoulder-axis swinging under the press is penalized and then invalidated instead of becoming the primary learned response.

### WALK-HAZARD-045 — Preparation distance for moving hazards
**Status:** IN PROGRESS

Later moving low bars, hurdles, and mixed hazards remain at least 6.5 m or 5 m ahead of the rig when selected so the policy has time to perform a meaningful movement.

### WALK-CARRY-046 — Complete carried missions and publish v0.7.4
**Status:** IN PROGRESS

All prior body integrity, feet-first control, passive-head/tail, preview, DPI, units, flip/spin, statistics, concurrency, persistence, launch, and package requirements are revalidated against the clean Runner source and Windows package before publication.
'''
if "### WALK-BRAND-040" not in mission:
    insert_at = mission.index("## v0.7.3 live-runtime correction")
    mission = mission[:insert_at] + section + "\n" + mission[insert_at:]
write("missioncache.md", mission)

write("RELEASE_NOTES_v0.7.4.md", r'''# Runner v0.7.4

- Removes the former project brand from owned source, UI, executable, package, persistence names, documentation, and tests.
- Replaces the sand-sim enemy title with `AUTONOMOUS RIG TRAINER` and `AUTONOMOUS PHYSICS LOCOMOTION LAB`.
- Removes the external GUI dependency and keeps the required bitmap font locally.
- Replaces the first moving low-bar duck lesson with a stationary overhead compression platen.
- The platen waits for stance, descends gradually, holds, retracts, and requires stable recovery.
- Adds one-way underside collision so the platen cannot clip through the model.
- Invalidates excessive press penetration and repeated robotic torso/shoulder-axis swinging.
- Keeps arms neutral during the compression lesson and teaches ducking through hips and knees.
- Moves low-bar traversal later and increases preparation distance for moving hazards.
- Invalidates v0.7.3 policy and autonomy persistence with v0.7.4 semantics.
- Carries forward and revalidates all open mission-ledger requirements before release.
''')

# Remove obsolete release scaffolding and update the source package names.
for path in (ROOT / ".github" / "workflows").glob("*v073*") if (ROOT / ".github" / "workflows").exists() else ():
    path.unlink(missing_ok=True)

# This applicator is deliberately one-shot; final source must contain no scaffolding or old brand word.
Path(__file__).unlink()
print("materialized Runner v0.7.4 rebrand and compression-first duck lesson")
