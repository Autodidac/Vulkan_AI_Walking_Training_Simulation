from __future__ import annotations

from pathlib import Path
import shutil
import sys


ROOT = Path(__file__).resolve().parents[1]


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def write(path: Path, text: str) -> None:
    path.write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def materialize_sources(generated: Path) -> None:
    for name in ("app.cpp", "autonomy_curriculum.cpp"):
        source = generated / name
        if not source.is_file():
            raise RuntimeError(f"generated source missing: {source}")
        shutil.copyfile(source, ROOT / "src" / name)


def simplify_cmake() -> None:
    path = ROOT / "CMakeLists.txt"
    text = path.read_text(encoding="utf-8")
    start = text.index('set(RUNNER_GENERATED_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/v0715")')
    end_marker = """set_source_files_properties(
    "${RUNNER_GENERATED_SOURCE_DIR}/app.cpp"
    "${RUNNER_GENERATED_SOURCE_DIR}/autonomy_curriculum.cpp"
    PROPERTIES GENERATED TRUE)

"""
    end = text.index(end_marker, start) + len(end_marker)
    text = text[:start] + text[end:]
    text = replace_once(
        text,
        '    "${RUNNER_GENERATED_SOURCE_DIR}/autonomy_curriculum.cpp"',
        "    src/autonomy_curriculum.cpp",
        "direct autonomy curriculum source",
    )
    text = replace_once(
        text,
        '        src/main.cpp "${RUNNER_GENERATED_SOURCE_DIR}/app.cpp" src/canvas.cpp src/renderer.cpp',
        "        src/main.cpp src/app.cpp src/canvas.cpp src/renderer.cpp",
        "direct application source",
    )
    write(path, text)


def update_training_semantics() -> None:
    path = ROOT / "src" / "ppo.hpp"
    text = path.read_text(encoding="utf-8")
    text = replace_once(
        text,
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1400u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1500u;",
        "v0.7.15 training semantics",
    )
    write(path, text)


def update_mission_cache() -> None:
    path = ROOT / "missioncache.md"
    text = path.read_text(encoding="utf-8")
    marker = "# Runner v0.7.15 viewport, terrain, and failed-policy recovery"
    if marker in text:
        return
    section = """# Runner v0.7.15 viewport, terrain, and failed-policy recovery

**Release state:** IMPLEMENTED — Linux and Windows package validation in progress.

- [x] Pull the live world camera from 90 to 22 pixels per meter so the rig is a small course subject instead of filling the viewport.
- [x] Move the live ground framing upward and place the rig left of center with more course visible ahead.
- [x] Remove the duplicate interpolated terrain polyline and moving dashed pseudo-ground.
- [x] Suppress the zero-distance sign that rendered as a large opaque column.
- [x] Render exposed, active, and near-surface terrain as fine granular cells while retaining deep inactive uniform macro tiles.
- [x] Isolate v0.7.15 autosaves from poisoned v0.7.14 policies.
- [x] Restore the verified champion after catastrophic invalid or backward evaluations.
- [x] Reset a failed policy nursery after three catastrophic evaluations when no champion exists.
- [x] Bump the policy-training semantics to v0.7.15.
- [ ] Pass the complete Linux deterministic suite.
- [ ] Pass the complete Windows SDL3/Vulkan build, tests, diagnostics, installation, and extracted-package audit.
- [ ] Merge, publish Runner v0.7.15, and remove temporary validation infrastructure and stale observer work.

"""
    write(path, section + text)


def update_changelog() -> None:
    path = ROOT / "CHANGELOG.md"
    text = path.read_text(encoding="utf-8")
    marker = "## v0.7.15"
    if marker in text:
        return
    section = """## v0.7.15 - Viewport, terrain, and failed-policy recovery

- Pulled the live course camera back and reframed the terrain so the rig appears as a small training subject with meaningful course visibility.
- Removed the duplicate surface polyline, moving pseudo-ground dashes, and zero-distance obstruction.
- Preserved fine granular cells at exposed and active terrain while restricting macro-tile quads to deep inactive uniform material.
- Added automatic champion restoration for catastrophic invalid or backward generations and nursery reset when no valid champion exists.
- Isolated v0.7.15 autosaves and policy semantics from failed v0.7.14 training state.

"""
    first_break = text.find("\n")
    if first_break == -1:
        write(path, text + "\n\n" + section)
    else:
        write(path, text[: first_break + 1] + "\n" + section + text[first_break + 1 :].lstrip("\n"))


def cleanup() -> None:
    (ROOT / "cmake" / "generate_runner_v0715_sources.cmake").unlink()
    Path(__file__).unlink()


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: materialize_runner_v0715.py <generated-source-dir>")
    generated = Path(sys.argv[1]).resolve()
    materialize_sources(generated)
    simplify_cmake()
    update_training_semantics()
    update_mission_cache()
    update_changelog()
    cleanup()


if __name__ == "__main__":
    main()
