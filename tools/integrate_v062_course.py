from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


def replace_section(text: str, start_marker: str, end_marker: str,
                    replacement: str, label: str) -> str:
    start = text.find(start_marker)
    if start < 0:
        raise SystemExit(f"{label}: start marker not found")
    end = text.find(end_marker, start)
    if end < 0:
        raise SystemExit(f"{label}: end marker not found")
    return text[:start] + replacement + text[end:]


header = Path("src/simulation.hpp")
text = header.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    [[nodiscard]] inline int first_course_feature_sequence(float root_x, float course_progress,
        float spacing = 5.5f, float lead_distance = 4.5f) noexcept
    {
        return static_cast<int>(std::ceil((root_x + course_progress + lead_distance) / spacing));
    }

    [[nodiscard]] inline float course_feature_world_x(int sequence, float course_progress,
        float spacing = 5.5f) noexcept
    {
        return static_cast<float>(sequence) * spacing - course_progress;
    }
""",
    """    inline constexpr float course_marker_spacing_m = 8.0f;
    inline constexpr int course_safe_runway_markers = 3;
    inline constexpr int course_feature_cycle_length = 5;

    [[nodiscard]] inline int first_course_feature_sequence(float root_x, float course_progress,
        float spacing = course_marker_spacing_m, float lead_distance = 4.5f) noexcept
    {
        return static_cast<int>(std::ceil((root_x + course_progress + lead_distance) / spacing));
    }

    [[nodiscard]] inline float course_feature_world_x(int sequence, float course_progress,
        float spacing = course_marker_spacing_m) noexcept
    {
        return static_cast<float>(sequence) * spacing - course_progress;
    }

    [[nodiscard]] inline float course_marker_distance_m(int sequence,
        float spacing = course_marker_spacing_m) noexcept
    {
        return static_cast<float>(sequence) * spacing;
    }

    [[nodiscard]] inline CourseFeatureKind scheduled_course_feature(CourseStage stage,
        int marker_sequence) noexcept
    {
        const int relative = std::max(0, marker_sequence - course_safe_runway_markers);
        const int selector = relative % course_feature_cycle_length;
        if (stage == CourseStage::ramps || stage == CourseStage::uneven)
            return CourseFeatureKind::rock;
        if (stage == CourseStage::hurdles)
            return selector == 0 ? CourseFeatureKind::rock : CourseFeatureKind::hurdle;
        if (stage == CourseStage::duck_bars)
        {
            if (selector == 0)
                return CourseFeatureKind::rock;
            if (selector == 1)
                return CourseFeatureKind::hurdle;
            return CourseFeatureKind::overhead_bar;
        }
        switch (selector)
        {
        case 0: return CourseFeatureKind::rock;
        case 1: return CourseFeatureKind::hurdle;
        case 2: return CourseFeatureKind::overhead_bar;
        case 3: return CourseFeatureKind::moving_hazard;
        default: return CourseFeatureKind::projectile;
        }
    }
""",
    "mile-marker schedule helpers",
)
text = replace_once(
    text,
    """        [[nodiscard]] float course_speed() const noexcept
        {
            return course_stage_ == CourseStage::balance ? 0.0f : 0.45f + course_difficulty_ * 0.75f;
        }
""",
    """        [[nodiscard]] float course_speed() const noexcept
        {
            return course_stage_ == CourseStage::balance ? 0.0f : 0.70f + course_difficulty_ * 0.90f;
        }
""",
    "bounded faster virtual course speed",
)
header.write_text(text, encoding="utf-8")

source = Path("src/simulation.cpp")
text = source.read_text(encoding="utf-8")
new_rebuild = """    void Environment::rebuild_course_features() noexcept
    {
        course_features_.clear();
        if (course_stage_ == CourseStage::balance || course_stage_ == CourseStage::walk)
            return;

        const float root_x = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float progress = course_progress();
        const int first_sequence = first_course_feature_sequence(root_x, progress);
        const float treadmill_velocity = -course_speed();

        auto variation_for = [](int sequence) noexcept
        {
            std::uint32_t value = static_cast<std::uint32_t>(sequence) * 747796405u + 2891336453u;
            value ^= value >> 16u;
            value *= 2246822519u;
            value ^= value >> 13u;
            return static_cast<float>(value & 0xffffu) / 65535.0f;
        };

        constexpr int visible_markers = 14;
        for (int offset = 0; offset < visible_markers; ++offset)
        {
            const int sequence = first_sequence + offset;
            if (sequence < course_safe_runway_markers)
                continue;

            const float variation = variation_for(sequence);
            const float x = course_feature_world_x(sequence, progress);
            const float ground = ground_height_at(x);
            const CourseFeatureKind kind = scheduled_course_feature(course_stage_, sequence);

            switch (kind)
            {
            case CourseFeatureKind::rock:
            {
                const float radius = 0.16f + variation * (0.15f + course_difficulty_ * 0.08f);
                course_features_.push_back({
                    kind, { x, ground + radius }, {}, radius, { treadmill_velocity, 0.0f }
                });
                break;
            }
            case CourseFeatureKind::hurdle:
            {
                const float height = 0.24f + course_difficulty_ * 0.34f + variation * 0.12f;
                course_features_.push_back({
                    kind, { x, ground + height * 0.5f }, { 0.14f, height * 0.5f }, 0.0f,
                    { treadmill_velocity, 0.0f }
                });
                break;
            }
            case CourseFeatureKind::overhead_bar:
            {
                const float clearance = 3.65f - course_difficulty_ * 0.82f - variation * 0.16f;
                course_features_.push_back({
                    kind, { x, ground + clearance + 0.12f }, { 1.05f, 0.12f }, 0.0f,
                    { treadmill_velocity, 0.0f }
                });
                break;
            }
            case CourseFeatureKind::moving_hazard:
            {
                const float oscillation = std::sin(
                    elapsed_seconds_ * (1.7f + course_difficulty_) + static_cast<float>(sequence));
                const float radius = 0.19f + course_difficulty_ * 0.10f;
                course_features_.push_back({
                    kind,
                    { x + oscillation * 0.85f, ground + 1.05f + oscillation * 0.38f },
                    {},
                    radius,
                    { treadmill_velocity + oscillation * 0.35f, 0.0f }
                });
                break;
            }
            case CourseFeatureKind::projectile:
            {
                const float throw_phase = std::fmod(
                    elapsed_seconds_ * (0.72f + course_difficulty_ * 0.28f)
                        + static_cast<float>(sequence) * 0.37f,
                    1.0f);
                const float throw_speed = 2.8f + course_difficulty_ * 2.2f;
                const float arc = 4.0f * throw_phase * (1.0f - throw_phase);
                const float radius = 0.14f + variation * 0.08f;
                course_features_.push_back({
                    kind,
                    { x + 2.2f - throw_phase * 4.4f, ground + 1.15f + arc * 1.55f },
                    {},
                    radius,
                    { treadmill_velocity - throw_speed, (1.0f - throw_phase * 2.0f) * 2.4f }
                });
                break;
            }
            }
        }
    }

"""
text = replace_section(text, "    void Environment::rebuild_course_features() noexcept",
    "    void Environment::reset(", new_rebuild, "mile-marker feature generator")
source.write_text(text, encoding="utf-8")

app = Path("src/app.cpp")
text = app.read_text(encoding="utf-8")
text = replace_once(
    text,
    "            constexpr float marker_spacing = 10.0f;",
    "            constexpr float marker_spacing = sim::course_marker_spacing_m;",
    "shared marker spacing",
)
app.write_text(text, encoding="utf-8")

tests = Path("tests/core_tests.cpp")
text = tests.read_text(encoding="utf-8")
anchor = """    require(std::abs((advanced_x - anchored_x) + 1.0f) < 0.0001f,
        "course debris does not advance in world space solely from course progress");
"""
replacement = anchor + """    require(std::abs(sim::course_marker_distance_m(4) - 32.0f) < 0.0001f,
        "course mile-marker spacing is not shared with obstacle scheduling");
    require(sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 3)
            == sim::CourseFeatureKind::rock
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 4)
            == sim::CourseFeatureKind::hurdle
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 5)
            == sim::CourseFeatureKind::overhead_bar
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 6)
            == sim::CourseFeatureKind::moving_hazard
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 7)
            == sim::CourseFeatureKind::projectile,
        "moving-hazard lesson does not schedule every obstacle class on consecutive markers");
    require(sim::course_marker_distance_m(sim::course_safe_runway_markers) >= 24.0f,
        "course does not provide enough safe runway before the first obstacle marker");
"""
text = replace_once(text, anchor, replacement, "mile-marker schedule tests")
tests.write_text(text, encoding="utf-8")

missions = Path("MISSIONS.md")
text = missions.read_text(encoding="utf-8")
mission = """## WALK-COURSE-002 — Mile-marker obstacle schedule

**Status:** ACTIVE

All generated course elements must be anchored to shared course/mile-marker coordinates rather than actor position. Rocks, hurdles, overhead bars, moving hazards, and thrown projectiles must each appear in the advanced lesson. Every lesson receives a visible safe runway before its first obstacle, while bounded virtual course speed keeps the next marker from taking excessive real time to arrive.

**Acceptance:**

- Marker rendering and obstacle generation use the same spacing constant.
- The first three markers form a 24-metre safe runway.
- Advanced lessons cycle rock, hurdle, overhead bar, moving hazard, and projectile on consecutive markers.
- Moving hazards oscillate around their marker; projectiles originate and arc around their marker.
- No obstacle position includes actor/root translation.
- Virtual course speed brings the first obstacle into view promptly without removing the safe runway.
- Full Windows/Vulkan build, deterministic schedule tests, diagnostics, package, and exact-source evidence pass.

"""
if "## WALK-COURSE-002" not in text:
    marker = "## Current warning\n"
    if marker not in text:
        raise SystemExit("mission ledger: Current warning anchor not found")
    text = text.replace(marker, mission + marker, 1)
missions.write_text(text, encoding="utf-8")

readme = Path("README.md")
text = readme.read_text(encoding="utf-8")
needle = "The v0.6.2 interface also uses larger typography, wider responsive side panels, wrapped trainer messages, grouped runtime/result cards, and split world telemetry so labels remain readable instead of overlapping.\n"
replacement = needle + "\nCourse markers and obstacles now share one eight-metre schedule. Each lesson starts with three clear markers of safe runway, then advanced training cycles rocks, hurdles, overhead bars, moving hazards, and thrown projectiles at consecutive markers. The virtual course moves quickly enough to reach those events in practical training time while every feature remains anchored to course coordinates rather than following the actor.\n"
text = replace_once(text, needle, replacement, "README mile-marker schedule note")
readme.write_text(text, encoding="utf-8")
