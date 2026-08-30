cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
project(Epoch2DWalkEngine VERSION 0.7.48 LANGUAGES CXX)

option(RUNNER_BUILD_APP "Build the SDL3/Vulkan Runner application." ON)
option(RUNNER_BUILD_TESTS "Build deterministic core tests." ON)
option(RUNNER_ENABLE_TSAN "Enable ThreadSanitizer for CPU-only tests." OFF)

find_package(Threads REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(RUNNER_GENERATED_ASSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated-assets")
file(MAKE_DIRECTORY "${RUNNER_GENERATED_ASSET_DIR}")
execute_process(
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_runner_icon.py"
        "${RUNNER_GENERATED_ASSET_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE RUNNER_ICON_RESULT)
if(NOT RUNNER_ICON_RESULT EQUAL 0)
    message(FATAL_ERROR "Runner screenshot icon generation failed")
endif()
set(RUNNER_ICON_SOURCE "${RUNNER_GENERATED_ASSET_DIR}/runner_icon_source.png")
set(RUNNER_ICON_SOURCE_SHA256 "${RUNNER_GENERATED_ASSET_DIR}/runner_icon_source.sha256")
set(RUNNER_ICON_PNG "${RUNNER_GENERATED_ASSET_DIR}/runner_icon.png")
set(RUNNER_ICON_PNG_512 "${RUNNER_GENERATED_ASSET_DIR}/runner_icon_512.png")
set(RUNNER_ICON_BMP "${RUNNER_GENERATED_ASSET_DIR}/runner_icon.bmp")
set(RUNNER_ICON_ICO "${RUNNER_GENERATED_ASSET_DIR}/runner.ico")

include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(RUNNER_SANDHYBRID_PACKAGE_ROOT "$ENV{RUNNER_SANDHYBRID_PACKAGE_ROOT}"
    CACHE PATH "Offline EpochSimEngine v2.5.26 package root containing lib/cmake/EpochSimEngine.")
set(RUNNER_SANDHYBRID_SOURCE_ID "14f55d30df3e406215facf069c8b50082d7439eb")
if(RUNNER_SANDHYBRID_PACKAGE_ROOT)
    find_package(EpochSimEngine 2.5.26 EXACT CONFIG REQUIRED
        PATHS "${RUNNER_SANDHYBRID_PACKAGE_ROOT}" NO_DEFAULT_PATH)
    set(runner_sandhybrid_SOURCE_DIR "${RUNNER_SANDHYBRID_PACKAGE_ROOT}")
    message(STATUS "Using offline EpochSimEngine ${EpochSimEngine_VERSION} at ${RUNNER_SANDHYBRID_PACKAGE_ROOT}")
else()
    set(SANDHYBRID_BUILD_APP OFF CACHE BOOL "" FORCE)
    set(SANDHYBRID_BUILD_VULKAN_RUNTIME OFF CACHE BOOL "" FORCE)
    set(SANDHYBRID_WARNINGS_AS_ERRORS ON CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(runner_sandhybrid
        GIT_REPOSITORY https://epoch.adamrushford.chatgpt.site/git/simengine.git
        GIT_TAG 14f55d30df3e406215facf069c8b50082d7439eb
        GIT_SHALLOW FALSE
        GIT_SUBMODULES ""
        GIT_SUBMODULES_RECURSE FALSE)
    FetchContent_MakeAvailable(runner_sandhybrid)
endif()
if(NOT TARGET EpochSimEngine::EpochSimEngine)
    message(FATAL_ERROR "EpochSimEngine v2.5.26 core target was not created")
endif()

add_library(RunnerCore STATIC
    src/simulation.cpp
    src/pixel_art.cpp
    src/acceptance.cpp
    src/ppo_network.cpp
    src/ppo_trainer.cpp
    src/ppo_parallel.cpp
    src/rig_training_diagnostic.cpp
    src/course_completion_diagnostic.cpp
    src/hybrid_brain_diagnostic.cpp
    src/self_imitation.cpp
    src/training_checkpoint.cpp
    src/autonomy_runtime.cpp
    src/autonomy_commands.cpp
    src/autonomy_curriculum.cpp
    src/autonomy_persistence.cpp)
add_library(Runner::Core ALIAS RunnerCore)
add_library(Epoch2DWalkEngine::Epoch2DWalkEngine ALIAS RunnerCore)
target_include_directories(RunnerCore PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Epoch2DWalkEngine>")
target_link_libraries(RunnerCore PUBLIC Threads::Threads EpochSimEngine::EpochSimEngine)
target_compile_features(RunnerCore PUBLIC cxx_std_23)
target_compile_definitions(RunnerCore PUBLIC
    RUNNER_SANDHYBRID_SOURCE_COMMIT="${RUNNER_SANDHYBRID_SOURCE_ID}")
set_target_properties(RunnerCore PROPERTIES
    OUTPUT_NAME Epoch2DWalkEngine
    EXPORT_NAME Epoch2DWalkEngine
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON)

function(runner_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
    endif()
endfunction()
runner_enable_warnings(RunnerCore)

if(RUNNER_ENABLE_TSAN AND NOT MSVC)
    target_compile_options(RunnerCore PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(RunnerCore PRIVATE -fsanitize=thread)
endif()

if(RUNNER_BUILD_APP)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(Vulkan 1.3 REQUIRED)
    find_package(unofficial-shaderc CONFIG REQUIRED)

    add_executable(RunnerShaderCompiler tools/shader_compiler.cpp)
    target_link_libraries(RunnerShaderCompiler PRIVATE unofficial::shaderc::shaderc)
    target_compile_features(RunnerShaderCompiler PRIVATE cxx_std_23)
    set_target_properties(RunnerShaderCompiler PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    runner_enable_warnings(RunnerShaderCompiler)

    set(RUNNER_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    set(RUNNER_SHADER_OUTPUTS)
    foreach(shader IN ITEMS flat.vert flat.frag)
        set(source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}")
        set(output "${RUNNER_SHADER_OUTPUT_DIR}/${shader}.spv")
        add_custom_command(
            OUTPUT "${output}"
            COMMAND "$<TARGET_FILE:RunnerShaderCompiler>" "${source}" "${output}"
            DEPENDS "${source}" RunnerShaderCompiler
            VERBATIM
            COMMENT "Compiling ${shader} with vcpkg shaderc")
        list(APPEND RUNNER_SHADER_OUTPUTS "${output}")
    endforeach()
    add_custom_target(RunnerShaders DEPENDS ${RUNNER_SHADER_OUTPUTS})

    add_executable(RunnerRigDefaults tools/rig_defaults.cpp)
    target_link_libraries(RunnerRigDefaults PRIVATE Runner::Core)
    target_include_directories(RunnerRigDefaults PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerRigDefaults PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerRigDefaults)

    set(RUNNER_RIG_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/default-rigs")
    set(RUNNER_RIG_OUTPUTS
        "${RUNNER_RIG_OUTPUT_DIR}/human.rig"
        "${RUNNER_RIG_OUTPUT_DIR}/chicken.rig"
        "${RUNNER_RIG_OUTPUT_DIR}/dog.rig"
        "${RUNNER_RIG_OUTPUT_DIR}/hexapod.rig")
    add_custom_command(
        OUTPUT ${RUNNER_RIG_OUTPUTS}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RUNNER_RIG_OUTPUT_DIR}"
        COMMAND "$<TARGET_FILE:RunnerRigDefaults>" "${RUNNER_RIG_OUTPUT_DIR}"
        DEPENDS RunnerRigDefaults
        VERBATIM
        COMMENT "Generating validated species rig defaults")
    add_custom_target(RunnerRigFiles ALL DEPENDS ${RUNNER_RIG_OUTPUTS})

    add_executable(Runner
        src/main.cpp
        src/app.cpp
        src/canvas.cpp src/renderer.cpp
        src/app.hpp src/pixel_art.hpp src/renderer.hpp src/math.hpp
        src/ui_font.hpp src/ui_layout.hpp src/view_camera.hpp
        src/locomotion_strategy.hpp src/preview_sync.hpp
        src/species_art_layout.hpp
        src/training_explainer.hpp src/ui_render_contract.hpp
        src/ui_frame_probe.hpp)
    add_dependencies(Runner RunnerShaders RunnerRigFiles)
    target_link_libraries(Runner PRIVATE Runner::Core SDL3::SDL3 Vulkan::Vulkan)
    target_compile_features(Runner PRIVATE cxx_std_23)
    target_compile_definitions(Runner PRIVATE
        RUNNER_VERSION="${PROJECT_VERSION}"
        RUNNER_SHADER_DIRECTORY="shaders"
        RUNNER_ASSET_DIRECTORY="assets")
    set_target_properties(Runner PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Runner>")
    runner_enable_warnings(Runner)

    if(WIN32)
        string(REPLACE "\\" "/" RUNNER_ICON_ICO_RC "${RUNNER_ICON_ICO}")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/runner_icon.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/runner_icon.rc" @ONLY)
        target_sources(Runner PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/runner_icon.rc")
    endif()

    add_custom_command(TARGET Runner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:Runner>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_RIG_OUTPUT_DIR}/human.rig" "$<TARGET_FILE_DIR:Runner>/human.rig"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_RIG_OUTPUT_DIR}/chicken.rig" "$<TARGET_FILE_DIR:Runner>/chicken.rig"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_RIG_OUTPUT_DIR}/dog.rig" "$<TARGET_FILE_DIR:Runner>/dog.rig"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_RIG_OUTPUT_DIR}/hexapod.rig" "$<TARGET_FILE_DIR:Runner>/hexapod.rig"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Runner>/assets/ui"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_ICON_SOURCE}" "$<TARGET_FILE_DIR:Runner>/assets/ui/runner_icon_source.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_ICON_SOURCE_SHA256}" "$<TARGET_FILE_DIR:Runner>/assets/ui/runner_icon_source.sha256"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_ICON_PNG}" "$<TARGET_FILE_DIR:Runner>/assets/ui/runner_icon.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_ICON_PNG_512}" "$<TARGET_FILE_DIR:Runner>/assets/ui/runner_icon_512.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_ICON_BMP}" "$<TARGET_FILE_DIR:Runner>/assets/ui/runner_icon.bmp"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RUNNER_ICON_ICO}" "$<TARGET_FILE_DIR:Runner>/assets/ui/runner.ico"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${RUNNER_SHADER_OUTPUT_DIR}" "$<TARGET_FILE_DIR:Runner>/shaders"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Runner>/docs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/SANDHYBRID_INTEGRATION_BRIDGE.md" "$<TARGET_FILE_DIR:Runner>/docs/SANDHYBRID_INTEGRATION_BRIDGE.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0716_CAMERA_BATCH.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0716_CAMERA_BATCH.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0718_RUNTIME_RECOVERY.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0718_RUNTIME_RECOVERY.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0720_UI_PREVIEW_ICON.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0720_UI_PREVIEW_ICON.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0721_READABLE_TELEMETRY.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0721_READABLE_TELEMETRY.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0725_ART_LEG_HOTFIX.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0725_ART_LEG_HOTFIX.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0726_TRAINING_TRUTH.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0726_TRAINING_TRUTH.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0727_RIG_TRAINING_EVIDENCE.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0727_RIG_TRAINING_EVIDENCE.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0728_COURSE_COMPLETION.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0728_COURSE_COMPLETION.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0729_MODULAR_ART_REMAKE.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0729_MODULAR_ART_REMAKE.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0730_SUSTAINED_WALK_RECOVERY.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0730_SUSTAINED_WALK_RECOVERY.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0731_ACTIVE_TERRAIN_CURRICULUM_ART.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0731_ACTIVE_TERRAIN_CURRICULUM_ART.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0732_SHUTTLE_FACING_HANDS.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0732_SHUTTLE_FACING_HANDS.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0733_GRANULAR_FACING_RIGLAB.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0733_GRANULAR_FACING_RIGLAB.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0734_CURRICULUM_SAFE_RIG_OPTIMIZATION.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0734_CURRICULUM_SAFE_RIG_OPTIMIZATION.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0735_PIP_POSTURE_TRUTH.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0735_PIP_POSTURE_TRUTH.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0736_AUTHORED_GAIT_RUNTIME.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0736_AUTHORED_GAIT_RUNTIME.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0737_HYBRID_LOCOMOTION_TERRAIN.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0737_HYBRID_LOCOMOTION_TERRAIN.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0738_TURN_TOPOLOGY_STATS.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0738_TURN_TOPOLOGY_STATS.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0739_STATIC_CELLS_DIRECTION.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0739_STATIC_CELLS_DIRECTION.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0740_PHYSICAL_FACING_RETURN.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0740_PHYSICAL_FACING_RETURN.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0741_FOUR_RIG_NATURAL_GAIT.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0741_FOUR_RIG_NATURAL_GAIT.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0742_SPECIES_ANATOMY_SCALE.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0742_SPECIES_ANATOMY_SCALE.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0743_AUTHORED_LIVE_MORPHOLOGY.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0743_AUTHORED_LIVE_MORPHOLOGY.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0744_DOG_ART_RIG_ASSEMBLY.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0744_DOG_ART_RIG_ASSEMBLY.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0745_HUMAN_SUPPORT_SPECIES_ART.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0745_HUMAN_SUPPORT_SPECIES_ART.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${runner_sandhybrid_SOURCE_DIR}/missioncache.md" "$<TARGET_FILE_DIR:Runner>/docs/SandHybrid-missioncache.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0746_EXACT_CELLS_NATURAL_GAIT_TURN.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0746_EXACT_CELLS_NATURAL_GAIT_TURN.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0747_SPECIES_RIGLAB_MASTERY_RELEASE.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0747_SPECIES_RIGLAB_MASTERY_RELEASE.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0748_CONTACT_LED_GAIT_SPECIES_ART.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0748_CONTACT_LED_GAIT_SPECIES_ART.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/EPOCH2D_WALK_ENGINE_LIBRARY.md" "$<TARGET_FILE_DIR:Runner>/docs/EPOCH2D_WALK_ENGINE_LIBRARY.md"
        VERBATIM)

    install(TARGETS Runner RUNTIME DESTINATION . COMPONENT RunnerRuntime)
    if(WIN32)
        install(FILES
            "$<TARGET_FILE_DIR:Runner>/SDL3.dll"
            "$<TARGET_FILE_DIR:Runner>/vulkan-1.dll"
            DESTINATION . COMPONENT RunnerRuntime)
    endif()
    if(WIN32)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/run.bat" DESTINATION . COMPONENT RunnerRuntime)
    else()
        install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/run.sh" DESTINATION . COMPONENT RunnerRuntime)
    endif()
    install(FILES ${RUNNER_RIG_OUTPUTS} DESTINATION . COMPONENT RunnerRuntime)
    install(DIRECTORY "${RUNNER_SHADER_OUTPUT_DIR}/" DESTINATION shaders COMPONENT RunnerRuntime)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION assets COMPONENT RunnerRuntime)
    install(FILES "${RUNNER_ICON_SOURCE}" "${RUNNER_ICON_SOURCE_SHA256}"
        "${RUNNER_ICON_PNG}" "${RUNNER_ICON_PNG_512}"
        "${RUNNER_ICON_BMP}" "${RUNNER_ICON_ICO}" DESTINATION assets/ui COMPONENT RunnerRuntime)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/SANDHYBRID_INTEGRATION_BRIDGE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0716_CAMERA_BATCH.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0717_EYE_TEST_CORRECTION.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0718_RUNTIME_RECOVERY.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0719_GENERAL_LOCOMOTION.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0720_UI_PREVIEW_ICON.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0721_READABLE_TELEMETRY.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0722_BLACK_FRAME_HOTFIX.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0725_ART_LEG_HOTFIX.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0726_TRAINING_TRUTH.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0727_RIG_TRAINING_EVIDENCE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0728_COURSE_COMPLETION.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0729_MODULAR_ART_REMAKE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0730_SUSTAINED_WALK_RECOVERY.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0731_ACTIVE_TERRAIN_CURRICULUM_ART.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0732_SHUTTLE_FACING_HANDS.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0733_GRANULAR_FACING_RIGLAB.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0734_CURRICULUM_SAFE_RIG_OPTIMIZATION.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0735_PIP_POSTURE_TRUTH.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0736_AUTHORED_GAIT_RUNTIME.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0737_HYBRID_LOCOMOTION_TERRAIN.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0738_TURN_TOPOLOGY_STATS.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0739_STATIC_CELLS_DIRECTION.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0740_PHYSICAL_FACING_RETURN.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0741_FOUR_RIG_NATURAL_GAIT.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0742_SPECIES_ANATOMY_SCALE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0743_AUTHORED_LIVE_MORPHOLOGY.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0744_DOG_ART_RIG_ASSEMBLY.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0745_HUMAN_SUPPORT_SPECIES_ART.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0746_EXACT_CELLS_NATURAL_GAIT_TURN.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0747_SPECIES_RIGLAB_MASTERY_RELEASE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0748_CONTACT_LED_GAIT_SPECIES_ART.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/EPOCH2D_WALK_ENGINE_LIBRARY.md"
        DESTINATION docs COMPONENT RunnerRuntime)
    install(FILES "${runner_sandhybrid_SOURCE_DIR}/missioncache.md" DESTINATION docs RENAME SandHybrid-missioncache.md COMPONENT RunnerRuntime)
endif()
set(EPOCH2D_WALK_ENGINE_PUBLIC_HEADERS
    src/acceptance.hpp
    src/autonomy.hpp
    src/course_completion_diagnostic.hpp
    src/deformable_terrain.hpp
    src/hybrid_brain_diagnostic.hpp
    src/locomotion_strategy.hpp
    src/math.hpp
    src/pixel_art.hpp
    src/ppo.hpp
    src/preview_sync.hpp
    src/rig_training_diagnostic.hpp
    src/simulation.hpp
    src/species_art_layout.hpp
    src/training_explainer.hpp)

set(EPOCH2D_WALK_ENGINE_CMAKE_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/Epoch2DWalkEngine")
configure_package_config_file(
    cmake/Epoch2DWalkEngineConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/Epoch2DWalkEngineConfig.cmake"
    INSTALL_DESTINATION "${EPOCH2D_WALK_ENGINE_CMAKE_INSTALL_DIR}")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/Epoch2DWalkEngineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(TARGETS RunnerCore
    EXPORT Epoch2DWalkEngineTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Epoch2DWalkEngineDevelopment
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Epoch2DWalkEngineDevelopment
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Epoch2DWalkEngineDevelopment
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Epoch2DWalkEngine)
install(FILES ${EPOCH2D_WALK_ENGINE_PUBLIC_HEADERS}
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Epoch2DWalkEngine/Engine.hpp"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Epoch2DWalkEngine
    COMPONENT Epoch2DWalkEngineDevelopment)
install(EXPORT Epoch2DWalkEngineTargets
    FILE Epoch2DWalkEngineTargets.cmake
    NAMESPACE Epoch2DWalkEngine::
    DESTINATION "${EPOCH2D_WALK_ENGINE_CMAKE_INSTALL_DIR}"
    COMPONENT Epoch2DWalkEngineDevelopment)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/Epoch2DWalkEngineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/Epoch2DWalkEngineConfigVersion.cmake"
    DESTINATION "${EPOCH2D_WALK_ENGINE_CMAKE_INSTALL_DIR}"
    COMPONENT Epoch2DWalkEngineDevelopment)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/EPOCH2D_WALK_ENGINE_LIBRARY.md"
    DESTINATION docs COMPONENT Epoch2DWalkEngineDevelopment)


if(RUNNER_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_test(NAME Runner.RepositoryHygiene COMMAND ${CMAKE_COMMAND}
        -DRUNNER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/repository_audit.cmake)
    set_tests_properties(Runner.RepositoryHygiene PROPERTIES TIMEOUT 30)

    add_executable(Epoch2DWalkEngineLibraryExample
        examples/library_probe.cpp)
    target_link_libraries(Epoch2DWalkEngineLibraryExample PRIVATE
        Epoch2DWalkEngine::Epoch2DWalkEngine)
    target_compile_features(Epoch2DWalkEngineLibraryExample PRIVATE cxx_std_23)
    runner_enable_warnings(Epoch2DWalkEngineLibraryExample)
    add_test(NAME Epoch2DWalkEngine.LibraryAPI
        COMMAND Epoch2DWalkEngineLibraryExample)
    set_tests_properties(Epoch2DWalkEngine.LibraryAPI PROPERTIES TIMEOUT 30)

    add_executable(RunnerPixelArtTests tests/pixel_art_tests.cpp)
    target_link_libraries(RunnerPixelArtTests PRIVATE Runner::Core)
    target_compile_features(RunnerPixelArtTests PRIVATE cxx_std_23)
    target_compile_definitions(RunnerPixelArtTests PRIVATE RUNNER_SOURCE_ASSET_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    runner_enable_warnings(RunnerPixelArtTests)
    add_test(NAME Runner.PixelArt COMMAND RunnerPixelArtTests)
    set_tests_properties(Runner.PixelArt PROPERTIES TIMEOUT 30)

    add_executable(RunnerViewCameraTests tests/view_camera_tests.cpp)
    target_include_directories(RunnerViewCameraTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerViewCameraTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerViewCameraTests)
    add_test(NAME Runner.ViewCamera COMMAND RunnerViewCameraTests)
    set_tests_properties(Runner.ViewCamera PROPERTIES TIMEOUT 30)

    add_executable(RunnerV0717EyeTestTests tests/v0717_eye_test_tests.cpp)
    target_link_libraries(RunnerV0717EyeTestTests PRIVATE Runner::Core)
    target_compile_features(RunnerV0717EyeTestTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0717EyeTestTests)
    add_test(NAME Runner.V0717EyeTest COMMAND RunnerV0717EyeTestTests "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    set_tests_properties(Runner.V0717EyeTest PROPERTIES TIMEOUT 180)

    add_executable(RunnerV0718RuntimeRecoveryTests tests/v0718_runtime_recovery_tests.cpp)
    target_link_libraries(RunnerV0718RuntimeRecoveryTests PRIVATE Runner::Core)
    target_compile_features(RunnerV0718RuntimeRecoveryTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0718RuntimeRecoveryTests)
    add_test(NAME Runner.V0718RuntimeRecovery COMMAND RunnerV0718RuntimeRecoveryTests)
    set_tests_properties(Runner.V0718RuntimeRecovery PROPERTIES TIMEOUT 90)

    add_executable(RunnerV0719GeneralLocomotionTests tests/v0719_general_locomotion_tests.cpp)
    target_include_directories(RunnerV0719GeneralLocomotionTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0719GeneralLocomotionTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0719GeneralLocomotionTests)
    add_test(NAME Runner.V0719GeneralLocomotion COMMAND RunnerV0719GeneralLocomotionTests)
    set_tests_properties(Runner.V0719GeneralLocomotion PROPERTIES TIMEOUT 30)

    add_executable(RunnerV0720UiTests
        tests/v0720_ui_tests.cpp src/canvas.cpp)
    target_include_directories(RunnerV0720UiTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0720UiTests PRIVATE cxx_std_23)
    target_compile_definitions(RunnerV0720UiTests PRIVATE
        RUNNER_GENERATED_ASSET_DIRECTORY="${RUNNER_GENERATED_ASSET_DIR}")
    runner_enable_warnings(RunnerV0720UiTests)
    add_test(NAME Runner.V0720Ui COMMAND RunnerV0720UiTests)
    set_tests_properties(Runner.V0720Ui PROPERTIES TIMEOUT 30)

    add_executable(RunnerV0721ReadableTelemetryTests
        tests/v0721_readable_telemetry_tests.cpp)
    target_link_libraries(RunnerV0721ReadableTelemetryTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0721ReadableTelemetryTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0721ReadableTelemetryTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0721ReadableTelemetryTests)
    add_test(NAME Runner.V0721ReadableTelemetry
        COMMAND RunnerV0721ReadableTelemetryTests)
    set_tests_properties(Runner.V0721ReadableTelemetry PROPERTIES TIMEOUT 30)

    add_executable(RunnerV0721RigGaitTests
        tests/v0721_rig_gait_tests.cpp)
    target_link_libraries(RunnerV0721RigGaitTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0721RigGaitTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0721RigGaitTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0721RigGaitTests)
    add_test(NAME Runner.V0721RigGait COMMAND RunnerV0721RigGaitTests)
    set_tests_properties(Runner.V0721RigGait PROPERTIES TIMEOUT 90)

    add_executable(RunnerV0723GrayFrameTests
        tests/v0723_gray_frame_tests.cpp src/app.cpp src/canvas.cpp)
    target_link_libraries(RunnerV0723GrayFrameTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0723GrayFrameTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0723GrayFrameTests PRIVATE cxx_std_23)
    target_compile_definitions(RunnerV0723GrayFrameTests PRIVATE
        RUNNER_VERSION="${PROJECT_VERSION}")
    runner_enable_warnings(RunnerV0723GrayFrameTests)
    add_test(NAME Runner.V0723GrayFrames COMMAND RunnerV0723GrayFrameTests)
    set_tests_properties(Runner.V0723GrayFrames PROPERTIES TIMEOUT 120)

    add_executable(RunnerCoreTests tests/core_tests.cpp)
    target_link_libraries(RunnerCoreTests PRIVATE Runner::Core)
    target_compile_features(RunnerCoreTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerCoreTests)
    add_test(NAME Runner.Core COMMAND RunnerCoreTests)

    add_executable(RunnerDeformableTerrainTests tests/deformable_terrain_tests.cpp)
    target_link_libraries(RunnerDeformableTerrainTests PRIVATE Runner::Core)
    target_compile_features(RunnerDeformableTerrainTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerDeformableTerrainTests)
    add_test(NAME Runner.DeformableTerrain COMMAND RunnerDeformableTerrainTests)
    set_tests_properties(Runner.DeformableTerrain PROPERTIES TIMEOUT 45)

    add_executable(RunnerSandHybridIntegrationTests tests/sandhybrid_integration_tests.cpp)
    target_link_libraries(RunnerSandHybridIntegrationTests PRIVATE Runner::Core)
    target_compile_features(RunnerSandHybridIntegrationTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerSandHybridIntegrationTests)
    add_test(NAME Runner.SandHybridIntegration COMMAND RunnerSandHybridIntegrationTests)
    set_tests_properties(Runner.SandHybridIntegration PROPERTIES TIMEOUT 60)

    add_executable(RunnerV0724StructuralMetricsIconTests
        tests/v0724_structural_metrics_icon_tests.cpp)
    target_link_libraries(RunnerV0724StructuralMetricsIconTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0724StructuralMetricsIconTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0724StructuralMetricsIconTests PRIVATE cxx_std_23)
    target_compile_definitions(RunnerV0724StructuralMetricsIconTests PRIVATE
        RUNNER_GENERATED_ASSET_DIRECTORY="${RUNNER_GENERATED_ASSET_DIR}"
        RUNNER_SOURCE_ICON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/runner_icon_source.rgba.zlib.b64")
    runner_enable_warnings(RunnerV0724StructuralMetricsIconTests)
    add_test(NAME Runner.V0724StructuralMetricsIcon
        COMMAND RunnerV0724StructuralMetricsIconTests)
    set_tests_properties(Runner.V0724StructuralMetricsIcon PROPERTIES TIMEOUT 180)

    add_executable(RunnerV0725ArtLegHotfixTests
        tests/v0725_art_leg_hotfix_tests.cpp)
    target_link_libraries(RunnerV0725ArtLegHotfixTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0725ArtLegHotfixTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0725ArtLegHotfixTests PRIVATE cxx_std_23)
    target_compile_definitions(RunnerV0725ArtLegHotfixTests PRIVATE
        RUNNER_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    runner_enable_warnings(RunnerV0725ArtLegHotfixTests)
    add_test(NAME Runner.V0725ArtLegHotfix
        COMMAND RunnerV0725ArtLegHotfixTests)
    set_tests_properties(Runner.V0725ArtLegHotfix PROPERTIES TIMEOUT 180)

    add_executable(RunnerV0726TrainingTruthTests
        tests/v0726_training_truth_tests.cpp)
    target_link_libraries(RunnerV0726TrainingTruthTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0726TrainingTruthTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0726TrainingTruthTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0726TrainingTruthTests)
    add_test(NAME Runner.V0726TrainingTruth
        COMMAND RunnerV0726TrainingTruthTests)
    set_tests_properties(Runner.V0726TrainingTruth PROPERTIES TIMEOUT 180)

    add_executable(RunnerV0730ColdStartTests
        tests/v0730_cold_start_tests.cpp)
    target_link_libraries(RunnerV0730ColdStartTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0730ColdStartTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0730ColdStartTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0730ColdStartTests)
    add_test(NAME Runner.V0730ReferenceFrame
        COMMAND RunnerV0730ColdStartTests --references)
    set_tests_properties(Runner.V0730ReferenceFrame PROPERTIES TIMEOUT 1800)
    add_test(NAME Runner.V0730ColdStart
        COMMAND RunnerV0730ColdStartTests --learner)
    set_tests_properties(Runner.V0730ColdStart PROPERTIES TIMEOUT 1800)
    add_executable(RunnerV0731CrouchLearningTests
        tests/v0731_crouch_learning_tests.cpp)
    target_link_libraries(RunnerV0731CrouchLearningTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0731CrouchLearningTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0731CrouchLearningTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0731CrouchLearningTests)
    add_test(NAME Runner.V0731CrouchLearning
        COMMAND RunnerV0731CrouchLearningTests)
    set_tests_properties(Runner.V0731CrouchLearning PROPERTIES TIMEOUT 1800)

    add_executable(RunnerV0732ShuttleFacingTests
        tests/v0732_shuttle_facing_tests.cpp)
    target_link_libraries(RunnerV0732ShuttleFacingTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0732ShuttleFacingTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0732ShuttleFacingTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0732ShuttleFacingTests)
    add_test(NAME Runner.V0732ShuttleFacing
        COMMAND RunnerV0732ShuttleFacingTests)
    set_tests_properties(Runner.V0732ShuttleFacing PROPERTIES TIMEOUT 180)

    add_executable(RunnerV0728CourseCompletionTests
        tests/v0728_course_completion_tests.cpp)
    target_link_libraries(RunnerV0728CourseCompletionTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0728CourseCompletionTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0728CourseCompletionTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0728CourseCompletionTests)
    add_test(NAME Runner.V0728CourseCompletion
        COMMAND RunnerV0728CourseCompletionTests)
    set_tests_properties(Runner.V0728CourseCompletion PROPERTIES TIMEOUT 180)

    add_executable(RunnerV0729ModularArtTests
        tests/v0729_modular_art_tests.cpp)
    target_link_libraries(RunnerV0729ModularArtTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0729ModularArtTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0729ModularArtTests PRIVATE cxx_std_23)
    target_compile_definitions(RunnerV0729ModularArtTests PRIVATE
        RUNNER_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    runner_enable_warnings(RunnerV0729ModularArtTests)
    add_test(NAME Runner.V0729ModularArt
        COMMAND RunnerV0729ModularArtTests)
    set_tests_properties(Runner.V0729ModularArt PROPERTIES TIMEOUT 180)

    add_executable(RunnerV0734CurriculumRigTests
        tests/v0734_curriculum_rig_tests.cpp)
    target_link_libraries(RunnerV0734CurriculumRigTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0734CurriculumRigTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0734CurriculumRigTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0734CurriculumRigTests)
    add_test(NAME Runner.V0734CurriculumRig
        COMMAND RunnerV0734CurriculumRigTests)
    set_tests_properties(Runner.V0734CurriculumRig PROPERTIES TIMEOUT 180)
    add_executable(RunnerV0735PipPostureTests
        tests/v0735_pip_posture_tests.cpp)
    target_link_libraries(RunnerV0735PipPostureTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0735PipPostureTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(RunnerV0735PipPostureTests PRIVATE
        RUNNER_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(RunnerV0735PipPostureTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0735PipPostureTests)
    add_test(NAME Runner.V0735PipPosture
        COMMAND RunnerV0735PipPostureTests)
    set_tests_properties(Runner.V0735PipPosture PROPERTIES TIMEOUT 180)

    add_executable(RunnerV0736AuthoredRuntimeTests
        tests/v0736_authored_runtime_tests.cpp)
    target_link_libraries(RunnerV0736AuthoredRuntimeTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0736AuthoredRuntimeTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0736AuthoredRuntimeTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0736AuthoredRuntimeTests)
    add_test(NAME Runner.V0736AuthoredRuntime
        COMMAND RunnerV0736AuthoredRuntimeTests)
    set_tests_properties(Runner.V0736AuthoredRuntime PROPERTIES TIMEOUT 180)

    add_executable(RunnerV0737LocomotionTerrainTests
        tests/v0737_locomotion_terrain_tests.cpp)
    target_link_libraries(RunnerV0737LocomotionTerrainTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0737LocomotionTerrainTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0737LocomotionTerrainTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0737LocomotionTerrainTests)
    add_test(NAME Runner.V0737LocomotionTerrain
        COMMAND RunnerV0737LocomotionTerrainTests)
    set_tests_properties(Runner.V0737LocomotionTerrain PROPERTIES TIMEOUT 180)

    add_executable(RunnerV0742SpeciesRigTests
        tests/v0742_species_rig_tests.cpp)
    target_link_libraries(RunnerV0742SpeciesRigTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0742SpeciesRigTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0742SpeciesRigTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0742SpeciesRigTests)
    add_test(NAME Runner.V0742SpeciesRig
        COMMAND RunnerV0742SpeciesRigTests)
    set_tests_properties(Runner.V0742SpeciesRig PROPERTIES TIMEOUT 180)
    add_executable(RunnerV0744DogArtRigTests
        tests/v0744_dog_art_rig_tests.cpp)
    target_link_libraries(RunnerV0744DogArtRigTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0744DogArtRigTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(RunnerV0744DogArtRigTests PRIVATE
        RUNNER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(RunnerV0744DogArtRigTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0744DogArtRigTests)
    add_test(NAME Runner.V0744DogArtRig COMMAND RunnerV0744DogArtRigTests)
    set_tests_properties(Runner.V0744DogArtRig PROPERTIES TIMEOUT 60)

    add_executable(RunnerV0748ContactArtTests
        tests/v0748_contact_art_tests.cpp)
    target_link_libraries(RunnerV0748ContactArtTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0748ContactArtTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(RunnerV0748ContactArtTests PRIVATE
        RUNNER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(RunnerV0748ContactArtTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerV0748ContactArtTests)
    add_test(NAME Runner.V0748ContactArt COMMAND RunnerV0748ContactArtTests)
    set_tests_properties(Runner.V0748ContactArt PROPERTIES TIMEOUT 180)


    add_executable(RunnerLiveAcceptanceTests tests/live_acceptance_tests.cpp)
    target_link_libraries(RunnerLiveAcceptanceTests PRIVATE Runner::Core)
    target_compile_features(RunnerLiveAcceptanceTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerLiveAcceptanceTests)
    add_test(NAME Runner.LiveAcceptance COMMAND RunnerLiveAcceptanceTests)
    set_tests_properties(Runner.LiveAcceptance PROPERTIES TIMEOUT 120)

    add_executable(RunnerConcurrencyBenchmark tests/concurrency_benchmark.cpp)
    target_link_libraries(RunnerConcurrencyBenchmark PRIVATE Runner::Core)
    target_compile_features(RunnerConcurrencyBenchmark PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerConcurrencyBenchmark)
    add_test(NAME Runner.ConcurrencyBenchmark COMMAND RunnerConcurrencyBenchmark)
    set_tests_properties(Runner.ConcurrencyBenchmark PROPERTIES TIMEOUT 180)

    add_executable(RunnerRuntimePipelineTests tests/runtime_pipeline_tests.cpp)
    target_link_libraries(RunnerRuntimePipelineTests PRIVATE Runner::Core)
    target_compile_features(RunnerRuntimePipelineTests PRIVATE cxx_std_23)
    runner_enable_warnings(RunnerRuntimePipelineTests)
    if(RUNNER_ENABLE_TSAN AND NOT MSVC)
        target_compile_options(RunnerRuntimePipelineTests PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(RunnerRuntimePipelineTests PRIVATE -fsanitize=thread)
    endif()
    add_test(NAME Runner.RuntimePipeline COMMAND RunnerRuntimePipelineTests)
    set_tests_properties(Runner.RuntimePipeline PROPERTIES TIMEOUT 90)

    if(TARGET Runner)
        add_test(NAME Runner.PackageLayout COMMAND $<TARGET_FILE:Runner> --diagnose-package)
        set_tests_properties(Runner.PackageLayout PROPERTIES TIMEOUT 30)
        add_test(NAME Runner.CameraDiagnostic COMMAND $<TARGET_FILE:Runner> --diagnose-camera)
        set_tests_properties(Runner.CameraDiagnostic PROPERTIES TIMEOUT 30)
        add_test(NAME Runner.CourseDiagnostic COMMAND $<TARGET_FILE:Runner> --diagnose-course)
        set_tests_properties(Runner.CourseDiagnostic PROPERTIES TIMEOUT 180)
        add_test(NAME Runner.ArtDiagnostic COMMAND $<TARGET_FILE:Runner> --diagnose-art)
        set_tests_properties(Runner.ArtDiagnostic PROPERTIES TIMEOUT 30)
        add_test(NAME Runner.UiDiagnostic COMMAND $<TARGET_FILE:Runner> --diagnose-ui)
        set_tests_properties(Runner.UiDiagnostic PROPERTIES TIMEOUT 30)
        add_test(NAME Runner.HybridBrainDiagnostic COMMAND $<TARGET_FILE:Runner> --diagnose-hybrid-brain)
        set_tests_properties(Runner.HybridBrainDiagnostic PROPERTIES TIMEOUT 180)
    endif()
endif()
