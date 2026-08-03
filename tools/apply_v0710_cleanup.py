from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


def patch_simulation() -> None:
    text = read("src/simulation.cpp")
    text = replace_once(
        text,
        """                const DuckPressProfile profile = duck_press_profile(
                    elapsed_seconds_, course_difficulty_, rest_head_top);
                constexpr float half_height = 0.14f;
                course_features_.push_back({
                    CourseFeatureKind::duck_press,
                    { root_x, profile.bottom_y + half_height },""",
        """                const DuckPressProfile profile = duck_press_profile(
                    elapsed_seconds_, course_difficulty_, rest_head_top);
                const float press_anchor_x = blueprint_.root_node < blueprint_.nodes.size()
                    ? blueprint_.nodes[blueprint_.root_node].x : root_x;
                constexpr float half_height = 0.14f;
                course_features_.push_back({
                    CourseFeatureKind::duck_press,
                    { press_anchor_x, profile.bottom_y + half_height },""",
        "duck press anchor",
    )
    text = replace_once(
        text,
        """                        particle.position += correction;
                        particle.previous += correction * 0.18f;
                        duck_press_contact_this_step_ = true;""",
        """                        // Move the current and previous positions together so the
                        // vertical constraint preserves velocity instead of injecting a
                        // fresh downward impulse on every solver iteration. The press has
                        // no horizontal authority and therefore cannot drag the rig back.
                        particle.position += correction;
                        particle.previous += correction;
                        duck_press_contact_this_step_ = true;""",
        "press contact velocity preservation",
    )
    write("src/simulation.cpp", text)


def patch_autonomy_header() -> None:
    text = read("src/autonomy.hpp")
    text = replace_once(
        text,
        """    inline constexpr int mastery_lock_confirmations = 8;

    [[nodiscard]] inline bool strict_balance_mastery(""",
        """    inline constexpr int mastery_lock_confirmations = 8;
    inline constexpr int balance_mastery_lock_confirmations = 3;
    inline constexpr float standing_mastery_joint_speed_limit = 10.0f;

    [[nodiscard]] inline int required_mastery_confirmations(
        sim::CourseStage stage) noexcept
    {
        return stage == sim::CourseStage::balance
            ? balance_mastery_lock_confirmations : mastery_lock_confirmations;
    }

    [[nodiscard]] inline bool strict_balance_mastery(""",
        "mastery constants",
    )
    text = replace_once(
        text,
        """            && metrics.evaluation_spin_turns <= standing_mastery_spin_limit
            && metrics.evaluation_max_joint_speed <= 8.0f;""",
        """            && metrics.evaluation_spin_turns <= standing_mastery_spin_limit
            && metrics.evaluation_max_joint_speed <= standing_mastery_joint_speed_limit;""",
        "standing joint speed gate",
    )
    write("src/autonomy.hpp", text)


def patch_curriculum() -> None:
    text = read("src/autonomy_curriculum.cpp")
    text = replace_once(
        text,
        """        mastery_streak_ = stage_mastered_locked() ? mastery_streak_ + 1 : 0;
        if (worker_.has_best_policy() && metrics.evaluation_valid)""",
        """        mastery_streak_ = stage_mastered_locked() ? mastery_streak_ + 1 : 0;
        const int required_confirmations = required_mastery_confirmations(stage_);
        if (worker_.has_best_policy() && metrics.evaluation_valid)""",
        "stage-specific mastery confirmations",
    )
    text = replace_once(
        text,
        """        else if (mastery_streak_ >= mastery_lock_confirmations)
        {
            advance_stage_locked();""",
        """        else if (mastery_streak_ >= required_confirmations)
        {
            advance_stage_locked();""",
        "mastery advance threshold",
    )
    text = replace_once(
        text,
        """                    \"STAGE VALID {}/6 SEEDS - STRICT STAND {:.1f}/{:.1f}S  SPIN {:.2f}/{:.2f}  MASTERY {}/{}\",
                    valid_seeds, metrics.evaluation_longest_stance,
                    standing_mastery_seconds, metrics.evaluation_spin_turns,
                    standing_mastery_spin_limit, mastery_streak_,
                    mastery_lock_confirmations);""",
        """                    \"STAGE VALID {}/6 SEEDS - STAND {:.1f}/{:.1f}S  SPIN {:.2f}/{:.2f}  JOINT {:.1f}/{:.1f}  MASTERY {}/{}\",
                    valid_seeds, metrics.evaluation_longest_stance,
                    standing_mastery_seconds, metrics.evaluation_spin_turns,
                    standing_mastery_spin_limit, metrics.evaluation_max_joint_speed,
                    standing_mastery_joint_speed_limit, mastery_streak_,
                    required_confirmations);""",
        "standing blocker telemetry",
    )
    text = replace_once(
        text,
        """                    sim::course_stage_name(stage_), mastery_streak_,
                    mastery_lock_confirmations);""",
        """                    sim::course_stage_name(stage_), mastery_streak_,
                    required_confirmations);""",
        "later-stage mastery telemetry",
    )
    write("src/autonomy_curriculum.cpp", text)


def patch_tests() -> None:
    path = "tests/core_tests.cpp"
    text = read(path)
    text = replace_once(
        text,
        """        static bool press_collision_resolves_below(Environment& environment) noexcept
        {
            if (!environment.valid_node(environment.blueprint_.head_node))
                return false;
            Particle& head = environment.particles_[environment.blueprint_.head_node];
            const float bottom = head.position.y + head.radius * 0.45f;
            environment.course_features_.clear();
            environment.course_features_.push_back({
                CourseFeatureKind::duck_press,
                { head.position.x, bottom + 0.16f }, { 1.5f, 0.16f }, 0.0f, {}, -2
            });
            environment.duck_press_contact_this_step_ = false;
            environment.duck_press_max_penetration_ = 0.0f;
            environment.solve_course();
            return environment.duck_press_contact_this_step_
                && head.position.y + head.radius <= bottom + 0.0001f;
        }""",
        """        static bool press_collision_resolves_below(Environment& environment) noexcept
        {
            if (!environment.valid_node(environment.blueprint_.head_node))
                return false;
            Particle& head = environment.particles_[environment.blueprint_.head_node];
            head.previous = head.position - Vec2{ 0.07f, -0.11f };
            const Vec2 velocity_before = head.position - head.previous;
            const float x_before = head.position.x;
            const float bottom = head.position.y + head.radius * 0.45f;
            environment.course_features_.clear();
            environment.course_features_.push_back({
                CourseFeatureKind::duck_press,
                { head.position.x, bottom + 0.16f }, { 1.5f, 0.16f }, 0.0f,
                { 0.0f, -0.5f }, -2
            });
            environment.duck_press_contact_this_step_ = false;
            environment.duck_press_max_penetration_ = 0.0f;
            environment.solve_course();
            const Vec2 velocity_after = head.position - head.previous;
            return environment.duck_press_contact_this_step_
                && head.position.y + head.radius <= bottom + 0.0001f
                && std::abs(head.position.x - x_before) < 0.000001f
                && length(velocity_after - velocity_before) < 0.000001f;
        }

        static bool press_anchor_remains_fixed(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            if (!environment.valid_node(environment.blueprint_.root_node))
                return false;
            const float expected = environment.blueprint_.nodes[
                environment.blueprint_.root_node].x;
            Particle& root = environment.particles_[environment.blueprint_.root_node];
            root.position.x += 1.75f;
            root.previous.x += 1.75f;
            environment.elapsed_seconds_ = 3.5f;
            environment.duck_press_completed_ = false;
            environment.rebuild_course_features();
            return environment.course_features_.size() == 1u
                && environment.course_features_.front().kind == CourseFeatureKind::duck_press
                && std::abs(environment.course_features_.front().center.x - expected) < 0.000001f;
        }""",
        "press test access",
    )
    text = replace_once(
        text,
        """    require(press_retract.retracting && press_retract.vertical_velocity > 0.0f,
        \"duck press does not retract after the hold\");

    require(ui_layout::top_bar_box""",
        """    require(press_retract.retracting && press_retract.vertical_velocity > 0.0f,
        \"duck press does not retract after the hold\");
    sim::Environment press_collision_environment(sim::CreatureBlueprint::humanoid(), 71u);
    require(sim::EnvironmentTestAccess::press_collision_resolves_below(
            press_collision_environment),
        \"duck press collision injects velocity or horizontal drag\");
    sim::Environment press_anchor_environment(sim::CreatureBlueprint::humanoid(), 73u);
    require(sim::EnvironmentTestAccess::press_anchor_remains_fixed(
            press_anchor_environment),
        \"duck press follows a sliding rig instead of staying fixed over the station\");

    require(ui_layout::top_bar_box""",
        "press regression tests",
    )
    text = replace_once(
        text,
        """    require(rl::mastery_lock_confirmations >= 8,
        \"staged mastery locks after too few repeated evaluations\");""",
        """    require(rl::balance_mastery_lock_confirmations == 3
            && rl::mastery_lock_confirmations >= 8
            && rl::required_mastery_confirmations(sim::CourseStage::balance) == 3
            && rl::required_mastery_confirmations(sim::CourseStage::duck_press) >= 8,
        \"standing and later-stage mastery confirmation counts are incorrect\");
    rl::TrainingMetrics standing_mastery{};
    standing_mastery.evaluation_valid = true;
    standing_mastery.evaluation_invalid_runs = 0u;
    standing_mastery.evaluation_longest_stance = rl::standing_mastery_seconds;
    standing_mastery.evaluation_survival = rl::standing_mastery_seconds;
    standing_mastery.evaluation_spin_turns = rl::standing_mastery_spin_limit;
    standing_mastery.evaluation_max_joint_speed =
        rl::standing_mastery_joint_speed_limit;
    require(rl::strict_balance_mastery(standing_mastery),
        \"all-six-seed standing evidence cannot satisfy the mastery gate\");
    standing_mastery.evaluation_max_joint_speed += 0.01f;
    require(!rl::strict_balance_mastery(standing_mastery),
        \"standing mastery accepts joint speed above its visible limit\");""",
        "mastery regression tests",
    )
    write(path, text)


def patch_art() -> None:
    text = read("src/app.cpp")
    text = replace_once(
        text,
        """#include <filesystem>
#include <format>""",
        """#include <filesystem>
#include <fstream>
#include <format>""",
        "art file include",
    )
    text = replace_once(
        text,
        """        constexpr Color leg{ 0.89f, 0.42f, 0.15f, 1.0f };

        void fill_rounded_rect""",
        """        constexpr Color leg{ 0.89f, 0.42f, 0.15f, 1.0f };

        struct PixelArt
        {
            int width{};
            int height{};
            std::vector<Color> pixels{};

            [[nodiscard]] bool loaded() const noexcept
            {
                return width > 0 && height > 0
                    && pixels.size() == static_cast<std::size_t>(width * height);
            }
        };

        [[nodiscard]] bool read_ppm_number(std::istream& input, int& value)
        {
            input >> std::ws;
            while (input.peek() == '#')
            {
                std::string ignored{};
                std::getline(input, ignored);
                input >> std::ws;
            }
            return static_cast<bool>(input >> value);
        }

        [[nodiscard]] bool load_p3_pixel_art(const std::filesystem::path& path,
            PixelArt& art, std::string& error)
        {
            std::ifstream input(path);
            if (!input)
            {
                error = \"Could not open original Runner artwork: \" + path.string();
                return false;
            }

            std::string magic{};
            input >> magic;
            int width{};
            int height{};
            int maximum{};
            if (magic != \"P3\" || !read_ppm_number(input, width)
                || !read_ppm_number(input, height)
                || !read_ppm_number(input, maximum)
                || width <= 0 || height <= 0 || width > 256 || height > 256
                || maximum <= 0 || maximum > 65535)
            {
                error = \"Original Runner artwork is not a valid bounded P3 PPM: \"
                    + path.string();
                return false;
            }

            PixelArt loaded{};
            loaded.width = width;
            loaded.height = height;
            loaded.pixels.reserve(static_cast<std::size_t>(width * height));
            const float inverse_maximum = 1.0f / static_cast<float>(maximum);
            for (int index = 0; index < width * height; ++index)
            {
                int red{};
                int green{};
                int blue{};
                if (!read_ppm_number(input, red) || !read_ppm_number(input, green)
                    || !read_ppm_number(input, blue)
                    || red < 0 || green < 0 || blue < 0
                    || red > maximum || green > maximum || blue > maximum)
                {
                    error = \"Original Runner artwork has incomplete pixel data: \"
                        + path.string();
                    return false;
                }
                loaded.pixels.push_back({
                    static_cast<float>(red) * inverse_maximum,
                    static_cast<float>(green) * inverse_maximum,
                    static_cast<float>(blue) * inverse_maximum,
                    1.0f
                });
            }
            art = std::move(loaded);
            error.clear();
            return true;
        }

        void draw_pixel_art(render::Canvas& canvas, const PixelArt& art,
            Vec2 position, float pixel_size)
        {
            if (!art.loaded() || pixel_size <= 0.0f)
                return;
            for (int y = 0; y < art.height; ++y)
            {
                for (int x = 0; x < art.width; ++x)
                {
                    const Vec2 minimum = position
                        + Vec2{ static_cast<float>(x) * pixel_size,
                            static_cast<float>(y) * pixel_size };
                    canvas.quad(minimum, minimum + Vec2{ pixel_size, pixel_size },
                        art.pixels[static_cast<std::size_t>(y * art.width + x)]);
                }
            }
        }

        void fill_rounded_rect""",
        "pixel art loader",
    )
    text = replace_once(
        text,
        """        float joint_test_phase{};
        float camera_x{};
        std::string status{ \"AUTOPILOT STARTING\" };""",
        """        float joint_test_phase{};
        float camera_x{};
        PixelArt original_runner_art{};
        std::string status{ \"AUTOPILOT STARTING\" };""",
        "art state",
    )
    text = replace_once(
        text,
        """            if (width >= 1080)
                add_text(canvas, { 20.0f, 50.0f }, \"AUTONOMOUS PHYSICS LOCOMOTION LAB\", 1.05f, muted);

            const float tab_width""",
        """            if (width >= 1080)
                add_text(canvas, { 20.0f, 50.0f }, \"AUTONOMOUS PHYSICS LOCOMOTION LAB\", 1.05f, muted);

            if (original_runner_art.loaded() && width >= 1280)
            {
                constexpr float art_scale = 2.35f;
                const Rect art_frame{
                    { 315.0f, 8.0f },
                    { static_cast<float>(original_runner_art.width) * art_scale + 12.0f,
                      static_cast<float>(original_runner_art.height) * art_scale + 12.0f }
                };
                add_rounded_rect(canvas, art_frame, 7.0f, rgb(0x0a1018), accent_dim, 1.0f);
                draw_pixel_art(canvas, original_runner_art,
                    art_frame.position + Vec2{ 6.0f, 6.0f }, art_scale);
                add_text(canvas,
                    { art_frame.position.x + art_frame.size.x + 12.0f, 18.0f },
                    \"ORIGINAL RUNNER ART\", 1.10f, body_light);
                add_text(canvas,
                    { art_frame.position.x + art_frame.size.x + 12.0f, 45.0f },
                    \"PACKAGED / LIVE\", 0.95f, muted);
            }

            const float tab_width""",
        "top-bar artwork",
    )
    text = replace_once(
        text,
        """    bool Application::initialize(const std::filesystem::path&, std::string& error)
    {
        impl_->trainer.set_autosave_paths""",
        """    bool Application::initialize(const std::filesystem::path& asset_directory,
        std::string& error)
    {
        if (!load_p3_pixel_art(asset_directory / \"chicken.ppm\",
                impl_->original_runner_art, error))
            return false;

        impl_->trainer.set_autosave_paths""",
        "art initialization",
    )
    write("src/app.cpp", text)


def patch_documents() -> None:
    cache = read("missioncache.md")
    cache = re.sub(
        r"^\*\*Release state:\*\*.*$",
        "**Release state:** REOPENED — live screenshot shows duck-press backward sliding and Stand curriculum failing to advance despite six valid seeds; v0.7.10 publication blocked pending corrected package evidence.",
        cache,
        count=1,
        flags=re.MULTILINE,
    )
    marker = "## v0.7.10 live regression correction"
    if marker not in cache:
        cache = cache.rstrip() + f"""

{marker}

### WALK-PRESS-109 — Duck press must not drag the rig backward
**Status:** IN VALIDATION

The press stays fixed over the authored test station, resolves contact vertically only, preserves particle velocity during positional correction, and has deterministic regression coverage for zero horizontal authority and zero solver-injected impulse.

### WALK-CURRICULUM-110 — Stand mastery must advance into crouch training
**Status:** IN VALIDATION

Stand qualification and mastery use the same visible 10 rad/s joint-speed limit. Three consecutive all-six-seed strict Stand evaluations advance to Static Crouch; later stages retain eight-confirmation mastery. The status line exposes stance, spin, joint-speed, and mastery blockers.

### WALK-RELEASE-111 — Revalidate and publish corrected Runner v0.7.10
**Status:** IN VALIDATION

Re-run Linux warnings-as-errors, all deterministic tests, full Windows Vulkan build/tests, installed and extracted package diagnostics, acceptance matrix, checksum/manifest audit, published-asset re-download, and branch/PR cleanup after WALK-PRESS-109 and WALK-CURRICULUM-110 pass.

### WALK-ART-112 — Display and package the original Runner artwork
**Status:** IN VALIDATION

The original `assets/chicken.ppm` artwork is loaded during application startup, rendered in the live top bar, and remains a required packaged runtime asset. Missing or malformed art fails initialization instead of silently reverting to a placeholder.
"""
    elif "WALK-ART-112" not in cache:
        cache = cache.rstrip() + """

### WALK-ART-112 — Display and package the original Runner artwork
**Status:** IN VALIDATION

The original `assets/chicken.ppm` artwork is loaded during application startup, rendered in the live top bar, and remains a required packaged runtime asset. Missing or malformed art fails initialization instead of silently reverting to a placeholder.
"""
    for evidence_marker in ("## v0.7.10 validation evidence", "## v0.7.10 immutable release evidence"):
        if evidence_marker in cache:
            cache = cache[:cache.index(evidence_marker)].rstrip() + "\n"
    write("missioncache.md", cache)

    changelog = read("CHANGELOG.md")
    anchor = "## [0.7.10] - 2026-08-03\n"
    addition = """

### Live regression fixes

- Fixed the duck press so it remains anchored over the test station instead of following a displaced rig.
- Removed solver-injected press velocity that could convert vertical compression into backward sliding.
- Aligned Stand mastery with the visible 10 rad/s qualification limit and exposed joint-speed blockers in the status line.
- Reduced only the Stand lock to three consecutive all-six-seed confirmations; later curriculum stages retain eight confirmations.
- Loaded and rendered the original Runner pixel artwork from the packaged `assets/chicken.ppm` asset.
"""
    if "### Live regression fixes" not in changelog:
        changelog = replace_once(changelog, anchor, anchor + addition, "v0.7.10 changelog anchor")
    elif "original Runner pixel artwork" not in changelog:
        changelog = replace_once(
            changelog,
            "- Reduced only the Stand lock to three consecutive all-six-seed confirmations; later curriculum stages retain eight confirmations.",
            "- Reduced only the Stand lock to three consecutive all-six-seed confirmations; later curriculum stages retain eight confirmations.\n"
            "- Loaded and rendered the original Runner pixel artwork from the packaged `assets/chicken.ppm` asset.",
            "art changelog bullet",
        )
    write("CHANGELOG.md", changelog)


def main() -> None:
    patch_simulation()
    patch_autonomy_header()
    patch_curriculum()
    patch_tests()
    patch_art()
    patch_documents()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
