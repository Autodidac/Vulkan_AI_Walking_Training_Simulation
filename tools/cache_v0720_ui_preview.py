#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / "missioncache.md"
text = path.read_text(encoding="utf-8")
marker = "# Carried open work\n"
section = r'''# Runner v0.7.20 viewport, preview continuity, and application identity

**Release state:** CACHED BEFORE IMPLEMENTATION — RELEASE BLOCKING.

The v0.7.19 user eye test proves two release-blocking defects remain. First, the live preview still restarts near the beginning despite background training showing longer motion. Source inspection identifies an unconditional `live_.set_course(...)` call during every published snapshot synchronization, which resets the live environment even when neither the rig nor course changed. Second, Windows high-DPI operation feeds framebuffer dimensions into application layout while Vulkan presents with a different surface extent, so the entire UI is effectively scaled down, the side panel is narrower than designed, text becomes unreadable, and world geometry can remain visible in the inter-panel gap. The screenshot also shows inconsistent spacing, weak telemetry contrast, crowded status text, and no coherent application icon.

### WALK-DPI-253 — Use one explicit application coordinate space
**Status:** OPEN — RELEASE BLOCKING

Application layout, input, text, and widgets use logical window coordinates. Vulkan independently maps that logical canvas to the actual swapchain extent. Mouse coordinates, text measurement, layout calculations, and renderer push constants must agree at 100%, 125%, 150%, and 200% Windows display scaling without half-size UI or repeated swapchain recreation.

### WALK-CLIP-254 — Keep world and PIP rendering inside their viewports
**Status:** OPEN — RELEASE BLOCKING

Terrain, distance signs, hazards, particles, rigs, and PIP content may not appear in the side panel, panel gap, margins, title bar, or outside their intended cards. The rendering path must provide deterministic clipping or an equivalent final mask; draw order alone is not accepted as the boundary contract.

### WALK-READABILITY-255 — Make all live text readable at normal desktop distances
**Status:** OPEN — RELEASE BLOCKING

Use consistent text scales, line heights, contrast, card backgrounds, and minimum fitting limits. Primary status, current lesson, stage work, training results, controls, PIP state, and bottom controller state must remain readable without microscopic fallback text. Do not cram unrelated telemetry onto one line merely to avoid layout work.

### WALK-LAYOUT-256 — Align the complete live interface
**Status:** OPEN — RELEASE BLOCKING

Top bar, world, PIP, side panel, buttons, cards, tabs, labels, and margins use shared layout constants and consistent alignment. Validate 1280x820, 1600x900, 1920x1080, the observed 2047x1112 high-DPI window, and 2560x1440. No box overlap, cropped label, negative dimension, or content escaping its parent is allowed.

### WALK-PREVIEW-CONTINUITY-257 — Stop resetting live motion on every publication
**Status:** OPEN — RELEASE BLOCKING

A normal immutable training publication must update telemetry and controller parameters without restarting the large live environment. Reset only for a real rig change, real course/difficulty change, explicit user reset, or terminal episode. A newly improved champion may be adopted without forcing the visible rig back to the starting line.

### WALK-PREVIEW-TRUTH-258 — Make the large preview represent retained progress
**Status:** OPEN — RELEASE BLOCKING

When a validated champion exists, the large preview uses retained champion parameters and runs a complete deterministic episode. Before a champion exists it may show the current policy, but it must not imply progress by replaying a two-step fragment. UI state clearly distinguishes current exploratory policy, retained champion, and terminal restart.

### WALK-SOURCE-CLEANUP-259 — Remove configure-time source patch indirection
**Status:** OPEN — RELEASE BLOCKING

Fold the generated v0.7.19 source patches into canonical source files, remove the Python source-rewriter and generated-source CMake path, eliminate stale v0.7.6/v0.7.17 strings and paths, and keep one direct C++23 implementation. Release builds and IDE navigation must compile the same files developers edit.

### WALK-ICON-260 — Add a complete Runner application icon set
**Status:** OPEN — RELEASE BLOCKING

Add a high-contrast Runner robot/speed icon as a transparent PNG, multi-resolution Windows ICO, and runtime window icon. Embed the ICO into the Windows executable and package the source PNG/BMP assets. The icon must remain recognizable at 16, 32, 48, 64, 128, and 256 pixels.

### WALK-UI-TEST-261 — Add deterministic UI, DPI, clipping, and preview tests
**Status:** OPEN — RELEASE BLOCKING

Tests cover logical-to-surface scaling, mouse mapping, all supported layout sizes, panel/PIP containment, gap masking or clip ranges, readable minimum text scales, preview reset decisions, and the presence/validity of every icon size. Existing locomotion, terrain, concurrency, and acceptance suites remain mandatory.

### WALK-RELEASE-262 — Publish audited Runner v0.7.20
**Status:** OPEN — RELEASE BLOCKING

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, complete deterministic and live acceptance suites, UI diagnostic at all required dimensions, installed/extracted execution, runtime icon verification, ZIP/checksum/manifest, release re-download byte verification, and a final branch/workflow cleanup leaving only `main`.

'''
if "# Runner v0.7.20 viewport, preview continuity, and application identity" in text:
    raise SystemExit("v0.7.20 mission section already exists")
if marker not in text:
    raise SystemExit("mission insertion marker missing")
text = text.replace(marker, section + marker, 1)
path.write_text(text, encoding="utf-8", newline="\n")
