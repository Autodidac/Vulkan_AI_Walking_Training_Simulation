from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"missing target in {path}: {old[:120]!r}")
    write(path, text.replace(old, new, 1))


def replace_regex(path: str, pattern: str, replacement: str) -> None:
    text = read(path)
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(f"regex matched {count} times in {path}: {pattern[:100]!r}")
    write(path, updated)


replace_once(
    "src/simulation.hpp",
    """        case CourseStage::duck_press:
            return duck_seconds >= 0.50f && obstacles_passed >= 1u;""",
    """        case CourseStage::duck_press:
            return duck_seconds >= 0.50f && obstacles_passed >= 2u;""",
)

replace_regex(
    "src/simulation.hpp",
    r"\[\[nodiscard\]\] inline float duck_obstacle_approach_weight\(float distance_ahead\) noexcept\n    \{.*?\n    \}",
    """[[nodiscard]] inline float duck_obstacle_approach_weight(float distance_ahead) noexcept
    {
        if (distance_ahead <= -1.25f || distance_ahead >= 8.0f)
            return 0.0f;
        if (distance_ahead <= 2.25f)
            return 1.0f;
        return clamp((8.0f - distance_ahead) / 5.75f, 0.0f, 1.0f);
    }""",
)

replace_regex(
    "src/simulation.hpp",
    r"\[\[nodiscard\]\] float course_speed\(\) const noexcept\n        \{.*?\n        \}",
    """[[nodiscard]] float course_speed() const noexcept
        {
            if (course_stage_ == CourseStage::balance
                || course_stage_ == CourseStage::ramps
                || course_stage_ == CourseStage::duck_bars)
                return 0.0f;
            if (course_stage_ == CourseStage::duck_press)
                return duck_press_completed_ ? 0.58f + course_difficulty_ * 0.12f : 0.0f;
            if (course_stage_ == CourseStage::uneven)
                return 0.82f + course_difficulty_ * 0.88f;
            if (course_stage_ == CourseStage::hurdles)
                return 1.05f + course_difficulty_ * 0.95f;
            return 1.20f + course_difficulty_ * 1.05f;
        }""",
)

replace_once(
    "src/simulation.hpp",
    """        void solve_distance(const DistanceConstraint& constraint) noexcept;
        void stabilize_passive_appendages() noexcept;""",
    """        void solve_distance(const DistanceConstraint& constraint) noexcept;
        void separate_support_clusters() noexcept;
        void stabilize_passive_appendages() noexcept;""",
)

replace_regex(
    "src/simulation.cpp",
    r"    CreatureBlueprint CreatureBlueprint::chicken\(\)\n    \{.*?\n    \}\n\n    CreatureBlueprint CreatureBlueprint::biped",
    """    CreatureBlueprint CreatureBlueprint::chicken()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.52f }, { 0.72f, 2.62f },
            { 0.88f, 3.12f }, { 1.08f, 3.52f }, { 1.42f, 3.48f },
            { -0.92f, 2.76f }, { -1.30f, 2.98f },
            { -0.28f, 1.50f }, { -0.38f, 0.30f },
            { 0.34f, 1.52f }, { 0.46f, 0.30f },
            { 0.08f, 2.66f }
        };
        result.radii = {
            0.42f, 0.38f, 0.23f, 0.28f, 0.11f,
            0.24f, 0.13f, 0.18f, 0.14f, 0.18f, 0.14f, 0.26f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.98f },
            { 2, 3, 0.0f, 0.98f }, { 3, 4, 0.0f, 0.94f },
            { 0, 5, 0.0f, 0.92f }, { 5, 6, 0.0f, 0.88f },
            { 0, 11, 0.0f, 0.82f }, { 1, 11, 0.0f, 0.82f },
            { 0, 7, 0.0f, 1.0f }, { 7, 8, 0.0f, 1.0f },
            { 0, 9, 0.0f, 1.0f }, { 9, 10, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 7 }, MotorConstraint{ 0, 7, 8 },
            MotorConstraint{ 1, 0, 9 }, MotorConstraint{ 0, 9, 10 }
        };
        result.active_motor_count = 4;
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 3;
        result.left_contact_node = 8;
        result.right_contact_node = 10;
        add_passive_feet(result, 0.15f, 0.25f);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 32.0f, 54.0f, 0.042f, 0.047f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::biped""",
)

replace_once(
    "src/simulation.cpp",
    """                const float radius = ankle < rig.radii.size()
                    ? clamp(rig.radii[ankle] * 0.64f, 0.090f, 0.125f) : 0.105f;""",
    """                const float radius = ankle < rig.radii.size()
                    ? clamp(rig.radii[ankle] * 0.50f, 0.070f, 0.095f) : 0.082f;""",
)
replace_once(
    "src/simulation.cpp",
    """                rig.nodes.push_back({
                    ankle_position.x - heel_reach * 0.55f,
                    ankle_position.y - 0.185f
                });""",
    """                const float side = ankle_position.x < rig.nodes[rig.root_node].x ? -1.0f : 1.0f;
                rig.nodes.push_back({
                    ankle_position.x - side * heel_reach * 0.20f,
                    ankle_position.y - 0.205f
                });""",
)
replace_once(
    "src/simulation.cpp",
    """                rig.nodes.push_back({
                    ankle_position.x + toe_reach * 0.72f,
                    ankle_position.y - 0.195f
                });""",
    """                rig.nodes.push_back({
                    ankle_position.x + side * toe_reach * 0.78f,
                    ankle_position.y - 0.210f
                });""",
)

replace_once(
    "src/simulation.cpp",
    """    bool Environment::body_integrity_valid() const noexcept""",
    """    void Environment::separate_support_clusters() noexcept
    {
        auto support_nodes = [&](bool left)
        {
            std::array<std::uint16_t, 16> nodes{};
            std::size_t count = 0;
            const std::uint16_t primary = left
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            if (valid_node(primary))
                nodes[count++] = primary;
            const auto& additional = left
                ? blueprint_.additional_left_contact_nodes
                : blueprint_.additional_right_contact_nodes;
            for (const std::uint16_t node : additional)
            {
                if (count < nodes.size() && valid_node(node))
                    nodes[count++] = node;
            }
            return std::pair{ nodes, count };
        };

        const auto [left_nodes, left_count] = support_nodes(true);
        const auto [right_nodes, right_count] = support_nodes(false);
        for (std::size_t li = 0; li < left_count; ++li)
        {
            Particle& left = particles_[left_nodes[li]];
            for (std::size_t ri = 0; ri < right_count; ++ri)
            {
                Particle& right = particles_[right_nodes[ri]];
                const float minimum_gap = left.radius + right.radius + 0.055f;
                const float horizontal = right.position.x - left.position.x;
                if (horizontal >= minimum_gap)
                    continue;
                const float correction = (minimum_gap - horizontal) * 0.5f;
                left.position.x -= correction;
                left.previous.x -= correction * 0.35f;
                right.position.x += correction;
                right.previous.x += correction * 0.35f;
            }
        }
    }

    bool Environment::body_integrity_valid() const noexcept""",
)
replace_once(
    "src/simulation.cpp",
    """            solve_ground(dt);
            solve_course();""",
    """            solve_ground(dt);
            solve_course();
            separate_support_clusters();""",
)

replace_regex(
    "src/simulation.cpp",
    r"        if \(course_stage_ == CourseStage::duck_press\)\n        \{.*?\n            return;\n        \}\n        const int first_sequence",
    """        if (course_stage_ == CourseStage::duck_press)
        {
            const float rest_head_top = valid_node(blueprint_.head_node)
                ? blueprint_.nodes[blueprint_.head_node].y
                    + particles_[blueprint_.head_node].radius
                : 4.30f;
            if (!duck_press_completed_)
            {
                float minimum_x = blueprint_.nodes.empty() ? -0.5f : blueprint_.nodes.front().x;
                float maximum_x = minimum_x;
                for (const Vec2 node : blueprint_.nodes)
                {
                    minimum_x = std::min(minimum_x, node.x);
                    maximum_x = std::max(maximum_x, node.x);
                }
                const float half_width = clamp(
                    (maximum_x - minimum_x) * 0.42f + 0.45f, 0.82f, 1.20f);
                const DuckPressProfile profile = duck_press_profile(
                    elapsed_seconds_, course_difficulty_, rest_head_top);
                constexpr float half_height = 0.14f;
                course_features_.push_back({
                    CourseFeatureKind::duck_press,
                    { root_x, profile.bottom_y + half_height },
                    { half_width, half_height }, 0.0f,
                    { 0.0f, profile.vertical_velocity }, -2
                });
                return;
            }

            constexpr float cycle = 11.0f;
            const float travel_time = std::max(0.0f, elapsed_seconds_ - 9.0f);
            float local = std::fmod(travel_time * course_speed(), cycle);
            if (local < 0.0f)
                local += cycle;
            float x = root_x + 7.5f - local;
            int sequence = 100 + static_cast<int>(std::floor(
                travel_time * course_speed() / cycle));
            if (x < root_x - 1.5f)
            {
                x += cycle;
                ++sequence;
            }
            const float clearance = rest_head_top
                - (0.58f + course_difficulty_ * 0.08f);
            course_features_.push_back({
                CourseFeatureKind::overhead_bar,
                { x, clearance + 0.12f }, { 0.88f, 0.12f }, 0.0f,
                { -course_speed(), 0.0f }, sequence
            });
            return;
        }
        const int first_sequence""",
)

replace_once(
    "src/simulation.cpp",
    """        if (duck_press_max_penetration_ > 0.18f)
            invalidate(InvalidMotion::press_penetration);""",
    """        if (duck_press_max_penetration_ > 0.24f)
            invalidate(InvalidMotion::press_penetration);""",
)

replace_once(
    "src/simulation.cpp",
    """        const float torso_swing_penalty = course_stage_ == CourseStage::duck_press
            ? std::max(0.0f, std::abs(torso_turn_speed_) - 0.35f) * 0.018f : 0.0f;""",
    """        const float torso_swing_penalty = course_stage_ == CourseStage::duck_press
            ? std::max(0.0f, std::abs(torso_turn_speed_) - 0.22f) * 0.030f : 0.0f;""",
)
replace_once(
    "src/simulation.cpp",
    """        if (course_stage_ == CourseStage::duck_press && duck_obstacle_weight_ > 0.10f
            && std::abs(torso_turn_speed_) > 1.10f)""",
    """        if (course_stage_ == CourseStage::duck_press && duck_obstacle_weight_ > 0.10f
            && std::abs(torso_turn_speed_) > 0.85f)""",
)

app = read("src/app.cpp")
app = app.replace(
    """                    add_rounded_rect(canvas, rect, 4.0f,
                        feature.kind == sim::CourseFeatureKind::hurdle ? yellow : accent_dim,
                        feature.kind == sim::CourseFeatureKind::hurdle ? yellow : accent, 1.0f);""",
    """                    const Color feature_fill = feature.kind == sim::CourseFeatureKind::hurdle
                        ? yellow : feature.kind == sim::CourseFeatureKind::duck_press
                            ? rgb(0x315b70) : accent_dim;
                    const Color feature_outline = feature.kind == sim::CourseFeatureKind::hurdle
                        ? yellow : accent;
                    add_rounded_rect(canvas, rect, 4.0f,
                        feature_fill, feature_outline, 1.0f);""",
)
write("src/app.cpp", app)

replace_once(
    "tests/core_tests.cpp",
    """        static void qualify_stable_stance(Environment& environment) noexcept
        {""",
    """        static void force_fused_supports(Environment& environment) noexcept
        {
            if (!environment.valid_node(environment.blueprint_.left_contact_node)
                || !environment.valid_node(environment.blueprint_.right_contact_node))
                return;
            const float center = 0.5f * (
                environment.particles_[environment.blueprint_.left_contact_node].position.x
                + environment.particles_[environment.blueprint_.right_contact_node].position.x);
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (!environment.blueprint_.is_support_seed(index))
                    continue;
                environment.particles_[index].position.x = center;
                environment.particles_[index].previous.x = center;
            }
        }

        static void separate_supports(Environment& environment) noexcept
        {
            environment.separate_support_clusters();
        }

        static float primary_support_gap(const Environment& environment) noexcept
        {
            return environment.particles_[environment.blueprint_.right_contact_node].position.x
                - environment.particles_[environment.blueprint_.left_contact_node].position.x;
        }

        static void complete_duck_press(Environment& environment) noexcept
        {
            environment.duck_press_completed_ = true;
            environment.elapsed_seconds_ = 10.0f;
            environment.rebuild_course_features();
        }

        static void qualify_stable_stance(Environment& environment) noexcept
        {""",
)
replace_once(
    "tests/core_tests.cpp",
    """    sim::Environment press_environment(sim::CreatureBlueprint::humanoid(), 17);""",
    """    const sim::CreatureBlueprint chicken = sim::CreatureBlueprint::chicken();
    require(chicken.head_node < chicken.nodes.size()
            && chicken.nodes[chicken.head_node].x > chicken.nodes[chicken.torso_node].x
            && chicken.nodes[chicken.head_node].y > chicken.nodes[chicken.torso_node].y,
        "chicken preset does not have a raised forward bird head");
    require(chicken.nodes[6].x < chicken.nodes[chicken.root_node].x - 1.0f
            && chicken.nodes[4].x > chicken.nodes[chicken.head_node].x,
        "chicken preset lacks a distinct tail and beak");

    sim::Environment fused_feet(sim::CreatureBlueprint::humanoid(), 19);
    sim::EnvironmentTestAccess::force_fused_supports(fused_feet);
    sim::EnvironmentTestAccess::separate_supports(fused_feet);
    require(sim::EnvironmentTestAccess::primary_support_gap(fused_feet) > 0.18f,
        "left and right feet can remain fused into one support blob");

    sim::Environment press_environment(sim::CreatureBlueprint::humanoid(), 17);""",
)
replace_once(
    "tests/core_tests.cpp",
    """    require(sim::EnvironmentTestAccess::press_collision_resolves_below(press_environment),
        "duck press clips through the model instead of resolving below the platen");""",
    """    require(sim::EnvironmentTestAccess::press_collision_resolves_below(press_environment),
        "duck press clips through the model instead of resolving below the platen");
    sim::EnvironmentTestAccess::complete_duck_press(press_environment);
    require(!press_environment.course_features().empty()
            && press_environment.course_features().front().kind == sim::CourseFeatureKind::overhead_bar,
        "duck press never advances to the later moving low-bar lesson");
    const sim::CourseFeature& later_bar = press_environment.course_features().front();
    require(later_bar.center.x
            - press_environment.particles()[press_environment.blueprint().root_node].position.x >= 6.0f,
        "later low bar starts too close for a meaningful crouch response");
    require(later_bar.half_extent.x > later_bar.half_extent.y * 5.0f,
        "later low bar is not horizontal or is effectively an undodgeable wall");""",
)

tests = read("tests/core_tests.cpp")
tests = tests.replace(
    "sim::stage_skill_evidence(sim::CourseStage::duck_press, 0u, 0.6f, 0u, 0.0f, 0u, 1u)",
    "sim::stage_skill_evidence(sim::CourseStage::duck_press, 0u, 0.6f, 0u, 0.0f, 0u, 2u)",
)
write("tests/core_tests.cpp", tests)

mission = read("missioncache.md")
addition = """

### WALK-FEET-047 — Prevent fused support plates
**Status:** IN PROGRESS

Left and right feet use smaller outward-facing plates plus a solver separation constraint. They may contact the ground together but cannot occupy the same support blob.

### WALK-CHICKEN-048 — Rebuild the chicken preset as a bird
**Status:** IN PROGRESS

The preset has a horizontal body, raised neck and head, visible beak, tail, two articulated legs, separate feet, and only leg motors. A generic upright biped does not satisfy this mission.

### WALK-DUCK-049 — Complete two-part duck learning
**Status:** IN PROGRESS

The rig must first survive the stationary compression platen, hold a leg-driven crouch, recover, then pass a horizontal moving low bar that begins at least 6 m ahead. Stage completion requires evidence from both obstacles.

### WALK-OBSERVE-050 — Make the duck obstacle learnable
**Status:** IN PROGRESS

The policy receives the platen/low-bar geometry early enough to act, teacher assistance remains leg-only, torso-axis swinging is penalized, and the later bar cannot appear as an unavoidable vertical wall.
"""
if "### WALK-FEET-047" not in mission:
    anchor = mission.index("## v0.7.3 live-runtime correction")
    mission = mission[:anchor] + addition + "\n" + mission[anchor:]
write("missioncache.md", mission)

Path(__file__).unlink()
print("fixed fused feet, ankle clearance, chicken anatomy, and the two-part duck curriculum")
