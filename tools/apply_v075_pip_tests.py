from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def load(name: str) -> str:
    return (ROOT / name).read_text(encoding="utf-8")


def save(name: str, text: str) -> None:
    (ROOT / name).write_text(text, encoding="utf-8", newline="\n")


def replace(name: str, old: str, new: str) -> None:
    text = load(name)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"missing target in {name}: {old[:140]!r}")
    save(name, text.replace(old, new, 1))


def sub(name: str, pattern: str, replacement: str) -> None:
    text = load(name)
    changed, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(f"pattern matched {count} times in {name}: {pattern[:120]!r}")
    save(name, changed)


replace("src/autonomy_persistence.cpp",
'''            if (!qualification.valid
                || !stage_display_sample_eligible(stage_, environment))
                continue;
            const float tiebreak = environment.distance_travelled() * 10.0f
                + environment.elapsed_seconds();
            if (representative == nullptr
                || qualification.quality_key > representative_quality
                || (qualification.quality_key == representative_quality
                    && tiebreak > representative_tiebreak))
            {
                representative = &environment;
                representative_quality = qualification.quality_key;
                representative_tiebreak = tiebreak;
            }''',
'''            if (!stage_display_sample_eligible(stage_, environment))
                continue;
            const std::uint64_t display_quality = qualification.valid
                ? qualification.quality_key
                : pack_quality(
                    static_cast<std::uint16_t>(std::min<std::uint32_t>(
                        environment.alternating_steps(), 65535u)),
                    static_cast<std::uint16_t>(std::min<std::uint32_t>(
                        environment.obstacles_passed(), 65535u)),
                    quality_bucket(environment.crouch_walk_distance(), 100.0f),
                    quality_bucket(environment.crouch_walk_seconds(), 100.0f));
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
            }''')

sub("src/app.cpp",
    r'''        void draw_training_pip\(Rect rect\)\n        \{.*?\n        \}\n\n        void draw_live_panel''',
'''        void draw_training_pip(Rect rect)
        {
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x071019, 0.98f), accent_dim, 1.5f);
            if (!trainer.has_training_preview())
            {
                add_text(canvas, rect.position + Vec2{ 13.0f, 10.0f },
                    "BEST VALID CROUCH-WALK SAMPLE", 1.03f, accent);
                add_text_fit(canvas, rect.position + Vec2{ 13.0f, 44.0f },
                    "NO FOOT-ONLY MOVING CROUCH SAMPLE YET", 1.00f, muted,
                    rect.size.x - 26.0f);
                return;
            }

            const sim::Environment& environment = trainer.training_preview();
            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(environment.course_stage(), environment);
            add_text(canvas, rect.position + Vec2{ 13.0f, 10.0f },
                qualification.valid ? "STAGE-QUALIFIED CROUCH WALK"
                    : "BEST VALID IN-PROGRESS CROUCH WALK",
                1.00f, qualification.valid ? green : accent);
            const Rect inner{ rect.position + Vec2{ 9.0f, 35.0f },
                { rect.size.x - 18.0f, rect.size.y - 61.0f } };
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty() || rig.root_node >= particles.size())
                return;
            if (!environment.body_integrity_valid() || environment.non_foot_grounded()
                || !environment.duck_active())
            {
                add_text_fit(canvas, rect.position + Vec2{ 13.0f, 44.0f },
                    "SAMPLE REJECTED - CROUCH MUST STAY ON FEET", 0.98f, danger,
                    rect.size.x - 26.0f);
                return;
            }

            float minimum_x = std::numeric_limits<float>::infinity();
            float maximum_x = -std::numeric_limits<float>::infinity();
            float minimum_y = std::numeric_limits<float>::infinity();
            float maximum_y = -std::numeric_limits<float>::infinity();
            for (const sim::Particle& particle : particles)
            {
                minimum_x = std::min(minimum_x, particle.position.x - particle.radius);
                maximum_x = std::max(maximum_x, particle.position.x + particle.radius);
                minimum_y = std::min(minimum_y, particle.position.y - particle.radius);
                maximum_y = std::max(maximum_y, particle.position.y + particle.radius);
            }

            const float root_x = particles[rig.root_node].position.x;
            const sim::CourseFeature* next_feature = nullptr;
            float next_dx = std::numeric_limits<float>::infinity();
            for (const sim::CourseFeature& feature : environment.course_features())
            {
                const float dx = feature.center.x - root_x;
                if (dx >= -0.5f && dx < next_dx)
                {
                    next_dx = dx;
                    next_feature = &feature;
                }
            }
            if (next_feature != nullptr && next_dx <= 8.0f)
            {
                minimum_x = std::min(minimum_x,
                    next_feature->center.x - sim::course_feature_half_width(*next_feature) - 0.30f);
                maximum_x = std::max(maximum_x,
                    next_feature->center.x + sim::course_feature_half_width(*next_feature) + 0.30f);
                maximum_y = std::max(maximum_y,
                    sim::course_feature_top(*next_feature) + 0.25f);
            }
            minimum_x -= 0.35f;
            maximum_x += 0.35f;
            const float camera = (minimum_x + maximum_x) * 0.5f;
            const float ground = environment.ground_height_at(camera);
            minimum_y = std::min(minimum_y, ground - 0.15f);
            const float world_width = std::max(1.20f, maximum_x - minimum_x);
            const float world_height = std::max(1.20f, maximum_y - minimum_y);
            const float horizontal_scale = (inner.size.x - 18.0f) / world_width;
            const float vertical_scale = (inner.size.y * 0.78f) / world_height;
            const float scale = std::clamp(
                std::min(horizontal_scale, vertical_scale), 12.0f, 40.0f);

            std::vector<Vec2> ground_points{};
            ground_points.reserve(65);
            for (int sample = 0; sample <= 64; ++sample)
            {
                const float fraction = static_cast<float>(sample) / 64.0f;
                const float world_x = camera
                    + (fraction - 0.5f) * inner.size.x / scale;
                ground_points.push_back(world_to_screen(
                    { world_x, environment.ground_height_at(world_x) },
                    inner, camera, scale, 0.80f));
            }
            canvas.polyline(ground_points, 3.0f, rgb(0x51606c));

            for (const sim::CourseFeature& feature : environment.course_features())
            {
                const Vec2 point = world_to_screen(feature.center, inner, camera, scale, 0.80f);
                if (point.x < inner.position.x - 20.0f
                    || point.x > inner.position.x + inner.size.x + 20.0f)
                    continue;
                if (feature.kind == sim::CourseFeatureKind::rock
                    || feature.kind == sim::CourseFeatureKind::moving_hazard
                    || feature.kind == sim::CourseFeatureKind::projectile)
                {
                    canvas.circle(point, feature.radius * scale,
                        feature.kind == sim::CourseFeatureKind::projectile ? danger : yellow, 18);
                }
                else
                {
                    const Vec2 minimum = world_to_screen(feature.center - feature.half_extent,
                        inner, camera, scale, 0.80f);
                    const Vec2 maximum = world_to_screen(feature.center + feature.half_extent,
                        inner, camera, scale, 0.80f);
                    add_rounded_rect(canvas,
                        { { minimum.x, maximum.y },
                          { maximum.x - minimum.x, minimum.y - maximum.y } },
                        3.0f, accent_dim, accent, 1.0f);
                }
            }
            draw_creature(environment, inner, camera, scale);
            add_text_fit(canvas, rect.position + Vec2{ 13.0f, rect.size.y - 22.0f },
                std::format("FOOT-ONLY  {:.1f}S  {:.2f}M  STEPS {}  PASSED {}",
                    environment.crouch_walk_seconds(), environment.crouch_walk_distance(),
                    environment.alternating_steps(), environment.obstacles_passed()),
                0.78f, green, rect.size.x - 26.0f, 0.66f);
        }

        void draw_live_panel''')

replace("tests/core_tests.cpp",
'''        static void complete_duck_press(Environment& environment) noexcept
        {
            environment.duck_press_completed_ = true;
            environment.elapsed_seconds_ = 10.0f;
            environment.rebuild_course_features();
        }''',
'''        static void complete_duck_press(Environment& environment) noexcept
        {
            environment.duck_press_completed_ = true;
            environment.duck_walk_started_seconds_ = 9.0f;
            environment.elapsed_seconds_ = 10.0f;
            environment.rebuild_course_features();
        }

        static void qualify_crouch_walk(Environment& environment) noexcept
        {
            qualify_stable_stance(environment);
            environment.duck_press_completed_ = true;
            environment.duck_walk_started_seconds_ = 1.0f;
            environment.duck_active_ = true;
            environment.duck_recovery_count_ = 1u;
            environment.duck_seconds_ = 3.0f;
            environment.crouch_walk_seconds_ = 2.5f;
            environment.crouch_walk_distance_ = 1.2f;
            environment.alternating_steps_ = 5u;
            environment.obstacles_passed_ = 4u;
        }

        static void force_non_foot_contact(Environment& environment) noexcept
        {
            environment.non_foot_grounded_ = true;
        }''')

replace("tests/core_tests.cpp",
'''    require(sim::powered_joint_launch(sim::CourseStage::ramps, 1.0f, 0.08f),''',
'''    require(!sim::duck_ground_contact_allowed(true, true)
            && sim::duck_ground_contact_allowed(true, false)
            && sim::duck_ground_contact_allowed(false, true),
        "foot-only duck contact rule is not strict");
    require(!sim::stage_skill_evidence(sim::CourseStage::duck_press,
            3u, 3.0f, 0u, 0.0f, 0u, 4u)
            && !sim::stage_skill_evidence(sim::CourseStage::duck_press,
                5u, 1.5f, 0u, 0.0f, 0u, 4u)
            && sim::stage_skill_evidence(sim::CourseStage::duck_press,
                5u, 3.0f, 0u, 0.0f, 0u, 4u),
        "duck stage can qualify without sustained crouch walking and obstacles");
    require(sim::powered_joint_launch(sim::CourseStage::ramps, 1.0f, 0.08f),''')

replace("tests/core_tests.cpp",
'''    sim::EnvironmentTestAccess::complete_duck_press(press_environment);
    require(!press_environment.course_features().empty()''',
'''    sim::EnvironmentTestAccess::complete_duck_press(press_environment);
    require(std::abs(press_environment.ground_height_at(0.0f)
            - press_environment.ground_height_at(1.25f)) > 0.005f,
        "crouch-walk lesson ground remains flat and stable");
    require(!press_environment.course_features().empty()''')

replace("tests/core_tests.cpp",
'''    require(later_bar.half_extent.x > later_bar.half_extent.y * 5.0f,
        "later low bar is not horizontal or is effectively an undodgeable wall");''',
'''    require(later_bar.half_extent.x > later_bar.half_extent.y * 5.0f,
        "later low bar is not horizontal or is effectively an undodgeable wall");
    sim::Environment qualified_crouch(sim::CreatureBlueprint::humanoid(), 29);
    qualified_crouch.set_course(sim::CourseStage::duck_press, 0.5f);
    sim::EnvironmentTestAccess::qualify_crouch_walk(qualified_crouch);
    require(rl::stage_motion_qualification(sim::CourseStage::duck_press,
            qualified_crouch).valid
            && rl::stage_display_sample_eligible(sim::CourseStage::duck_press,
                qualified_crouch),
        "valid foot-only crouch walk is rejected from qualification or PIP");
    sim::EnvironmentTestAccess::force_non_foot_contact(qualified_crouch);
    require(!rl::stage_motion_qualification(sim::CourseStage::duck_press,
            qualified_crouch).valid
            && !rl::stage_display_sample_eligible(sim::CourseStage::duck_press,
                qualified_crouch),
        "body-contact crouch can qualify or appear in the training PIP");''')

mission_path = ROOT / "missioncache.md"
mission = mission_path.read_text(encoding="utf-8")
entry = '''

## v0.7.5 crouch-walk and training-PIP correction

### WALK-DUCK-067 — Foot-only duck contact
**Status:** IN PROGRESS

Whenever a rig is in a recognised duck, only semantic foot/support nodes may touch terrain. Knee, hand, arm, torso, head, tail, or any other body contact immediately invalidates the attempt and cannot enter elite state, imitation state, or the training PIP.

### WALK-DUCK-068 — Replace static folding with crouch walking
**Status:** IN PROGRESS

The compression platen remains only the introductory lesson. Qualification then requires sustained low posture, alternating footfalls, actual forward crouch-walk distance, controlled foot-only support, and recovery. Ten thousand updates spent folding in place are not progress.

### WALK-TERRAIN-069 — Crouch obstacle avoidance on unstable ground
**Status:** IN PROGRESS

After compression and recovery, the rig must crouch-walk over uneven terrain while passing low bars and small ground hazards. Obstacles begin with useful reaction distance and stage completion requires multiple passes.

### WALK-PIP-070 — Show the real full crouch-walk attempt
**Status:** IN PROGRESS

The training PIP rejects disconnected, folded, non-foot-contact, stale, or non-crouching samples. It fits the entire connected rig, nearby uneven terrain, and the next obstacle, and displays live crouch time, distance, alternating steps, and passes.

### WALK-CHECKPOINT-071 — Invalidate the failed 10,000-update duck policy
**Status:** IN PROGRESS

The v0.7.5 training-semantics and autonomy-state versions prevent the prior static shoulder-folding duck policy from resuming as valid progress. New autosave paths start the corrected lesson cleanly.

### WALK-CHICKEN-072 — Preserve the working chicken
**Status:** IN PROGRESS

The current chicken anatomy and behavior are intentionally preserved in this pass. Crouch-walk and PIP changes must not regress the chicken preset.
'''
if "### WALK-DUCK-067" not in mission:
    mission += entry
mission_path.write_text(mission, encoding="utf-8", newline="\n")

(ROOT / "RELEASE_NOTES_v0.7.5.md").write_text('''# Runner v0.7.5

- Replaces static duck folding with a foot-only crouch-walk lesson.
- Adds uneven crouch terrain, low-bar avoidance, small ground hazards, and useful reaction distance.
- Invalidates any duck attempt where knees, hands, torso, head, tail, or other non-foot nodes touch terrain.
- Rebuilds the training PIP around a current valid moving crouch, the full connected rig, nearby terrain, and the next obstacle.
- Invalidates v0.7.4 duck checkpoints and autosaves so the failed 10,000-update policy cannot masquerade as progress.
- Preserves the current working chicken preset.
''', encoding="utf-8", newline="\n")

Path(__file__).unlink()
print("materialized v0.7.5 PIP, tests, mission ledger, and release notes")
