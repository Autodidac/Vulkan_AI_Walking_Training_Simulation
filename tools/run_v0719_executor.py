#!/usr/bin/env python3
from __future__ import annotations

import apply_v0719_general_locomotion as executor


def replace_first(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count < 1:
        raise RuntimeError(f"{label}: expected at least one match, found {count}")
    return text.replace(old, new, 1)


# The migration contains intentional repeated reward fragments. Apply the first
# source-ordered match. Workflow YAML is updated separately through the GitHub
# repository API because Actions tokens may not rewrite workflow files.
executor.replace_once = replace_first
executor.patch_pr_validation = lambda: None
executor.patch_release_workflow = lambda: None
raise SystemExit(executor.main())
