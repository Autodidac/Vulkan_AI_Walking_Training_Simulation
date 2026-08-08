#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

import apply_v0724_structural_metrics_icon as executor

ROOT = Path(__file__).resolve().parents[1]


def replace_between_fixed(text: str, start: str, end: str,
    replacement: str, label: str) -> str:
    first = text.find(start)
    if first < 0:
        raise RuntimeError(f"{label}: start marker not found")
    last = text.find(end, first)
    if last < 0:
        raise RuntimeError(f"{label}: end marker not found")
    if replacement.endswith(end):
        replacement = replacement[:-len(end)]
    return text[:first] + replacement + text[last:]


executor.replace_between = replace_between_fixed
result = executor.main()

test_path = ROOT / "tests/v0724_structural_metrics_icon_tests.cpp"
test = test_path.read_text(encoding="utf-8")
test = test.replace(
    '''        if (lhs.nodes != rhs.nodes || lhs.radii != rhs.radii
            || lhs.bones.size() != rhs.bones.size()
''',
    '''        if (lhs.nodes.size() != rhs.nodes.size()
            || lhs.radii.size() != rhs.radii.size()
            || lhs.bones.size() != rhs.bones.size()
''')
test = test.replace(
    '''            return false;
        for (std::size_t index = 0; index < lhs.bones.size(); ++index)
''',
    '''            return false;
        for (std::size_t index = 0; index < lhs.nodes.size(); ++index)
        {
            if (lhs.nodes[index].x != rhs.nodes[index].x
                || lhs.nodes[index].y != rhs.nodes[index].y
                || lhs.radii[index] != rhs.radii[index])
                return false;
        }
        for (std::size_t index = 0; index < lhs.bones.size(); ++index)
''', 1)
test_path.write_text(test, encoding="utf-8", newline="\n")

raise SystemExit(result)
