#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
APPLICATOR = ROOT / "tools/apply_v0716_batch25.py"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def main() -> int:
    text = APPLICATOR.read_text(encoding="utf-8")

    text = replace_once(
        text,
        '    text = replace_all_checked(text, "0.7.15", "0.7.16", 10, "workflow version")\n'
        '    text = replace_all_checked(text, "v0.7.15", "v0.7.16", 4, "workflow tag")',
        '    text = replace_all_checked(text, "v0.7.15", "v0.7.16", 4, "workflow tag")\n'
        '    text = replace_all_checked(text, "0.7.15", "0.7.16", 10, "workflow version")',
        "workflow replacement order",
    )

    identifier = '    text = replace_all_checked(text, "v0715", "v0716", 5, "workflow identifier")\n'
    text = replace_once(
        text,
        identifier,
        identifier + '    text = text.replace("0x0007\\\'1503u", "0x0007\\\'1600u")\n',
        "workflow semantics audit",
    )

    live_anchor = '    text = replace_once(text, old_live, new_live, "adaptive live camera")'
    live_fix = (
        '    old_live = "\\n".join(("        " + line if line else line)\n'
        '        for line in old_live.split("\\n"))\n'
        '    new_live = "\\n".join(("        " + line if line else line)\n'
        '        for line in new_live.split("\\n"))\n'
    )
    text = replace_once(
        text,
        live_anchor,
        live_fix + live_anchor,
        "live camera indentation repair",
    )

    text = replace_once(
        text,
        "inline constexpr float minimum_pixels_per_meter = 30.0f;",
        "inline constexpr float minimum_pixels_per_meter = 30.24f;",
        "minimum zoom test contract",
    )

    ui_anchor = 'def patch_ui_layout() -> None:\n    text = read("src/ui_layout.hpp")\n'
    ui_replacement = (
        ui_anchor
        + '    text = replace_once(\n'
        + '        text,\n'
        + '        "#include <algorithm>\\n",\n'
        + '        "#include <algorithm>\\n#include <cstdint>\\n",\n'
        + '        "self-contained ui layout include",\n'
        + '    )\n'
    )
    text = replace_once(
        text,
        ui_anchor,
        ui_replacement,
        "ui layout include repair",
    )

    APPLICATOR.write_text(text, encoding="utf-8", newline="\n")
    subprocess.run([sys.executable, str(APPLICATOR), "implement"], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
