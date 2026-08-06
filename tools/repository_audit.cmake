if(NOT DEFINED RUNNER_SOURCE_DIR)
    message(FATAL_ERROR "RUNNER_SOURCE_DIR was not provided")
endif()

foreach(required IN ITEMS
        AGENTS.md CHANGELOG.md missioncache.md README.md
        docs/SANDHYBRID_INTEGRATION_BRIDGE.md
        docs/RUNNER_V0716_CAMERA_BATCH.md
        docs/RUNNER_V0717_EYE_TEST_CORRECTION.md
        docs/RUNNER_V0718_RUNTIME_RECOVERY.md
        tools/generate_v0718_sources.py
        tests/v0718_runtime_recovery_tests.cpp
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

file(READ "${RUNNER_SOURCE_DIR}/CMakeLists.txt" cmake_text)
foreach(reference IN ITEMS
        "project(Runner VERSION 0.7.18 LANGUAGES CXX)"
        "generate_v0718_sources.py"
        "RunnerV0718RuntimeRecoveryTests"
        "RUNNER_V0718_RUNTIME_RECOVERY.md")
    string(FIND "${cmake_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "CMake v0.7.18 contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/missioncache.md" mission_text)
foreach(reference IN ITEMS
        "WALK-RUNTIME-RESET-211"
        "WALK-TOTAL-UPDATES-212"
        "WALK-MARKERS-214"
        "WALK-WALK-BOOTSTRAP-219"
        "WALK-RELEASE-225"
        "Runner v0.7.19 equipment, carry, and target curriculum")
    string(FIND "${mission_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Mission cache v0.7.18 contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/autonomy.hpp" autonomy_text)
foreach(reference IN ITEMS
        "nursery_policy_reset_allowed"
        "stage_fresh_updates"
        "stage_required_updates")
    string(FIND "${autonomy_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Autonomy recovery contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/ui_layout.hpp" layout_text)
string(FIND "${layout_text}" "10.0f : 15.24f" marker_pos)
if(marker_pos EQUAL -1)
    message(FATAL_ERROR "Near-course marker spacing is not the v0.7.18 contract")
endif()

file(READ "${RUNNER_SOURCE_DIR}/tools/generate_v0718_sources.py" generator_text)
foreach(reference IN ITEMS
        "EXTENDED NURSERY BUDGET EXHAUSTED"
        "TOTAL UPDATES"
        "START"
        "TAB VIEW"
        "runner-v0718-runtime-autosave.eppo"
        "optional_art_enabled{ false }")
    string(FIND "${generator_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Generated runtime recovery contract missing: ${reference}")
    endif()
endforeach()

file(GLOB release_notes "${RUNNER_SOURCE_DIR}/RELEASE_NOTES*.md")
if(release_notes)
    message(FATAL_ERROR "Per-release note files remain; CHANGELOG.md is canonical")
endif()

file(GLOB_RECURSE markdown_files "${RUNNER_SOURCE_DIR}/*.md")
foreach(path IN LISTS markdown_files)
    if(path MATCHES "/build/")
        continue()
    endif()
    get_filename_component(name "${path}" NAME)
    string(TOLOWER "${name}" lower_name)
    if(lower_name MATCHES "mission" AND NOT lower_name STREQUAL "missioncache.md")
        message(FATAL_ERROR "Duplicate mission document remains: ${path}")
    endif()
endforeach()

foreach(stale IN ITEMS
        tools/v0718.trigger
        tools/v0718.prtrigger
        tools/v0718-executor-merge-trigger.txt
        tools/apply_v0718_runtime_recovery.py
        tools/finalize_v0718_runtime_recovery.py
        .github/workflows/apply-v0718-runtime-recovery.yml)
    if(EXISTS "${RUNNER_SOURCE_DIR}/${stale}")
        message(FATAL_ERROR "Temporary v0.7.18 executor file remains: ${stale}")
    endif()
endforeach()

message(STATUS "Runner v0.7.18 repository hygiene passed")
