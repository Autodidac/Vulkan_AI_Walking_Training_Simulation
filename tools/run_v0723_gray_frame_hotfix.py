#!/usr/bin/env python3
from pathlib import Path
import runpy

script = Path(__file__).with_name("apply_v0723_gray_frame_hotfix.py")
text = script.read_text(encoding="utf-8")
text = text.replace(
    "- [`docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md`](docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md) documents the explicit transparent-border and final-frame visibility contract.\\n",
    "- [`docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md`](docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md) documents the opaque border-fill regression and visible-frame tests.\\n")
text = text.replace(
    'section_anchor = "## v0.7.22 black-frame rendering hotfix\\n"',
    'section_anchor = "## v0.7.22 black-frame hotfix\\n"')
script.write_text(text, encoding="utf-8", newline="\n")
runpy.run_path(str(script), run_name="__main__")
