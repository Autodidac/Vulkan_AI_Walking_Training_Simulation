#!/usr/bin/env python3
from __future__ import annotations

import apply_v0721_rig_gait_repair as executor


def patch_repository_audit() -> None:
    path = "tools/repository_audit.cmake"
    text = executor.read(path)
    text = executor.replace_once(text,
        "        tests/v0721_readable_telemetry_tests.cpp\n",
        "        tests/v0721_readable_telemetry_tests.cpp\n"
        "        tests/v0721_rig_gait_tests.cpp\n",
        "audit rig/gait test file")
    text = executor.replace_once(text,
        '''        "WALK-HUMAN-STATUS-263"
        "WALK-ADVANCED-269"
        "WALK-TELEMETRY-TEST-272"
        "WALK-RELEASE-275")
''',
        '''        "WALK-HUMAN-STATUS-263"
        "WALK-ADVANCED-269"
        "WALK-TELEMETRY-TEST-272"
        "WALK-AUTO-TUNING-275"
        "WALK-SIDE-GAIT-276"
        "WALK-RIG-LAB-279"
        "WALK-RELEASE-283")
''', "audit current missions")
    text = executor.replace_once(text,
        '''        tools/apply_v0721_readable_telemetry.py
        .github/workflows/apply-v0721-readable-telemetry.yml)
''',
        '''        tools/apply_v0721_readable_telemetry.py
        .github/workflows/apply-v0721-readable-telemetry.yml
        tools/cache_v0721_rig_repair.py
        tools/apply_v0721_rig_gait_repair.py
        tools/run_v0721_rig_gait_repair.py
        .github/workflows/cache-v0721-rig-repair.yml
        .github/workflows/apply-v0721-rig-gait-repair.yml)
''', "audit temporary rig repair tools")
    executor.write(path, text)


executor.patch_repository_audit = patch_repository_audit
raise SystemExit(executor.main())
