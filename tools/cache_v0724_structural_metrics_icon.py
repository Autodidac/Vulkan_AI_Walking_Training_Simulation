#!/usr/bin/env python3
from pathlib import Path

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')
heading = '# Runner v0.7.24 fixed skeleton, truthful telemetry, and screenshot icon\n'
if heading not in text:
    section = '''# Runner v0.7.24 fixed skeleton, truthful telemetry, and screenshot icon

**Release state:** CACHED BEFORE IMPLEMENTATION — RELEASE BLOCKING.

Direct packaged v0.7.23 eye testing shows three remaining release-blocking failures. First, the generated neon icon is not the screenshot crop requested by the user. Second, load-bearing leg bones visibly shorten under body weight and duck/walk forces even though automatic anatomy mutation was disabled; the remaining stiffness mutation and final solver order still permit a fixed game character to compress. Third, the default dashboard labels rollout terminations as valid skill attempts, reports duck-press completions as generic obstacles, and can show 100% lesson progress with zero mastery confirmations. The screenshot therefore displays large totals that are numerically real internal episodes but semantically wrong for a normal user.

### WALK-SCREENSHOT-ICON-296 — Use the supplied screenshot as the application icon
**Status:** OPEN — RELEASE BLOCKING

Replace the generated illustration with an exact square crop of the supplied v0.7.17 armored Runner screenshot. Preserve those screenshot pixels as the canonical source, generate PNG/BMP/multi-resolution ICO assets from that source without redrawing it, embed the ICO in the Windows executable, and package the source crop for audit.

### WALK-BONE-LENGTH-297 — Make skeleton segment lengths invariant
**Status:** OPEN — RELEASE BLOCKING

Distance constraints represent rigid bones, not springs. After every simulation step, each bone must remain within a tight tolerance of its authored rest length under standing, duck pressure, walking, impacts, and terrain contact. A leg may rotate at joints but may not visibly shorten, stretch, telescope, or collapse into the pelvis.

### WALK-LOAD-BEARING-298 — End each solver step in a structurally valid pose
**Status:** OPEN — RELEASE BLOCKING

Reorder/finalize constraint projection so the last motor, terrain, and ground operations cannot leave load-bearing chains compressed after the final iteration. Add a final rigid projection and support-safe vertical correction that preserves lengths rather than pushing individual foot particles through the chain.

### WALK-AUTO-STIFFNESS-299 — Stop automatic tuning from weakening bones
**Status:** OPEN — RELEASE BLOCKING

Automatic controller refinement may tune motor strength and joint range only. It may not lower bone stiffness or accept a controller by making the skeleton compliant. Loaded/manual rigs are canonicalized to rigid structural constraints while preserving node positions, topology, semantics, and compatible policy parameters.

### WALK-DEBUG-TRUTH-300 — Count passed skill checks instead of merely nonterminal motion
**Status:** OPEN — RELEASE BLOCKING

`VALID`/`PASSED` episode totals must mean the completed run satisfied the current stage qualification and body-integrity gates. A run that merely avoided a terminal physics fault but failed walking, crouch, support, progress, or body-contact evidence is a failed stage check, not a valid skill attempt.

### WALK-PROGRESS-301 — Do not show 100% with zero mastery confirmations
**Status:** OPEN — RELEASE BLOCKING

Separate training work from mastery. Display controller-update/episode/test sampling as `TRAINING WORK`, display completed evaluations as `TESTS RUN`, and display consecutive passing confirmations as `MASTERY PASSES x/y`. Overall lesson progress must reserve explicit progress for mastery confirmations and cannot reach 100% until the required confirmation streak is complete.

### WALK-TOTALS-302 — Use stage-neutral, understandable totals
**Status:** OPEN — RELEASE BLOCKING

Replace misleading `OBSTACLES PASSED` and ambiguous `VALID` labels with `FEATURES CLEARED`, `PASSED STAGE CHECKS`, and `FAILED STAGE CHECKS`. Explain that hundreds of parallel simulated episodes are expected and are not equivalent to hundreds of human-visible tests. Session, rig, and all-time deltas must remain monotonic and correctly based.

### WALK-VISUAL-303 — Clean the default rig presentation
**Status:** OPEN — RELEASE BLOCKING

Keep optional armor disabled by default, retain clear near/far side layering, reduce oversized joint blobs, give support stubs compact sprite-like feet, and preserve an unmistakable side-view silhouette. Rendering changes remain presentation-only and may not alter physics nodes or collision radii.

### WALK-STATE-304 — Isolate corrected structural semantics
**Status:** OPEN — RELEASE BLOCKING

Bump training/autonomy semantics and use `runner-v0724-*` autosave paths. Importing v0.7.21/v0.7.23 policies is explicit or migrates only after rigidifying the associated blueprint; a compliant old rig may not silently resume.

### WALK-REGRESSION-305 — Test screenshot icon, bone invariance, truthful totals, and progress
**Status:** OPEN — RELEASE BLOCKING

Add deterministic tests for screenshot-source identity, generated icon formats, bone-length error under static load/duck/walk soak, automatic-tuning anatomy and stiffness preservation, stage-qualified episode accounting, training-work versus mastery progress, stage-neutral labels, all preset integrity, and the complete existing Linux/Windows/UI/package matrix.

### WALK-RELEASE-306 — Publish audited Runner v0.7.24
**Status:** OPEN — RELEASE BLOCKING

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, complete deterministic/live/UI/rig/structural/telemetry tests, installed and independently extracted execution, screenshot-icon verification, ZIP/checksum/manifest creation, published-asset re-download byte verification, and cleanup leaving only `main`.

'''
    marker = '# Carried open work\n'
    if marker not in text:
        raise RuntimeError('carried-work marker missing')
    text = text.replace(marker, section + marker, 1)
text = text.replace('# Runner v0.7.24 equipment, carry, and target curriculum',
                    '# Runner v0.7.25 equipment, carry, and target curriculum')
text = text.replace('separated from the v0.7.23 rounded-outline hotfix',
                    'separated from the v0.7.24 structural/telemetry release')
path.write_text(text, encoding='utf-8', newline='\n')
