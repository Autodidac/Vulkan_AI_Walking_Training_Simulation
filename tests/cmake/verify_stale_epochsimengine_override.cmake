if(NOT DEFINED RUNNER_ROOT OR "${RUNNER_ROOT}" STREQUAL "")
    message(FATAL_ERROR "RUNNER_ROOT is required")
endif()
if(NOT DEFINED RUNNER_BINARY_DIR OR "${RUNNER_BINARY_DIR}" STREQUAL "")
    message(FATAL_ERROR "RUNNER_BINARY_DIR is required")
endif()

set(fixture_root "${RUNNER_ROOT}/tests/cmake/stale_epochsimengine_override")
set(fixture_project "${fixture_root}/CMakeLists.txt")
set(stale_project "${fixture_root}/fake-2.5.9/CMakeLists.txt")
foreach(required IN ITEMS "${fixture_project}" "${stale_project}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR
            "Exact-version negative fixture is incomplete: ${required}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --fresh
        "-DRUNNER_ROOT=${RUNNER_ROOT}"
        -S "${fixture_root}"
        -B "${RUNNER_BINARY_DIR}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error)

if("${configure_result}" STREQUAL "0")
    message(FATAL_ERROR
        "Stale EpochSimEngine 2.5.9 override was accepted as 2.5.29")
endif()

set(configure_transcript "${configure_output}\n${configure_error}")
string(REGEX REPLACE "[ \t\r\n]+" " " normalized_transcript
    "${configure_transcript}")
foreach(expected IN ITEMS
    "Epoch2DWalkEngine requires exact EpochSimEngine 2.5.29"
    "stale FetchContent source"
    "reports 2.5.9"
    "Clear a stale")
    string(FIND "${normalized_transcript}" "${expected}" expected_position)
    if(expected_position EQUAL -1)
        message(FATAL_ERROR
            "Stale dependency configure failed for the wrong reason; missing '${expected}'.\n"
            "--- configure transcript ---\n${configure_transcript}")
    endif()
endforeach()

message(STATUS
    "Exact EpochSimEngine guard rejected stale 2.5.9 with the required diagnostic")
