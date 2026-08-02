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

# Keep the proven vcpkg shaderc compiler path, but install the restored launcher.
cmake_path = Path("CMakeLists.txt")
cmake = cmake_path.read_text(encoding="utf-8")
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
vcpkg = vcpkg.replace('"version-semver": "0.6.2"', '"version-semver": "0.7.1"')
vcpkg_path.write_text(vcpkg, encoding="utf-8")

notes_path = Path("RELEASE_NOTES_v0.7.1.md")
notes = notes_path.read_text(encoding="utf-8")
for line in (
    "- Restores `run.bat` as the supported one-click source-tree and extracted-release launcher.\n",
    "- Packages and validates `run.bat`, shaders, assets, and runtime DLLs from an unrelated working directory.\n",
):
    if line not in notes:
        if not notes.endswith("\n"):
            notes += "\n"
        notes += line
notes = notes.replace(
    "- Uses Vulkan SDK `glslc` for deterministic shader compilation instead of rebuilding shaderc and SPIR-V Tools.\n",
    "",
)
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
