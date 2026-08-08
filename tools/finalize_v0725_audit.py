#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / "tools/repository_audit.cmake"
text = path.read_text(encoding="utf-8")

checks = r'''

# Runner v0.7.25 EpochGui logical-font and noob-progress synchronization.
file(READ "${RUNNER_SOURCE_DIR}/src/app.cpp" v0725_app_text)
foreach(reference IN ITEMS
        "font::make_bitmap_font_metrics"
        "TRAINING SAMPLES READY"
        "format_work_counter(\"RUNS\""
        "format_work_counter(\"TESTS\"")
    string(FIND "${v0725_app_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "v0.7.25 readable font/progress contract missing: ${reference}")
    endif()
endforeach()
string(FIND "${v0725_app_text}" "constexpr float ui_font_scale" legacy_font_scale_pos)
if(NOT legacy_font_scale_pos EQUAL -1)
    message(FATAL_ERROR "Legacy bitmap-cell font multiplier remains")
endif()

file(READ "${RUNNER_SOURCE_DIR}/src/ui_font.hpp" v0725_font_text)
foreach(reference IN ITEMS
        "130f33fe31d73564a35a622f3bb5ddcc2b5105d5"
        "default_logical_height = 16.0F"
        "make_bitmap_font_metrics"
        "case '%'")
    string(FIND "${v0725_font_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "EpochGui font synchronization missing: ${reference}")
    endif()
endforeach()
'''

if "# Runner v0.7.25 EpochGui logical-font" not in text:
    marker = 'message(STATUS "Runner v0.7.25 repository hygiene passed")'
    if marker not in text:
        marker = 'message(STATUS "Runner v0.7.24 repository hygiene passed")'
    if marker not in text:
        raise RuntimeError("repository audit status marker not found")
    text = text.replace(marker, checks + "\n" + marker, 1)

path.write_text(text, encoding="utf-8", newline="\n")
print("Runner v0.7.25 repository audit finalized")
