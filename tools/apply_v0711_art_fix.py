from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


def patch_cmake() -> None:
    text = read("CMakeLists.txt")
    text = replace_once(text,
        "project(Runner VERSION 0.7.10 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.11 LANGUAGES CXX)",
        "project version")
    text = replace_once(text,
        "    src/simulation.cpp\n    src/acceptance.cpp",
        "    src/simulation.cpp\n    src/pixel_art.cpp\n    src/acceptance.cpp",
        "pixel art core source")
    text = replace_once(text,
        "        src/main.cpp src/app.cpp src/canvas.cpp src/renderer.cpp\n        src/app.hpp src/renderer.hpp src/math.hpp src/ui_font.hpp)",
        "        src/main.cpp src/app.cpp src/canvas.cpp src/renderer.cpp\n        src/app.hpp src/pixel_art.hpp src/renderer.hpp src/math.hpp src/ui_font.hpp)",
        "app pixel art header")
    anchor = """    add_executable(RunnerCoreTests tests/core_tests.cpp)
    target_link_libraries(RunnerCoreTests PRIVATE Runner::Core)"""
    addition = """    add_executable(RunnerPixelArtTests tests/pixel_art_tests.cpp)
    target_link_libraries(RunnerPixelArtTests PRIVATE Runner::Core)
    target_compile_features(RunnerPixelArtTests PRIVATE cxx_std_23)
    target_compile_definitions(RunnerPixelArtTests PRIVATE
        RUNNER_SOURCE_ASSET_DIRECTORY=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\")
    set_target_properties(RunnerPixelArtTests PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    runner_enable_warnings(RunnerPixelArtTests)
    add_test(NAME Runner.PixelArt COMMAND RunnerPixelArtTests)
    set_tests_properties(Runner.PixelArt PROPERTIES TIMEOUT 30)

    add_executable(RunnerCoreTests tests/core_tests.cpp)
    target_link_libraries(RunnerCoreTests PRIVATE Runner::Core)"""
    text = replace_once(text, anchor, addition, "pixel art test target")
    write("CMakeLists.txt", text)


def patch_app() -> None:
    text = read("src/app.cpp")
    text = replace_once(text,
        '#include "app.hpp"\n#include "autonomy.hpp"',
        '#include "app.hpp"\n#include "autonomy.hpp"\n#include "pixel_art.hpp"',
        "pixel art include")
    text = text.replace("#include <fstream>\n", "", 1)

    beginning = text.index("        struct PixelArt\n")
    ending = text.index("        void fill_rounded_rect", beginning)
    replacement = """        void draw_pixel_art(render::Canvas& canvas, const art::PixelArt& artwork,
            Vec2 position, float pixel_size)
        {
            if (!artwork.loaded() || pixel_size <= 0.0f)
                return;
            for (int y = 0; y < artwork.height; ++y)
            {
                for (int x = 0; x < artwork.width; ++x)
                {
                    const Vec2 minimum = position
                        + Vec2{ static_cast<float>(x) * pixel_size,
                            static_cast<float>(y) * pixel_size };
                    canvas.quad(minimum, minimum + Vec2{ pixel_size, pixel_size },
                        artwork.pixels[static_cast<std::size_t>(
                            y * artwork.width + x)]);
                }
            }
        }

"""
    text = text[:beginning] + replacement + text[ending:]
    text = replace_once(text,
        "        PixelArt original_runner_art{};",
        "        art::PixelArt original_runner_art{};",
        "art state type")
    text = replace_once(text,
        """        if (!load_p3_pixel_art(asset_directory / "chicken.ppm",
                impl_->original_runner_art, error))
            return false;

        impl_->trainer.set_autosave_paths""",
        """        std::string artwork_error{};
        if (!art::load_p3_pixel_art(asset_directory / "chicken.ppm",
                impl_->original_runner_art, artwork_error))
        {
            impl_->original_runner_art = {};
            impl_->status = "ARTWORK WARNING - " + artwork_error;
            impl_->status_time = 9.0f;
        }

        impl_->trainer.set_autosave_paths""",
        "nonfatal artwork load")
    text = replace_once(text,
        """        impl_->status = message;
        impl_->status_time = 6.0f;
        error.clear();""",
        """        if (impl_->original_runner_art.loaded())
        {
            impl_->status = message;
            impl_->status_time = 6.0f;
        }
        error.clear();""",
        "preserve artwork warning")
    write("src/app.cpp", text)


def patch_main() -> None:
    text = read("src/main.cpp")
    text = replace_once(text,
        '#include "app.hpp"\n#include "renderer.hpp"',
        '#include "app.hpp"\n#include "pixel_art.hpp"\n#include "renderer.hpp"',
        "main pixel art include")
    old = """        if (!std::filesystem::is_directory(asset_directory, filesystem_error))
        {
            error = "Missing packaged asset directory: " + asset_directory.string();
            if (filesystem_error)
                error += " (" + filesystem_error.message() + ")";
            return false;
        }
        error.clear();
        return true;"""
    new = """        if (!std::filesystem::is_directory(asset_directory, filesystem_error))
        {
            error = "Missing packaged asset directory: " + asset_directory.string();
            if (filesystem_error)
                error += " (" + filesystem_error.message() + ")";
            return false;
        }

        runner::art::PixelArt packaged_art{};
        if (!runner::art::load_p3_pixel_art(
                asset_directory / "chicken.ppm", packaged_art, error))
            return false;
        if (!packaged_art.loaded())
        {
            error = "Packaged Runner artwork decoded incompletely";
            return false;
        }
        error.clear();
        return true;"""
    text = replace_once(text, old, new, "package artwork validation")
    write("src/main.cpp", text)


def patch_docs() -> None:
    cache = read("missioncache.md")
    cache = re.sub(
        r"^\*\*Target:\*\*.*$",
        "**Target:** Runner v0.7.11",
        cache,
        count=1,
        flags=re.MULTILINE,
    )
    cache = re.sub(
        r"^\*\*Release state:\*\*.*$",
        "**Release state:** REOPENED — v0.7.10 packaged startup rejected the valid original P3 artwork; v0.7.11 parser/package correction is in validation.",
        cache,
        count=1,
        flags=re.MULTILINE,
    )
    cache = re.sub(
        r"(### WALK-ART-112[^\n]*\n)\*\*Status:\*\*[^\n]*",
        r"\1**Status:** REOPENED — released v0.7.10 rejects the packaged valid P3 file at application initialization",
        cache,
        count=1,
    )
    marker = "## v0.7.11 packaged artwork startup correction"
    if marker not in cache:
        cache = cache.rstrip() + f"""

{marker}

### WALK-PPM-114 — Parse the original packaged P3 artwork portably
**Status:** IN VALIDATION

Replace formatted-stream parsing with a binary byte tokenizer that accepts standard ASCII whitespace, CRLF, comments, and an optional UTF-8 BOM while enforcing bounded dimensions, channel ranges, exact pixel count, and no unexpected trailing tokens.

### WALK-ARTSAFE-115 — Decorative artwork cannot brick Runner startup
**Status:** IN VALIDATION

The application uses the original artwork when valid. A missing or malformed user-side decorative asset produces a visible warning and continues without the decoration; it cannot terminate the trainer. Release/package validation remains strict and rejects an invalid packaged asset.

### WALK-PKGART-116 — Package diagnostics must exercise the real artwork loader
**Status:** IN VALIDATION

`Runner.exe --diagnose-package` parses `assets/chicken.ppm` with the same loader used at application startup. Deterministic tests parse the exact repository asset and BOM/comment/CRLF fixtures, so a release cannot pass by checking only that the asset directory exists.

### WALK-RELEASE-117 — Publish corrected Runner v0.7.11
**Status:** IN VALIDATION

Run Linux warnings-as-errors and all tests, full Windows SDL3/Vulkan build and tests, build-tree/installed/extracted package diagnostics, executable-relative `run.bat`, checksum/manifest audit, release re-download, and repository cleanup before publication.
"""
    write("missioncache.md", cache)

    changelog = read("CHANGELOG.md")
    entry = """## [0.7.11] - 2026-08-03

### Fixed

- Replaced the Windows-fragile formatted-stream P3 parser with a portable binary tokenizer supporting comments, CRLF, and an optional UTF-8 BOM.
- Made decorative artwork failure nonfatal during normal startup while keeping packaged-release validation strict.
- Extended `--diagnose-package` and deterministic tests to parse the exact packaged `assets/chicken.ppm` file.

"""
    if "## [0.7.11]" not in changelog:
        insert_at = changelog.find("## [0.7.10]")
        if insert_at < 0:
            changelog = entry + changelog
        else:
            changelog = changelog[:insert_at] + entry + changelog[insert_at:]
    write("CHANGELOG.md", changelog)


def main() -> None:
    patch_cmake()
    patch_app()
    patch_main()
    patch_docs()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
