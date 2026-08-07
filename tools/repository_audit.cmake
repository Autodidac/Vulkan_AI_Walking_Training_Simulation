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
        docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md
        assets/ui/runner_icon_concept.svg
        tools/generate_runner_icon.py
        tests/v0718_runtime_recovery_tests.cpp
        tests/v0719_general_locomotion_tests.cpp
        tests/v0720_ui_tests.cpp
        tests/v0721_readable_telemetry_tests.cpp
        tests/v0721_rig_gait_tests.cpp
        tests/v0722_visible_frame_tests.cpp
        src/locomotion_strategy.hpp
        src/preview_sync.hpp
        src/training_explainer.hpp
        src/ui_render_contract.hpp
        src/ui_frame_probe.hpp
        src/runner_icon.rc.in
        src/ui_layout.hpp)
    if(NOT EXISTS "${RUNNER_SOURCE_DIR}/${required}")
        message(FATAL_ERROR "Missing required repository file: ${required}")
    endif()
endforeach()

file(READ "${RUNNER_SOURCE_DIR}/CMakeLists.txt" cmake_text)
foreach(reference IN ITEMS
        "project(Runner VERSION 0.7.22 LANGUAGES CXX)"
        "generate_runner_icon.py"
        "src/autonomy_commands.cpp"
        "src/main.cpp"
        "RunnerV0720UiTests"
        "RunnerV0721ReadableTelemetryTests"
        "RunnerV0722VisibleFrameTests"
        "RUNNER_V0720_UI_PREVIEW_ICON.md"
        "RUNNER_V0721_READABLE_TELEMETRY.md"
        "RUNNER_V0722_BLACK_FRAME_HOTFIX.md"
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
        "WALK-AUTO-TUNING-275"
        "WALK-SIDE-GAIT-276"
        "WALK-RIG-LAB-279"
        "WALK-RELEASE-283"
        "WALK-BLACK-FRAME-284"
        "WALK-FRAME-DIAGNOSTIC-287"
        "WALK-RELEASE-289")
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
        "visible_application_frames"
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


file(READ "${RUNNER_SOURCE_DIR}/src/app.cpp" app_text)
string(FIND "${app_text}" "Color{}" opaque_default_pos)
if(NOT opaque_default_pos EQUAL -1)
    message(FATAL_ERROR "Opaque default Color remains in application border rendering")
endif()
string(FIND "${app_text}" "ui_render::transparent_fill" transparent_fill_pos)
if(transparent_fill_pos EQUAL -1)
    message(FATAL_ERROR "Explicit transparent border-fill contract is not used")
endif()

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
        .github/workflows/apply-v0721-readable-telemetry.yml
        tools/cache_v0721_rig_repair.py
        tools/apply_v0721_rig_gait_repair.py
        tools/run_v0721_rig_gait_repair.py
        .github/workflows/cache-v0721-rig-repair.yml
        .github/workflows/apply-v0721-rig-gait-repair.yml
        tools/cache_v0722_black_frame.py
        tools/apply_v0722_black_frame_hotfix.py
        tools/add_v0722_frame_diagnostic.py
        tools/fix_v0722_validation.py
        tools/v0722.cache-trigger
        tools/v0722.cache-trigger2
        tools/v0722.cache-trigger3
        .github/workflows/cache-v0722-black-frame.yml
        .github/workflows/apply-v0722-black-frame.yml)
    if(EXISTS "${RUNNER_SOURCE_DIR}/${stale}")
        message(FATAL_ERROR "Temporary or stale source generator remains: ${stale}")
    endif()
endforeach()

message(STATUS "Runner v0.7.22 repository hygiene passed")
