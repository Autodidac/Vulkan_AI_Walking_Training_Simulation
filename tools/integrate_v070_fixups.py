from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "CMakeLists.txt"
text = path.read_text(encoding="utf-8")
old = "set(CMAKE_CXX_SCAN_FOR_MODULES ON)"
new = "if(MSVC)\n    set(CMAKE_CXX_SCAN_FOR_MODULES ON)\nelse()\n    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)\nendif()"
if text.count(old) != 1:
    raise RuntimeError("expected CMAKE_CXX_SCAN_FOR_MODULES setting not found")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
print("Applied v0.7 integration fixups.")
