#!/usr/bin/env python3
from pathlib import Path

path = Path("missioncache.md")
text = path.read_text(encoding="utf-8")
heading = "# Runner v0.7.23 true rounded-outline rendering hotfix\n"
if heading not in text:
    section = r'''# Runner v0.7.23 true rounded-outline rendering hotfix

**Release state:** CACHED BEFORE IMPLEMENTATION — RELEASE BLOCKING.

Direct packaged v0.7.22 eye testing proves the black-frame correction exposed the second half of the same rendering defect. `add_rounded_rect` still paints the entire outer rounded rectangle with the outline color before attempting to draw a transparent inset. Alpha blending cannot erase the already-written outline fill, so the Live world, dashboard, training PIP, Rig Lab viewport, and Rig Lab pages become uniform gray—the linear-space border color converted through the sRGB swapchain. The current final-frame test was weakened to accept source-vertex color diversity even when the final composite is one flat color; it therefore certified a visibly broken package.

### WALK-TRUE-OUTLINE-290 — Replace destructive fake borders with real outline geometry
**Status:** OPEN — RELEASE BLOCKING

A border-only rounded rectangle must emit only an inset perimeter stroke. It may not paint the full card first, rely on transparent geometry to erase pixels, or alter the center of the underlying content. Filled cards render their fill once and then layer a bounded outline stroke on top.

### WALK-ROUNDING-291 — Preserve rounded corners without full-surface overdraw
**Status:** OPEN — RELEASE BLOCKING

Implement a closed rounded perimeter from straight segments and quarter arcs, clamped for tiny rectangles and thick borders. The stroke stays inside the requested bounds, remains finite, respects Canvas clipping, and does not introduce gaps, corner spikes, or opaque center geometry.

### WALK-COMPOSITE-292 — Test the final composite rather than source vertices
**Status:** OPEN — RELEASE BLOCKING

Render a known colored background, add a border-only rounded rectangle, and prove the center pixel remains the original background while an edge sample contains the outline. Final-frame validation must require useful final color diversity; hidden source geometry underneath a uniform overlay is not accepted.

### WALK-ALL-VIEWS-293 — Revalidate Live and every Rig Lab page
**Status:** OPEN — RELEASE BLOCKING

The complete final draw order must preserve visible world, dashboard, PIP, Rig Lab viewport, and `PRESETS`, `STRUCTURE`, `MOTORS`, and `TEST` content. Repeated page switching must not leak clip state or create black, gray, or single-color cards.

### WALK-HOTFIX-COMPAT-294 — Preserve v0.7.21 training and rig semantics
**Status:** OPEN — RELEASE BLOCKING

This remains a rendering-only repair. Preserve policy dimensions, gait rules, terrain, fixed anatomy, readable telemetry, `runner-v0721-*` autosaves, and training semantics `0x0007'2101`. Bump only the application/package version to `0.7.23`.

### WALK-RELEASE-295 — Publish audited Runner v0.7.23
**Status:** OPEN — RELEASE BLOCKING

Require Linux GCC 14 warnings-as-errors, complete deterministic/live/UI/rig tests, full Windows SDL3/Vulkan build, center-preservation and final-composite diagnostics, installed/extracted execution, ZIP/checksum/manifest creation, published-asset re-download byte verification, and cleanup leaving only `main`.

'''
    marker = "# Carried open work\n"
    if marker not in text:
        raise SystemExit("carried-work marker not found")
    text = text.replace(marker, section + marker, 1)

text = text.replace(
    "# Runner v0.7.23 equipment, carry, and target curriculum",
    "# Runner v0.7.24 equipment, carry, and target curriculum")
text = text.replace(
    "intentionally separated from the v0.7.22 rendering hotfix",
    "intentionally separated from the v0.7.23 rounded-outline hotfix")
path.write_text(text, encoding="utf-8", newline="\n")
print("Runner v0.7.23 gray-frame missions cached")
