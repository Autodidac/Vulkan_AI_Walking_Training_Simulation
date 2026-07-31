from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


header = Path("src/simulation.hpp")
text = header.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    struct CourseFeature
    {
        CourseFeatureKind kind{};
        Vec2 center{};
        Vec2 half_extent{};
        float radius{};
        Vec2 velocity{};
    };

    enum class InvalidMotion : std::uint8_t
""",
    """    struct CourseFeature
    {
        CourseFeatureKind kind{};
        Vec2 center{};
        Vec2 half_extent{};
        float radius{};
        Vec2 velocity{};
    };

    [[nodiscard]] inline float course_feature_observation_size(
        const CourseFeature& feature) noexcept
    {
        switch (feature.kind)
        {
        case CourseFeatureKind::moving_hazard:
        case CourseFeatureKind::rock:
        case CourseFeatureKind::projectile:
            return feature.radius;
        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
            return std::max(feature.half_extent.x, feature.half_extent.y);
        }
        return 0.0f;
    }

    enum class InvalidMotion : std::uint8_t
""",
    "course feature observation size",
)
text = replace_once(
    text,
    """    [[nodiscard]] inline bool recovery_terminal_fall(bool geometric_fall,
        bool hard_fall, bool recovery_active) noexcept
    {
        return hard_fall || (geometric_fall && !recovery_active);
    }

    [[nodiscard]] inline bool qualifies_alternating_step(int previous_side, int strike_side,
""",
    """    [[nodiscard]] inline bool recovery_should_start(bool collided,
        float uprightness, bool geometric_fall, bool hard_fall) noexcept
    {
        constexpr float destabilized_collision_uprightness = 0.88f;
        constexpr float independent_recovery_uprightness = 0.72f;
        return !hard_fall && (
            (collided && uprightness < destabilized_collision_uprightness)
            || uprightness < independent_recovery_uprightness
            || geometric_fall);
    }

    [[nodiscard]] inline bool recovery_terminal_fall(bool geometric_fall,
        bool hard_fall, bool recovery_active) noexcept
    {
        return hard_fall || (geometric_fall && !recovery_active);
    }

    [[nodiscard]] inline bool qualifies_alternating_step(int previous_side, int strike_side,
""",
    "bounded recovery trigger",
)
header.write_text(text, encoding="utf-8")

source = Path("src/simulation.cpp")
text = source.read_text(encoding="utf-8")
text = replace_once(
    text,
    """        if (!recovery_active_ && !hard_fall
            && (collided_this_step_ || upright < 0.72f || geometric_fall))
""",
    """        if (!recovery_active_ && recovery_should_start(
            collided_this_step_, upright, geometric_fall, hard_fall))
""",
    "recovery exploit trigger",
)
text = replace_once(
    text,
    """            result[24] = nearest->kind == CourseFeatureKind::moving_hazard
                ? nearest->radius : std::max(nearest->half_extent.x, nearest->half_extent.y);
""",
    """            result[24] = course_feature_observation_size(*nearest);
""",
    "radial obstacle observation",
)
source.write_text(text, encoding="utf-8")

tests = Path("tests/core_tests.cpp")
text = tests.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    require(!sim::recovery_terminal_fall(true, false, true),
        "recoverable near-fall terminated during its recovery window");
""",
    """    require(!sim::recovery_should_start(true, 0.95f, false, false),
        "harmless upright obstacle contact created a rewardable recovery event");
    require(sim::recovery_should_start(true, 0.80f, false, false),
        "destabilizing collision did not start recovery");
    require(sim::recovery_should_start(false, 0.68f, false, false),
        "major balance loss did not start recovery without a collision");
    require(!sim::recovery_should_start(true, 0.40f, true, true),
        "hard ground impact incorrectly opened a recovery window");

    require(!sim::recovery_terminal_fall(true, false, true),
        "recoverable near-fall terminated during its recovery window");
""",
    "recovery exploit tests",
)
text = replace_once(
    text,
    """    const std::array<sim::CreatureBlueprint, 5> presets{
""",
    """    const sim::CourseFeature rock_feature{
        sim::CourseFeatureKind::rock, {}, {}, 0.27f, {}
    };
    const sim::CourseFeature projectile_feature{
        sim::CourseFeatureKind::projectile, {}, {}, 0.19f, { -4.0f, 1.0f }
    };
    const sim::CourseFeature hurdle_feature{
        sim::CourseFeatureKind::hurdle, {}, { 0.14f, 0.42f }, 0.0f, {}
    };
    require(std::abs(sim::course_feature_observation_size(rock_feature) - 0.27f) < 0.0001f,
        "rock radius is absent from policy observations");
    require(std::abs(sim::course_feature_observation_size(projectile_feature) - 0.19f) < 0.0001f,
        "projectile radius is absent from policy observations");
    require(std::abs(sim::course_feature_observation_size(hurdle_feature) - 0.42f) < 0.0001f,
        "rectangular obstacle extent is incorrect in policy observations");

    const std::array<sim::CreatureBlueprint, 5> presets{
""",
    "obstacle extent tests",
)
text = text.replace("epochrunner-v060-core-test.eppo", "epochrunner-v061-core-test.eppo")
text = text.replace(
    "EpochRunner v0.6.0 procedural course, recovery, concurrency, gait, and rig-edit tests passed",
    "EpochRunner v0.6.1 obstacle observation, recovery, concurrency, gait, and rig-edit tests passed",
)
tests.write_text(text, encoding="utf-8")

cmake = Path("CMakeLists.txt")
text = cmake.read_text(encoding="utf-8")
text = replace_once(
    text,
    "project(EpochRunner VERSION 0.6.0 LANGUAGES CXX)",
    "project(EpochRunner VERSION 0.6.1 LANGUAGES CXX)",
    "CMake version",
)
cmake.write_text(text, encoding="utf-8")

manifest = Path("vcpkg.json")
text = manifest.read_text(encoding="utf-8")
text = replace_once(
    text,
    '"version-semver": "0.6.0"',
    '"version-semver": "0.6.1"',
    "vcpkg version",
)
manifest.write_text(text, encoding="utf-8")

readme = Path("README.md")
text = readme.read_text(encoding="utf-8")
text = replace_once(
    text,
    "EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, vcpkg manifest mode, and a compact PPO controller. Version 0.6.0 replaces manual train/run switching with a continuously operating autonomous curriculum.\n",
    "EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, vcpkg manifest mode, and a compact PPO controller. Version 0.6.1 completes radial obstacle observations and closes the harmless-contact recovery reward exploit while retaining the autonomous curriculum introduced in v0.6.0.\n",
    "README release identity",
)
readme.write_text(text, encoding="utf-8")

benchmark = Path("tests/concurrency_benchmark.cpp")
text = benchmark.read_text(encoding="utf-8")
text = text.replace(
    "EpochRunner v0.6 speed-mode throughput benchmark passed",
    "EpochRunner v0.6.1 speed-mode throughput benchmark passed",
)
benchmark.write_text(text, encoding="utf-8")

missions = Path("MISSIONS.md")
text = missions.read_text(encoding="utf-8")
section = """
## WALK-OBS-001 — Complete obstacle sensing and recovery reward integrity

**Status:** ACTIVE

Every physical obstacle class must expose its actual collision size to the policy. Radial rocks, projectiles, and moving hazards use radius; hurdles and overhead bars use rectangular extent. Harmless upright contact must not open a rewardable recovery event.

**Acceptance:**

- Rock, projectile, and moving-hazard radius is present in observations.
- Hurdle and overhead-bar extent remains present in observations.
- Destabilizing impacts and major balance loss start recovery.
- Harmless upright contact cannot farm recovery bonuses.
- Hard falls remain terminal.
- Full Windows/Vulkan build, deterministic tests, diagnostics, package, and checksum pass.

"""
if "## WALK-OBS-001" not in text:
    warning = text.index("## Current warning")
    text = text[:warning] + section + text[warning:]
missions.write_text(text, encoding="utf-8")
