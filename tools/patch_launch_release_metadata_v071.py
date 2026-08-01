from __future__ import annotations

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


notes_path = Path("RELEASE_NOTES_v0.7.1.md")
notes = notes_path.read_text(encoding="utf-8")
launch_note = (
    "- Fixes startup from Visual Studio, shortcuts, extracted release folders, and unrelated "
    "working directories by resolving shaders and assets beside the executable.\n"
)
if launch_note not in notes:
    if not notes.endswith("\n"):
        notes += "\n"
    notes += launch_note
notes_path.write_text(notes, encoding="utf-8")

writer_path = Path("tools/write_missioncache_v071.py")
writer = writer_path.read_text(encoding="utf-8")
launch_section = '''### WALK-LAUNCH-021 — Executable-relative clean-folder launch
**Status:** VERIFIED

EpochRunner resolves shaders and assets from `SDL_GetBasePath()` rather than the caller's working directory. Visual Studio launches from the executable directory. The build tree and installed release folder both pass `--diagnose-package` when invoked from an unrelated directory, and the package includes the required shaders, assets, and runtime DLLs.

'''
writer = replace_once(
    writer,
    "## Runtime architecture\n",
    launch_section + "## Runtime architecture\n",
    "runtime architecture section",
)
writer = writer.replace(
    "- executable version and Vulkan diagnostic: passed;\n",
    "- executable version, Vulkan diagnostic, and unrelated-working-directory package launch: passed;\n",
)
writer = writer.replace(
    '        "- Vulkan diagnostic: passed.\\n"\n',
    '        "- Vulkan diagnostic and clean-folder package launch from an unrelated working directory: passed.\\n"\n',
)
writer = writer.replace(
    '        "- Vulkan diagnostic: passed\\n"\n',
    '        "- Vulkan diagnostic and clean-folder package launch: passed\\n"\n',
)
writer_path.write_text(writer, encoding="utf-8")
