from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / "tools" / "repository_audit.cmake"
text = path.read_text(encoding="utf-8")
old = '''string(FIND "${cmake_text}" "project(Runner VERSION 0.7.12 LANGUAGES CXX)" version_position)
if(version_position EQUAL -1)
    message(FATAL_ERROR "CMake project version is not 0.7.12")
endif()'''
new = '''string(FIND "${cmake_text}" "project(Runner VERSION 0.7.13 LANGUAGES CXX)" version_position)
if(version_position EQUAL -1)
    message(FATAL_ERROR "CMake project version is not 0.7.13")
endif()'''
if text.count(old) != 1:
    raise RuntimeError(f"expected one repository version invariant, found {text.count(old)}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
Path(__file__).unlink()
