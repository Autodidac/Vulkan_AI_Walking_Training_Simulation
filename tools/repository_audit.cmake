if(NOT DEFINED RUNNER_SOURCE_DIR)
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
