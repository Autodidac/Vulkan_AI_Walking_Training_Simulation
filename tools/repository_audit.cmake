if(NOT DEFINED RUNNER_SOURCE_DIR)
    message(FATAL_ERROR "RUNNER_SOURCE_DIR was not provided")
endif()

foreach(required IN ITEMS
        AGENTS.md CHANGELOG.md missioncache.md README.md
        docs/SANDHYBRID_INTEGRATION_BRIDGE.md
        docs/RUNNER_V0718_RUNTIME_RECOVERY.md
        docs/RUNNER_V0719_GENERAL_LOCOMOTION.md
        docs/RUNNER_V0720_UI_PREVIEW_ICON.md
        docs/RUNNER_V0721_READABLE_TELEMETRY.md
        assets/ui/runner_icon_concept.svg
        tools/generate_runner_icon.py
        tests/v0718_runtime_recovery_tests.cpp
        tests/v0719_general_locomotion_tests.cpp
        tests/v0720_ui_tests.cpp
        tests/v0721_readable_telemetry_tests.cpp
        src/locomotion_strategy.hpp
        src/preview_sync.hpp
        src/training_explainer.hpp
        src/runner_icon.rc.in
        src/ui_layout.hpp)
    if(NOT EXISTS "${RUNNER_SOURCE_DIR}/${required}")
        message(FATAL_ERROR "Missing required repository file: ${required}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/CMakeLists.txt" cmake_text)
foreach(reference IN ITEMS
        "project(Runner VERSION 0.7.21 LANGUAGES CXX)"
        "generate_runner_icon.py"
        "src/autonomy_commands.cpp"
        "src/main.cpp"
        "RunnerV0720UiTests"
        "RunnerV0721ReadableTelemetryTests"
        "RUNNER_V0720_UI_PREVIEW_ICON.md"
        "RUNNER_V0721_READABLE_TELEMETRY.md"
        "runner_icon.rc")
    string(FIND "${cmake_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "CMake v0.7.21 contract missing: ${reference}")
    endif()
endforeach()
foreach(stale IN ITEMS
        "RUNNER_GENERATED_DIR"
        "generate_v0719_sources.py"
        "generated-v0719")
    string(FIND "${cmake_text}" "${stale}" pos)
    if(NOT pos EQUAL -1)
        message(FATAL_ERROR "Stale generated-source contract remains: ${stale}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/missioncache.md" mission_text)
foreach(reference IN ITEMS
        "WALK-DPI-253"
        "WALK-CLIP-254"
        "WALK-PREVIEW-CONTINUITY-257"
        "WALK-ICON-260"
        "WALK-RELEASE-262"
        "WALK-HUMAN-STATUS-263"
        "WALK-ADVANCED-269"
        "WALK-TELEMETRY-TEST-272"
        "WALK-RELEASE-275")
    string(FIND "${mission_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Mission cache v0.7.21 contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/autonomy_runtime.cpp" runtime_text)
foreach(reference IN ITEMS
        "preview_sync::decide"
        "if (decision.replace_course)"
        "if (decision.reset_episode)")
    string(FIND "${runtime_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Preview continuity contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/main.cpp" main_text)
foreach(reference IN ITEMS
        "--diagnose-ui"
        "SDL_SetWindowIcon"
        "application.frame(input, dt, logical_width, logical_height)"
        "logical_width, logical_height")
    string(FIND "${main_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "DPI/icon runtime contract missing: ${reference}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/src/renderer.hpp" renderer_text)
foreach(reference IN ITEMS "push_clip" "canvas_width" "drawable_width")
    string(FIND "${renderer_text}" "${reference}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Renderer clipping/DPI contract missing: ${reference}")
    endif()
endforeach()

file(GLOB release_notes "${RUNNER_SOURCE_DIR}/RELEASE_NOTES*.md")
if(release_notes)
    message(FATAL_ERROR "Per-release note files remain; CHANGELOG.md is canonical")
endif()

foreach(stale IN ITEMS
        tools/generate_v0719_sources.py
        tools/apply_v0720_release.py
        tools/fix_v0720_validation.py
        .github/workflows/apply-v0720-release.yml
        .github/workflows/fix-v0720-validation.yml
        tools/apply_v0721_readable_telemetry.py
        .github/workflows/apply-v0721-readable-telemetry.yml)
    if(EXISTS "${RUNNER_SOURCE_DIR}/${stale}")
        message(FATAL_ERROR "Temporary or stale source generator remains: ${stale}")
    endif()
endforeach()

message(STATUS "Runner v0.7.21 repository hygiene passed")
