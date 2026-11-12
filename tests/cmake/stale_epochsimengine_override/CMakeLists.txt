cmake_minimum_required(VERSION 3.28)
project(RunnerStaleEpochSimEngineOverride LANGUAGES NONE)

if(NOT DEFINED RUNNER_ROOT)
    message(FATAL_ERROR "RUNNER_ROOT is required")
endif()

include(FetchContent)
set(FETCHCONTENT_SOURCE_DIR_RUNNER_SANDHYBRID
    "${CMAKE_CURRENT_SOURCE_DIR}/fake-2.5.9"
    CACHE PATH "Intentional stale override fixture" FORCE)
FetchContent_Declare(runner_sandhybrid
    GIT_REPOSITORY https://invalid.example/unused.git
    GIT_TAG unused-by-source-override)
FetchContent_MakeAvailable(runner_sandhybrid)
if(NOT TARGET EpochSimEngine::EpochSimEngine)
    message(FATAL_ERROR "Stale fixture did not create the expected target")
endif()

get_directory_property(resolved_version
    DIRECTORY "${runner_sandhybrid_SOURCE_DIR}"
    DEFINITION PROJECT_VERSION)
include("${RUNNER_ROOT}/cmake/RequireExactEpochSimEngineVersion.cmake")
runner_require_exact_epochsimengine_version(
    "${resolved_version}" "2.5.29"
    "stale FetchContent source ${runner_sandhybrid_SOURCE_DIR}")
