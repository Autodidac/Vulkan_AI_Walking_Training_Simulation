#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")


replace_once(
    "src/ui_layout.hpp",
    "    inline constexpr float minimum_content_width = 1150.0f;\n"
    "    inline constexpr float minimum_content_height = 700.0f;\n",
    "    inline constexpr float minimum_window_width = 1280.0f;\n"
    "    inline constexpr float minimum_window_height = 820.0f;\n"
    "    inline constexpr float minimum_content_width = 1150.0f;\n"
    "    inline constexpr float minimum_content_height = 700.0f;\n")
replace_once(
    "src/ui_layout.hpp",
    "    [[nodiscard]] constexpr bool supported_window(float width, float height) noexcept\n"
    "    {\n"
    "        const Box content = content_box(width, height);\n"
    "        return content.width >= minimum_content_width\n"
    "            && content.height >= minimum_content_height;\n"
    "    }\n",
    "    [[nodiscard]] constexpr bool supported_window(float width, float height) noexcept\n"
    "    {\n"
    "        const Box content = content_box(width, height);\n"
    "        return width >= minimum_window_width\n"
    "            && height >= minimum_window_height\n"
    "            && content.width >= minimum_content_width\n"
    "            && content.height >= minimum_content_height;\n"
    "    }\n")
replace_once(
    "tests/core_tests.cpp",
    "    require(ui_layout::live_layout_valid(1100.0f, 902.0f),\n"
    "        \"supported minimum live layout overlaps its panel, telemetry, or PIP\");\n"
    "    require(!ui_layout::supported_window(1099.0f, 902.0f)\n"
    "            && !ui_layout::supported_window(1100.0f, 901.0f),\n"
    "        \"undersized windows are incorrectly treated as fully supported\");\n"
    "    const ui_layout::Box minimum_content = ui_layout::content_box(1100.0f, 902.0f);\n",
    "    require(ui_layout::live_layout_valid(1280.0f, 820.0f),\n"
    "        \"supported minimum live layout overlaps its panel, telemetry, or PIP\");\n"
    "    require(!ui_layout::supported_window(1279.0f, 820.0f)\n"
    "            && !ui_layout::supported_window(1280.0f, 819.0f),\n"
    "        \"undersized windows are incorrectly treated as fully supported\");\n"
    "    const ui_layout::Box minimum_content = ui_layout::content_box(1280.0f, 820.0f);\n")
replace_once(
    "src/main.cpp",
    "#include <chrono>\n#include <cstdio>\n",
    "#include <chrono>\n#include <cmath>\n#include <cstdio>\n")
replace_once(
    "tests/v0720_ui_tests.cpp",
    "#include <array>\n#include <cmath>\n",
    "#include <algorithm>\n#include <array>\n#include <cmath>\n")
print("Runner v0.7.20 validation repair applied")
