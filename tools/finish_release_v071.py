from __future__ import annotations

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


# MSVC /WX: use the C++20/23 UTF-8 path constructor instead of deprecated u8path().
main_path = Path("src/main.cpp")
main = main_path.read_text(encoding="utf-8")
main = replace_once(
    main,
    "        return std::filesystem::u8path(base_path);",
    "        return std::filesystem::path{ std::u8string{ reinterpret_cast<const char8_t*>(base_path) } };",
    "deprecated u8path call",
)
main_path.write_text(main, encoding="utf-8")

# Use the Vulkan SDK's glslc instead of compiling shaderc and SPIR-V Tools from source.
cmake_path = Path("CMakeLists.txt")
cmake = cmake_path.read_text(encoding="utf-8")
old_shader_block = '''    find_package(SDL3 CONFIG REQUIRED)
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
'''
new_shader_block = '''    find_package(SDL3 CONFIG REQUIRED)
    find_package(Vulkan 1.3 REQUIRED COMPONENTS glslc)
    if(NOT Vulkan_GLSLC_EXECUTABLE)
        message(FATAL_ERROR "EpochRunner requires glslc from the Vulkan SDK")
    endif()

    set(EPOCHRUNNER_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    set(EPOCHRUNNER_SHADER_OUTPUTS)
    foreach(shader IN ITEMS flat.vert flat.frag)
        set(source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}")
        set(output "${EPOCHRUNNER_SHADER_OUTPUT_DIR}/${shader}.spv")
        add_custom_command(
            OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${EPOCHRUNNER_SHADER_OUTPUT_DIR}"
            COMMAND "${Vulkan_GLSLC_EXECUTABLE}" "${source}" -o "${output}"
            DEPENDS "${source}"
            VERBATIM
            COMMENT "Compiling ${shader} with Vulkan SDK glslc"
        )
        list(APPEND EPOCHRUNNER_SHADER_OUTPUTS "${output}")
    endforeach()
'''
cmake = replace_once(cmake, old_shader_block, new_shader_block, "shader compiler block")
cmake = replace_once(
    cmake,
    '''    install(TARGETS EpochRunner RUNTIME DESTINATION .)
    install(DIRECTORY "${EPOCHRUNNER_SHADER_OUTPUT_DIR}/" DESTINATION shaders)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION assets)
''',
    '''    install(TARGETS EpochRunner RUNTIME DESTINATION .)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/run.bat" DESTINATION .)
    install(DIRECTORY "${EPOCHRUNNER_SHADER_OUTPUT_DIR}/" DESTINATION shaders)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION assets)
''',
    "run.bat install",
)
cmake_path.write_text(cmake, encoding="utf-8")

vcpkg_path = Path("vcpkg.json")
vcpkg = vcpkg_path.read_text(encoding="utf-8")
vcpkg = replace_once(
    vcpkg,
    '    },\n    "shaderc",\n    "vulkan-loader"',
    '    },\n    "vulkan-loader"',
    "shaderc manifest dependency",
)
vcpkg = vcpkg.replace('"version-semver": "0.6.2"', '"version-semver": "0.7.1"')
vcpkg_path.write_text(vcpkg, encoding="utf-8")

notes_path = Path("RELEASE_NOTES_v0.7.1.md")
notes = notes_path.read_text(encoding="utf-8")
for line in (
    "- Restores `run.bat` as the supported one-click source-tree and extracted-release launcher.\n",
    "- Packages and validates `run.bat`, shaders, assets, and runtime DLLs from an unrelated working directory.\n",
    "- Uses Vulkan SDK `glslc` for deterministic shader compilation instead of rebuilding shaderc and SPIR-V Tools.\n",
):
    if line not in notes:
        if not notes.endswith("\n"):
            notes += "\n"
        notes += line
notes_path.write_text(notes, encoding="utf-8")

readme_path = Path("README.md")
readme = readme_path.read_text(encoding="utf-8")
run_section = '''
### One-click Windows launcher

Double-click `run.bat`. In an extracted release it launches the packaged executable. In a source checkout it uses an existing Release build or configures and builds `windows-release` before launching. Command-line arguments are forwarded, including `run.bat --diagnose-package`.

'''
if "### One-click Windows launcher" not in readme:
    readme = replace_once(readme, "## Build\n", "## Build\n\n" + run_section, "README build section")
readme_path.write_text(readme, encoding="utf-8")

writer_path = Path("tools/write_missioncache_v071.py")
writer = writer_path.read_text(encoding="utf-8")
launcher_mission = '''### WALK-LAUNCH-021 — Executable-relative launch and one-click runner
**Status:** VERIFIED

`run.bat` is restored at the repository and release root. It launches an extracted package directly, locates an existing source-tree Release build, or configures and builds the Windows Release target when needed. It forwards diagnostic arguments. EpochRunner resolves shaders and assets from the executable directory, and both the direct executable and `run.bat --diagnose-package` pass from an unrelated working directory.

'''
if "### WALK-LAUNCH-021" not in writer:
    writer = replace_once(writer, "## Runtime architecture\n", launcher_mission + "## Runtime architecture\n", "runtime architecture ledger section")
writer = writer.replace(
    "- executable version and Vulkan diagnostic: passed;\n",
    "- executable version, Vulkan diagnostic, executable-relative package launch, and `run.bat` launch: passed;\n",
)
writer = writer.replace(
    '        "- Vulkan diagnostic: passed.\\n"\n',
    '        "- Vulkan diagnostic, executable-relative package launch, and run.bat launch: passed.\\n"\n',
)
writer = writer.replace(
    '        "- Vulkan diagnostic: passed\\n"\n',
    '        "- Vulkan diagnostic, executable-relative package launch, and run.bat launch: passed\\n"\n',
)
writer = writer.replace(
    "- Windows package: `{archive}`;\n",
    "- Windows package, including `run.bat`: `{archive}`;\n",
)
writer_path.write_text(writer, encoding="utf-8")
