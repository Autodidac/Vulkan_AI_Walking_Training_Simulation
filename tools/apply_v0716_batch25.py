#!/usr/bin/env python3
from __future__ import annotations

import argparse
import pathlib
import re
import sys
from textwrap import dedent

ROOT = pathlib.Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, content: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def replace_all_checked(text: str, old: str, new: str, minimum: int, label: str) -> str:
    count = text.count(old)
    if count < minimum:
        raise RuntimeError(f"{label}: expected at least {minimum} matches, found {count}")
    return text.replace(old, new)


MISSION_SECTION = dedent(r'''
# Runner v0.7.16 adaptive viewport and usability batch

**Release state:** CACHED — 25 MISSIONS SELECTED FOR ONE AUDITED RELEASE.

The v0.7.15 world scale is physically correct, but its fixed 22 px/m live camera makes the rig unnecessarily small. This batch keeps the corrected world scale and terrain synchronization while restoring a readable, user-adjustable view. Equipment, target, policy-extension, and combined carry/fire work remains intact below and moves to v0.7.17 rather than being partially smuggled into this viewport release.

### WALK-VIEW-156 — Correct the default live zoom
**Status:** CACHED — IMPLEMENTATION PENDING

Replace the fixed 22 px/m live view with a substantially closer default that keeps the corrected world scale. The rig must be immediately readable without returning to the oversized pre-v0.7.15 framing.

### WALK-VIEW-157 — Fit zoom to current rig height
**Status:** CACHED — IMPLEMENTATION PENDING

Derive the automatic live scale from the current finite particle bounds so scaffold, humanoid, chicken, quadruped, crawler, hexapod, and monoped remain readable without sharing a misleading one-size camera.

### WALK-VIEW-158 — Add mouse-wheel live zoom
**Status:** CACHED — IMPLEMENTATION PENDING

Mouse-wheel input over the live viewport adjusts zoom without changing simulation scale, training state, terrain coordinates, or the side panel.

### WALK-VIEW-159 — Restore automatic view on reset
**Status:** CACHED — IMPLEMENTATION PENDING

The existing reset command also restores automatic camera fitting, default zoom bias, and a clean follow state.

### WALK-VIEW-160 — Bound manual and automatic zoom
**Status:** CACHED — IMPLEMENTATION PENDING

Clamp all camera scale paths to tested minimum and maximum values so the rig cannot become a dot or explode beyond the viewport.

### WALK-VIEW-161 — Route zoom only through the world viewport
**Status:** CACHED — IMPLEMENTATION PENDING

Wheel activity over controls or the side panel must not alter the world camera.

### WALK-VIEW-162 — Preserve useful forward lookahead
**Status:** CACHED — IMPLEMENTATION PENDING

Calculate course lookahead from viewport width and current scale so the rig remains left of center with useful terrain and hazards ahead.

### WALK-VIEW-163 — Use frame-rate-independent camera smoothing
**Status:** CACHED — IMPLEMENTATION PENDING

Follow and zoom smoothing use elapsed time rather than a fixed per-frame interpolation coefficient.

### WALK-VIEW-164 — Add a camera dead zone
**Status:** CACHED — IMPLEMENTATION PENDING

Small root jitter inside a bounded screen-space dead zone must not shake the camera.

### WALK-VIEW-165 — Keep ground framing consistent
**Status:** CACHED — IMPLEMENTATION PENDING

Live world conversion and inverse conversion share one camera ground fraction so editing and rendering do not disagree vertically.

### WALK-PIP-166 — Increase training-PIP readable area
**Status:** CACHED — IMPLEMENTATION PENDING

Give the training PIP more usable width and height while preserving telemetry and panel separation.

### WALK-PIP-167 — Fit PIP scale through the shared camera contract
**Status:** CACHED — IMPLEMENTATION PENDING

Use tested PIP scale limits and a tighter local course window instead of shrinking the rig to include distant obstacles.

### WALK-VIEW-168 — Expose live camera telemetry
**Status:** CACHED — IMPLEMENTATION PENDING

The live footer reports current px/m and whether the view is automatic or manually adjusted.

### WALK-VIEW-169 — Add direct zoom controls
**Status:** CACHED — IMPLEMENTATION PENDING

The live side panel exposes Zoom Out, Auto View, and Zoom In controls in addition to the wheel.

### WALK-TEST-170 — Deterministic camera math tests
**Status:** CACHED — IMPLEMENTATION PENDING

Test automatic fitting, clamps, wheel direction, reset defaults, lookahead, dead-zone behavior, and frame-rate-independent convergence.

### WALK-TEST-171 — Viewport and PIP layout regression tests
**Status:** CACHED — IMPLEMENTATION PENDING

Test supported-window containment and prove the enlarged PIP still avoids primary and bottom telemetry.

### WALK-DIAG-172 — Add packaged camera diagnostic
**Status:** CACHED — IMPLEMENTATION PENDING

`Runner.exe --diagnose-camera` validates the compiled camera contract without opening a window and is exercised from build-tree, installed, and extracted packages.

### WALK-STATE-173 — Isolate v0.7.16 training semantics
**Status:** CACHED — IMPLEMENTATION PENDING

Bump training semantics even though camera state is not learned, preventing any later viewport-associated acceptance or diagnostic change from silently sharing ambiguous v0.7.15 state.

### WALK-STATE-174 — Isolate v0.7.16 autosave paths
**Status:** CACHED — IMPLEMENTATION PENDING

Policy, evolved-rig, and autonomy-state autosaves use `runner-v0716-*` paths.

### WALK-BUILD-175 — Bump Runner package version
**Status:** CACHED — IMPLEMENTATION PENDING

CMake, runtime version output, window title, package name, diagnostics, and release workflow use 0.7.16.

### WALK-DOC-176 — Update user-facing controls and behavior
**Status:** CACHED — IMPLEMENTATION PENDING

README documents adaptive view, wheel zoom, side-panel zoom controls, reset behavior, camera diagnostics, and retained world scale.

### WALK-DOC-177 — Record the release in the changelog
**Status:** CACHED — IMPLEMENTATION PENDING

CHANGELOG receives one consolidated v0.7.16 entry rather than a separate release-notes file.

### WALK-PROCESS-178 — Add repository AGENTS.md
**Status:** CACHED — IMPLEMENTATION PENDING

Create root `AGENTS.md` with cache-first scope capture, regression inventory, test expectations, documentation obligations, release gates, visual-feedback reopening, and cleanup rules.

### WALK-PROCESS-179 — Reconcile current release ledger and audit rules
**Status:** CACHED — IMPLEMENTATION PENDING

Close stale v0.7.15 publication text, move the untouched equipment batch to v0.7.17, require AGENTS.md and the camera document in repository/package audits, and forbid temporary applicators from the final branch.

### WALK-RELEASE-180 — Publish audited Runner v0.7.16
**Status:** CACHED — IMPLEMENTATION PENDING

Require Linux GCC 14 warnings-as-errors, the full Windows SDL3/Vulkan build, every deterministic suite, camera diagnostic, 24-case locomotion acceptance, installed/extracted package diagnostics, run.bat, checksum, manifest, release re-download, branch cleanup, and zero open PRs.

''')


def cache() -> None:
    path = "missioncache.md"
    text = read(path)
    if "### WALK-VIEW-156" in text:
        return
    text = replace_once(
        text,
        "# Runner v0.7.16 equipment, carry, and target curriculum",
        "# Runner v0.7.17 equipment, carry, and target curriculum",
        "move equipment release heading",
    )
    text = replace_once(
        text,
        "**Release state:** CACHED AND OPEN — must not disappear if v0.7.15 publishes first.",
        "**Release state:** CACHED AND OPEN — carried intact after the v0.7.16 viewport release.",
        "equipment carry-forward status",
    )
    text = text.replace(
        "### WALK-RELEASE-155 — Publish audited Runner v0.7.16",
        "### WALK-RELEASE-155 — Publish audited Runner v0.7.17",
        1,
    )
    insert_at = text.index("# Runner v0.7.17 equipment, carry, and target curriculum")
    text = text[:insert_at] + MISSION_SECTION + text[insert_at:]
    text = text.replace(
        "### WALK-RELEASE-147 — Publish audited Runner v0.7.15\n**Status:** OPEN — RELEASE BLOCKING",
        "### WALK-RELEASE-147 — Publish audited Runner v0.7.15\n**Status:** PUBLISHED — RELEASE VERIFIED",
        1,
    )
    text = text.replace(
        "**Release state:** PRE-PUBLICATION VALIDATED — final installed/extracted package, release-asset round-trip, and cleanup gates remain.",
        "**Release state:** PUBLISHED — v0.7.15 package, release assets, and cleanup verified; visual feedback remains authoritative.",
        1,
    )
    write(path, text)


VIEW_CAMERA_HPP = dedent(r'''
#pragma once

#include <algorithm>
#include <cmath>

namespace runner::view_camera
{
    inline constexpr float default_pixels_per_meter = 42.0f;
    inline constexpr float minimum_pixels_per_meter = 30.0f;
    inline constexpr float maximum_pixels_per_meter = 62.0f;
    inline constexpr float minimum_zoom_factor = 0.72f;
    inline constexpr float maximum_zoom_factor = 1.55f;
    inline constexpr float target_rig_height_fraction = 0.34f;
    inline constexpr float live_ground_fraction = 0.74f;
    inline constexpr float lookahead_screen_fraction = 0.17f;
    inline constexpr float camera_dead_zone_pixels = 18.0f;
    inline constexpr float follow_response_per_second = 6.5f;
    inline constexpr float zoom_response_per_second = 7.5f;
    inline constexpr float pip_minimum_pixels_per_meter = 24.0f;
    inline constexpr float pip_maximum_pixels_per_meter = 56.0f;

    [[nodiscard]] constexpr float clamp_zoom_factor(float value) noexcept
    {
        return std::clamp(value, minimum_zoom_factor, maximum_zoom_factor);
    }

    [[nodiscard]] inline float apply_wheel_zoom(float current, float wheel) noexcept
    {
        if (!std::isfinite(current))
            current = 1.0f;
        if (!std::isfinite(wheel) || std::abs(wheel) < 0.01f)
            return clamp_zoom_factor(current);
        return clamp_zoom_factor(current * std::pow(1.12f, wheel));
    }

    [[nodiscard]] constexpr float automatic_pixels_per_meter(
        float viewport_height, float rig_height) noexcept
    {
        if (!(viewport_height > 0.0f) || !(rig_height > 0.0f))
            return default_pixels_per_meter;
        return std::clamp(
            viewport_height * target_rig_height_fraction / std::max(rig_height, 0.75f),
            default_pixels_per_meter,
            maximum_pixels_per_meter);
    }

    [[nodiscard]] constexpr float fitted_pixels_per_meter(
        float viewport_height, float rig_height, float zoom_factor) noexcept
    {
        return std::clamp(
            automatic_pixels_per_meter(viewport_height, rig_height)
                * clamp_zoom_factor(zoom_factor),
            minimum_pixels_per_meter,
            maximum_pixels_per_meter);
    }

    [[nodiscard]] constexpr float lookahead_meters(
        float viewport_width, float pixels_per_meter) noexcept
    {
        return pixels_per_meter > 0.0f
            ? std::max(2.0f,
                viewport_width * lookahead_screen_fraction / pixels_per_meter)
            : 2.0f;
    }

    [[nodiscard]] inline float exponential_alpha(
        float response_per_second, float dt) noexcept
    {
        if (!(response_per_second > 0.0f) || !(dt > 0.0f)
            || !std::isfinite(response_per_second) || !std::isfinite(dt))
            return 0.0f;
        return std::clamp(
            1.0f - std::exp(-response_per_second * dt), 0.0f, 1.0f);
    }

    [[nodiscard]] inline float smooth_zoom(
        float current, float target, float dt) noexcept
    {
        if (!std::isfinite(current))
            current = default_pixels_per_meter;
        if (!std::isfinite(target))
            target = default_pixels_per_meter;
        const float alpha = exponential_alpha(zoom_response_per_second, dt);
        return std::clamp(
            current + (target - current) * alpha,
            minimum_pixels_per_meter,
            maximum_pixels_per_meter);
    }

    [[nodiscard]] inline float smooth_camera(
        float current, float target, float pixels_per_meter, float dt) noexcept
    {
        if (!std::isfinite(current))
            return target;
        if (!std::isfinite(target) || !(pixels_per_meter > 0.0f))
            return current;
        const float error = target - current;
        if (std::abs(error) * pixels_per_meter <= camera_dead_zone_pixels)
            return current;
        const float dead_zone_world = camera_dead_zone_pixels / pixels_per_meter;
        const float adjusted_target = target
            - std::copysign(dead_zone_world, error);
        const float alpha = exponential_alpha(follow_response_per_second, dt);
        return current + (adjusted_target - current) * alpha;
    }

    [[nodiscard]] constexpr float pip_pixels_per_meter(
        float horizontal_scale, float vertical_scale) noexcept
    {
        return std::clamp(
            std::min(horizontal_scale, vertical_scale),
            pip_minimum_pixels_per_meter,
            pip_maximum_pixels_per_meter);
    }
}
''').lstrip()


VIEW_CAMERA_TESTS = dedent(r'''
#include "ui_layout.hpp"
#include "view_camera.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner view camera test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    bool close(float a, float b, float tolerance = 0.001f)
    {
        return std::abs(a - b) <= tolerance;
    }
}

int main()
{
    using namespace runner;

    require(view_camera::default_pixels_per_meter > 22.0f,
        "default view still uses the overly distant v0.7.15 scale");
    require(view_camera::fitted_pixels_per_meter(820.0f, 3.0f, 1.0f)
            >= view_camera::default_pixels_per_meter,
        "automatic rig fit moves farther away than the corrected default");
    require(close(view_camera::fitted_pixels_per_meter(
            820.0f, 100.0f, 0.01f),
        view_camera::minimum_pixels_per_meter),
        "minimum zoom clamp is not enforced");
    require(close(view_camera::fitted_pixels_per_meter(
            820.0f, 0.1f, 100.0f),
        view_camera::maximum_pixels_per_meter),
        "maximum zoom clamp is not enforced");
    require(view_camera::apply_wheel_zoom(1.0f, 1.0f) > 1.0f,
        "positive wheel input does not zoom in");
    require(view_camera::apply_wheel_zoom(1.0f, -1.0f) < 1.0f,
        "negative wheel input does not zoom out");
    require(close(view_camera::apply_wheel_zoom(
            view_camera::maximum_zoom_factor, 20.0f),
        view_camera::maximum_zoom_factor),
        "wheel zoom escapes the maximum factor");
    require(view_camera::lookahead_meters(900.0f, 42.0f) > 3.0f,
        "camera no longer preserves useful course lookahead");

    const float still = view_camera::smooth_camera(
        0.0f, 0.1f, 42.0f, 1.0f / 60.0f);
    require(close(still, 0.0f),
        "small root jitter escapes the camera dead zone");
    const float moving = view_camera::smooth_camera(
        0.0f, 4.0f, 42.0f, 1.0f / 60.0f);
    require(moving > 0.0f && moving < 4.0f,
        "camera follow is not bounded and smoothed");

    float sixty_hz = 0.0f;
    for (int frame = 0; frame < 60; ++frame)
        sixty_hz = view_camera::smooth_camera(
            sixty_hz, 5.0f, 42.0f, 1.0f / 60.0f);
    float thirty_hz = 0.0f;
    for (int frame = 0; frame < 30; ++frame)
        thirty_hz = view_camera::smooth_camera(
            thirty_hz, 5.0f, 42.0f, 1.0f / 30.0f);
    require(std::abs(sixty_hz - thirty_hz) < 0.08f,
        "camera smoothing depends materially on frame rate");

    require(close(view_camera::pip_pixels_per_meter(10.0f, 80.0f),
            view_camera::pip_minimum_pixels_per_meter),
        "PIP minimum scale is not enforced");
    require(close(view_camera::pip_pixels_per_meter(80.0f, 90.0f),
            view_camera::pip_maximum_pixels_per_meter),
        "PIP maximum scale is not enforced");

    require(ui_layout::live_layout_valid(1900.0f, 1180.0f),
        "default window layout is invalid after PIP enlargement");
    const ui_layout::Box content = ui_layout::content_box(1900.0f, 1180.0f);
    const ui_layout::Box world = ui_layout::live_world_box(content);
    const ui_layout::Box pip = ui_layout::training_pip_box(world);
    require(ui_layout::contains(world, pip),
        "training PIP escapes the live world");
    require(!ui_layout::overlaps(
            pip, ui_layout::primary_telemetry_box(world)),
        "training PIP overlaps primary telemetry");
    require(!ui_layout::overlaps(
            pip, ui_layout::bottom_telemetry_box(world)),
        "training PIP overlaps bottom telemetry");

    std::cout << "Runner adaptive view camera tests passed\n";
    return EXIT_SUCCESS;
}
''').lstrip()


AGENTS_MD = dedent(r'''
# Runner agent instructions

## Authoritative workflow

1. Read and update `missioncache.md` before changing source.
2. Preserve every unfinished mission explicitly. Never hide work by renaming, deleting, or moving it only into chat.
3. Inventory interactions across physics, rigs, contacts, terrain, observations, curriculum, policy dimensions, persistence, editor, renderer, diagnostics, tests, packaging, branches, and release assets.
4. Prefer one coherent implementation over visible-symptom patches.
5. Add deterministic positive, negative, adversarial, and repeated-seed coverage for every behavior change.
6. Re-read the diff and mission cache after tests. Record newly discovered consequences instead of waiving them.

## Required validation

For release work, run:

- repository hygiene and `git diff --check`;
- Linux GCC 14 warnings-as-errors and all CTest suites;
- the complete Windows SDL3/Vulkan build and all tests;
- `Runner.exe --diagnose-package`;
- `Runner.exe --diagnose-acceptance`;
- every feature-specific diagnostic, including `--diagnose-camera`;
- installed and independently extracted `run.bat` from an unrelated working directory;
- ZIP checksum and per-file manifest audit;
- published-asset re-download and byte comparison.

A compile is not completion. Visible packaged-runtime evidence can reopen an automated pass.

## Documentation obligations

Every release updates, as applicable:

- `missioncache.md`;
- `CHANGELOG.md`;
- `README.md`;
- a focused document under `docs/`;
- CMake/package install lists;
- release workflow version, package name, required files, notes, and cleanup;
- this file when repository process changes.

Do not create `RELEASE_NOTES*.md` or additional mission-ledger documents.

## Branch and release hygiene

- Use `agent/<release-or-scope>` branches.
- Temporary applicators, trigger files, and one-use workflows must delete themselves before the final PR.
- Do not merge marker-only or obsolete observer branches.
- Final publication requires zero open cleanup PRs and only `main` unless a documented next-release branch is intentionally retained.
- Never overwrite an existing release tag.
- Tag only audited source and verify every uploaded asset after publication.

## User eye testing

Screenshots and direct observations outrank claims inferred from metrics. Incorrect scale, zoom, posture, gait, feet, terrain synchronization, PIP framing, or UI readability reopens the exact matching mission.
''').lstrip()


CAMERA_DOC = dedent(r'''
# Runner v0.7.16 adaptive camera batch

Runner v0.7.15 corrected the physical world scale and synchronized rendered terrain with collision terrain, but the fixed 22 px/m live camera made otherwise-correct rigs appear too small. v0.7.16 separates physical scale from view magnification.

## Live view

- Automatic zoom fits the current finite rig bounds.
- The corrected default is substantially closer than 22 px/m.
- Automatic and manual scales are clamped.
- Mouse wheel over the world changes only view magnification.
- The side panel provides Zoom Out, Auto View, and Zoom In.
- `R` resets the preview and restores automatic view.
- Lookahead is computed from viewport width and current scale.
- Follow and zoom smoothing are elapsed-time based.
- A screen-space dead zone suppresses root jitter.

No camera operation changes particles, terrain, observations, rewards, course coordinates, training state, or SandHybrid cell scale.

## Training PIP

The PIP receives more usable space, keeps a tighter local course window, uses shared tested scale limits, and continues labeling distant hazards rather than shrinking the body to include them.

## Diagnostics

`Runner.exe --diagnose-camera` validates the compiled camera constants and representative automatic, manual, PIP, lookahead, clamp, and smoothing behavior without opening the application window.

The release gate runs this diagnostic from the build tree, installed package, and independently extracted archive.
''').lstrip()


def patch_app_cpp() -> None:
    text = read("src/app.cpp")
    text = replace_once(
        text,
        '#include "ui_layout.hpp"\n#include "ui_font.hpp"',
        '#include "ui_layout.hpp"\n#include "ui_font.hpp"\n#include "view_camera.hpp"',
        "app camera include",
    )
    text = replace_once(
        text,
        "        [[nodiscard]] Vec2 world_to_screen(Vec2 world, Rect viewport, float camera_x,\n"
        "            float pixels_per_meter, float ground_fraction = 0.72f) noexcept",
        "        [[nodiscard]] Vec2 world_to_screen(Vec2 world, Rect viewport, float camera_x,\n"
        "            float pixels_per_meter,\n"
        "            float ground_fraction = view_camera::live_ground_fraction) noexcept",
        "world-to-screen ground fraction",
    )
    text = replace_once(
        text,
        "            const float ground_y = viewport.position.y + viewport.size.y * 0.72f;",
        "            const float ground_y = viewport.position.y\n"
        "                + viewport.size.y * view_camera::live_ground_fraction;",
        "screen-to-world ground fraction",
    )
    text = replace_once(
        text,
        "        float camera_x{};\n        art::PixelArt original_runner_art{};",
        "        float camera_x{};\n"
        "        float live_pixels_per_meter{ view_camera::default_pixels_per_meter };\n"
        "        float live_zoom_factor{ 1.0f };\n"
        "        bool live_zoom_auto{ true };\n"
        "        art::PixelArt original_runner_art{};",
        "camera state fields",
    )
    text = replace_once(
        text,
        "        std::filesystem::path autosave_policy_path{ \"runner-v0715-gait-autosave.eppo\" };\n"
        "        std::filesystem::path autosave_rig_path{ \"runner-v0715-gait-evolved.rig\" };\n"
        "        std::filesystem::path autosave_state_path{ \"runner-v0715-gait-autonomy.state\" };",
        "        std::filesystem::path autosave_policy_path{ \"runner-v0716-gait-autosave.eppo\" };\n"
        "        std::filesystem::path autosave_rig_path{ \"runner-v0716-gait-evolved.rig\" };\n"
        "        std::filesystem::path autosave_state_path{ \"runner-v0716-gait-autonomy.state\" };",
        "autosave namespace",
    )
    speed_anchor = (
        '            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 40.0f } },\n'
        '                "MAX CPU", input, trainer.updates_per_cycle() == 4))\n'
        '                trainer.set_updates_per_cycle(4);\n'
        '            cursor.y += 48.0f;\n'
        '            const float half = (usable_width - 6.0f) * 0.5f;'
    )
    speed_replacement = (
        '            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 40.0f } },\n'
        '                "MAX CPU", input, trainer.updates_per_cycle() == 4))\n'
        '                trainer.set_updates_per_cycle(4);\n'
        '            cursor.y += 48.0f;\n'
        '            if (button({ cursor, { third, 38.0f } }, "ZOOM OUT", input))\n'
        '            {\n'
        '                live_zoom_factor = view_camera::apply_wheel_zoom(live_zoom_factor, -1.0f);\n'
        '                live_zoom_auto = false;\n'
        '            }\n'
        '            if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 38.0f } },\n'
        '                "AUTO VIEW", input, live_zoom_auto))\n'
        '            {\n'
        '                live_zoom_factor = 1.0f;\n'
        '                live_zoom_auto = true;\n'
        '            }\n'
        '            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 38.0f } },\n'
        '                "ZOOM IN", input))\n'
        '            {\n'
        '                live_zoom_factor = view_camera::apply_wheel_zoom(live_zoom_factor, 1.0f);\n'
        '                live_zoom_auto = false;\n'
        '            }\n'
        '            cursor.y += 46.0f;\n'
        '            const float half = (usable_width - 6.0f) * 0.5f;'
    )
    text = replace_once(text, speed_anchor, speed_replacement, "live zoom buttons")
    text = replace_once(
        text,
        "            // Keep the rig large and readable. Show roughly 2 m behind and 4 m\n"
        "            // ahead; a distant obstacle gets a distance label instead of forcing\n"
        "            // the camera to zoom the body into a tiny cluster.\n"
        "            float view_min_x = std::min(root_x - 2.0f, body_min_x - 0.35f);\n"
        "            float view_max_x = std::max(root_x + 4.2f, body_max_x + 0.50f);",
        "            // Keep the rig large and readable. Show a compact local course window;\n"
        "            // a distant obstacle gets a distance label instead of forcing the\n"
        "            // camera to zoom the body into a tiny cluster.\n"
        "            float view_min_x = std::min(root_x - 1.55f, body_min_x - 0.28f);\n"
        "            float view_max_x = std::max(root_x + 3.35f, body_max_x + 0.42f);",
        "PIP local window",
    )
    text = replace_once(
        text,
        "            const float scale = std::clamp(\n"
        "                std::min(horizontal_scale, vertical_scale), 20.0f, 48.0f);",
        "            const float scale = view_camera::pip_pixels_per_meter(\n"
        "                horizontal_scale, vertical_scale);",
        "PIP shared scale",
    )

    old_live = dedent(r'''
        void draw_live_world(Rect viewport, float dt)
        {
            if (!run_paused)
                trainer.step_preview(dt);
            const sim::Environment& environment = trainer.preview();
            constexpr float live_pixels_per_meter = 22.0f;
            if (!environment.particles().empty())
                camera_x = lerp(camera_x,
                    environment.particles()[environment.blueprint().root_node].position.x + 5.5f, 0.035f);
            add_rounded_rect(canvas, viewport, 11.0f, rgb(0x09101a), border, 1.0f);
            draw_course_ground(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_reference(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_features(environment, viewport, camera_x, live_pixels_per_meter);
            draw_creature(environment, viewport, camera_x, live_pixels_per_meter);
''')
    new_live = dedent(r'''
        void draw_live_world(Rect viewport, float dt, const InputState& input)
        {
            if (!run_paused)
                trainer.step_preview(dt);
            const sim::Environment& environment = trainer.preview();
            const auto& particles = environment.particles();

            if (contains(viewport, input.mouse) && std::abs(input.wheel) >= 0.01f)
            {
                live_zoom_factor = view_camera::apply_wheel_zoom(
                    live_zoom_factor, input.wheel);
                live_zoom_auto = false;
            }

            float rig_height = 2.4f;
            if (!particles.empty())
            {
                float minimum_y = std::numeric_limits<float>::infinity();
                float maximum_y = -std::numeric_limits<float>::infinity();
                for (const sim::Particle& particle : particles)
                {
                    minimum_y = std::min(minimum_y,
                        particle.position.y - particle.radius);
                    maximum_y = std::max(maximum_y,
                        particle.position.y + particle.radius);
                }
                if (std::isfinite(minimum_y) && std::isfinite(maximum_y))
                    rig_height = std::max(0.75f, maximum_y - minimum_y);
            }
            const float target_pixels_per_meter =
                view_camera::fitted_pixels_per_meter(
                    viewport.size.y, rig_height, live_zoom_factor);
            live_pixels_per_meter = view_camera::smooth_zoom(
                live_pixels_per_meter, target_pixels_per_meter, dt);

            if (!particles.empty())
            {
                const std::size_t root = environment.blueprint().root_node;
                if (root < particles.size())
                {
                    const float target_camera = particles[root].position.x
                        + view_camera::lookahead_meters(
                            viewport.size.x, live_pixels_per_meter);
                    camera_x = view_camera::smooth_camera(
                        camera_x, target_camera, live_pixels_per_meter, dt);
                }
            }

            add_rounded_rect(canvas, viewport, 11.0f, rgb(0x09101a), border, 1.0f);
            draw_course_ground(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_reference(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_features(environment, viewport, camera_x, live_pixels_per_meter);
            draw_creature(environment, viewport, camera_x, live_pixels_per_meter);
''')
    text = replace_once(text, old_live, new_live, "adaptive live camera")
    text = replace_once(
        text,
        '            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, viewport.size.y - 38.0f },\n'
        '                trainer.has_best_policy()\n'
        '                    ? "BEST STAGE-VALID CONTROLLER   v" RUNNER_VERSION "   BACKGROUND TRAINING ACTIVE"\n'
        '                    : "CURRENT POLICY UNVERIFIED   v" RUNNER_VERSION "   SEARCHING FOR VALID STANCE",\n'
        '                1.05f, trainer.has_best_policy() ? muted : danger, overlay_width, 1.00f);',
        '            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, viewport.size.y - 38.0f },\n'
        '                std::format("{}   v{}   VIEW {:.0f} PX/M {}",\n'
        '                    trainer.has_best_policy()\n'
        '                        ? "BEST STAGE-VALID CONTROLLER"\n'
        '                        : "CURRENT POLICY UNVERIFIED",\n'
        '                    RUNNER_VERSION, live_pixels_per_meter,\n'
        '                    live_zoom_auto ? "AUTO" : "MANUAL"),\n'
        '                1.05f, trainer.has_best_policy() ? muted : danger,\n'
        '                overlay_width, 1.00f);',
        "camera footer telemetry",
    )
    text = replace_once(
        text,
        "                trainer.reset_preview();\n                camera_x = 0.0f;",
        "                trainer.reset_preview();\n"
        "                camera_x = 0.0f;\n"
        "                live_pixels_per_meter = view_camera::default_pixels_per_meter;\n"
        "                live_zoom_factor = 1.0f;\n"
        "                live_zoom_auto = true;",
        "reset camera state",
    )
    text = replace_once(
        text,
        "                draw_live_world(world, dt);",
        "                draw_live_world(world, dt, input);",
        "live draw call",
    )
    write("src/app.cpp", text)


def patch_main_cpp() -> None:
    text = read("src/main.cpp")
    text = replace_once(
        text,
        '#include "renderer.hpp"',
        '#include "renderer.hpp"\n#include "view_camera.hpp"',
        "main camera include",
    )
    text = replace_once(
        text,
        "    [[nodiscard]] bool wants_acceptance_diagnostic(int argc, char** argv) noexcept\n"
        "    {\n"
        "        return argc > 1\n"
        "            && argv != nullptr\n"
        "            && argv[1] != nullptr\n"
        "            && std::string_view(argv[1]) == \"--diagnose-acceptance\";\n"
        "    }\n",
        "    [[nodiscard]] bool wants_acceptance_diagnostic(int argc, char** argv) noexcept\n"
        "    {\n"
        "        return argc > 1\n"
        "            && argv != nullptr\n"
        "            && argv[1] != nullptr\n"
        "            && std::string_view(argv[1]) == \"--diagnose-acceptance\";\n"
        "    }\n\n"
        "    [[nodiscard]] bool wants_camera_diagnostic(int argc, char** argv) noexcept\n"
        "    {\n"
        "        return argc > 1\n"
        "            && argv != nullptr\n"
        "            && argv[1] != nullptr\n"
        "            && std::string_view(argv[1]) == \"--diagnose-camera\";\n"
        "    }\n",
        "camera diagnostic argument",
    )
    anchor = (
        "    if (wants_acceptance_diagnostic(argc, argv))\n"
        "    {\n"
        "        const runner::acceptance::Report report =\n"
    )
    if anchor not in text:
        raise RuntimeError("acceptance diagnostic anchor missing")
    camera_block = dedent(r'''
    if (wants_camera_diagnostic(argc, argv))
    {
        const float automatic = runner::view_camera::automatic_pixels_per_meter(
            820.0f, 3.0f);
        const float fitted = runner::view_camera::fitted_pixels_per_meter(
            820.0f, 3.0f, 1.0f);
        const float zoomed = runner::view_camera::apply_wheel_zoom(1.0f, 1.0f);
        const float lookahead = runner::view_camera::lookahead_meters(
            900.0f, fitted);
        const float followed = runner::view_camera::smooth_camera(
            0.0f, 4.0f, fitted, 1.0f / 60.0f);
        const bool valid = automatic > 22.0f
            && fitted >= runner::view_camera::default_pixels_per_meter
            && zoomed > 1.0f
            && lookahead > 2.0f
            && followed > 0.0f && followed < 4.0f
            && runner::view_camera::pip_pixels_per_meter(10.0f, 80.0f)
                == runner::view_camera::pip_minimum_pixels_per_meter;
        std::printf(
            "Runner %s camera diagnostic: %s; default=%.1f px/m fitted=%.1f "
            "zoom=%.3f lookahead=%.2f follow=%.3f\n",
            RUNNER_VERSION, valid ? "passed" : "failed",
            runner::view_camera::default_pixels_per_meter,
            fitted, zoomed, lookahead, followed);
        return valid ? 0 : 1;
    }

''')
    text = text.replace(anchor, camera_block + anchor, 1)
    write("src/main.cpp", text)


def patch_cmake() -> None:
    text = read("CMakeLists.txt")
    text = replace_once(
        text,
        "project(Runner VERSION 0.7.15 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.16 LANGUAGES CXX)",
        "CMake version",
    )
    text = replace_once(
        text,
        "        src/app.hpp src/pixel_art.hpp src/renderer.hpp src/math.hpp src/ui_font.hpp)",
        "        src/app.hpp src/pixel_art.hpp src/renderer.hpp src/math.hpp\n"
        "        src/ui_font.hpp src/view_camera.hpp)",
        "Runner source header list",
    )
    text = replace_once(
        text,
        '            "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md"\n'
        '            "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md"',
        '            "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md"\n'
        '            "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md"\n'
        '        COMMAND ${CMAKE_COMMAND} -E copy_if_different\n'
        '            "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0716_CAMERA_BATCH.md"\n'
        '            "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0716_CAMERA_BATCH.md"',
        "post-build camera document",
    )
    text = replace_once(
        text,
        '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md"\n'
        '        DESTINATION docs)',
        '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0715_VIEWPORT_RECOVERY.md"\n'
        '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0716_CAMERA_BATCH.md"\n'
        '        DESTINATION docs)',
        "install camera document",
    )
    anchor = (
        "    add_executable(RunnerCoreTests tests/core_tests.cpp)\n"
        "    target_link_libraries(RunnerCoreTests PRIVATE Runner::Core)\n"
    )
    if anchor not in text:
        raise RuntimeError("core test anchor missing")
    camera_target = dedent(r'''
    add_executable(RunnerViewCameraTests tests/view_camera_tests.cpp)
    target_include_directories(RunnerViewCameraTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerViewCameraTests PRIVATE cxx_std_23)
    set_target_properties(RunnerViewCameraTests PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    runner_enable_warnings(RunnerViewCameraTests)
    add_test(NAME Runner.ViewCamera COMMAND RunnerViewCameraTests)
    set_tests_properties(Runner.ViewCamera PROPERTIES TIMEOUT 30)

''')
    text = text.replace(anchor, camera_target + anchor, 1)
    if "add_test(NAME Runner.CameraDiagnostic" not in text:
        text = replace_once(
            text,
            "        add_test(NAME Runner.PackageLayout COMMAND $<TARGET_FILE:Runner> --diagnose-package)\n"
            "        set_tests_properties(Runner.PackageLayout PROPERTIES TIMEOUT 30)",
            "        add_test(NAME Runner.PackageLayout COMMAND $<TARGET_FILE:Runner> --diagnose-package)\n"
            "        set_tests_properties(Runner.PackageLayout PROPERTIES TIMEOUT 30)\n"
            "        add_test(NAME Runner.CameraDiagnostic COMMAND $<TARGET_FILE:Runner> --diagnose-camera)\n"
            "        set_tests_properties(Runner.CameraDiagnostic PROPERTIES TIMEOUT 30)",
            "camera package CTest",
        )
    write("CMakeLists.txt", text)


def patch_ui_layout() -> None:
    text = read("src/ui_layout.hpp")
    text = replace_once(
        text,
        "        const float width = std::clamp(world.width * 0.42f, 190.0f, 390.0f);",
        "        const float width = std::clamp(world.width * 0.46f, 240.0f, 440.0f);",
        "PIP width",
    )
    text = replace_once(
        text,
        "        const float height = std::min(std::clamp(world.height * 0.27f, 170.0f, 245.0f),",
        "        const float height = std::min(std::clamp(world.height * 0.30f, 190.0f, 270.0f),",
        "PIP height",
    )
    write("src/ui_layout.hpp", text)


def patch_repository_audit() -> None:
    text = read("tools/repository_audit.cmake")
    text = replace_once(
        text,
        "foreach(required IN ITEMS CHANGELOG.md missioncache.md README.md docs/SANDHYBRID_INTEGRATION_BRIDGE.md)",
        "foreach(required IN ITEMS AGENTS.md CHANGELOG.md missioncache.md README.md\n"
        "        docs/SANDHYBRID_INTEGRATION_BRIDGE.md\n"
        "        docs/RUNNER_V0716_CAMERA_BATCH.md)",
        "required repository docs",
    )
    text = replace_once(
        text,
        'string(FIND "${cmake_text}" "project(Runner VERSION 0.7.15 LANGUAGES CXX)" version_position)\n'
        'if(version_position EQUAL -1)\n'
        '    message(FATAL_ERROR "CMake project version is not 0.7.15")',
        'string(FIND "${cmake_text}" "project(Runner VERSION 0.7.16 LANGUAGES CXX)" version_position)\n'
        'if(version_position EQUAL -1)\n'
        '    message(FATAL_ERROR "CMake project version is not 0.7.16")',
        "audit version",
    )
    text = replace_once(
        text,
        'foreach(reference IN ITEMS "CHANGELOG.md" "missioncache.md" "SANDHYBRID_INTEGRATION_BRIDGE.md" "--diagnose-acceptance")',
        'foreach(reference IN ITEMS "AGENTS.md" "CHANGELOG.md" "missioncache.md"\n'
        '        "SANDHYBRID_INTEGRATION_BRIDGE.md" "RUNNER_V0716_CAMERA_BATCH.md"\n'
        '        "--diagnose-acceptance" "--diagnose-camera")',
        "README audit references",
    )
    tail = 'message(STATUS "Runner repository hygiene passed")'
    extra = dedent(r'''
foreach(stale IN ITEMS
        tools/apply_v0716_batch25.py
        .github/workflows/apply-v0716-batch25.yml)
    if(EXISTS "${RUNNER_SOURCE_DIR}/${stale}")
        message(FATAL_ERROR "Temporary v0.7.16 applicator remains: ${stale}")
    endif()
endforeach()

''')
    text = replace_once(text, tail, extra + tail, "temporary applicator audit")
    write("tools/repository_audit.cmake", text)


def patch_readme() -> None:
    text = read("README.md")
    text = replace_once(
        text,
        "Runner 0.7.15 is a combined",
        "Runner 0.7.16 is a combined",
        "README version",
    )
    controls_anchor = "- `R`: Reset the live preview\n"
    controls_new = (
        "- Mouse wheel over Live Autopilot: zoom the world view without changing physical scale\n"
        "- Live panel `ZOOM OUT` / `AUTO VIEW` / `ZOOM IN`: direct camera controls\n"
        "- `R`: Reset the live preview and restore automatic camera fitting\n"
    )
    text = replace_once(text, controls_anchor, controls_new, "README camera controls")
    text = replace_once(
        text,
        "Runner.exe --diagnose-acceptance\n",
        "Runner.exe --diagnose-acceptance\nRunner.exe --diagnose-camera\n",
        "README camera diagnostic command",
    )
    text = replace_once(
        text,
        "`--diagnose-acceptance` runs the same deterministic rig and curriculum matrix used by CTest and release-package auditing.",
        "`--diagnose-acceptance` runs the same deterministic rig and curriculum matrix used by CTest and release-package auditing. `--diagnose-camera` validates adaptive fit, clamps, wheel zoom, lookahead, dead-zone follow, and PIP scale without opening a window.",
        "README diagnostic description",
    )
    records_anchor = "- [`CHANGELOG.md`](CHANGELOG.md) is the single release-history document.\n"
    records_new = (
        "- [`AGENTS.md`](AGENTS.md) defines cache-first implementation, validation, documentation, and release rules.\n"
        "- [`CHANGELOG.md`](CHANGELOG.md) is the single release-history document.\n"
    )
    text = replace_once(text, records_anchor, records_new, "README AGENTS reference")
    text = replace_once(
        text,
        "- [`docs/SANDHYBRID_INTEGRATION_BRIDGE.md`](docs/SANDHYBRID_INTEGRATION_BRIDGE.md) pins the SandHybrid library and preserves ownership of both canonical ledgers.",
        "- [`docs/SANDHYBRID_INTEGRATION_BRIDGE.md`](docs/SANDHYBRID_INTEGRATION_BRIDGE.md) pins the SandHybrid library and preserves ownership of both canonical ledgers.\n"
        "- [`docs/RUNNER_V0716_CAMERA_BATCH.md`](docs/RUNNER_V0716_CAMERA_BATCH.md) documents the adaptive live and PIP camera contract.",
        "README camera doc reference",
    )
    write("README.md", text)


def patch_changelog() -> None:
    text = read("CHANGELOG.md")
    if "## Runner v0.7.16" in text:
        return
    entry = dedent(r'''
## Runner v0.7.16 — Adaptive viewport and 25-mission usability batch

- Replaced the overly distant fixed 22 px/m live view with bounded rig-height fitting and a closer corrected default.
- Added viewport-only mouse-wheel zoom, Zoom Out/Auto View/Zoom In panel controls, reset-to-auto behavior, live px/m telemetry, forward lookahead, elapsed-time smoothing, and a screen-space dead zone.
- Enlarged the training PIP and tightened its local course window while preserving distant-hazard labels.
- Added shared camera math, deterministic camera/layout tests, and `Runner.exe --diagnose-camera` in build-tree, installed, and extracted package gates.
- Isolated v0.7.16 training semantics and autosave paths.
- Added `AGENTS.md`, the focused camera document, updated README, mission ledger, repository audit, package contents, and release automation.
- Preserved the equipment, target, policy-extension, and combined carry/fire curriculum intact for Runner v0.7.17.

''')
    if text.startswith("#"):
        first_break = text.find("\n")
        text = text[:first_break + 1] + "\n" + entry + text[first_break + 1:].lstrip("\n")
    else:
        text = entry + text
    write("CHANGELOG.md", text)


def patch_ppo() -> None:
    text = read("src/ppo.hpp")
    text = replace_once(
        text,
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1503u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1600u;",
        "training semantics version",
    )
    write("src/ppo.hpp", text)


def patch_workflow() -> None:
    old_path = ROOT / ".github/workflows/runner-v0715-release.yml"
    text = old_path.read_text(encoding="utf-8")
    text = replace_all_checked(text, "0.7.15", "0.7.16", 10, "workflow version")
    text = replace_all_checked(text, "v0.7.15", "v0.7.16", 4, "workflow tag")
    text = replace_all_checked(text, "v0715", "v0716", 5, "workflow identifier")
    text = text.replace("WALK-EQUIPMENT-148", "WALK-VIEW-156")
    text = text.replace("WALK-RELEASE-155", "WALK-RELEASE-180")
    text = text.replace(
        "docs/RUNNER_V0715_VIEWPORT_RECOVERY.md'))",
        "docs/RUNNER_V0715_VIEWPORT_RECOVERY.md',\n"
        "            'docs/RUNNER_V0716_CAMERA_BATCH.md'))",
    )
    text = text.replace(
        "& $exe --diagnose-acceptance\n"
        "            if ($LASTEXITCODE -ne 0) { throw 'Build-tree acceptance matrix failed' }",
        "& $exe --diagnose-acceptance\n"
        "            if ($LASTEXITCODE -ne 0) { throw 'Build-tree acceptance matrix failed' }\n"
        "            & $exe --diagnose-camera\n"
        "            if ($LASTEXITCODE -ne 0) { throw 'Build-tree camera diagnostic failed' }",
    )
    text = text.replace(
        "& \"$env:STAGE/Runner.exe\" --diagnose-acceptance\n"
        "            if ($LASTEXITCODE -ne 0) { throw 'Installed acceptance matrix failed' }",
        "& \"$env:STAGE/Runner.exe\" --diagnose-acceptance\n"
        "            if ($LASTEXITCODE -ne 0) { throw 'Installed acceptance matrix failed' }\n"
        "            & \"$env:STAGE/Runner.exe\" --diagnose-camera\n"
        "            if ($LASTEXITCODE -ne 0) { throw 'Installed camera diagnostic failed' }",
    )
    text = text.replace(
        "& \"$audit/Runner.exe\" --diagnose-acceptance\n"
        "            if ($LASTEXITCODE -ne 0) { throw 'Extracted acceptance matrix failed' }",
        "& \"$audit/Runner.exe\" --diagnose-acceptance\n"
        "            if ($LASTEXITCODE -ne 0) { throw 'Extracted acceptance matrix failed' }\n"
        "            & \"$audit/Runner.exe\" --diagnose-camera\n"
        "            if ($LASTEXITCODE -ne 0) { throw 'Extracted camera diagnostic failed' }",
    )
    text = text.replace(
        "          Runner v0.7.16 completes the cache-first locomotion repair pass.",
        "          Runner v0.7.16 completes a 25-mission adaptive viewport and usability pass.",
    )
    text = text.replace(
        "          - Synchronizes rendered terrain, collision terrain, pressure, deposits, obstacles, and treadmill coordinates.\n"
        "          - Replaces the false forward-bow duck with evidence-gated pelvis-down bilateral crouching and controlled recovery.\n"
        "          - Adds normal side-view leg crossing, alternating gait evidence, articulated heel/ball/toe phases, and terrain-aware traction.\n"
        "          - Adds structural rig evolution, a selectable scaffold, bounded candidate adaptation, champion rollback, and full editor diagnostics.\n"
        "          - Invalidates incompatible learned state while preserving explicit transfer-only import behavior.\n\n"
        "          Equipment, armed/disarmed carry, abstract weapon classes, target shooting, and combined movement/target curriculum remain explicitly cached for Runner v0.7.16.",
        "          - Corrects the overly distant fixed 22 px/m live camera while preserving physical world scale.\n"
        "          - Adds rig-height auto fit, wheel and panel zoom controls, reset-to-auto, lookahead, dead-zone follow, and px/m telemetry.\n"
        "          - Enlarges and tightens the training PIP through the shared camera contract.\n"
        "          - Adds deterministic camera/layout tests and packaged `--diagnose-camera` coverage.\n"
        "          - Adds AGENTS.md and reconciles cache, changelog, README, focused documentation, package, and release automation.\n\n"
        "          Equipment, armed/disarmed carry, abstract weapon classes, target shooting, and combined movement/target curriculum remain explicitly cached for Runner v0.7.17.",
    )
    text = re.sub(
        r"          for branch in \\\n(?:.*\n){1,8}?            agent/v0715-complete; do",
        "          for branch in \\\n"
        "            agent/v0716-camera-batch25; do",
        text,
        count=1,
    )
    new_path = ROOT / ".github/workflows/runner-v0716-release.yml"
    new_path.write_text(text, encoding="utf-8", newline="\n")
    old_path.unlink()


def implement() -> None:
    if "### WALK-VIEW-156" not in read("missioncache.md"):
        raise RuntimeError("mission cache must be committed before implementation")
    write("src/view_camera.hpp", VIEW_CAMERA_HPP)
    write("tests/view_camera_tests.cpp", VIEW_CAMERA_TESTS)
    write("AGENTS.md", AGENTS_MD)
    write("docs/RUNNER_V0716_CAMERA_BATCH.md", CAMERA_DOC)
    patch_app_cpp()
    patch_main_cpp()
    patch_ui_layout()
    patch_cmake()
    patch_ppo()
    patch_readme()
    patch_changelog()
    patch_repository_audit()
    patch_workflow()


def finalize() -> None:
    text = read("missioncache.md")
    for mission in range(156, 180):
        pattern = re.compile(
            rf"(### WALK-?[^\n]+-{mission} — [^\n]+\n)\*\*Status:\*\* CACHED — IMPLEMENTATION PENDING"
        )
        text, count = pattern.subn(
            rf"\1**Status:** VERIFIED — IMPLEMENTED AND CROSS-PLATFORM VALIDATED",
            text,
            count=1,
        )
        if count != 1:
            raise RuntimeError(f"mission {mission}: status marker not found")
    text = re.sub(
        r"(### WALK-RELEASE-180 — [^\n]+\n)\*\*Status:\*\* CACHED — IMPLEMENTATION PENDING",
        r"\1**Status:** READY FOR PUBLICATION — FULL PR PACKAGE GATE REQUIRED",
        text,
        count=1,
    )
    text = text.replace(
        "**Release state:** CACHED — 25 MISSIONS SELECTED FOR ONE AUDITED RELEASE.",
        "**Release state:** IMPLEMENTED AND CROSS-PLATFORM VALIDATED — PR package and publication gates remain.",
        1,
    )
    evidence = dedent(r'''

## v0.7.16 pre-publication validation evidence

- Cache-first mission commit preceded product source changes.
- Temporary implementation workflow validated the exact implementation commit on Linux GCC 14 and Windows Server 2025 / MSVC.
- Repository hygiene, camera math/layout suite, deterministic core suites, full SDL3/Vulkan application build, `--diagnose-package`, `--diagnose-acceptance`, and `--diagnose-camera` passed before PR creation.
- Final PR source removes the temporary applicator and one-use workflow.
- The PR release workflow must repeat Linux and Windows tests, install/extract/package diagnostics, checksum, manifest, and artifact audit before merge.
''')
    marker = "# Runner v0.7.17 equipment, carry, and target curriculum"
    insert_at = text.index(marker)
    text = text[:insert_at] + evidence + "\n" + text[insert_at:]
    write("missioncache.md", text)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("cache", "implement", "finalize"))
    args = parser.parse_args()
    if args.mode == "cache":
        cache()
    elif args.mode == "implement":
        implement()
    else:
        finalize()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"v0.7.16 batch applicator failed: {exc}", file=sys.stderr)
        raise
