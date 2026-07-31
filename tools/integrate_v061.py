from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


cmake = Path("CMakeLists.txt")
text = cmake.read_text(encoding="utf-8")
text = replace_once(
    text,
    "project(EpochRunner VERSION 0.6.0 LANGUAGES CXX)",
    "project(EpochRunner VERSION 0.6.1 LANGUAGES CXX)",
    "CMake version",
)
cmake.write_text(text, encoding="utf-8")

manifest = Path("vcpkg.json")
text = manifest.read_text(encoding="utf-8")
text = replace_once(
    text,
    '"version-semver": "0.6.0"',
    '"version-semver": "0.6.1"',
    "vcpkg version",
)
manifest.write_text(text, encoding="utf-8")

readme = Path("README.md")
text = readme.read_text(encoding="utf-8")
anchor = (
    "EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, "
    "vcpkg manifest mode, and a compact PPO controller. Version 0.6.0 replaces manual "
    "train/run switching with a continuously operating autonomous curriculum.\n"
)
replacement = (
    "EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, "
    "vcpkg manifest mode, and a compact PPO controller. Version 0.6.1 completes radial "
    "obstacle observations and closes the harmless-contact recovery reward exploit while "
    "retaining the autonomous curriculum introduced in v0.6.0.\n"
)
text = replace_once(text, anchor, replacement, "README release identity")
readme.write_text(text, encoding="utf-8")

tests = Path("tests/core_tests.cpp")
text = tests.read_text(encoding="utf-8")
text = text.replace("epochrunner-v060-core-test.eppo", "epochrunner-v061-core-test.eppo")
text = text.replace(
    "EpochRunner v0.6.0 procedural course, recovery, concurrency, gait, and rig-edit tests passed",
    "EpochRunner v0.6.1 obstacle observation, recovery, concurrency, gait, and rig-edit tests passed",
)
tests.write_text(text, encoding="utf-8")

benchmark = Path("tests/concurrency_benchmark.cpp")
text = benchmark.read_text(encoding="utf-8")
text = text.replace(
    "EpochRunner v0.6 speed-mode throughput benchmark passed",
    "EpochRunner v0.6.1 speed-mode throughput benchmark passed",
)
benchmark.write_text(text, encoding="utf-8")

missions = Path("MISSIONS.md")
text = missions.read_text(encoding="utf-8")
section = """
## WALK-OBS-001 — Complete obstacle sensing and recovery reward integrity

**Status:** ACTIVE

Every physical obstacle class must expose its actual collision size to the policy. Radial rocks, projectiles, and moving hazards use radius; hurdles and overhead bars use rectangular extent. Harmless upright contact must not open a rewardable recovery event.

**Acceptance:**

- Rock, projectile, and moving-hazard radius is present in observations.
- Hurdle and overhead-bar extent remains present in observations.
- Destabilizing impacts and major balance loss start recovery.
- Harmless upright contact cannot farm recovery bonuses.
- Hard falls remain terminal.
- Full Windows/Vulkan build, deterministic tests, diagnostics, package, and checksum pass.

"""
if "## WALK-OBS-001" not in text:
    warning = text.index("## Current warning")
    text = text[:warning] + section + text[warning:]
missions.write_text(text, encoding="utf-8")
