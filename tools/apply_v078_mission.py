from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def replace_once(path: str, old: str, new: str) -> None:
    file = ROOT / path
    text = file.read_text(encoding='utf-8')
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: expected one match, found {count}')
    file.write_text(text.replace(old, new, 1), encoding='utf-8')

# Rebuild the chicken around a true vertical semantic torso axis. The previous
# horizontal root->torso axis made the shared standing gate interpret a healthy
# bird body as permanently fallen, which matches the 0/6 live screenshot.
simulation = ROOT / 'src/simulation.cpp'
text = simulation.read_text(encoding='utf-8')
pattern = r"    CreatureBlueprint CreatureBlueprint::chicken\(\)\n    \{.*?\n    \}\n\n    CreatureBlueprint CreatureBlueprint::biped\(\)"
replacement = '''    CreatureBlueprint CreatureBlueprint::chicken()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.40f }, { 0.72f, 2.48f },
            { 0.98f, 3.04f }, { 1.18f, 3.50f }, { 1.54f, 3.46f },
            { -0.92f, 2.64f }, { -1.36f, 2.84f },
            { -0.42f, 1.42f }, { -0.58f, 0.28f },
            { 0.42f, 1.42f }, { 0.58f, 0.28f },
            { 0.02f, 3.12f }
        };
        result.radii = {
            0.42f, 0.38f, 0.23f, 0.28f, 0.11f,
            0.24f, 0.13f, 0.18f, 0.14f, 0.18f, 0.14f, 0.27f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.98f },
            { 2, 3, 0.0f, 0.98f }, { 3, 4, 0.0f, 0.94f },
            { 0, 2, 0.0f, 0.94f }, { 1, 3, 0.0f, 0.94f },
            { 0, 5, 0.0f, 0.92f }, { 5, 6, 0.0f, 0.88f },
            { 0, 6, 0.0f, 0.86f }, { 1, 5, 0.0f, 0.82f },
            { 0, 11, 0.0f, 0.96f }, { 1, 11, 0.0f, 0.92f },
            { 11, 2, 0.0f, 0.92f }, { 11, 3, 0.0f, 0.90f },
            { 11, 7, 0.0f, 0.84f }, { 11, 9, 0.0f, 0.84f },
            { 0, 7, 0.0f, 1.0f }, { 7, 8, 0.0f, 1.0f },
            { 0, 9, 0.0f, 1.0f }, { 9, 10, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 11, 0, 7 }, MotorConstraint{ 0, 7, 8 },
            MotorConstraint{ 11, 0, 9 }, MotorConstraint{ 0, 9, 10 }
        };
        result.active_motor_count = 4;
        result.root_node = 0;
        result.torso_node = 11;
        result.head_node = 3;
        result.left_contact_node = 8;
        result.right_contact_node = 10;
        add_passive_feet(result, 0.17f, 0.29f);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 28.0f, 52.0f, 0.038f, 0.044f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::biped()'''
text, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
if count != 1:
    raise RuntimeError(f'src/simulation.cpp: chicken replacement matched {count}')
simulation.write_text(text, encoding='utf-8')

# Use terrain enclosure rather than transient particle count as the no-escape
# criterion. Deposited sand becomes terrain, so a density-only test could never
# recognize a completed burial after the particles had correctly transformed.
replace_once('src/simulation.cpp', '''        const bool trapped = burial_depth_ > 0.32f
            && (obstruction_mask_ & 0x3u) == 0x3u
            && (left_density + right_density) > 1.4f;
''', '''        const float left_wall = ground_height_at(root.position.x - 0.70f)
            - (root.position.y - root.radius);
        const float right_wall = ground_height_at(root.position.x + 0.70f)
            - (root.position.y - root.radius);
        const bool trapped = burial_depth_ > 0.32f
            && (obstruction_mask_ & 0x3u) == 0x3u
            && left_wall > 0.18f && right_wall > 0.18f;
''')

# Add a procedural biomechanical layer derived from the live rig state. It uses
# no image dependency and remains subtle enough to preserve telemetry and hit
# testing while giving live, PIP, and rig-lab bodies the requested anatomy,
# neural-chip, node-network, and walker-study visual language.
app = ROOT / 'src/app.cpp'
app_text = app.read_text(encoding='utf-8')
needle = '''        void draw_creature(const sim::Environment& environment, Rect viewport, float camera,
            float scale, bool show_nodes = false)
'''
overlay = '''        void draw_biomechanical_overlay(const sim::Environment& environment,
            Rect viewport, float camera, float scale)
        {
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty())
                return;

            auto point = [&](std::size_t index)
            {
                return world_to_screen(particles[index].position, viewport, camera, scale);
            };
            auto ring = [&](Vec2 center, float radius, Color color)
            {
                std::array<Vec2, 33> points{};
                for (std::size_t index = 0; index < points.size(); ++index)
                {
                    const float angle = static_cast<float>(index)
                        / static_cast<float>(points.size() - 1u) * pi * 2.0f;
                    points[index] = center + Vec2{ std::cos(angle), std::sin(angle) } * radius;
                }
                canvas.polyline(points, 1.35f, color);
            };

            const float phase = session_runtime_seconds;
            const Vec2 ghost_offset{ 18.0f, -8.0f };
            for (std::size_t index = 0; index < rig.bones.size(); ++index)
            {
                const sim::DistanceConstraint& bone = rig.bones[index];
                if (bone.a >= particles.size() || bone.b >= particles.size())
                    continue;
                const Vec2 a = point(bone.a);
                const Vec2 b = point(bone.b);
                canvas.line(a, b, 1.25f, with_alpha(accent, 0.22f));
                canvas.line(a + ghost_offset, b + ghost_offset, 0.85f,
                    with_alpha(body_light, 0.10f));
                const float packet_phase = std::fmod(phase * 0.62f
                    + static_cast<float>(index) * 0.173f, 1.0f);
                const Vec2 packet = a + (b - a) * packet_phase;
                canvas.circle(packet, 2.2f, with_alpha(accent, 0.62f), 12);
            }

            for (std::size_t index = 0; index < particles.size(); ++index)
            {
                const bool semantic = index == rig.root_node || index == rig.torso_node
                    || index == rig.head_node || rig.is_support_seed(index);
                if (!semantic)
                    continue;
                const float radius = (index < rig.radii.size() ? rig.radii[index] : 0.15f)
                    * scale + 7.0f + std::sin(phase * 2.4f
                        + static_cast<float>(index)) * 1.8f;
                ring(point(index), radius, with_alpha(
                    index == rig.head_node ? body_light : accent, 0.42f));
            }

            if (rig.torso_node < particles.size())
            {
                const Vec2 center = point(rig.torso_node) + Vec2{ 24.0f, -28.0f };
                const Rect chip{ center - Vec2{ 18.0f, 12.0f }, { 36.0f, 24.0f } };
                add_rounded_rect(canvas, chip, 4.0f, rgb(0x091923, 0.72f),
                    with_alpha(accent, 0.62f), 1.0f);
                canvas.line(center - Vec2{ 8.0f, 0.0f }, center + Vec2{ 8.0f, 0.0f },
                    1.3f, with_alpha(accent, 0.72f));
                canvas.line(center - Vec2{ 0.0f, 7.0f }, center + Vec2{ 0.0f, 7.0f },
                    1.3f, with_alpha(accent, 0.72f));
                for (int pin = -1; pin <= 1; ++pin)
                {
                    const float y = center.y + static_cast<float>(pin) * 7.0f;
                    canvas.line({ chip.position.x - 5.0f, y }, { chip.position.x, y },
                        1.0f, with_alpha(body_light, 0.52f));
                    canvas.line({ chip.position.x + chip.size.x, y },
                        { chip.position.x + chip.size.x + 5.0f, y },
                        1.0f, with_alpha(body_light, 0.52f));
                }
            }
        }

        void draw_creature(const sim::Environment& environment, Rect viewport, float camera,
            float scale, bool show_nodes = false)
'''
if app_text.count(needle) != 1:
    raise RuntimeError(f'src/app.cpp: draw_creature insertion matched {app_text.count(needle)}')
app_text = app_text.replace(needle, overlay, 1)

point_block = '''            auto point = [&](std::size_t index)
            {
                return world_to_screen(particles[index].position, viewport, camera, scale);
            };
            for (const sim::DistanceConstraint& bone : rig.bones)
'''
point_replacement = '''            auto point = [&](std::size_t index)
            {
                return world_to_screen(particles[index].position, viewport, camera, scale);
            };
            draw_biomechanical_overlay(environment, viewport, camera, scale);
            for (const sim::DistanceConstraint& bone : rig.bones)
'''
if app_text.count(point_block) != 1:
    raise RuntimeError(f'src/app.cpp: live overlay call matched {app_text.count(point_block)}')
app_text = app_text.replace(point_block, point_replacement, 1)

lab_block = '''            auto preview_screen = [&](std::size_t index)
            {
                return world_to_screen(preview[index], viewport, 0.0f, scale);
            };
            for (const sim::DistanceConstraint& bone : blueprint.bones)
'''
lab_replacement = '''            auto preview_screen = [&](std::size_t index)
            {
                return world_to_screen(preview[index], viewport, 0.0f, scale);
            };
            for (std::size_t index = 0; index < blueprint.bones.size(); ++index)
            {
                const sim::DistanceConstraint& bone = blueprint.bones[index];
                if (bone.a >= preview.size() || bone.b >= preview.size())
                    continue;
                const Vec2 a = preview_screen(bone.a);
                const Vec2 b = preview_screen(bone.b);
                canvas.line(a, b, 1.25f, with_alpha(accent, 0.24f));
                const float packet_phase = std::fmod(session_runtime_seconds * 0.55f
                    + static_cast<float>(index) * 0.137f, 1.0f);
                canvas.circle(a + (b - a) * packet_phase, 2.4f,
                    with_alpha(body_light, 0.68f), 12);
            }
            for (std::size_t index = 0; index < preview.size(); ++index)
            {
                if (index != blueprint.root_node && index != blueprint.torso_node
                    && index != blueprint.head_node && !blueprint.is_support_seed(index))
                    continue;
                const Vec2 center = preview_screen(index);
                const float radius = 13.0f + std::sin(session_runtime_seconds * 2.2f
                    + static_cast<float>(index)) * 2.0f;
                std::array<Vec2, 25> halo{};
                for (std::size_t point_index = 0; point_index < halo.size(); ++point_index)
                {
                    const float angle = static_cast<float>(point_index)
                        / static_cast<float>(halo.size() - 1u) * pi * 2.0f;
                    halo[point_index] = center
                        + Vec2{ std::cos(angle), std::sin(angle) } * radius;
                }
                canvas.polyline(halo, 1.25f, with_alpha(accent, 0.44f));
            }
            for (const sim::DistanceConstraint& bone : blueprint.bones)
'''
if app_text.count(lab_block) != 1:
    raise RuntimeError(f'src/app.cpp: rig lab overlay matched {app_text.count(lab_block)}')
app_text = app_text.replace(lab_block, lab_replacement, 1)
app.write_text(app_text, encoding='utf-8')

# Extend deterministic acceptance with the exact live failure: a bird-shaped
# chicken must qualify strict balance on six seeded runs rather than being
# interpreted as a horizontal fallen humanoid.
core = ROOT / 'tests/core_tests.cpp'
core_text = core.read_text(encoding='utf-8')
chicken_anchor = '''    require(chicken.nodes[6].x < chicken.nodes[chicken.root_node].x - 1.0f
            && chicken.nodes[4].x > chicken.nodes[chicken.head_node].x,
        "chicken preset lacks a distinct tail and beak");

'''
chicken_tests = '''    require(chicken.nodes[6].x < chicken.nodes[chicken.root_node].x - 1.0f
            && chicken.nodes[4].x > chicken.nodes[chicken.head_node].x,
        "chicken preset lacks a distinct tail and beak");
    require(chicken.nodes[chicken.torso_node].y
            > chicken.nodes[chicken.root_node].y + 0.55f,
        "chicken semantic torso axis is not vertically load-bearing");
    require(std::ranges::any_of(chicken.bones, [&](const sim::DistanceConstraint& bone)
        {
            return (bone.a == chicken.root_node && bone.b == chicken.torso_node)
                || (bone.b == chicken.root_node && bone.a == chicken.torso_node);
        }) && std::ranges::any_of(chicken.bones, [&](const sim::DistanceConstraint& bone)
        {
            return (bone.a == chicken.torso_node && bone.b == chicken.head_node)
                || (bone.b == chicken.torso_node && bone.a == chicken.head_node);
        }),
        "chicken root, torso, and head do not form an intact semantic spine");

    {
        constexpr std::size_t chicken_seed_count = 6u;
        std::uint32_t valid_chicken_seeds = 0u;
        for (std::size_t seed_index = 0; seed_index < chicken_seed_count; ++seed_index)
        {
            const std::uint64_t seed = 0xC11C000u
                + static_cast<std::uint64_t>(seed_index) * 4099u;
            sim::Environment environment{ chicken, seed };
            environment.set_course(sim::CourseStage::balance, 0.25f);
            const std::array<float, sim::action_count> raw_action{};
            for (int frame = 0; frame < 1200; ++frame)
            {
                const auto action = rl::effective_policy_action(
                    environment, raw_action, sim::CourseStage::balance);
                const sim::StepResult step = environment.step(action);
                if (environment.valid_motion()
                    && environment.longest_stable_stance_seconds()
                        >= rl::standing_mastery_seconds)
                    break;
                if (step.terminated)
                    break;
            }
            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(sim::CourseStage::balance, environment);
            const bool accepted = qualification.valid
                && environment.body_integrity_valid()
                && environment.longest_stable_stance_seconds()
                    >= rl::standing_mastery_seconds
                && environment.uncontrolled_spin_turns() <= 0.55f;
            valid_chicken_seeds += accepted ? 1u : 0u;
            if (!accepted)
            {
                std::cerr << "chicken balance seed " << seed
                    << " rejection=" << qualification.rejection_mask
                    << " invalid=" << static_cast<int>(environment.invalid_reason())
                    << " stance=" << environment.longest_stable_stance_seconds()
                    << " spin=" << environment.uncontrolled_spin_turns()
                    << " survival=" << environment.elapsed_seconds() << std::endl;
            }
        }
        require(valid_chicken_seeds == chicken_seed_count,
            "chicken balance still reproduces the live 0/6 valid-seed regression");
    }

'''
if core_text.count(chicken_anchor) != 1:
    raise RuntimeError(f'tests/core_tests.cpp: chicken test anchor matched {core_text.count(chicken_anchor)}')
core.write_text(core_text.replace(chicken_anchor, chicken_tests, 1), encoding='utf-8')

# Broaden material acceptance to cover deterministic repeated impacts, partial
# burial with an escape side, and honest no-escape termination after deposition.
terrain_test = ROOT / 'tests/deformable_terrain_tests.cpp'
terrain_text = terrain_test.read_text(encoding='utf-8')
terrain_text = terrain_text.replace('namespace { void require(bool ok, std::string_view message)', '''namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static Vec2 root_position(const Environment& environment) noexcept
        {
            return environment.particles_[environment.blueprint_.root_node].position;
        }

        static void deposit_world(Environment& environment, float world_x,
            float amount, float firmness) noexcept
        {
            environment.terrain_.deposit(world_x + environment.course_progress(),
                amount, firmness);
        }

        static void refresh_material_metrics(Environment& environment, float dt) noexcept
        {
            environment.update_material_metrics(dt);
        }

        static void add_material(Environment& environment, MaterialParticle item)
        {
            environment.material_particles_.push_back(item);
            environment.rebuild_course_features();
        }
    };
}

namespace { void require(bool ok, std::string_view message)''', 1)
terrain_anchor = '''    for(const sim::MaterialParticle& item:environment.material_particles())
        require(item.position.y+item.radius>=environment.ground_height_at(item.position.x)-0.005f,"material tunneled below terrain");
    return EXIT_SUCCESS;
'''
terrain_extra = '''    for(const sim::MaterialParticle& item:environment.material_particles())
        require(item.position.y+item.radius>=environment.ground_height_at(item.position.x)-0.005f,"material tunneled below terrain");

    sim::Environment deterministic_a(sim::CreatureBlueprint::quadruped(),0x778899u);
    sim::Environment deterministic_b(sim::CreatureBlueprint::quadruped(),0x778899u);
    deterministic_a.set_course(sim::CourseStage::moving_hazards,0.90f);
    deterministic_b.set_course(sim::CourseStage::moving_hazards,0.90f);
    for(int frame=0;frame<480;++frame)
    {
        static_cast<void>(deterministic_a.step(idle));
        static_cast<void>(deterministic_b.step(idle));
    }
    require(deterministic_a.material_event_count()==deterministic_b.material_event_count(),
        "seeded repeated material events are not deterministic");
    require(deterministic_a.material_particles().size()==deterministic_b.material_particles().size(),
        "seeded material population diverged");
    for(std::size_t i=0;i<deterministic_a.material_particles().size();++i)
    {
        const auto& a=deterministic_a.material_particles()[i];
        const auto& b=deterministic_b.material_particles()[i];
        require(std::abs(a.position.x-b.position.x)<1.0e-5f
                && std::abs(a.position.y-b.position.y)<1.0e-5f,
            "seeded material trajectory diverged");
    }

    sim::Environment escape(sim::CreatureBlueprint::quadruped(),0xE5CA9Eu);
    escape.set_course(sim::CourseStage::moving_hazards,0.75f);
    const Vec2 escape_root=sim::EnvironmentTestAccess::root_position(escape);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.80f,5.2f,0.22f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.25f,4.4f,0.18f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x+0.35f,1.1f,0.25f);
    sim::EnvironmentTestAccess::refresh_material_metrics(escape,1.0f/60.0f);
    require(escape.burial_depth()>0.05f,"partial burial scenario produced no burial depth");
    require(escape.free_space_direction()>0.0f,"partial burial did not identify the open escape side");

    sim::Environment trapped(sim::CreatureBlueprint::quadruped(),0xB091EDu);
    trapped.set_course(sim::CourseStage::moving_hazards,0.90f);
    const Vec2 trapped_root=sim::EnvironmentTestAccess::root_position(trapped);
    for(float offset: {-0.90f,-0.55f,-0.20f,0.20f,0.55f,0.90f})
        sim::EnvironmentTestAccess::deposit_world(trapped,trapped_root.x+offset,6.5f,0.20f);
    for(int frame=0;frame<180;++frame)
        sim::EnvironmentTestAccess::refresh_material_metrics(trapped,1.0f/60.0f);
    require(trapped.invalid_reason()==sim::InvalidMotion::buried_no_escape,
        "complete terrain burial does not terminate honestly");

    sim::Environment impact(sim::CreatureBlueprint::quadruped(),0x1A2B3Cu);
    impact.set_course(sim::CourseStage::moving_hazards,0.85f);
    const Vec2 impact_root=sim::EnvironmentTestAccess::root_position(impact);
    sim::EnvironmentTestAccess::add_material(impact,{sim::MaterialKind::rock,
        impact_root+Vec2{0.10f,1.60f},{-0.20f,-5.50f},0.24f,0.94f,true});
    sim::EnvironmentTestAccess::add_material(impact,{sim::MaterialKind::sand,
        impact_root+Vec2{-0.65f,1.85f},{0.35f,-4.80f},0.07f,0.42f,true});
    for(int frame=0;frame<180;++frame)
        static_cast<void>(impact.step(idle));
    require(std::isfinite(impact.burial_depth())
            && std::isfinite(impact.incoming_time_to_impact()),
        "direct and glancing impact produced invalid material state");
    for(const sim::MaterialParticle& item:impact.material_particles())
        require(item.position.y+item.radius>=impact.ground_height_at(item.position.x)-0.005f,
            "direct impact tunneled material below terrain");
    return EXIT_SUCCESS;
'''
if terrain_text.count(terrain_anchor) != 1:
    raise RuntimeError(f'tests/deformable_terrain_tests.cpp: extension anchor matched {terrain_text.count(terrain_anchor)}')
terrain_test.write_text(terrain_text.replace(terrain_anchor, terrain_extra, 1), encoding='utf-8')

# Reconcile every reopened/live mission and add explicit acceptance for this pass.
mission = ROOT / 'missioncache.md'
mission_text = mission.read_text(encoding='utf-8')
mission_text = mission_text.replace('''### WALK-CHICKEN-048 — Rebuild the chicken preset as a bird
**Status:** PACKAGE VERIFIED
''', '''### WALK-CHICKEN-048 — Rebuild the chicken preset as a bird
**Status:** REOPENED BY v0.7.7 LIVE SCREENSHOT — corrected by WALK-CHICKEN-096
''', 1)
mission_text = mission_text.replace('''### WALK-LIVE-066 — Screenshot-level packaged-runtime acceptance
**Status:** PACKAGE VERIFIED — AWAITING LIVE CONFIRMATION
''', '''### WALK-LIVE-066 — Screenshot-level packaged-runtime acceptance
**Status:** IN PROGRESS — v0.7.7 chicken screenshot showed rearward collapse, 0.3 s best stance, 1.11 turns, and 0/6 valid seeds
''', 1)
mission_text = mission_text.replace('''### WALK-REG-029 — Reopen live simulation quality after v0.7.2 screenshots
**Status:** PACKAGE VERIFIED — AWAITING LIVE CONFIRMATION
''', '''### WALK-REG-029 — Reopen live simulation quality after v0.7.2 screenshots
**Status:** IN PROGRESS — carried into v0.7.8 chicken and material correction
''', 1)
mission_text = mission_text.replace('''### WALK-OBS-018 — Non-overlapping eight-motor observation layout
**Status:** VERIFIED

The humanoid retains forty non-overlapping observation channels covering eight angles, eight velocities, contacts, foot placement, terrain, obstacle, stage, and phase state.
''', '''### WALK-OBS-018 — Non-overlapping eight-motor observation layout
**Status:** VERIFIED — expanded without overlap in v0.7.8

The humanoid retains fifty non-overlapping observation channels: the original eight angles, eight velocities, contacts, foot placement, obstacle, stage, and phase state plus terrain firmness, looseness, slope, burial, obstruction, incoming material, and escape direction.
''', 1)
if 'WALK-CHICKEN-096' in mission_text:
    raise RuntimeError('missioncache.md: v0.7.8 completion missions already exist')
mission_text = mission_text.rstrip() + '''

### WALK-CHICKEN-096 — Correct live chicken balance regression
**Status:** IN PROGRESS

Use a real vertical semantic torso above the horizontal bird body, keep the raised neck, head, beak, tail, two articulated legs, and separate feet, and preserve leg-only motors. Six deterministic balance seeds must all sustain strict standing mastery without body collapse, integrity loss, or more than 0.55 uncontrolled turns.

### WALK-VISUAL-097 — Biomechanical rig animation treatment
**Status:** IN PROGRESS

Decorate live rigs, training PIP, and rig-lab previews with procedural anatomy rings, neural-link pulses, semantic-node halos, faint motion-study ghosts, and a small neural-chip motif. The effect must be generated from current rig state, require no external image asset, preserve telemetry readability, and never alter physics or input hit testing.

### WALK-ACCEPT-098 — Complete all v0.7.8 mission acceptance
**Status:** IN PROGRESS

Reconcile every open or screenshot-reopened ledger item, run strict chicken six-seed balance acceptance, seeded deformable-terrain conservation and collapse tests, deterministic repeated material events, partial burial with an escape side, full burial with honest termination, direct and glancing impacts, Linux warnings-as-errors, the complete Windows Vulkan package, executable-relative launch, ZIP manifest, SHA-256, and release re-download audit.
'''
mission.write_text(mission_text.rstrip() + '\n', encoding='utf-8')

notes = ROOT / 'RELEASE_NOTES_v0.7.8.md'
notes_text = notes.read_text(encoding='utf-8').rstrip()
notes_text += '''
- Rebuilds the chicken around a vertical semantic torso and central load-bearing brace while retaining its horizontal bird body, raised head, beak, tail, leg-only motors, and separate feet; six seeded strict-balance runs now guard the live 0/6 regression.
- Adds procedural biomechanical animation overlays to live rigs, the training PIP, and rig lab: semantic anatomy rings, neural-link pulses, motion-study ghosts, node halos, and a compact neural-chip motif with no new asset dependency.
- Expands material acceptance with deterministic repeated events, partial burial and escape-side detection, full no-escape burial termination, and direct/glancing impact anti-tunneling checks.
'''
notes.write_text(notes_text.rstrip() + '\n', encoding='utf-8')

# Update future validation runs so the new missions and source markers cannot be
# silently removed after this one-shot applicator has materialized the changes.
workflow = ROOT / '.github/workflows/validate-runner-v078.yml'
workflow_text = workflow.read_text(encoding='utf-8')
workflow_text = workflow_text.replace("              'WALK-ESCAPE-094', 'WALK-RELEASE-095'\n",
    "              'WALK-ESCAPE-094', 'WALK-RELEASE-095',\n              'WALK-CHICKEN-096', 'WALK-VISUAL-097', 'WALK-ACCEPT-098'\n", 1)
workflow_text = workflow_text.replace("              'terrain tests': ('total_height_volume', tests),\n",
    "              'terrain tests': ('buried_no_escape', tests),\n              'chicken balance': ('valid_chicken_seeds == chicken_seed_count', Path('tests/core_tests.cpp').read_text(encoding='utf-8')),\n              'biomechanical overlay': ('draw_biomechanical_overlay', Path('src/app.cpp').read_text(encoding='utf-8')),\n", 1)
workflow.write_text(workflow_text, encoding='utf-8')

Path(__file__).unlink()
