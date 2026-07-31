from __future__ import annotations

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


def replace_region(text: str, start: str, end: str, replacement: str, label: str) -> str:
    begin = text.find(start)
    if begin < 0:
        if replacement in text:
            return text
        raise SystemExit(f"{label}: start anchor not found")
    finish = text.find(end, begin + len(start))
    if finish < 0:
        raise SystemExit(f"{label}: end anchor not found")
    return text[:begin] + replacement + text[finish:]


root = Path(".")

cmake = root / "CMakeLists.txt"
text = cmake.read_text(encoding="utf-8")
text = replace_once(
    text,
    "project(EpochRunner VERSION 0.5.0 LANGUAGES CXX)",
    "project(EpochRunner VERSION 0.6.0 LANGUAGES CXX)",
    "CMake version",
)
cmake.write_text(text, encoding="utf-8")

vcpkg = root / "vcpkg.json"
text = vcpkg.read_text(encoding="utf-8")
text = replace_once(
    text,
    '"version-semver": "0.5.0"',
    '"version-semver": "0.6.0"',
    "vcpkg version",
)
vcpkg.write_text(text, encoding="utf-8")

header = root / "src/simulation.hpp"
text = header.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    enum class CourseFeatureKind : std::uint8_t
    {
        hurdle,
        overhead_bar,
        moving_hazard
    };
""",
    """    enum class CourseFeatureKind : std::uint8_t
    {
        hurdle,
        overhead_bar,
        moving_hazard,
        rock,
        projectile
    };

    [[nodiscard]] inline std::string_view course_feature_name(CourseFeatureKind kind) noexcept
    {
        switch (kind)
        {
        case CourseFeatureKind::hurdle: return "HURDLE";
        case CourseFeatureKind::overhead_bar: return "LOW BAR";
        case CourseFeatureKind::moving_hazard: return "MOVING HAZARD";
        case CourseFeatureKind::rock: return "ROCK";
        case CourseFeatureKind::projectile: return "THROWN OBJECT";
        }
        return "OBSTACLE";
    }
""",
    "course feature kinds",
)
text = replace_once(
    text,
    """        [[nodiscard]] float ground_height() const noexcept { return 0.0f; }
        [[nodiscard]] float ground_height_at(float x) const noexcept;
        [[nodiscard]] float collision_count() const noexcept { return collision_count_; }
""",
    """        [[nodiscard]] float ground_height() const noexcept { return 0.0f; }
        [[nodiscard]] float ground_height_at(float x) const noexcept;
        [[nodiscard]] float course_speed() const noexcept
        {
            return course_stage_ == CourseStage::balance ? 0.0f : 0.45f + course_difficulty_ * 0.75f;
        }
        [[nodiscard]] float course_progress() const noexcept { return elapsed_seconds_ * course_speed(); }
        [[nodiscard]] bool recovering() const noexcept { return recovery_active_; }
        [[nodiscard]] std::uint32_t recovery_events() const noexcept { return recovery_events_; }
        [[nodiscard]] std::uint32_t recovery_successes() const noexcept { return recovery_successes_; }
        [[nodiscard]] float collision_count() const noexcept { return collision_count_; }
""",
    "course telemetry getters",
)
text = replace_once(
    text,
    """        bool collided_this_step_{};
        InvalidMotion invalid_reason_{ InvalidMotion::none };
""",
    """        bool collided_this_step_{};
        bool recovery_active_{};
        float recovery_started_seconds_{};
        float recovery_best_upright_{ 1.0f };
        std::uint32_t recovery_events_{};
        std::uint32_t recovery_successes_{};
        InvalidMotion invalid_reason_{ InvalidMotion::none };
""",
    "recovery state",
)
header.write_text(text, encoding="utf-8")

source = root / "src/simulation.cpp"
text = source.read_text(encoding="utf-8")

ground_function = """    float Environment::ground_height_at(float x) const noexcept
    {
        if (course_stage_ == CourseStage::balance || course_stage_ == CourseStage::walk)
            return 0.0f;

        constexpr float segment_length = 7.0f;
        const float course_x = std::max(0.0f, x + course_progress());
        const int segment = static_cast<int>(std::floor(course_x / segment_length)) % 5;
        const float local = std::fmod(course_x, segment_length) / segment_length;
        const float smooth = local * local * (3.0f - 2.0f * local);
        const float amplitude = 0.18f + course_difficulty_ * 0.42f;

        float height = 0.0f;
        switch (segment)
        {
        case 0:
            height = 0.0f;
            break;
        case 1:
            height = amplitude * smooth;
            break;
        case 2:
            height = amplitude;
            break;
        case 3:
            height = amplitude * (1.0f - smooth);
            break;
        default:
            height = std::sin(local * pi) * amplitude * 0.78f;
            break;
        }

        if (course_stage_ >= CourseStage::uneven)
        {
            const float roughness = course_difficulty_ * 0.065f;
            height += std::sin(course_x * 0.83f) * roughness;
            height += std::sin(course_x * 2.17f + 0.7f) * roughness * 0.42f;
        }
        return std::max(-0.06f, height);
    }
"""

feature_function = """    void Environment::rebuild_course_features() noexcept
    {
        course_features_.clear();
        if (course_stage_ == CourseStage::balance || course_stage_ == CourseStage::walk)
            return;

        const float root_x = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        constexpr float spacing = 5.5f;
        const float progress = course_progress();
        const int first_sequence = static_cast<int>(std::floor(progress / spacing));
        const float phase = std::fmod(progress, spacing);
        const float treadmill_velocity = -course_speed();

        auto variation_for = [](int sequence) noexcept
        {
            std::uint32_t value = static_cast<std::uint32_t>(sequence) * 747796405u + 2891336453u;
            value ^= value >> 16u;
            value *= 2246822519u;
            value ^= value >> 13u;
            return static_cast<float>(value & 0xffffu) / 65535.0f;
        };

        for (int offset = 0; offset < 10; ++offset)
        {
            const int sequence = first_sequence + offset;
            const int selector = ((sequence % 5) + 5) % 5;
            const float variation = variation_for(sequence);
            const float x = root_x + 4.5f + static_cast<float>(offset) * spacing - phase;
            const float ground = ground_height_at(x);

            CourseFeatureKind kind = CourseFeatureKind::rock;
            if (selector == 1 && course_stage_ >= CourseStage::hurdles)
                kind = CourseFeatureKind::hurdle;
            else if (selector == 2 && course_stage_ >= CourseStage::duck_bars)
                kind = CourseFeatureKind::overhead_bar;
            else if (selector == 3 && course_stage_ >= CourseStage::moving_hazards)
                kind = CourseFeatureKind::moving_hazard;
            else if (selector == 4 && course_stage_ >= CourseStage::moving_hazards)
                kind = CourseFeatureKind::projectile;
            else if (selector >= 3 && course_stage_ >= CourseStage::hurdles)
                kind = CourseFeatureKind::hurdle;

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

text = replace_region(
    text,
    "    float Environment::ground_height_at(float x) const noexcept\n    {",
    "\n    void Environment::rebuild_course_features() noexcept",
    ground_function,
    "ground profile",
)
text = replace_region(
    text,
    "    void Environment::rebuild_course_features() noexcept\n    {",
    "\n    void Environment::reset(std::uint64_t seed)",
    feature_function,
    "procedural features",
)

text = replace_once(
    text,
    """        collided_this_step_ = false;
        invalid_reason_ = InvalidMotion::none;
        rebuild_course_features();
""",
    """        collided_this_step_ = false;
        recovery_active_ = false;
        recovery_started_seconds_ = 0.0f;
        recovery_best_upright_ = 1.0f;
        recovery_events_ = 0;
        recovery_successes_ = 0;
        invalid_reason_ = InvalidMotion::none;
        rebuild_course_features();
""",
    "recovery reset",
)

text = replace_once(
    text,
    """                if (feature.kind == CourseFeatureKind::moving_hazard)
                {
                    const Vec2 delta = particle.position - feature.center;
                    const float distance = length(delta);
                    const float minimum = particle.radius + feature.radius;
                    if (distance >= minimum)
                        continue;
                    const Vec2 normal = distance > 1.0e-5f ? delta / distance : Vec2{ -1.0f, 0.0f };
                    const Vec2 correction = normal * (minimum - distance);
                    particle.position += correction;
                    particle.previous += correction * 0.25f;
                    collided_this_step_ = true;
                    continue;
                }
""",
    """                if (feature.kind == CourseFeatureKind::moving_hazard
                    || feature.kind == CourseFeatureKind::rock
                    || feature.kind == CourseFeatureKind::projectile)
                {
                    const Vec2 delta = particle.position - feature.center;
                    const float distance = length(delta);
                    const float minimum = particle.radius + feature.radius;
                    if (distance >= minimum)
                        continue;
                    const Vec2 normal = distance > 1.0e-5f ? delta / distance : Vec2{ -1.0f, 0.0f };
                    const Vec2 correction = normal * (minimum - distance);
                    particle.position += correction;
                    particle.previous += correction * 0.25f;
                    if (feature.kind == CourseFeatureKind::projectile)
                        particle.previous -= feature.velocity * (1.0f / 60.0f) * 0.34f;
                    else if (feature.kind == CourseFeatureKind::moving_hazard)
                        particle.previous -= feature.velocity * (1.0f / 60.0f) * 0.12f;
                    collided_this_step_ = true;
                    continue;
                }
""",
    "circular obstacle collision",
)

text = replace_once(
    text,
    """        fallen_ = particles_[blueprint_.torso_node].position.y < torso_floor
            || particles_[blueprint_.head_node].position.y < head_floor;

        const float allowed_airtime = course_stage_ == CourseStage::hurdles ? 1.30f
""",
    """        fallen_ = particles_[blueprint_.torso_node].position.y < torso_floor
            || particles_[blueprint_.head_node].position.y < head_floor;

        float recovery_reward = 0.0f;
        const bool supported = particles_[blueprint_.left_contact_node].grounded
            || particles_[blueprint_.right_contact_node].grounded;
        if (!recovery_active_ && !fallen_ && (collided_this_step_ || upright < 0.72f))
        {
            recovery_active_ = true;
            recovery_started_seconds_ = elapsed_seconds_;
            recovery_best_upright_ = upright;
            ++recovery_events_;
        }
        if (recovery_active_)
        {
            const float improvement = upright - recovery_best_upright_;
            if (improvement > 0.0f)
                recovery_reward += improvement * 0.10f;
            recovery_best_upright_ = std::max(recovery_best_upright_, upright);
            const float recovery_time = elapsed_seconds_ - recovery_started_seconds_;
            if (upright >= 0.90f && supported && recovery_time >= 0.12f)
            {
                recovery_active_ = false;
                ++recovery_successes_;
                recovery_reward += 0.14f;
            }
            else if (fallen_ || recovery_time > 3.0f)
            {
                recovery_active_ = false;
                recovery_reward -= 0.10f;
            }
        }

        const float allowed_airtime = course_stage_ == CourseStage::hurdles ? 1.30f
""",
    "recovery objective",
)

text = replace_once(
    text,
    """        if (invalid_reason_ != InvalidMotion::none)
            last_reward_ -= 5.0f;
""",
    """        last_reward_ += recovery_reward;
        if (invalid_reason_ != InvalidMotion::none)
        {
            recovery_active_ = false;
            last_reward_ -= 5.0f;
        }
""",
    "recovery reward application",
)

text = replace_once(
    text,
    """        result[17] = 1.0f;
""",
    """        result[17] = recovery_active_ ? clamp(torso.y, -1.0f, 1.0f) : 1.0f;
""",
    "recovery observation",
)

text = replace_once(
    text,
    """            result[22] = nearest->kind == CourseFeatureKind::hurdle ? -1.0f
                : nearest->kind == CourseFeatureKind::overhead_bar ? 0.0f : 1.0f;
""",
    """            switch (nearest->kind)
            {
            case CourseFeatureKind::hurdle: result[22] = -1.0f; break;
            case CourseFeatureKind::rock: result[22] = -0.5f; break;
            case CourseFeatureKind::overhead_bar: result[22] = 0.0f; break;
            case CourseFeatureKind::moving_hazard: result[22] = 0.5f; break;
            case CourseFeatureKind::projectile: result[22] = 1.0f; break;
            }
""",
    "obstacle type observation",
)
source.write_text(text, encoding="utf-8")

app = root / "src/app.cpp"
text = app.read_text(encoding="utf-8")

reference_renderer = """        void draw_course_reference(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            const float progress = environment.course_progress();
            const float half_view = viewport.size.x * 0.5f / scale;
            const float left = camera - half_view - 2.0f;
            const float right = camera + half_view + 2.0f;

            constexpr float dash_spacing = 1.6f;
            const int first_dash = static_cast<int>(std::floor((left + progress) / dash_spacing));
            const int last_dash = static_cast<int>(std::ceil((right + progress) / dash_spacing));
            for (int index = first_dash; index <= last_dash; ++index)
            {
                const float x0 = static_cast<float>(index) * dash_spacing - progress;
                const float x1 = x0 + 0.72f;
                const Vec2 start = world_to_screen(
                    { x0, environment.ground_height_at(x0) + 0.035f }, viewport, camera, scale);
                const Vec2 end = world_to_screen(
                    { x1, environment.ground_height_at(x1) + 0.035f }, viewport, camera, scale);
                canvas.line(start, end, 3.0f, rgb(0xd6d9c4, 0.82f));
            }

            constexpr float marker_spacing = 10.0f;
            const int first_marker = static_cast<int>(std::floor((left + progress) / marker_spacing));
            const int last_marker = static_cast<int>(std::ceil((right + progress) / marker_spacing));
            for (int index = first_marker; index <= last_marker; ++index)
            {
                if (index < 0)
                    continue;
                const float distance = static_cast<float>(index) * marker_spacing;
                const float x = distance - progress;
                const float ground = environment.ground_height_at(x);
                const Vec2 base = world_to_screen({ x, ground }, viewport, camera, scale);
                const Vec2 top = world_to_screen({ x, ground + 0.72f }, viewport, camera, scale);
                canvas.line(base, top, 4.0f, accent_dim);
                const Rect sign{ top + Vec2{ -43.0f, -22.0f }, { 86.0f, 21.0f } };
                add_rounded_rect(canvas, sign, 4.0f, rgb(0x102431, 0.94f), accent, 1.0f);
                add_text(canvas, sign.position + Vec2{ 5.0f, 5.0f },
                    std::format("{:.0f} M / {:.3f} MI", distance, distance / 1609.344f),
                    0.76f, white);
            }
        }

"""

text = replace_once(
    text,
    """        void draw_course_features(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
""",
    reference_renderer + """        void draw_course_features(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
""",
    "road reference renderer",
)

feature_renderer = """        void draw_course_features(const sim::Environment& environment, Rect viewport,
            float camera, float scale)
        {
            for (const sim::CourseFeature& feature : environment.course_features())
            {
                const Vec2 feature_screen = world_to_screen(feature.center, viewport, camera, scale);
                if (feature.kind == sim::CourseFeatureKind::moving_hazard
                    || feature.kind == sim::CourseFeatureKind::rock
                    || feature.kind == sim::CourseFeatureKind::projectile)
                {
                    const Color fill = feature.kind == sim::CourseFeatureKind::rock
                        ? rgb(0x6c747d)
                        : feature.kind == sim::CourseFeatureKind::projectile ? rgb(0xf06a3e) : danger;
                    canvas.circle(feature_screen, feature.radius * scale, fill, 24);
                    if (feature.kind == sim::CourseFeatureKind::projectile)
                    {
                        const Vec2 trail = feature_screen - feature.velocity * (scale * 0.20f);
                        canvas.line(trail, feature_screen, 3.0f, yellow);
                    }
                }
                else
                {
                    const Vec2 minimum = world_to_screen(feature.center - feature.half_extent,
                        viewport, camera, scale);
                    const Vec2 maximum = world_to_screen(feature.center + feature.half_extent,
                        viewport, camera, scale);
                    const Rect rect{
                        { minimum.x, maximum.y },
                        { maximum.x - minimum.x, minimum.y - maximum.y }
                    };
                    add_rounded_rect(canvas, rect, 4.0f,
                        feature.kind == sim::CourseFeatureKind::hurdle ? yellow : accent_dim,
                        feature.kind == sim::CourseFeatureKind::hurdle ? yellow : accent, 1.0f);
                }

                add_text(canvas, feature_screen + Vec2{ -42.0f, -36.0f },
                    sim::course_feature_name(feature.kind), 0.82f,
                    feature.kind == sim::CourseFeatureKind::projectile ? yellow : muted);
            }
        }
"""

text = replace_region(
    text,
    "        void draw_course_features(const sim::Environment& environment, Rect viewport,\n            float camera, float scale)\n        {",
    "\n        void draw_creature(const sim::Environment& environment",
    feature_renderer,
    "feature renderer",
)

text = replace_once(
    text,
    """            draw_course_ground(environment, viewport, camera_x, 90.0f);
            draw_course_features(environment, viewport, camera_x, 90.0f);
""",
    """            draw_course_ground(environment, viewport, camera_x, 90.0f);
            draw_course_reference(environment, viewport, camera_x, 90.0f);
            draw_course_features(environment, viewport, camera_x, 90.0f);
""",
    "live course draw order",
)

text = replace_once(
    text,
    """                std::format("{:.1f} KM/H   {:.1f} M   {}", environment.forward_speed() * 3.6f,
                    environment.distance_travelled(), sim::invalid_motion_name(environment.invalid_reason())),
                1.45f, environment.valid_motion() ? green : danger);
""",
    """                std::format("{:.1f} KM/H   ACTUAL {:.1f} M   COURSE {:.1f} M   {}",
                    environment.forward_speed() * 3.6f, environment.distance_travelled(),
                    environment.course_progress(), sim::invalid_motion_name(environment.invalid_reason())),
                1.45f, environment.valid_motion() ? green : danger);
            add_text(canvas, viewport.position + Vec2{ 24.0f, 88.0f },
                std::format("RECOVERY {}   {}/{} SUCCESS",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.recovery_successes(), environment.recovery_events()),
                1.12f, environment.recovering() ? yellow : muted);
""",
    "live course telemetry",
)
app.write_text(text, encoding="utf-8")

tests = root / "tests/core_tests.cpp"
text = tests.read_text(encoding="utf-8")
course_tests = """
    {
        sim::Environment procedural{ humanoid, 0xC0A57u };
        procedural.set_course(sim::CourseStage::moving_hazards, 0.65f);
        const float initial_progress = procedural.course_progress();
        const float initial_height = procedural.ground_height_at(7.5f);
        const std::array<float, sim::action_count> zero_actions{};
        for (int frame = 0; frame < 90; ++frame)
        {
            const sim::StepResult result = procedural.step(zero_actions);
            require(std::isfinite(result.reward), "procedural obstacle reward is not finite");
            (void)result.terminated;
        }
        require(procedural.course_progress() > initial_progress,
            "procedural course does not advance when the creature is stationary");
        require(std::abs(procedural.ground_height_at(7.5f) - initial_height) > 0.001f,
            "procedural inclines and hills do not move through the training lane");

        std::array<bool, 5> found{};
        for (const sim::CourseFeature& feature : procedural.course_features())
            found[static_cast<std::size_t>(feature.kind)] = true;
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::hurdle)],
            "procedural course omitted hurdles");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::overhead_bar)],
            "procedural course omitted overhead bars");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::moving_hazard)],
            "procedural course omitted moving hazards");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::rock)],
            "procedural course omitted rocks");
        require(found[static_cast<std::size_t>(sim::CourseFeatureKind::projectile)],
            "procedural course omitted thrown objects");
    }

"""
text = replace_once(
    text,
    """    rl::PpoTrainer trainer{ humanoid, 16 };
""",
    course_tests + """    rl::PpoTrainer trainer{ humanoid, 16 };
""",
    "procedural course tests",
)
text = text.replace("epochrunner-v050-core-test.eppo", "epochrunner-v060-core-test.eppo")
text = text.replace(
    'std::cout << "EpochRunner v0.5.0 concurrency, gait, and rig-edit tests passed\\n";',
    'std::cout << "EpochRunner v0.6.0 procedural course, recovery, concurrency, gait, and rig-edit tests passed\\n";',
)
tests.write_text(text, encoding="utf-8")

missions = root / "MISSIONS.md"
text = missions.read_text(encoding="utf-8")
mission = """
## WALK-COURSE-001 — Procedural obstacle and recovery treadmill

**Status:** VERIFIED

The live and training environments must continuously expose motion through road dashes, numbered metre/mile markers, slopes, descents, hills, rocks, hurdles, low bars, moving hazards, and thrown projectiles. Course progression must continue at a bounded treadmill pace even when the creature produces no forward translation.

Disturbances open a timed recovery objective. Improving uprightness and regaining supported balance earns reward; failing to recover before the bounded window closes is penalized.

**Acceptance:**

- Course progress advances with zero forward creature motion.
- Inclines, declines, plateaus, hills, and uneven terrain remain continuous.
- Every physical obstacle class is generated procedurally and appears in observations.
- Road lines and numbered distance markers make course motion visible.
- Recovery state, attempts, and successes are visible in the live view.
- Core tests verify course advancement, terrain movement, obstacle diversity, and finite rewards.

"""
if "## WALK-COURSE-001" not in text:
    warning = "## Current warning"
    index = text.find(warning)
    if index < 0:
        text += mission
    else:
        text = text[:index] + mission + text[index:]
text = text.replace(
    "EpochRunner v0.5.0 passed its Windows build",
    "EpochRunner v0.6.0 adds the verified procedural obstacle/recovery treadmill. EpochRunner v0.5.0 passed its Windows build",
)
missions.write_text(text, encoding="utf-8")

readme = root / "README.md"
text = readme.read_text(encoding="utf-8")
if "## Procedural obstacle and recovery treadmill" not in text:
    insert = """
## Procedural obstacle and recovery treadmill

Version 0.6.0 continuously advances a bounded training course even when the creature does not translate. The course cycles through flat road, inclines, plateaus, declines, hills, uneven terrain, rocks, hurdles, low bars, moving hazards, and thrown projectiles. Road dashes and numbered metre/mile markers expose movement clearly in the live view.

Obstacle impacts and large balance errors start a timed recovery objective. Policies receive extra reward for restoring upright supported balance and a penalty when recovery times out.

"""
    marker = "## Default workflow"
    position = text.find(marker)
    if position < 0:
        text = insert + text
    else:
        text = text[:position] + insert + text[position:]
text = text.replace("Version 0.5.0", "Version 0.6.0")
readme.write_text(text, encoding="utf-8")
