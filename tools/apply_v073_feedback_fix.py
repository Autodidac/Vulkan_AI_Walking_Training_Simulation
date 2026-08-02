from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def write(relative: str, content: str) -> None:
    (ROOT / relative).write_text(content, encoding="utf-8", newline="\n")


def replace(relative: str, old: str, new: str, marker: str | None = None) -> None:
    text = read(relative)
    if marker is not None and marker in text:
        return
    if old not in text:
        raise RuntimeError(f"Expected text not found in {relative}: {old[:120]!r}")
    write(relative, text.replace(old, new, 1))


def insert_before(relative: str, needle: str, insertion: str, marker: str) -> None:
    text = read(relative)
    if marker in text:
        return
    if needle not in text:
        raise RuntimeError(f"Insertion point missing in {relative}: {needle!r}")
    write(relative, text.replace(needle, insertion + needle, 1))


def insert_after(relative: str, needle: str, insertion: str, marker: str) -> None:
    text = read(relative)
    if marker in text:
        return
    if needle not in text:
        raise RuntimeError(f"Insertion point missing in {relative}: {needle!r}")
    write(relative, text.replace(needle, needle + insertion, 1))


# The full-skeleton gate remains immediate for display/publication, but a valid
# freshly spawned body gets the same soft-start grace as policy actuation.
replace(
    "src/simulation.cpp",
    "if (elapsed_seconds_ >= 0.20f && !body_integrity_valid())",
    "if (elapsed_seconds_ >= 1.50f && !body_integrity_valid())",
    "elapsed_seconds_ >= 1.50f && !body_integrity_valid()",
)

# Separate controlled flips from generic destabilizing rotation.
replace(
    "src/simulation.hpp",
    "        [[nodiscard]] float maximum_spin_turns() const noexcept { return maximum_spin_turns_; }",
    "        [[nodiscard]] float maximum_flip_turns() const noexcept { return maximum_spin_turns_; }\n"
    "        [[nodiscard]] float maximum_spin_turns() const noexcept { return uncontrolled_spin_turns_; }\n"
    "        [[nodiscard]] float uncontrolled_spin_turns() const noexcept { return uncontrolled_spin_turns_; }",
    "maximum_flip_turns() const noexcept",
)
replace(
    "src/simulation.hpp",
    "        float maximum_spin_turns_{};",
    "        float maximum_spin_turns_{};\n        float uncontrolled_spin_turns_{};",
    "float uncontrolled_spin_turns_{};",
)
replace(
    "src/simulation.cpp",
    "        if (airborne)\n        {\n            current_airborne_rotation_ += torso_delta;\n            maximum_spin_turns_ = std::max(maximum_spin_turns_,\n                std::abs(current_airborne_rotation_) / (2.0f * pi));\n        }",
    "        if (airborne)\n        {\n            current_airborne_rotation_ += torso_delta;\n            const float airborne_turns = std::abs(current_airborne_rotation_) / (2.0f * pi);\n            if (powered_takeoff_ && stage_allows_controlled_flips(course_stage_))\n                maximum_spin_turns_ = std::max(maximum_spin_turns_, airborne_turns);\n            else\n                uncontrolled_spin_turns_ += std::abs(torso_delta) / (2.0f * pi);\n        }",
    "const float airborne_turns = std::abs(current_airborne_rotation_)",
)
replace(
    "src/simulation.cpp",
    "                maximum_spin_turns_ = std::max(maximum_spin_turns_, landed_turns);\n                if (stage_allows_controlled_flips(course_stage_) && landed_turns >= 0.75f)\n                {\n                    spin_landing_this_step_ = true;\n                    ++spin_landing_count_;\n                }",
    "                if (stage_allows_controlled_flips(course_stage_))\n                {\n                    maximum_spin_turns_ = std::max(maximum_spin_turns_, landed_turns);\n                    if (landed_turns >= 0.75f)\n                    {\n                        spin_landing_this_step_ = true;\n                        ++spin_landing_count_;\n                    }\n                }\n                else\n                {\n                    uncontrolled_spin_turns_ += landed_turns;\n                }",
    "uncontrolled_spin_turns_ += landed_turns;",
)
replace(
    "src/simulation.cpp",
    "        const float spin_landing_reward = spin_landing_this_step_\n            ? 0.20f + clamp(maximum_spin_turns_, 0.0f, 3.0f) * 0.08f : 0.0f;",
    "        const float spin_landing_reward = spin_landing_this_step_\n            ? 0.20f + clamp(maximum_spin_turns_, 0.0f, 3.0f) * 0.08f : 0.0f;\n"
    "        const bool controlled_flip_rotation = stage_allows_controlled_flips(course_stage_)\n"
    "            && powered_takeoff_;\n"
    "        const float uncontrolled_spin_penalty = controlled_flip_rotation ? 0.0f\n"
    "            : clamp(spin_delta_turns, 0.0f, 0.08f) * 0.18f;",
    "const float uncontrolled_spin_penalty = controlled_flip_rotation",
)
replace(
    "src/simulation.cpp",
    "        last_reward_ += recovery_reward;",
    "        last_reward_ += recovery_reward - uncontrolled_spin_penalty;",
    "recovery_reward - uncontrolled_spin_penalty",
)

# Keep trainer qualification tied to landed powered flips, never generic spin.
text = read("src/ppo.hpp")
if "environment.maximum_flip_turns()" not in text:
    text = text.replace("environment.maximum_spin_turns()", "environment.maximum_flip_turns()")
    write("src/ppo.hpp", text)
text = read("src/ppo_trainer.cpp")
if "maximum_flip_turns()" not in text:
    text = text.replace("maximum_spin_turns()", "maximum_flip_turns()")
    write("src/ppo_trainer.cpp", text)
text = read("src/ppo_parallel.cpp")
if "maximum_flip_turns()" not in text:
    text = text.replace("maximum_spin_turns()", "maximum_flip_turns()")
    write("src/ppo_parallel.cpp", text)

# Shared UI layout semantics for full-width headers and quarter-unit markers.
insert_after(
    "src/ui_layout.hpp",
    "    inline constexpr float bottom_telemetry_height = 52.0f;\n",
    "\n    enum class DistanceUnits { metric, imperial };\n\n"
    "    [[nodiscard]] constexpr Box top_bar_box(float window_width) noexcept\n"
    "    {\n"
    "        return { 0.0f, 0.0f, std::max(0.0f, window_width), top_bar_height };\n"
    "    }\n\n"
    "    [[nodiscard]] constexpr float course_reference_marker_spacing_m(\n"
    "        DistanceUnits units) noexcept\n"
    "    {\n"
    "        return units == DistanceUnits::metric ? 250.0f : 1609.344f * 0.25f;\n"
    "    }\n\n"
    "    [[nodiscard]] constexpr std::uint64_t lifetime_delta(\n"
    "        std::uint64_t total, std::uint64_t start) noexcept\n"
    "    {\n"
    "        return total >= start ? total - start : 0u;\n"
    "    }\n",
    "course_reference_marker_spacing_m",
)

# DPI-safe mouse/layout coordinates and an explicit full-drawable-width frame.
replace(
    "src/main.cpp",
    "        float mouse_x{};\n        float mouse_y{};\n        const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);\n        input.mouse = { mouse_x, mouse_y };\n        input.mouse_delta = input.mouse - previous_mouse;\n        previous_mouse = input.mouse;\n        input.left_down = is_down(mouse_buttons, SDL_BUTTON_LMASK);",
    "        float mouse_x{};\n        float mouse_y{};\n        const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);\n        int logical_width{};\n        int logical_height{};\n        int drawable_width{};\n        int drawable_height{};\n        SDL_GetWindowSize(window, &logical_width, &logical_height);\n        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);\n        const float mouse_scale_x = logical_width > 0\n            ? static_cast<float>(drawable_width) / static_cast<float>(logical_width) : 1.0f;\n        const float mouse_scale_y = logical_height > 0\n            ? static_cast<float>(drawable_height) / static_cast<float>(logical_height) : 1.0f;\n        input.mouse = { mouse_x * mouse_scale_x, mouse_y * mouse_scale_y };\n        input.mouse_delta = input.mouse - previous_mouse;\n        previous_mouse = input.mouse;\n        input.left_down = is_down(mouse_buttons, SDL_BUTTON_LMASK);",
    "const float mouse_scale_x = logical_width > 0",
)
replace(
    "src/main.cpp",
    "        int drawable_width{};\n        int drawable_height{};\n        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);\n        application.frame(input, dt, drawable_width, drawable_height);",
    "        application.frame(input, dt, drawable_width, drawable_height);",
    "SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);\n        const float mouse_scale_x",
)

# Application-level unit switch and lifetime/total counters.
insert_after(
    "src/app.cpp",
    "        bool run_paused{};\n",
    "        ui_layout::DistanceUnits distance_units{ ui_layout::DistanceUnits::metric };\n"
    "        float session_runtime_seconds{};\n"
    "        float rig_lifetime_seconds{};\n"
    "        std::uint64_t tracked_rig_signature{};\n"
    "        std::uint64_t rig_start_update{};\n"
    "        std::uint64_t rig_start_environment_steps{};\n",
    "ui_layout::DistanceUnits distance_units",
)
insert_before(
    "src/app.cpp",
    "        [[nodiscard]] bool has_direct_bone",
    "        [[nodiscard]] std::string format_speed(float meters_per_second) const\n"
    "        {\n"
    "            if (distance_units == ui_layout::DistanceUnits::metric)\n"
    "                return std::format(\"{:.1f} KM/H\", meters_per_second * 3.6f);\n"
    "            return std::format(\"{:.1f} MPH\", meters_per_second * 2.23693629f);\n"
    "        }\n\n"
    "        [[nodiscard]] std::string format_distance(float meters) const\n"
    "        {\n"
    "            if (distance_units == ui_layout::DistanceUnits::metric)\n"
    "                return meters >= 1000.0f ? std::format(\"{:.2f} KM\", meters / 1000.0f)\n"
    "                    : std::format(\"{:.1f} M\", meters);\n"
    "            const float feet = meters * 3.2808399f;\n"
    "            return feet >= 5280.0f ? std::format(\"{:.2f} MI\", feet / 5280.0f)\n"
    "                : std::format(\"{:.0f} FT\", feet);\n"
    "        }\n\n"
    "        [[nodiscard]] static std::string format_duration(float seconds)\n"
    "        {\n"
    "            const auto total = static_cast<std::uint64_t>(std::max(0.0f, seconds));\n"
    "            const std::uint64_t hours = total / 3600u;\n"
    "            const std::uint64_t minutes = (total / 60u) % 60u;\n"
    "            const std::uint64_t remaining = total % 60u;\n"
    "            return std::format(\"{:02}:{:02}:{:02}\", hours, minutes, remaining);\n"
    "        }\n\n",
    "format_speed(float meters_per_second)",
)
replace(
    "src/app.cpp",
    "            constexpr float bar_height = 82.0f;\n            canvas.quad({ 0.0f, 0.0f }, { static_cast<float>(width), bar_height }, rgb(0x0b1119));",
    "            const ui_layout::Box top_bar = ui_layout::top_bar_box(static_cast<float>(width));\n"
    "            canvas.quad({ top_bar.x, top_bar.y },\n"
    "                { top_bar.x + top_bar.width, top_bar.y + top_bar.height }, rgb(0x0b1119));",
    "const ui_layout::Box top_bar = ui_layout::top_bar_box",
)
replace(
    "src/app.cpp",
    "            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 40.0f } },\n                \"MAX CPU\", input, trainer.updates_per_cycle() == 4))\n                trainer.set_updates_per_cycle(4);\n            cursor.y += 57.0f;",
    "            if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 40.0f } },\n"
    "                \"MAX CPU\", input, trainer.updates_per_cycle() == 4))\n"
    "                trainer.set_updates_per_cycle(4);\n"
    "            cursor.y += 48.0f;\n"
    "            const float half = (usable_width - 6.0f) * 0.5f;\n"
    "            if (button({ cursor, { half, 36.0f } }, \"METRIC / 0.25 KM\", input,\n"
    "                distance_units == ui_layout::DistanceUnits::metric))\n"
    "                distance_units = ui_layout::DistanceUnits::metric;\n"
    "            if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 36.0f } },\n"
    "                \"IMPERIAL / 0.25 MI\", input,\n"
    "                distance_units == ui_layout::DistanceUnits::imperial))\n"
    "                distance_units = ui_layout::DistanceUnits::imperial;\n"
    "            cursor.y += 53.0f;",
    "METRIC / 0.25 KM",
)
replace(
    "src/app.cpp",
    "            add_text_fit(canvas, cursor, std::format(\"EVAL {:+.2f}   DIST {:.2f} M\",\n                metrics.evaluation_score, metrics.evaluation_distance), 1.10f,",
    "            add_text_fit(canvas, cursor, std::format(\"EVAL {:+.2f}   DIST {}\",\n                metrics.evaluation_score, format_distance(metrics.evaluation_distance)), 1.10f,",
    "DIST {}",
)
replace(
    "src/app.cpp",
    "            add_rounded_rect(canvas, { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 259.0f } },",
    "            add_rounded_rect(canvas, { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 323.0f } },",
    "usable_width + 14.0f, 323.0f",
)
replace(
    "src/app.cpp",
    "            add_text_fit(canvas, cursor, std::format(\"RIG GEN {}   ACCEPTED {}   REJECTED {}\",\n                autonomy.rig_generation, autonomy.accepted_rig_changes, autonomy.rejected_rig_changes),\n                1.02f, white, usable_width);\n            cursor.y += 29.0f;",
    "            add_text_fit(canvas, cursor, std::format(\"RIG GEN {}   ACCEPTED {}   REJECTED {}\",\n"
    "                autonomy.rig_generation, autonomy.accepted_rig_changes, autonomy.rejected_rig_changes),\n"
    "                1.02f, white, usable_width);\n"
    "            cursor.y += 29.0f;\n"
    "            add_text_fit(canvas, cursor, std::format(\"RIG LIFE {}   UPDATES {}   ENV STEPS {}\",\n"
    "                format_duration(rig_lifetime_seconds),\n"
    "                ui_layout::lifetime_delta(metrics.update, rig_start_update),\n"
    "                ui_layout::lifetime_delta(metrics.environment_steps, rig_start_environment_steps)),\n"
    "                1.00f, white, usable_width);\n"
    "            cursor.y += 29.0f;\n"
    "            add_text_fit(canvas, cursor, std::format(\"SESSION {}   TOTAL UPDATES {}   TOTAL ENV STEPS {}\",\n"
    "                format_duration(session_runtime_seconds), metrics.update, metrics.environment_steps),\n"
    "                0.98f, muted, usable_width);\n"
    "            cursor.y += 29.0f;",
    "RIG LIFE {}   UPDATES {}   ENV STEPS {}",
)
replace(
    "src/app.cpp",
    "            constexpr float marker_spacing = sim::course_marker_spacing_m;",
    "            const float marker_spacing = ui_layout::course_reference_marker_spacing_m(distance_units);",
    "course_reference_marker_spacing_m(distance_units)",
)
replace(
    "src/app.cpp",
    "                add_text(canvas, sign.position + Vec2{ 7.0f, 6.0f },\n                    std::format(\"{:.0f} M / {:.3f} MI\", distance, distance / 1609.344f),\n                    1.02f, white);",
    "                const std::string marker_label = distance_units == ui_layout::DistanceUnits::metric\n"
    "                    ? std::format(\"{:.2f} KM\", distance / 1000.0f)\n"
    "                    : std::format(\"{:.2f} MI\", distance / 1609.344f);\n"
    "                add_text(canvas, sign.position + Vec2{ 7.0f, 6.0f }, marker_label, 1.02f, white);",
    "const std::string marker_label",
)
replace(
    "src/app.cpp",
    "                std::format(\"{:.1f} KM/H   ACTUAL {:.2f} M\",\n                    environment.forward_speed() * 3.6f, environment.distance_travelled()),",
    "                std::format(\"{}   ACTUAL {}\",\n                    format_speed(environment.forward_speed()),\n                    format_distance(environment.distance_travelled())),",
    "format_speed(environment.forward_speed())",
)
replace(
    "src/app.cpp",
    "                std::format(\"COURSE {:.1f} M   {}\", environment.course_progress(),\n                    sim::invalid_motion_name(environment.invalid_reason())),",
    "                std::format(\"COURSE {}   {}\", format_distance(environment.course_progress()),\n                    sim::invalid_motion_name(environment.invalid_reason())),",
    "COURSE {}   {}",
)
replace(
    "src/app.cpp",
    "                std::format(\"STEPS {}  DUCK {:.1f} S  JUMP {}/{}  SPIN {:.1f}  PASSED {}\",\n                    environment.alternating_steps(), environment.duck_seconds(),\n                    environment.powered_jumps(), environment.landed_jumps(),\n                    environment.maximum_spin_turns(), environment.obstacles_passed()),",
    "                std::format(\"STEPS {}  DUCK {:.1f} S  JUMP {}/{}  FLIP {:.1f}  SPIN {:.1f}  PASSED {}\",\n"
    "                    environment.alternating_steps(), environment.duck_seconds(),\n"
    "                    environment.powered_jumps(), environment.landed_jumps(),\n"
    "                    environment.maximum_flip_turns(), environment.uncontrolled_spin_turns(),\n"
    "                    environment.obstacles_passed()),",
    "FLIP {:.1f}  SPIN {:.1f}",
)
replace(
    "src/app.cpp",
    "            trainer.synchronize();\n            canvas.clear();\n            canvas.reserve(120000);\n            status_time = std::max(0.0f, status_time - dt);",
    "            trainer.synchronize();\n"
    "            canvas.clear();\n"
    "            canvas.reserve(120000);\n"
    "            canvas.quad({ 0.0f, 0.0f }, { static_cast<float>(width), static_cast<float>(height) },\n"
    "                rgb(0x080a0d));\n"
    "            status_time = std::max(0.0f, status_time - dt);\n"
    "            session_runtime_seconds += std::max(0.0f, dt);\n"
    "            const std::uint64_t current_signature = trainer.rig_signature();\n"
    "            if (tracked_rig_signature == 0u || tracked_rig_signature != current_signature)\n"
    "            {\n"
    "                tracked_rig_signature = current_signature;\n"
    "                rig_lifetime_seconds = 0.0f;\n"
    "                rig_start_update = trainer.metrics().update;\n"
    "                rig_start_environment_steps = trainer.metrics().environment_steps;\n"
    "            }\n"
    "            else\n"
    "            {\n"
    "                rig_lifetime_seconds += std::max(0.0f, dt);\n"
    "            }",
    "tracked_rig_signature == 0u",
)

# Tests lock the requested quarter-unit markers, full-width bar, lifetime deltas,
# and flip/spin separation.
insert_before(
    "tests/core_tests.cpp",
    "    require(ui_layout::live_layout_valid(1100.0f, 902.0f),",
    "    require(ui_layout::top_bar_box(1970.0f).width == 1970.0f,\n"
    "        \"top GUI background does not span the full drawable width\");\n"
    "    require(std::abs(ui_layout::course_reference_marker_spacing_m(\n"
    "            ui_layout::DistanceUnits::metric) - 250.0f) < 0.001f,\n"
    "        \"metric markers are not quarter-kilometre spaced\");\n"
    "    require(std::abs(ui_layout::course_reference_marker_spacing_m(\n"
    "            ui_layout::DistanceUnits::imperial) - 402.336f) < 0.001f,\n"
    "        \"imperial markers are not quarter-mile spaced\");\n"
    "    require(ui_layout::lifetime_delta(120u, 20u) == 100u\n"
    "            && ui_layout::lifetime_delta(20u, 120u) == 0u,\n"
    "        \"rig lifetime counters can underflow\");\n",
    "quarter-kilometre spaced",
)
insert_before(
    "tests/core_tests.cpp",
    "    require(rl::policy_candidate_better(2u, 1.0f, 1u, 1000.0f, true),",
    "    {\n"
    "        sim::Environment flip_semantics{ humanoid, 0xF11Fu };\n"
    "        require(flip_semantics.maximum_flip_turns() == 0.0f\n"
    "                && flip_semantics.uncontrolled_spin_turns() == 0.0f,\n"
    "            \"fresh rig does not separate flip and spin counters\");\n"
    "    }\n\n",
    "fresh rig does not separate flip and spin counters",
)

mission = read("missioncache.md")
feedback_section = '''
### WALK-UI-035 — Full-width DPI-safe GUI background
**Status:** IN PROGRESS

The header and frame background must span the actual Vulkan drawable width at Windows DPI scaling. Mouse hit testing is converted from SDL logical coordinates to drawable coordinates.

### WALK-UNITS-036 — Metric/Imperial quarter markers
**Status:** IN PROGRESS

The live panel exposes Metric and Imperial modes. Course reference signs are spaced every 0.25 km or 0.25 mile and all speed/distance labels follow the selected mode.

### WALK-FLIP-037 — Separate flips from generic spin
**Status:** IN PROGRESS

A flip is powered airborne somersault rotation in the flip lesson followed by a landing. Generic rotation is tracked separately, is never accepted as flip evidence, and is penalized when it destabilizes other lessons.

### WALK-STATS-038 — Rig lifetime and cumulative runtime totals
**Status:** IN PROGRESS

Display current rig age, rig update delta, rig environment-step delta, session runtime, total updates, and total environment steps. Counter deltas are saturating and reset only when the rig signature changes.

'''
if "### WALK-UI-035" not in mission:
    anchor = "## v0.7.2 packaged-runtime regression correction"
    mission = mission.replace(anchor, feedback_section + anchor, 1)
    write("missioncache.md", mission)

notes = read("RELEASE_NOTES_v0.7.3.md")
extra_notes = '''- Fixes the short header/background at Windows DPI scaling by using drawable coordinates end-to-end.\n- Adds Metric and Imperial display modes with 0.25 km / 0.25 mile course markers.\n- Separates landed powered flips from uncontrolled generic spin and penalizes destabilizing spin outside flip training.\n- Adds current-rig lifetime counters plus session and cumulative runtime totals to the live panel.\n'''
if "0.25 km / 0.25 mile" not in notes:
    write("RELEASE_NOTES_v0.7.3.md", notes + extra_notes)

print("materialized v0.7.3 live feedback fixes")
