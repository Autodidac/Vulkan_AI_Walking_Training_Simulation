cmake_minimum_required(VERSION 3.28)

if(MSVC)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
else()
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

project(EpochRunner VERSION 0.7.1 LANGUAGES CXX)

option(EPOCHRUNNER_BUILD_APP "Build the SDL3/Vulkan EpochRunner application." ON)
option(EPOCHRUNNER_BUILD_TESTS "Build deterministic core tests." ON)
option(EPOCHRUNNER_ENABLE_TSAN "Enable ThreadSanitizer for CPU-only tests." OFF)

find_package(Threads REQUIRED)

add_library(EpochRunnerCore STATIC
    src/simulation.cpp
    src/ppo_network.cpp
    src/ppo_trainer.cpp
    src/ppo_parallel.cpp
    src/self_imitation.cpp
    src/training_checkpoint.cpp
    src/autonomy_runtime.cpp
    src/autonomy_commands.cpp
    src/autonomy_curriculum.cpp
    src/autonomy_persistence.cpp
)
add_library(EpochRunner::Core ALIAS EpochRunnerCore)

target_include_directories(EpochRunnerCore PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_link_libraries(EpochRunnerCore PUBLIC Threads::Threads)
target_compile_features(EpochRunnerCore PUBLIC cxx_std_23)
set_target_properties(EpochRunnerCore PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

function(epochrunner_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
        )
    endif()
endfunction()

epochrunner_enable_warnings(EpochRunnerCore)

if(EPOCHRUNNER_ENABLE_TSAN AND NOT MSVC)
    target_compile_options(EpochRunnerCore PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(EpochRunnerCore PRIVATE -fsanitize=thread)
endif()

if(EPOCHRUNNER_BUILD_APP)
    include(FetchContent)

    set(EPOCHGUI_ENABLE_ROUNDED_RECT ON CACHE BOOL "" FORCE)
    set(EPOCHGUI_ENABLE_INPUT ON CACHE BOOL "" FORCE)

    if(TARGET Autodidac::EpochGui)
        message(STATUS "EpochRunner: using existing EpochGui target")
    else()
        set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/epoch.gui.ixx"
            AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMakeLists.txt")
            message(STATUS "EpochRunner: using EpochGui source tree at ${CMAKE_CURRENT_SOURCE_DIR}/../..")
            add_subdirectory(
                "${CMAKE_CURRENT_SOURCE_DIR}/../.."
                "${CMAKE_CURRENT_BINARY_DIR}/epochgui"
            )
        else()
            FetchContent_Declare(
                EpochGui
                GIT_REPOSITORY https://github.com/Autodidac/EpochGui.git
                GIT_TAG 347ad52e8fc27deb08dea97e56a9b6d8c0db3af2
                GIT_SHALLOW FALSE
            )
            FetchContent_MakeAvailable(EpochGui)
        endif()
        set(BUILD_TESTING "${EPOCHRUNNER_BUILD_TESTS}" CACHE BOOL "" FORCE)
    endif()

    if(MSVC AND TARGET EpochGui)
        target_compile_options(EpochGui PRIVATE /utf-8)
    endif()

    find_package(SDL3 CONFIG REQUIRED)
    find_package(Vulkan 1.3 REQUIRED)
    find_package(unofficial-shaderc CONFIG REQUIRED)

    add_executable(EpochRunnerShaderCompiler
        tools/shader_compiler.cpp
    )
    target_link_libraries(EpochRunnerShaderCompiler PRIVATE
        unofficial::shaderc::shaderc
    )
    target_compile_features(EpochRunnerShaderCompiler PRIVATE cxx_std_23)
    set_target_properties(EpochRunnerShaderCompiler PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    epochrunner_enable_warnings(EpochRunnerShaderCompiler)

    set(EPOCHRUNNER_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    set(EPOCHRUNNER_SHADER_OUTPUTS)
    foreach(shader IN ITEMS flat.vert flat.frag)
        set(source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}")
        set(output "${EPOCHRUNNER_SHADER_OUTPUT_DIR}/${shader}.spv")
        add_custom_command(
            OUTPUT "${output}"
            COMMAND "$<TARGET_FILE:EpochRunnerShaderCompiler>" "${source}" "${output}"
            DEPENDS
                "${source}"
                EpochRunnerShaderCompiler
            VERBATIM
            COMMENT "Compiling ${shader} with vcpkg shaderc"
        )
        list(APPEND EPOCHRUNNER_SHADER_OUTPUTS "${output}")
    endforeach()
    add_custom_target(EpochRunnerShaders DEPENDS ${EPOCHRUNNER_SHADER_OUTPUTS})

    add_executable(EpochRunner
        src/main.cpp
        src/app.cpp
        src/canvas.cpp
        src/renderer.cpp
        src/app.hpp
        src/renderer.hpp
        src/math.hpp
    )
    add_dependencies(EpochRunner EpochRunnerShaders)
    target_link_libraries(EpochRunner PRIVATE
        EpochRunner::Core
        Autodidac::EpochGui
        SDL3::SDL3
        Vulkan::Vulkan
    )
    target_compile_features(EpochRunner PRIVATE cxx_std_23)
    target_compile_definitions(EpochRunner PRIVATE
        EPOCHRUNNER_VERSION="${PROJECT_VERSION}"
        EPOCHRUNNER_SHADER_DIRECTORY="shaders"
        EPOCHRUNNER_ASSET_DIRECTORY="assets"
    )
    set_target_properties(EpochRunner PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:EpochRunner>"
    )
    epochrunner_enable_warnings(EpochRunner)

    add_custom_command(TARGET EpochRunner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:EpochRunner>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${EPOCHRUNNER_SHADER_OUTPUT_DIR}"
            "$<TARGET_FILE_DIR:EpochRunner>/shaders"
        VERBATIM
    )

    install(TARGETS EpochRunner RUNTIME DESTINATION .)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/run.bat" DESTINATION .)
    install(DIRECTORY "${EPOCHRUNNER_SHADER_OUTPUT_DIR}/" DESTINATION shaders)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION assets)
endif()

if(EPOCHRUNNER_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(EpochRunnerCoreTests tests/core_tests.cpp)
    target_link_libraries(EpochRunnerCoreTests PRIVATE EpochRunner::Core)
    target_compile_features(EpochRunnerCoreTests PRIVATE cxx_std_23)
    set_target_properties(EpochRunnerCoreTests PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    epochrunner_enable_warnings(EpochRunnerCoreTests)
    add_test(NAME EpochRunner.Core COMMAND EpochRunnerCoreTests)

    add_executable(EpochRunnerConcurrencyBenchmark tests/concurrency_benchmark.cpp)
    target_link_libraries(EpochRunnerConcurrencyBenchmark PRIVATE EpochRunner::Core)
    target_compile_features(EpochRunnerConcurrencyBenchmark PRIVATE cxx_std_23)
    set_target_properties(EpochRunnerConcurrencyBenchmark PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    epochrunner_enable_warnings(EpochRunnerConcurrencyBenchmark)
    add_test(NAME EpochRunner.ConcurrencyBenchmark COMMAND EpochRunnerConcurrencyBenchmark)
    set_tests_properties(EpochRunner.ConcurrencyBenchmark PROPERTIES TIMEOUT 45)

    add_executable(EpochRunnerRuntimePipelineTests tests/runtime_pipeline_tests.cpp)
    target_link_libraries(EpochRunnerRuntimePipelineTests PRIVATE EpochRunner::Core)
    target_compile_features(EpochRunnerRuntimePipelineTests PRIVATE cxx_std_23)
    set_target_properties(EpochRunnerRuntimePipelineTests PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    epochrunner_enable_warnings(EpochRunnerRuntimePipelineTests)
    if(EPOCHRUNNER_ENABLE_TSAN AND NOT MSVC)
        target_compile_options(EpochRunnerRuntimePipelineTests PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(EpochRunnerRuntimePipelineTests PRIVATE -fsanitize=thread)
    endif()
    add_test(NAME EpochRunner.RuntimePipeline COMMAND EpochRunnerRuntimePipelineTests)
    set_tests_properties(EpochRunner.RuntimePipeline PROPERTIES TIMEOUT 90)

    if(TARGET EpochRunner)
        add_test(NAME EpochRunner.PackageLayout
            COMMAND $<TARGET_FILE:EpochRunner> --diagnose-package)
        set_tests_properties(EpochRunner.PackageLayout PROPERTIES TIMEOUT 30)
    endif()
endif()
