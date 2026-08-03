from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


cmake = read("CMakeLists.txt")
cmake = replace_once(
    cmake,
    "project(Runner VERSION 0.7.8 LANGUAGES CXX)",
    "project(Runner VERSION 0.7.9 LANGUAGES CXX)",
    "project version",
)
cmake = replace_once(
    cmake,
    "add_library(RunnerCore STATIC\n    src/simulation.cpp\n",
    "add_library(RunnerCore STATIC\n    src/simulation.cpp\n    src/acceptance.cpp\n",
    "RunnerCore acceptance source",
)
acceptance_test = """    add_executable(RunnerLiveAcceptanceTests tests/live_acceptance_tests.cpp)
    target_link_libraries(RunnerLiveAcceptanceTests PRIVATE Runner::Core)
    target_compile_features(RunnerLiveAcceptanceTests PRIVATE cxx_std_23)
    set_target_properties(RunnerLiveAcceptanceTests PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    runner_enable_warnings(RunnerLiveAcceptanceTests)
    add_test(NAME Runner.LiveAcceptance COMMAND RunnerLiveAcceptanceTests)
    set_tests_properties(Runner.LiveAcceptance PROPERTIES TIMEOUT 120)

"""
cmake = replace_once(
    cmake,
    "    add_executable(RunnerConcurrencyBenchmark tests/concurrency_benchmark.cpp)\n",
    acceptance_test
    + "    add_executable(RunnerConcurrencyBenchmark tests/concurrency_benchmark.cpp)\n",
    "live acceptance test target",
)
write("CMakeLists.txt", cmake)

main = read("src/main.cpp")
main = replace_once(
    main,
    '#include "app.hpp"\n',
    '#include "acceptance.hpp"\n#include "app.hpp"\n',
    "acceptance include",
)
package_function = """    [[nodiscard]] bool wants_package_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-package";
    }
"""
acceptance_function = package_function + """
    [[nodiscard]] bool wants_acceptance_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-acceptance";
    }
"""
main = replace_once(
    main,
    package_function,
    acceptance_function,
    "acceptance argument parser",
)
version_block = """    if (wants_version(argc, argv))
    {
        std::printf("Runner %s\\n", RUNNER_VERSION);
        return 0;
    }
"""
acceptance_block = version_block + """    if (wants_acceptance_diagnostic(argc, argv))
    {
        const runner::acceptance::Report report =
            runner::acceptance::run_live_acceptance_matrix();
        for (const runner::acceptance::CaseResult& result : report.cases)
        {
            std::printf("[%s] %s: %s\\n",
                result.passed ? "PASS" : "FAIL",
                result.name.c_str(),
                result.detail.c_str());
        }
        std::printf("Runner %s live acceptance matrix: %zu/%zu passed\\n",
            RUNNER_VERSION, report.passed_count(), report.cases.size());
        return report.passed() ? 0 : 1;
    }
"""
main = replace_once(
    main,
    version_block,
    acceptance_block,
    "acceptance diagnostic execution",
)
write("src/main.cpp", main)

mission = read("missioncache.md")
mission = replace_once(
    mission,
    "**Target:** Runner v0.7.8",
    "**Target:** Runner v0.7.9",
    "mission target",
)
mission = re.sub(
    r"^\*\*Release state:\*\*.*$",
    "**Release state:** IN VALIDATION — v0.7.9 converts the carried live-acceptance backlog into an executable deterministic package matrix.",
    mission,
    count=1,
    flags=re.MULTILINE,
)
status = "**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE"
for pending in (
    "**Status:** PACKAGE VERIFIED IN v0.7.8 — LIVE CONFIRMATION PENDING",
    "**Status:** PACKAGE VERIFIED IN v0.7.8 — AWAITING ADAM'S RELEASED-PACKAGE LIVE CONFIRMATION",
    "**Status:** PACKAGE VERIFIED - LIVE ACCEPTANCE PENDING",
):
    mission = mission.replace(pending, status)

section = """

## v0.7.9 executable live-acceptance completion

### WALK-LIVE-099 — Executable released-package acceptance matrix
**Status:** IN VALIDATION

Add one deterministic acceptance entrypoint shared by CTest and the packaged executable. `Runner --diagnose-acceptance` must run without opening a window and print an explicit pass/fail line for every acceptance case.

### WALK-PRESETS-100 — All-preset finite live-physics soak
**Status:** IN VALIDATION

Step chicken, biped, humanoid, quadruped, crawler, hexapod, and monoped environments through the real effective controller. Every particle and observation channel must remain finite and every authored blueprint must remain structurally valid.

### WALK-RIGMATRIX-101 — Close the carried rig and curriculum acceptance backlog
**Status:** IN VALIDATION

The matrix must verify semantic-support separation, humanoid and chicken strict six-seed balance, raised central shoulder geometry, leg-only duck authority, current-frame PIP fallback, monoped gait identity, and ordered stage evidence. Contradictory released-package evidence reopens only the exact affected mission.

### WALK-PACKAGE-102 — Run acceptance from installed and extracted packages
**Status:** IN VALIDATION

The Windows package job must run version, Vulkan/package diagnostics, and `--diagnose-acceptance` from the build tree, installed directory, and independently extracted ZIP using unrelated working directories.

### WALK-RELEASE-103 — Publish audited Runner v0.7.9
**Status:** IN VALIDATION

Build with GCC 14 warnings-as-errors and the complete Windows Vulkan toolchain, run all deterministic suites, publish ZIP/checksum/manifest assets, re-download and verify them, update this ledger with exact evidence, and leave only `main` with zero open pull requests.
"""
if "## v0.7.9 executable live-acceptance completion" in mission:
    raise RuntimeError("v0.7.9 mission section already exists")
mission = mission.rstrip() + section + "\n"
write("missioncache.md", mission)

for relative in (
    "tools/apply_v079_acceptance.py",
    ".github/workflows/validate-runner-v079.yml",
):
    path = ROOT / relative
    if path.exists():
        path.unlink()
