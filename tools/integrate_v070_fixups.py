from pathlib import Path

root = Path(__file__).resolve().parents[1]

cmake = root / "CMakeLists.txt"
text = cmake.read_text(encoding="utf-8")
old = "set(CMAKE_CXX_SCAN_FOR_MODULES ON)"
new = "if(MSVC)\n    set(CMAKE_CXX_SCAN_FOR_MODULES ON)\nelse()\n    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)\nendif()"
if text.count(old) != 1:
    raise RuntimeError("expected CMAKE_CXX_SCAN_FOR_MODULES setting not found")
cmake.write_text(text.replace(old, new, 1), encoding="utf-8")

ppo = root / "src/ppo.hpp"
text = ppo.read_text(encoding="utf-8")
old = "#include <string_view>\n#include <vector>"
new = "#include <string_view>\n#include <thread>\n#include <vector>"
if text.count(old) != 1:
    raise RuntimeError("expected ppo include block not found")
ppo.write_text(text.replace(old, new, 1), encoding="utf-8")

print("Applied v0.7 integration fixups.")
