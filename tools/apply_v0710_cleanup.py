from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = "0.7.10"
TODAY = "2026-08-03"
VALIDATION_WORKFLOW = ROOT / ".github/workflows/validate-runner-v0710.yml"
FINALIZER_WORKFLOW = ROOT / ".github/workflows/finalize-runner-v0710.yml"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    normalized = text.replace("\r\n", "\n").replace("\r", "\n")
    path.write_text(normalized.rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, pattern: str, replacement: str, label: str, flags: int = 0) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=flags)
    if count != 1:
        raise RuntimeError(f"Expected one {label} replacement, found {count}")
    return updated


def version_key(path: Path) -> tuple[int, ...]:
    match = re.search(r"v?(\d+)\.(\d+)\.(\d+)", path.name)
    if not match:
        return (0, 0, 0)
    return tuple(int(part) for part in match.groups())


def clean_release_body(text: str) -> str:
    lines = text.replace("\r\n", "\n").splitlines()
    while lines and not lines[0].strip():
        lines.pop(0)
    if lines and lines[0].lstrip().startswith("#"):
        lines.pop(0)
    while lines and not lines[0].strip():
        lines.pop(0)
    return "\n".join(lines).strip()


def consolidate_changelog() -> list[str]:
    release_files = sorted(
        [path for path in ROOT.glob("RELEASE_NOTES*.md") if path.is_file()],
        key=version_key,
        reverse=True,
    )

    sections: list[str] = [
        "# Changelog",
        "",
        "All notable Runner changes are recorded here. The authoritative work ledger is `missioncache.md`; this file is the single release-history document.",
        "",
        f"## [{VERSION}] - {TODAY}",
        "",
        "### Changed",
        "",
        "- Consolidated every per-release notes file into this single changelog.",
        "- Consolidated all mission documents into the single authoritative `missioncache.md` ledger.",
        "- Replaced stale and contradictory README history with current build, curriculum, validation, and repository guidance.",
        "- Simplified CMake setup and added a permanent repository-hygiene CTest.",
        "",
        "### Fixed",
        "",
        "- Corrected semantic-support overlap acceptance to use true two-dimensional clearance instead of horizontal distance only.",
        "- Prevented an empty acceptance report from passing by vacuous truth.",
        "- Expanded curriculum acceptance to verify every stage label and added matrix-shape checks.",
        "- Removed stale one-shot validation artifacts and obsolete release tooling from source control.",
    ]

    existing = ROOT / "CHANGELOG.md"
    if existing.exists():
        body = read(existing).strip()
        if body and f"## [{VERSION}]" not in body:
            sections.extend(["", "## Legacy changelog content", "", body])

    for path in release_files:
        version = ".".join(str(part) for part in version_key(path))
        body = clean_release_body(read(path))
        if body:
            sections.extend(["", f"## [{version}]", "", body])
        path.unlink()

    write(ROOT / "CHANGELOG.md", "\n".join(sections))
    return [path.name for path in release_files]


def consolidate_missions() -> list[str]:
    cache_path = ROOT / "missioncache.md"
    if not cache_path.exists():
        raise RuntimeError("missioncache.md is required")

    duplicates: list[Path] = []
    for path in ROOT.rglob("*.md"):
        if ".git" in path.parts or path == cache_path:
            continue
        if "mission" in path.name.lower():
            duplicates.append(path)

    cache = read(cache_path)
    imported_sections: list[str] = []
    for path in sorted(duplicates):
        body = read(path).strip()
        if body:
            relative = path.relative_to(ROOT).as_posix()
            imported_sections.extend([
                "",
                f"### Imported from `{relative}`",
                "",
                body,
            ])
        path.unlink()

    cache = replace_once(
        cache,
        r"\*\*Target:\*\*[^\n]*",
        f"**Target:** Runner v{VERSION}",
        "release target",
    )
    cache = replace_once(
        cache,
        r"\*\*Release state:\*\*[^\n]*",
        f"**Release state:** IN VALIDATION — Runner v{VERSION} changelog, mission-ledger, source, documentation, acceptance, package, and release cleanup.",
        "release state",
    )

    marker = "## v0.7.10 repository consolidation and cleanup"
    if marker not in cache:
        cache = cache.rstrip() + f"""

{marker}

### WALK-CHANGELOG-104 — Single authoritative changelog
**Status:** IN VALIDATION

Merge every `RELEASE_NOTES*.md` document into `CHANGELOG.md`, preserve release history, package the changelog, and prevent per-release note files from returning.

### WALK-CACHE-105 — Single authoritative mission cache
**Status:** IN VALIDATION

`missioncache.md` is the only mission-named Markdown document. Any legacy mission ledger is imported before deletion, and unfinished work remains explicit rather than disappearing during cleanup.

### WALK-SOURCE-106 — Source and documentation cleanup
**Status:** IN VALIDATION

Remove stale release triggers and validation debris, simplify duplicated CMake setup, replace contradictory README history, normalize text hygiene, and retain current build and runtime instructions.

### WALK-ACCEPT-107 — Harden executable acceptance
**Status:** IN VALIDATION

Use true two-dimensional semantic-support clearance, reject an empty acceptance report, verify every curriculum stage label, and retain deterministic live acceptance across all authored presets.

### WALK-RELEASE-108 — Publish audited Runner v0.7.10
**Status:** IN VALIDATION

Pass Linux warnings-as-errors, the complete Windows Vulkan build and tests, repository hygiene, build-tree/installed/extracted acceptance diagnostics, package manifest and checksum verification, release re-download audit, branch cleanup, and open-PR audit.
"""

    if imported_sections:
        cache = cache.rstrip() + "\n\n## Imported legacy mission ledgers\n" + "\n".join(imported_sections)

    write(cache_path, cache)
    return [path.relative_to(ROOT).as_posix() for path in duplicates]


def update_cmake() -> None:
    path = ROOT / "CMakeLists.txt"
    text = read(path)
    text = re.sub(
        r"cmake_minimum_required\(VERSION 3\.28\)\n\n(?:#.*\n)?if\(MSVC\)\n\s*set\(CMAKE_CXX_SCAN_FOR_MODULES OFF\)\nelse\(\)\n\s*set\(CMAKE_CXX_SCAN_FOR_MODULES OFF\)\nendif\(\)\n\nproject\(Runner VERSION [^)]+ LANGUAGES CXX\)",
        f"cmake_minimum_required(VERSION 3.28)\n\nset(CMAKE_CXX_SCAN_FOR_MODULES OFF)\n\nproject(Runner VERSION {VERSION} LANGUAGES CXX)",
        text,
        count=1,
    )
    if f"project(Runner VERSION {VERSION} LANGUAGES CXX)" not in text:
        text = replace_once(
            text,
            r"project\(Runner VERSION [^)]+ LANGUAGES CXX\)",
            f"project(Runner VERSION {VERSION} LANGUAGES CXX)",
            "project version",
        )
    if "Runner.RepositoryHygiene" not in text:
        text = replace_once(
            text,
            r"(if\(RUNNER_BUILD_TESTS\)\n\s*include\(CTest\)\n\s*enable_testing\(\)\n)",
            r"\1    add_test(NAME Runner.RepositoryHygiene\n        COMMAND ${CMAKE_COMMAND}\n            -DRUNNER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}\n            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/repository_audit.cmake)\n    set_tests_properties(Runner.RepositoryHygiene PROPERTIES TIMEOUT 30)\n\n",
            "repository hygiene test",
        )
    write(path, text)


def update_acceptance() -> None:
    path = ROOT / "src/acceptance.cpp"
    text = read(path)
    text = replace_once(
        text,
        r"const float clearance = std::abs\(\n\s*blueprint\.nodes\[second\]\.x - blueprint\.nodes\[first\]\.x\)\n\s*- first_radius - second_radius;",
        "const float delta_x = blueprint.nodes[second].x - blueprint.nodes[first].x;\n                    const float delta_y = blueprint.nodes[second].y - blueprint.nodes[first].y;\n                    const float clearance = std::hypot(delta_x, delta_y)\n                        - first_radius - second_radius;",
        "two-dimensional support clearance",
    )
    text = replace_once(
        text,
        r"return std::ranges::all_of\(cases,\n\s*\[\]\(const CaseResult& result\) \{ return result\.passed; \}\);",
        "return !cases.empty() && std::ranges::all_of(cases,\n            [](const CaseResult& result) { return result.passed; });",
        "non-empty report gate",
    )
    old = """            && sim::course_stage_name(sim::CourseStage::crouch_walk)
                == \"4. CROUCH WALK / UNEVEN AVOID\"
            && sim::stage_skill_evidence(sim::CourseStage::balance,"""
    new = """            && sim::course_stage_name(sim::CourseStage::crouch_walk)
                == \"4. CROUCH WALK / UNEVEN AVOID\"
            && sim::course_stage_name(sim::CourseStage::ramps) == \"5. JUMP / LAND\"
            && sim::course_stage_name(sim::CourseStage::hurdles)
                == \"6. MOVING LOW BAR / HURDLE\"
            && sim::course_stage_name(sim::CourseStage::duck_bars)
                == \"7. CONTROLLED FLIPS\"
            && sim::course_stage_name(sim::CourseStage::moving_hazards)
                == \"8. MIXED GOAL COURSE\"
            && sim::stage_skill_evidence(sim::CourseStage::balance,"""
    if old not in text:
        raise RuntimeError("Could not expand curriculum stage-name acceptance")
    text = text.replace(old, new, 1)
    write(path, text)

    test_path = ROOT / "tests/live_acceptance_tests.cpp"
    test = read(test_path)
    if "std::unordered_set" not in test:
        test = test.replace("#include <iostream>\n", "#include <iostream>\n#include <string>\n#include <unordered_set>\n")
        test = test.replace(
            "    for (const runner::acceptance::CaseResult& result : report.cases)\n",
            "    if (report.cases.size() < 10u)\n    {\n        std::cerr << \"Acceptance matrix unexpectedly contains only \"\n            << report.cases.size() << \" cases\\n\";\n        return EXIT_FAILURE;\n    }\n\n    std::unordered_set<std::string> names{};\n    for (const runner::acceptance::CaseResult& result : report.cases)\n",
            1,
        )
        test = test.replace(
            "        std::cout << (result.passed ? \"[PASS] \" : \"[FAIL] \")\n",
            "        if (!names.insert(result.name).second)\n        {\n            std::cerr << \"Duplicate acceptance case: \" << result.name << '\\n';\n            return EXIT_FAILURE;\n        }\n        std::cout << (result.passed ? \"[PASS] \" : \"[FAIL] \")\n",
            1,
        )
    write(test_path, test)


def write_repository_audit() -> None:
    content = r'''if(NOT DEFINED RUNNER_SOURCE_DIR)
    message(FATAL_ERROR "RUNNER_SOURCE_DIR was not provided")
endif()

file(GLOB release_notes "${RUNNER_SOURCE_DIR}/RELEASE_NOTES*.md")
if(release_notes)
    list(JOIN release_notes ", " release_note_list)
    message(FATAL_ERROR "Per-release note files remain: ${release_note_list}")
endif()

foreach(required IN ITEMS CHANGELOG.md missioncache.md README.md)
    if(NOT EXISTS "${RUNNER_SOURCE_DIR}/${required}")
        message(FATAL_ERROR "Missing required repository document: ${required}")
    endif()
endforeach()

file(GLOB_RECURSE markdown_files "${RUNNER_SOURCE_DIR}/*.md")
set(mission_documents "")
foreach(path IN LISTS markdown_files)
    get_filename_component(name "${path}" NAME)
    string(TOLOWER "${name}" lower_name)
    if(lower_name MATCHES "mission" AND NOT lower_name STREQUAL "missioncache.md")
        list(APPEND mission_documents "${path}")
    endif()
endforeach()
if(mission_documents)
    list(JOIN mission_documents ", " mission_document_list)
    message(FATAL_ERROR "Duplicate mission documents remain: ${mission_document_list}")
endif()

file(READ "${RUNNER_SOURCE_DIR}/CMakeLists.txt" cmake_text)
string(FIND "${cmake_text}" "project(Runner VERSION 0.7.10 LANGUAGES CXX)" version_position)
if(version_position EQUAL -1)
    message(FATAL_ERROR "CMake project version is not 0.7.10")
endif()

file(READ "${RUNNER_SOURCE_DIR}/README.md" readme_text)
foreach(reference IN ITEMS "CHANGELOG.md" "missioncache.md" "--diagnose-acceptance")
    string(FIND "${readme_text}" "${reference}" reference_position)
    if(reference_position EQUAL -1)
        message(FATAL_ERROR "README is missing required reference: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/acceptance.cpp" acceptance_text)
foreach(reference IN ITEMS "std::hypot" "!cases.empty()" "8. MIXED GOAL COURSE")
    string(FIND "${acceptance_text}" "${reference}" reference_position)
    if(reference_position EQUAL -1)
        message(FATAL_ERROR "Acceptance hardening is missing: ${reference}")
    endif()
endforeach()

message(STATUS "Runner repository hygiene passed")
'''
    write(ROOT / "tools/repository_audit.cmake", content)


def rewrite_readme() -> None:
    content = f'''# Runner

Runner {VERSION} is a C++23 SDL3/Vulkan locomotion laboratory with deterministic physics, a compact PPO trainer, persistent background workers, authored multi-leg rigs, deformable terrain, material hazards, and an executable acceptance matrix.

## Current curriculum

Training uses eight evidence-gated stages:

1. stand and balance;
2. static crouch, hold, and recover;
3. walk and run with real gait cycles;
4. crouch-walk over deformable uneven terrain and avoid low obstacles;
5. powered jump and controlled landing;
6. moving low-bar or hurdle traversal;
7. controlled flips with no more than three turns and a valid landing;
8. mixed traversal that preserves earlier skills.

Scalar reward cannot skip prerequisites. Wheel-sliding, body rolling, detached or fused supports, uncontrolled flight, excessive flips, motionless exploits, and invalid body contact cannot seed champion, imitation, evolution, or training-preview state.

## Authored rigs

The built-in presets are chicken, biped, humanoid, quadruped, four-leg crawler, hexapod, and monoped. Every preset has explicit support semantics and a rig-specific control path. The monoped uses a single-leg gait cycle rather than fake alternating biped steps.

## Runtime model

- The training worker owns mutable PPO, optimizer, curriculum, rig-evolution, and persistence state.
- The UI renders immutable publications and does not block on training updates.
- NORMAL, FASTER, and MAX select persistent CPU worker budgets.
- Vulkan is used for presentation; the compact policy and optimizer remain on CPU.
- Checkpoints and autosaves are versioned and written asynchronously through temporary-file and atomic-rename replacement.

## Terrain and hazards

A deterministic deformable sand heightfield drives collision, observations, evaluation, replay, live rendering, and the training PIP. Foot pressure compacts and displaces material. Falling sand deposits into terrain; rocks and debris bounce, roll, settle, and transfer impact velocity. Burial, obstruction, incoming material, and escape direction are observable, and no-escape burial terminates honestly.

## Controls

- `1`: Live Autopilot
- `2` or `3`: Rig Lab
- `Space`: Pause or resume background training
- `R`: Reset the live preview
- `S`: Save the current rig
- `L`: Load a rig
- `Delete`: Remove the selected non-required node
- `Shift + click`: Add a node
- `Ctrl + click`: Connect the selected node to another node

## Build and test

Requirements: CMake 3.28+, a C++23 compiler, Ninja, Vulkan 1.3+, SDL3, shaderc, and vcpkg manifest mode for the complete application.

Windows:

```powershell
cmake --preset windows-release --fresh
cmake --build --preset windows-release --parallel
ctest --preset windows-release --output-on-failure
```

Linux deterministic core suite:

```bash
cmake -S . -B build/linux -G Ninja \\
  -DRUNNER_BUILD_APP=OFF \\
  -DRUNNER_BUILD_TESTS=ON \\
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --parallel
ctest --test-dir build/linux --output-on-failure
```

## Run and diagnose

Double-click `run.bat` on Windows. An extracted release launches the adjacent executable; a source checkout uses or builds the configured Release target. Arguments are forwarded.

Useful diagnostics:

```text
Runner.exe --version
Runner.exe --diagnose-vulkan
Runner.exe --diagnose-package
Runner.exe --diagnose-acceptance
```

`--diagnose-acceptance` runs the same deterministic rig and curriculum matrix used by CTest and release-package auditing.

## Repository records

- [`CHANGELOG.md`](CHANGELOG.md) is the single release-history document.
- [`missioncache.md`](missioncache.md) is the single authoritative mission ledger with status, acceptance criteria, and immutable release evidence.

A release is incomplete until Linux and Windows tests, build-tree and installed diagnostics, independent archive extraction, checksum and manifest audits, release-asset re-download, branch cleanup, and open-PR audit all pass.
'''
    write(ROOT / "README.md", content)


def remove_stale_artifacts() -> list[str]:
    removed: list[str] = []
    patterns = [
        "VALIDATION_v*.txt",
        "VALIDATION_v*_FAILURE.txt",
        ".github/workflows/validate-runner-v0*.yml",
        ".github/workflows/finalize-runner-v0*.yml",
        ".github/workflows/publish-runner-v0*.yml",
        "tools/apply_v0*.py",
        "tools/publish_v0*.ps1",
    ]
    protected = {Path(__file__).resolve(), FINALIZER_WORKFLOW.resolve()}
    for pattern in patterns:
        for path in ROOT.glob(pattern):
            if not path.is_file() or path.resolve() in protected:
                continue
            removed.append(path.relative_to(ROOT).as_posix())
            path.unlink()
    return removed


def normalize_text_files() -> None:
    suffixes = {".md", ".txt", ".cmake", ".cpp", ".hpp", ".h", ".c", ".py", ".yml", ".yaml", ".json"}
    ignored_parts = {".git", "build", "vcpkg", "out"}
    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in suffixes:
            continue
        if any(part in ignored_parts for part in path.parts):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        lines = [line.rstrip() for line in text.replace("\r\n", "\n").replace("\r", "\n").split("\n")]
        write(path, "\n".join(lines))


def main() -> None:
    removed_notes = consolidate_changelog()
    merged_missions = consolidate_missions()
    update_cmake()
    update_acceptance()
    write_repository_audit()
    rewrite_readme()
    removed_artifacts = remove_stale_artifacts()
    normalize_text_files()

    if VALIDATION_WORKFLOW.exists():
        VALIDATION_WORKFLOW.unlink()
    Path(__file__).unlink()

    print(f"Consolidated {len(removed_notes)} release-note files")
    print(f"Merged {len(merged_missions)} duplicate mission documents")
    print(f"Removed {len(removed_artifacts)} stale release artifacts")


if __name__ == "__main__":
    main()
