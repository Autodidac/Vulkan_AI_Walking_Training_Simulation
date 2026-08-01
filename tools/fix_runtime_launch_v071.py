from __future__ import annotations

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


main_path = Path("src/main.cpp")
main = main_path.read_text(encoding="utf-8")
main = replace_once(
    main,
    "#include <SDL3/SDL.h>\n#include <SDL3/SDL_main.h>\n",
    "#include <SDL3/SDL.h>\n#include <SDL3/SDL_filesystem.h>\n#include <SDL3/SDL_main.h>\n",
    "SDL filesystem include",
)
main = replace_once(
    main,
    "#include <algorithm>\n#include <chrono>\n",
    "#include <algorithm>\n#include <array>\n#include <chrono>\n",
    "array include",
)
main = replace_once(
    main,
    '''    [[nodiscard]] bool wants_vulkan_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-vulkan";
    }

''',
    '''    [[nodiscard]] bool wants_vulkan_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-vulkan";
    }

    [[nodiscard]] bool wants_package_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-package";
    }

    [[nodiscard]] std::filesystem::path executable_directory()
    {
        const char* const base_path = SDL_GetBasePath();
        if (base_path == nullptr || *base_path == '\\0')
            return std::filesystem::current_path();
        return std::filesystem::u8path(base_path);
    }

    [[nodiscard]] bool validate_runtime_layout(const std::filesystem::path& base_directory,
        std::string& error)
    {
        const std::array required_files{
            std::filesystem::path{ EPOCHRUNNER_SHADER_DIRECTORY } / "flat.vert.spv",
            std::filesystem::path{ EPOCHRUNNER_SHADER_DIRECTORY } / "flat.frag.spv"
        };
        std::error_code filesystem_error{};
        for (const std::filesystem::path& relative : required_files)
        {
            const std::filesystem::path absolute = base_directory / relative;
            if (!std::filesystem::is_regular_file(absolute, filesystem_error))
            {
                error = "Missing packaged runtime file: " + absolute.string();
                if (filesystem_error)
                    error += " (" + filesystem_error.message() + ")";
                return false;
            }
            filesystem_error.clear();
        }

        const std::filesystem::path asset_directory =
            base_directory / EPOCHRUNNER_ASSET_DIRECTORY;
        if (!std::filesystem::is_directory(asset_directory, filesystem_error))
        {
            error = "Missing packaged asset directory: " + asset_directory.string();
            if (filesystem_error)
                error += " (" + filesystem_error.message() + ")";
            return false;
        }
        error.clear();
        return true;
    }

''',
    "package diagnostic helpers",
)
main = replace_once(
    main,
    '''    const bool diagnostic = wants_vulkan_diagnostic(argc, argv);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
''',
    '''    const bool package_diagnostic = wants_package_diagnostic(argc, argv);
    const bool diagnostic = wants_vulkan_diagnostic(argc, argv) || package_diagnostic;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
''',
    "diagnostic mode selection",
)
main = replace_once(
    main,
    '''        return 1;
    }

    if (!SDL_Vulkan_LoadLibrary(nullptr))
''',
    '''        return 1;
    }

    const std::filesystem::path base_directory = executable_directory();
    const std::filesystem::path shader_directory =
        base_directory / EPOCHRUNNER_SHADER_DIRECTORY;
    const std::filesystem::path asset_directory =
        base_directory / EPOCHRUNNER_ASSET_DIRECTORY;
    if (package_diagnostic)
    {
        std::string layout_error{};
        if (!validate_runtime_layout(base_directory, layout_error))
        {
            std::fprintf(stderr, "EpochRunner package diagnostic failed: %s\\n",
                layout_error.c_str());
            SDL_Quit();
            return 1;
        }
    }

    if (!SDL_Vulkan_LoadLibrary(nullptr))
''',
    "executable-relative runtime paths",
)
main = replace_once(
    main,
    '''            std::printf(
                "EpochRunner " EPOCHRUNNER_VERSION " SDL3 Vulkan diagnostic passed: backend enabled, video_driver=%s; "
                "the CI runner has no Vulkan presentation surface (%s)\\n",
                video_driver != nullptr ? video_driver : "unknown",
                vulkan_error.c_str());
''',
    '''            std::printf(
                package_diagnostic
                    ? "EpochRunner " EPOCHRUNNER_VERSION " package diagnostic passed: runtime files present, backend enabled, video_driver=%s; the CI runner has no Vulkan presentation surface (%s)\\n"
                    : "EpochRunner " EPOCHRUNNER_VERSION " SDL3 Vulkan diagnostic passed: backend enabled, video_driver=%s; the CI runner has no Vulkan presentation surface (%s)\\n",
                video_driver != nullptr ? video_driver : "unknown",
                vulkan_error.c_str());
''',
    "headless package diagnostic output",
)
main = replace_once(
    main,
    '''        std::printf(
            "EpochRunner " EPOCHRUNNER_VERSION " SDL3 Vulkan diagnostic passed: video_driver=%s, instance_extensions=%u\\n",
            video_driver != nullptr ? video_driver : "unknown",
            static_cast<unsigned int>(instance_extension_count));
''',
    '''        std::printf(
            package_diagnostic
                ? "EpochRunner " EPOCHRUNNER_VERSION " package diagnostic passed: runtime files present, video_driver=%s, instance_extensions=%u\\n"
                : "EpochRunner " EPOCHRUNNER_VERSION " SDL3 Vulkan diagnostic passed: video_driver=%s, instance_extensions=%u\\n",
            video_driver != nullptr ? video_driver : "unknown",
            static_cast<unsigned int>(instance_extension_count));
''',
    "package diagnostic output",
)
main = replace_once(
    main,
    '''    if (!renderer.initialize(window, std::filesystem::path(EPOCHRUNNER_SHADER_DIRECTORY), error))
''',
    '''    if (!renderer.initialize(window, shader_directory, error))
''',
    "absolute shader directory",
)
main = replace_once(
    main,
    '''    if (!application.initialize(std::filesystem::path(EPOCHRUNNER_ASSET_DIRECTORY), error))
''',
    '''    if (!application.initialize(asset_directory, error))
''',
    "absolute asset directory",
)
main_path.write_text(main, encoding="utf-8")

cmake_path = Path("CMakeLists.txt")
cmake = cmake_path.read_text(encoding="utf-8")
cmake = replace_once(
    cmake,
    '        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"\n',
    '        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:EpochRunner>"\n',
    "Visual Studio runtime working directory",
)
cmake = replace_once(
    cmake,
    '''    add_custom_command(TARGET EpochRunner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:EpochRunner>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${EPOCHRUNNER_SHADER_OUTPUT_DIR}"
            "$<TARGET_FILE_DIR:EpochRunner>/shaders"
        VERBATIM
    )

    install(TARGETS EpochRunner RUNTIME DESTINATION .)
''',
    '''    add_custom_command(TARGET EpochRunner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:EpochRunner>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${EPOCHRUNNER_SHADER_OUTPUT_DIR}"
            "$<TARGET_FILE_DIR:EpochRunner>/shaders"
        VERBATIM
    )

    if(WIN32)
        add_custom_command(TARGET EpochRunner POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:EpochRunner>
                $<TARGET_FILE_DIR:EpochRunner>
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()

    install(TARGETS EpochRunner RUNTIME DESTINATION .)
    if(WIN32)
        install(FILES $<TARGET_RUNTIME_DLLS:EpochRunner> DESTINATION .)
    endif()
''',
    "runtime dependency deployment",
)
cmake = replace_once(
    cmake,
    '''    add_test(NAME EpochRunner.RuntimePipeline COMMAND EpochRunnerRuntimePipelineTests)
    set_tests_properties(EpochRunner.RuntimePipeline PROPERTIES TIMEOUT 90)
''',
    '''    add_test(NAME EpochRunner.RuntimePipeline COMMAND EpochRunnerRuntimePipelineTests)
    set_tests_properties(EpochRunner.RuntimePipeline PROPERTIES TIMEOUT 90)

    if(TARGET EpochRunner)
        add_test(NAME EpochRunner.PackageLayout
            COMMAND $<TARGET_FILE:EpochRunner> --diagnose-package)
        set_tests_properties(EpochRunner.PackageLayout PROPERTIES TIMEOUT 30)
    endif()
''',
    "package layout test",
)
cmake_path.write_text(cmake, encoding="utf-8")
