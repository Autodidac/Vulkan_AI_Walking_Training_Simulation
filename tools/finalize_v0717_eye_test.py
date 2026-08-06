#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding='utf-8')


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding='utf-8', newline='\n')


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{label}: expected one match, found {count}')
    return text.replace(old, new, 1)


def patch_cmake() -> None:
    text = read('CMakeLists.txt')
    camera_copy = '''        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0716_CAMERA_BATCH.md"
            "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0716_CAMERA_BATCH.md"
'''
    text = replace_once(
        text,
        camera_copy,
        camera_copy + '''        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md"
            "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md"
''',
        'post-build v0.7.17 document',
    )
    text = replace_once(
        text,
        '''        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0716_CAMERA_BATCH.md"
        DESTINATION docs)''',
        '''        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0716_CAMERA_BATCH.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md"
        DESTINATION docs)''',
        'install v0.7.17 document',
    )
    text = replace_once(
        text,
        '''    add_test(NAME runner_v0717_eye_test COMMAND RunnerV0717EyeTestTests)
set_tests_properties(Runner.ViewCamera PROPERTIES TIMEOUT 30)''',
        '''    add_test(NAME Runner.V0717EyeTest
        COMMAND RunnerV0717EyeTestTests "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    set_tests_properties(Runner.V0717EyeTest PROPERTIES TIMEOUT 180)
set_tests_properties(Runner.ViewCamera PROPERTIES TIMEOUT 30)''',
        'v0.7.17 test asset path',
    )
    write('CMakeLists.txt', text)


def patch_main() -> None:
    text = read('src/main.cpp')
    anchor = '''        if (!packaged_art.loaded())
        {
            error = "Packaged Runner artwork decoded incompletely";
            return false;
        }
        error.clear();
        return true;
'''
    replacement = '''        if (!packaged_art.loaded())
        {
            error = "Packaged Runner artwork decoded incompletely";
            return false;
        }

        const std::filesystem::path optional_root = asset_directory / "optional"
            / "runner_armor_concepts";
        filesystem_error.clear();
        if (std::filesystem::is_directory(optional_root, filesystem_error))
        {
            const std::array optional_metadata{
                std::filesystem::path{ "PROVENANCE.md" },
                std::filesystem::path{ "source" } / "concept_modular_pair.ppm",
                std::filesystem::path{ "source" } / "concept_humanoid_parts.ppm",
                std::filesystem::path{ "source" } / "concept_helmeted_parts.ppm",
                std::filesystem::path{ "source" } / "concept_pixel_parts.ppm"
            };
            for (const std::filesystem::path& relative : optional_metadata)
            {
                const std::filesystem::path absolute = optional_root / relative;
                filesystem_error.clear();
                if (!std::filesystem::is_regular_file(absolute, filesystem_error))
                {
                    error = "Incomplete optional Runner art package: "
                        + absolute.string();
                    return false;
                }
            }

            const std::array optional_runtime{
                std::filesystem::path{ "runtime" } / "foot_side.ppm",
                std::filesystem::path{ "runtime" } / "helmet_side.ppm",
                std::filesystem::path{ "runtime" } / "torso_side.ppm",
                std::filesystem::path{ "runtime" } / "weapon_side.ppm"
            };
            for (const std::filesystem::path& relative : optional_runtime)
            {
                runner::art::PixelArt optional_art{};
                if (!runner::art::load_p3_pixel_art(
                        optional_root / relative, optional_art, error)
                    || !optional_art.loaded())
                {
                    if (error.empty())
                        error = "Optional Runner art decoded incompletely: "
                            + (optional_root / relative).string();
                    return false;
                }
            }
        }
        filesystem_error.clear();
        error.clear();
        return true;
'''
    text = replace_once(text, anchor, replacement,
        'conditional optional-art package validation')
    write('src/main.cpp', text)


def write_repository_audit() -> None:
    content = r'''if(NOT DEFINED RUNNER_SOURCE_DIR)
    message(FATAL_ERROR "RUNNER_SOURCE_DIR was not provided")
endif()

foreach(required IN ITEMS
        AGENTS.md CHANGELOG.md missioncache.md README.md
        docs/SANDHYBRID_INTEGRATION_BRIDGE.md
        docs/RUNNER_V0716_CAMERA_BATCH.md
        docs/RUNNER_V0717_EYE_TEST_CORRECTION.md
        assets/optional/runner_armor_concepts/PROVENANCE.md
        assets/optional/runner_armor_concepts/source/concept_modular_pair.ppm
        assets/optional/runner_armor_concepts/source/concept_humanoid_parts.ppm
        assets/optional/runner_armor_concepts/source/concept_helmeted_parts.ppm
        assets/optional/runner_armor_concepts/source/concept_pixel_parts.ppm
        assets/optional/runner_armor_concepts/runtime/foot_side.ppm
        assets/optional/runner_armor_concepts/runtime/helmet_side.ppm
        assets/optional/runner_armor_concepts/runtime/torso_side.ppm
        assets/optional/runner_armor_concepts/runtime/weapon_side.ppm)
    if(NOT EXISTS "${RUNNER_SOURCE_DIR}/${required}")
        message(FATAL_ERROR "Missing required repository file: ${required}")
    endif()
endforeach()

set(hash_contracts
    "assets/optional/runner_armor_concepts/source/concept_modular_pair.ppm|09c79a263713d0ed7e6274289d8a1bc2fd7e7fd2b61c02dee7519a57862d2227"
    "assets/optional/runner_armor_concepts/source/concept_humanoid_parts.ppm|28a21f48f1bafc97c962029fc0e5c35ab751d604cf375d99a676d615e3e18068"
    "assets/optional/runner_armor_concepts/source/concept_helmeted_parts.ppm|87b6356c22133615e572048bf50c940e8c8fce015c76e0ec90880a0a0189f3d5"
    "assets/optional/runner_armor_concepts/source/concept_pixel_parts.ppm|54ddcd8ce644cc5ca874ca1eeafe82ee4bd058bf9d3ce2a3c8839dc041d5e526"
    "assets/optional/runner_armor_concepts/runtime/foot_side.ppm|faae3a8fb5de1cd7544c370c451fd1dd7fccdb01c49ac5b00de2f51b3d55639d"
    "assets/optional/runner_armor_concepts/runtime/helmet_side.ppm|4b11a8568574670b379e9d4f2e2d7e292b7d93449391068d707d4e6122167ae1"
    "assets/optional/runner_armor_concepts/runtime/torso_side.ppm|1d18ca5be0dbb57d5ea99ef90c9c06684dda4b77c7c066d19466fa90b2bbdeec"
    "assets/optional/runner_armor_concepts/runtime/weapon_side.ppm|83de701268e712149c7f909fea7c01f8026d30f6cab0385715049b76061c3b0e")
foreach(contract IN LISTS hash_contracts)
    string(REPLACE "|" ";" fields "${contract}")
    list(GET fields 0 path)
    list(GET fields 1 expected)
    file(SHA256 "${RUNNER_SOURCE_DIR}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Optional art hash mismatch for ${path}: ${actual}")
    endif()
endforeach()

file(GLOB release_notes "${RUNNER_SOURCE_DIR}/RELEASE_NOTES*.md")
if(release_notes)
    list(JOIN release_notes ", " release_note_list)
    message(FATAL_ERROR "Per-release note files remain: ${release_note_list}")
endif()

file(GLOB_RECURSE markdown_files "${RUNNER_SOURCE_DIR}/*.md")
set(mission_documents "")
foreach(path IN LISTS markdown_files)
    if(path MATCHES "/build/")
        continue()
    endif()
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

foreach(stale IN ITEMS artifact published-audit published-final published-preliminary release-stage validation)
    if(EXISTS "${RUNNER_SOURCE_DIR}/${stale}")
        message(FATAL_ERROR "Stale release directory remains: ${stale}")
    endif()
endforeach()
if(EXISTS "${RUNNER_SOURCE_DIR}/src/autonomy.cpp")
    message(FATAL_ERROR "Obsolete monolithic autonomy.cpp remains")
endif()

file(READ "${RUNNER_SOURCE_DIR}/CMakeLists.txt" cmake_text)
string(FIND "${cmake_text}" "project(Runner VERSION 0.7.17 LANGUAGES CXX)" version_position)
if(version_position EQUAL -1)
    message(FATAL_ERROR "CMake project version is not 0.7.17")
endif()

file(READ "${RUNNER_SOURCE_DIR}/src/ppo.hpp" ppo_text)
string(FIND "${ppo_text}" "training_semantics_version = 0x0007'1700u" semantics_position)
if(semantics_position EQUAL -1)
    message(FATAL_ERROR "Training semantics are not isolated for v0.7.17")
endif()

file(READ "${RUNNER_SOURCE_DIR}/README.md" readme_text)
foreach(reference IN ITEMS
        "AGENTS.md" "CHANGELOG.md" "missioncache.md"
        "SANDHYBRID_INTEGRATION_BRIDGE.md"
        "RUNNER_V0716_CAMERA_BATCH.md"
        "RUNNER_V0717_EYE_TEST_CORRECTION.md"
        "--diagnose-acceptance" "--diagnose-camera"
        "OPTIONAL ART")
    string(FIND "${readme_text}" "${reference}" reference_position)
    if(reference_position EQUAL -1)
        message(FATAL_ERROR "README is missing required reference: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/missioncache.md" mission_text)
foreach(reference IN ITEMS "WALK-QUAD-CROUCH-181" "WALK-RELEASE-210"
        "# Runner v0.7.18 equipment, carry, and target curriculum")
    string(FIND "${mission_text}" "${reference}" reference_position)
    if(reference_position EQUAL -1)
        message(FATAL_ERROR "Mission cache is missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/acceptance.cpp" acceptance_text)
foreach(reference IN ITEMS "std::hypot" "!cases.empty()" "8. MIXED GOAL COURSE")
    string(FIND "${acceptance_text}" "${reference}" reference_position)
    if(reference_position EQUAL -1)
        message(FATAL_ERROR "Acceptance hardening is missing: ${reference}")
    endif()
endforeach()

foreach(stale IN ITEMS
        tools/finalize_v0717_eye_test.py
        tools/materialize_v0717_optional_art.py
        tools/runner-v0717-release.generated.yml
        .github/workflows/finalize-v0717-eye-test.yml
        .github/workflows/apply-v0717-eye-test.yml
        .github/workflows/runner-v0716-release.yml)
    if(EXISTS "${RUNNER_SOURCE_DIR}/${stale}")
        message(FATAL_ERROR "Temporary or superseded v0.7.17 file remains: ${stale}")
    endif()
endforeach()

message(STATUS "Runner repository hygiene passed")
'''
    write('tools/repository_audit.cmake', content)


def patch_readme_and_doc() -> None:
    text = read('README.md')
    if 'concept_modular_pair.ppm' not in text:
        text += '''

The optional package includes compact P3 reference sheets derived from all four supplied concepts under `assets/optional/runner_armor_concepts/source/`, plus validated P3 foot, helmet, torso, and weapon-preview sprites under `runtime/`. Removing the entire optional directory preserves procedural rendering and all training behavior.
'''
    write('README.md', text)

    doc = read('docs/RUNNER_V0717_EYE_TEST_CORRECTION.md')
    doc = doc.replace(
        'The four supplied concept sheets are packaged under\n`assets/optional/runner_armor_concepts/source/`.',
        'All four supplied concept sheets are represented by compact derived P3 references under\n`assets/optional/runner_armor_concepts/source/`, with original and packaged SHA-256 values recorded in `PROVENANCE.md`.',
    )
    write('docs/RUNNER_V0717_EYE_TEST_CORRECTION.md', doc)


def patch_missioncache() -> None:
    text = read('missioncache.md')
    text = text.replace(
        '**Release state:** CACHED BEFORE IMPLEMENTATION — five screenshot-reopened failures plus 25 additional compatible missions. The equipment/carry/target curriculum remains intact and is renumbered to v0.7.18 rather than discarded.',
        '**Release state:** IMPLEMENTED — CROSS-PLATFORM, PACKAGE, AND RELEASE VALIDATION PENDING. The equipment/carry/target curriculum remains intact for v0.7.18.',
        1,
    )
    text = text.replace(
        'Package the four supplied armor/character/weapon concept sheets under `assets/optional/runner_armor_concepts/source/` with stable filenames, a provenance/readme file, dimensions, SHA-256 values, and an explicit statement that they are optional user-supplied references.',
        'Package compact derived P3 references from all four supplied armor/character/weapon concept sheets under `assets/optional/runner_armor_concepts/source/` with stable filenames, provenance, dimensions, SHA-256 values, and an explicit visual-only boundary.',
        1,
    )
    for mission in range(181, 210):
        pattern = re.compile(
            rf'(### WALK-[^\n]+-{mission} — [^\n]+\n)'
            rf'\*\*Status:\*\* [^\n]+'
        )
        text, count = pattern.subn(
            rf'\1**Status:** IMPLEMENTED — VALIDATION PENDING',
            text,
            count=1,
        )
        if count != 1:
            raise RuntimeError(f'mission {mission} status not found')
    release_pattern = re.compile(
        r'(### WALK-RELEASE-210 — [^\n]+\n)\*\*Status:\*\* [^\n]+'
    )
    text, count = release_pattern.subn(
        r'\1**Status:** OPEN — RELEASE BLOCKING', text, count=1)
    if count != 1:
        raise RuntimeError('release mission 210 status not found')
    write('missioncache.md', text)


def main() -> int:
    patch_cmake()
    patch_main()
    write_repository_audit()
    patch_readme_and_doc()
    patch_missioncache()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
