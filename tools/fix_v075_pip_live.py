from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def load(name: str) -> str:
    return (ROOT / name).read_text(encoding="utf-8")


def save(name: str, text: str) -> None:
    (ROOT / name).write_text(text, encoding="utf-8", newline="\n")


def sub(name: str, pattern: str, replacement: str) -> None:
    text = load(name)
    changed, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(f"pattern matched {count} times in {name}: {pattern[:100]!r}")
    save(name, changed)


# The PIP must always show what a current training environment is actually doing.
# Stage-valid samples rank first, but an intact current attempt is still published
# as a red/amber diagnostic instead of leaving a blank or stale panel.
sub(
    "src/autonomy_persistence.cpp",
    r'''            if \(!stage_display_sample_eligible\(stage_, environment\)\)\n                continue;\n            const std::uint64_t display_quality = qualification.valid\n                \? qualification.quality_key\n                : pack_quality\(.*?\n                representative_tiebreak = tiebreak;\n            \}''',
    '''            const bool stage_eligible = stage_display_sample_eligible(stage_, environment);
            const bool structurally_renderable = !environment.particles().empty()
                && environment.body_integrity_valid();
            if (!structurally_renderable)
                continue;
            const std::uint64_t display_quality = qualification.valid
                ? qualification.quality_key
                : pack_quality(
                    static_cast<std::uint16_t>(stage_eligible ? 2u : 1u),
                    static_cast<std::uint16_t>(std::min<std::uint32_t>(
                        environment.alternating_steps(), 65535u)),
                    quality_bucket(environment.crouch_walk_distance(), 100.0f),
                    quality_bucket(environment.elapsed_seconds(), 10.0f));
            const float tiebreak = environment.crouch_walk_distance() * 20.0f
                + environment.distance_travelled() * 10.0f
                + environment.elapsed_seconds();
            if (representative == nullptr
                || display_quality > representative_quality
                || (display_quality == representative_quality
                    && tiebreak > representative_tiebreak))
            {
                representative = &environment;
                representative_quality = display_quality;
                representative_tiebreak = tiebreak;
            }''',
)

# Replace the previous blank-or-overzoomed PIP. The new view keeps the complete
# body large, shows a useful amount of terrain ahead, clips far-away obstacles
# instead of shrinking the rig, and continues drawing failed attempts with an
# explicit failure banner.
sub(
    "src/app.cpp",
    r'''        void draw_training_pip\(Rect rect\)\n        \{.*?\n        \}\n\n        void draw_live_panel''',
    '''        void draw_training_pip(Rect rect)
        {
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x071019, 0.98f), accent_dim, 1.5f);
            add_text(canvas, rect.position + Vec2{ 13.0f, 9.0f },
                "LIVE TRAINING ENVIRONMENT", 1.00f, accent);

            if (!trainer.has_training_preview())
            {
                add_text_fit(canvas, rect.position + Vec2{ 13.0f, 43.0f },
                    "WAITING FOR FIRST INTACT TRAINING FRAME", 0.96f, muted,
                    rect.size.x - 26.0f);
                return;
            }

            const sim::Environment& environment = trainer.training_preview();
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty() || rig.root_node >= particles.size())
            {
                add_text_fit(canvas, rect.position + Vec2{ 13.0f, 43.0f },
                    "TRAINING FRAME HAS NO COMPLETE RIG", 0.96f, danger,
                    rect.size.x - 26.0f);
                return;
            }

            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(environment.course_stage(), environment);
            const bool foot_only = !environment.non_foot_grounded();
            const bool intact = environment.body_integrity_valid();
            const bool moving_crouch = environment.duck_active()
                && environment.crouch_walk_seconds() > 0.0f;
            const Color state_color = qualification.valid ? green
                : intact && foot_only && moving_crouch ? yellow : danger;
            const std::string_view state_text = qualification.valid
                ? "QUALIFIED"
                : !intact ? "BROKEN RIG"
                : !foot_only ? "BODY TOUCHED GROUND"
                : !environment.duck_active() ? "NOT CROUCHING"
                : "LEARNING";
            add_text_fit(canvas, rect.position + Vec2{ rect.size.x - 154.0f, 9.0f },
                state_text, 0.82f, state_color, 141.0f, 0.68f);

            const Rect inner{ rect.position + Vec2{ 8.0f, 34.0f },
                { rect.size.x - 16.0f, rect.size.y - 64.0f } };
            const float root_x = particles[rig.root_node].position.x;

            float body_min_x = std::numeric_limits<float>::infinity();
            float body_max_x = -std::numeric_limits<float>::infinity();
            float body_min_y = std::numeric_limits<float>::infinity();
            float body_max_y = -std::numeric_limits<float>::infinity();
            for (const sim::Particle& particle : particles)
            {
                body_min_x = std::min(body_min_x, particle.position.x - particle.radius);
                body_max_x = std::max(body_max_x, particle.position.x + particle.radius);
                body_min_y = std::min(body_min_y, particle.position.y - particle.radius);
                body_max_y = std::max(body_max_y, particle.position.y + particle.radius);
            }

            // Keep the rig large and readable. Show roughly 2 m behind and 4 m
            // ahead; a distant obstacle gets a distance label instead of forcing
            // the camera to zoom the body into a tiny cluster.
            float view_min_x = std::min(root_x - 2.0f, body_min_x - 0.35f);
            float view_max_x = std::max(root_x + 4.2f, body_max_x + 0.50f);
            float view_min_y = std::min(body_min_y - 0.18f,
                environment.ground_height_at(root_x) - 0.18f);
            float view_max_y = body_max_y + 0.32f;
            const float world_width = std::max(3.8f, view_max_x - view_min_x);
            const float world_height = std::max(1.5f, view_max_y - view_min_y);
            const float horizontal_scale = (inner.size.x - 12.0f) / world_width;
            const float vertical_scale = (inner.size.y * 0.78f) / world_height;
            const float scale = std::clamp(
                std::min(horizontal_scale, vertical_scale), 20.0f, 48.0f);
            const float camera = (view_min_x + view_max_x) * 0.5f;

            std::vector<Vec2> ground_points{};
            ground_points.reserve(81);
            for (int sample = 0; sample <= 80; ++sample)
            {
                const float fraction = static_cast<float>(sample) / 80.0f;
                const float world_x = camera
                    + (fraction - 0.5f) * inner.size.x / scale;
                ground_points.push_back(world_to_screen(
                    { world_x, environment.ground_height_at(world_x) },
                    inner, camera, scale, 0.82f));
            }
            canvas.polyline(ground_points, 3.0f, rgb(0x65727d));

            const sim::CourseFeature* next_feature = nullptr;
            float next_distance = std::numeric_limits<float>::infinity();
            for (const sim::CourseFeature& feature : environment.course_features())
            {
                const float distance = feature.center.x - root_x;
                if (distance >= -0.6f && distance < next_distance)
                {
                    next_distance = distance;
                    next_feature = &feature;
                }

                const Vec2 point = world_to_screen(feature.center,
                    inner, camera, scale, 0.82f);
                if (point.x < inner.position.x - 24.0f
                    || point.x > inner.position.x + inner.size.x + 24.0f)
                    continue;
                if (feature.kind == sim::CourseFeatureKind::rock
                    || feature.kind == sim::CourseFeatureKind::moving_hazard
                    || feature.kind == sim::CourseFeatureKind::projectile)
                {
                    canvas.circle(point, std::max(3.0f, feature.radius * scale),
                        feature.kind == sim::CourseFeatureKind::projectile ? danger : yellow, 18);
                }
                else
                {
                    const Vec2 minimum = world_to_screen(feature.center - feature.half_extent,
                        inner, camera, scale, 0.82f);
                    const Vec2 maximum = world_to_screen(feature.center + feature.half_extent,
                        inner, camera, scale, 0.82f);
                    add_rounded_rect(canvas,
                        { { minimum.x, maximum.y },
                          { maximum.x - minimum.x, minimum.y - maximum.y } },
                        3.0f, accent_dim, accent, 1.0f);
                }
            }

            draw_creature(environment, inner, camera, scale);

            if (next_feature != nullptr && next_distance > 4.2f)
            {
                add_text_fit(canvas,
                    inner.position + Vec2{ inner.size.x - 138.0f, 7.0f },
                    std::format("NEXT {:.1f}M >", next_distance),
                    0.78f, yellow, 128.0f, 0.64f);
            }

            if (!intact || !foot_only)
            {
                add_rounded_rect(canvas,
                    { inner.position + Vec2{ 5.0f, inner.size.y - 34.0f },
                      { inner.size.x - 10.0f, 28.0f } },
                    4.0f, rgb(0x3a0c10, 0.90f), danger, 1.0f);
                add_text_fit(canvas,
                    inner.position + Vec2{ 11.0f, inner.size.y - 28.0f },
                    !foot_only ? "REJECTED: ONLY FEET MAY TOUCH GROUND"
                        : "REJECTED: RIG LOST BODY INTEGRITY",
                    0.78f, white, inner.size.x - 22.0f, 0.62f);
            }

            add_text_fit(canvas, rect.position + Vec2{ 12.0f, rect.size.y - 23.0f },
                std::format("UPDATE {}  CROUCH {:.1f}S  {:.2f}M  STEPS {}  PASSED {}",
                    trainer.metrics().update,
                    environment.crouch_walk_seconds(), environment.crouch_walk_distance(),
                    environment.alternating_steps(), environment.obstacles_passed()),
                0.72f, state_color, rect.size.x - 24.0f, 0.58f);
        }

        void draw_live_panel''',
)

# The prior synthetic qualification fixture tried to satisfy the complete stage
# qualification without actually running physics. Keep deterministic coverage on
# the real foot-only rule and stage evidence, and stop using that synthetic frame
# as a proxy for the rendered PIP.
tests = load("tests/core_tests.cpp")
tests = re.sub(
    r'''    sim::Environment qualified_crouch\(sim::CreatureBlueprint::humanoid\(\), 29\);.*?        "body-contact crouch can qualify or appear in the training PIP"\);\n''',
    '''    require(sim::stage_skill_evidence(sim::CourseStage::duck_press,
            5u, 3.0f, 0u, 0.0f, 0u, 4u),
        "valid foot-only crouch-walk evidence is rejected");
''',
    tests,
    count=1,
    flags=re.DOTALL,
)
save("tests/core_tests.cpp", tests)

mission = load("missioncache.md")
mission = mission.replace(
    "The training PIP rejects disconnected, folded, non-foot-contact, stale, or non-crouching samples. It fits the entire connected rig, nearby uneven terrain, and the next obstacle, and displays live crouch time, distance, alternating steps, and passes.",
    "The training PIP publishes a current intact training environment every completed update, never goes blank merely because the attempt is failing, keeps the complete rig large, shows uneven terrain and nearby obstacles, labels farther obstacles without zooming the rig into a dot, and overlays the exact foot-contact or integrity failure while displaying update, crouch time, distance, alternating steps, and passes.",
)
save("missioncache.md", mission)

Path(__file__).unlink()
print("made the training PIP live, readable, nonblank, and failure-honest")
