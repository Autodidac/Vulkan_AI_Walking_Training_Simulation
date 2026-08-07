#!/usr/bin/env python3
from __future__ import annotations

import apply_v0719_general_locomotion as executor


def replace_first(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count < 1:
        raise RuntimeError(f"{label}: expected at least one match, found {count}")
    return text.replace(old, new, 1)


executor.replace_once = replace_first
raise SystemExit(executor.main())
