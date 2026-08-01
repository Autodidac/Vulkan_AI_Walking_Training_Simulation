from __future__ import annotations

from pathlib import Path
import re

path = Path("tests/core_tests.cpp")
text = path.read_text(encoding="utf-8")
text, count = re.subn(
    r'(\s*<< " survival=" << assisted_stance\.elapsed_seconds\(\) << )\'\s*\'\s*;',
    r'\1std::endl;',
    text,
    count=1,
)
if count != 1:
    raise RuntimeError(f"expected one broken balance diagnostic literal, got {count}")
path.write_text(text, encoding="utf-8")
