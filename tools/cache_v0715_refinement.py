from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MISSION = ROOT / "missioncache.md"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


text = MISSION.read_text(encoding="utf-8")

replacements = [
    (
        "### WALK-PROCESS-138 — Cache-first dependency and regression discipline\n**Status:** ACTIVE — RELEASE BLOCKING",
        "### WALK-PROCESS-138 — Cache-first dependency and regression discipline\n**Status:** VERIFIED — CACHE-FIRST HISTORY AND OPEN v0.7.16 CARRY-FORWARD RECORDED",
        "process status",
    ),
    (
        "**Release state:** ACTIVE — no publication claim until every v0.7.15 release gate below passes.",
        "**Release state:** PRE-PUBLICATION VALIDATED — final installed/extracted package, release-asset round-trip, and cleanup gates remain.",
        "release state",
    ),
    (
        "### WALK-TERRAIN-139 — One visible and physical terrain state\n**Status:** IMPLEMENTED — REVALIDATION REQUIRED",
        "### WALK-TERRAIN-139 — One visible and physical terrain state\n**Status:** VERIFIED — DETERMINISTIC AND CROSS-PLATFORM; RELEASED VISUAL/PACKAGE REVIEW PENDING",
        "terrain status",
    ),
    (
        "### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow\n**Status:** IMPLEMENTED — GROUND-SOLVER STATIC-FRICTION MANIFOLD; FULL VALIDATION REQUIRED",
        "### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow\n**Status:** VERIFIED — PHYSICAL STATIC-FRICTION MANIFOLD AND ALL-PRESET HOLD/RECOVERY PASS; RELEASED VISUAL REVIEW PENDING",
        "crouch status",
    ),
    (
        "### WALK-SIDEGAIT-141 — Normal side-view limb crossing and alternating steps\n**Status:** IMPLEMENTED — DETERMINISTIC, CROSS-PLATFORM, AND SCREENSHOT VALIDATION REQUIRED",
        "### WALK-SIDEGAIT-141 — Normal side-view limb crossing and alternating steps\n**Status:** VERIFIED — DETERMINISTIC AND CROSS-PLATFORM; RELEASED VISUAL REVIEW PENDING",
        "gait status",
    ),
    (
        "### WALK-FEET-142 — Proper forward articulated feet and physical traction\n**Status:** IMPLEMENTED — MEASURED CONTACT ANCHORS WITH MOVING RELEASE; FULL VALIDATION REQUIRED",
        "### WALK-FEET-142 — Proper forward articulated feet and physical traction\n**Status:** VERIFIED — MEASURED STATIC CONTACTS, MOVING RELEASE, AND ADVERSARIAL TESTS PASS; RELEASED VISUAL REVIEW PENDING",
        "feet status",
    ),
    (
        "### WALK-EVOLUTION-143 — Structural rig evolution with nursery adaptation\n**Status:** IMPLEMENTED — ACTIVE JOINT GROWTH, NEUTRAL SLOT TRANSFER, AND PACKAGE VALIDATION REQUIRED",
        "### WALK-EVOLUTION-143 — Structural rig evolution with nursery adaptation\n**Status:** VERIFIED — ACTIVE JOINT GROWTH, NEUTRAL TRANSFER, NURSERY, AND CROSS-PLATFORM TESTS PASS; PACKAGE REVIEW PENDING",
        "evolution status",
    ),
    (
        "### WALK-EDITOR-144 — Complete controls for gait, feet, evolution, and diagnostics\n**Status:** IMPLEMENTED — DETERMINISTIC, CROSS-PLATFORM, AND SCREENSHOT VALIDATION REQUIRED",
        "### WALK-EDITOR-144 — Complete controls for gait, feet, evolution, and diagnostics\n**Status:** VERIFIED — DETERMINISTIC AND CROSS-PLATFORM; RELEASED VISUAL REVIEW PENDING",
        "editor status",
    ),
    (
        "### WALK-STATE-145 — Isolate corrected locomotion/evolution semantics\n**Status:** IMPLEMENTED — CROSS-PLATFORM AND PACKAGE VALIDATION REQUIRED",
        "### WALK-STATE-145 — Isolate corrected locomotion/evolution semantics\n**Status:** VERIFIED — CROSS-PLATFORM RESUME/TRANSFER ISOLATION PASSES; PACKAGE REVIEW PENDING",
        "state status",
    ),
    (
        "### WALK-REGRESSION-146 — Exhaustive interaction audit for v0.7.15\n**Status:** OPEN — RELEASE BLOCKING",
        "### WALK-REGRESSION-146 — Exhaustive interaction audit for v0.7.15\n**Status:** VALIDATED — FULL LINUX/WINDOWS SUITES AND RUNTIME DIAGNOSTICS PASS; FINAL PACKAGE AUDIT PENDING",
        "regression status",
    ),
]

for old, new, label in replacements:
    text = replace_once(text, old, new, label)

anchor = """**MSVC portability correction:** the transform round-trip remains a compile-time assertion, but now compares the signed constexpr error directly against positive and negative epsilon bounds. This avoids implementation-dependent `std::abs` constexpr support without weakening the invariant. Full cross-platform revalidation remains required.\n\n"""
evidence = anchor + """**Exact-source cross-platform validation:** refinement run `31018349657` tested commit `7778180ec754e0faa430af29918b804656554030`. Linux GCC 14 job `92348344345` passed the complete deterministic suite. Windows Server 2025 / VS 2026 MSVC 19.51 job `92348748327` built the SDL3/Vulkan application and every test target, passed 9/9 CTest suites, passed all eight preset Stand cases, all eight static crouch hold/recovery cases, the ordered curriculum evidence, and the complete 24/24 live acceptance matrix. `Runner.exe --diagnose-package` and `Runner.exe --diagnose-acceptance` also passed from an unrelated temporary directory. The hosted runner lacked a Vulkan presentation surface, but the Vulkan backend, loader/runtime files, shaders, executable, and noninteractive diagnostics were present and valid. No authored-coordinate crouch foot pinning remains.\n\n**Remaining v0.7.15 gate:** run the clean script-free PR workflow, install and audit the package, test `run.bat` from an unrelated directory, verify optional artwork/assets and fallback behavior, create checksums and a per-file manifest, merge, publish `v0.7.15`, re-download and byte-compare the release asset, then remove obsolete PRs/branches. Visual appearance remains subject to released screenshot/manual review and must reopen the exact mission if contradicted.\n\n"""
text = replace_once(text, anchor, evidence, "validation evidence")

MISSION.write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")
