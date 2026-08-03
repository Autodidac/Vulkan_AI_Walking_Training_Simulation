from pathlib import Path

root = Path(__file__).resolve().parents[1]
core = root / 'tests/core_tests.cpp'
text = core.read_text(encoding='utf-8')
anchor = '''    require(chicken.nodes[6].x < chicken.nodes[chicken.root_node].x - 1.0f
            && chicken.nodes[4].x > chicken.nodes[chicken.head_node].x,
        "chicken preset lacks a distinct tail and beak");

'''
addition = '''    require(chicken.nodes[6].x < chicken.nodes[chicken.root_node].x - 1.0f
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
if text.count(anchor) != 1:
    raise RuntimeError(f'chicken test anchor matched {text.count(anchor)}')
core.write_text(text.replace(anchor, addition, 1), encoding='utf-8')

terrain = root / 'tests/deformable_terrain_tests.cpp'
text = terrain.read_text(encoding='utf-8')
namespace_anchor = 'namespace { void require(bool ok, std::string_view message)'
namespace_replacement = '''namespace runner::sim
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

namespace { void require(bool ok, std::string_view message)'''
if text.count(namespace_anchor) != 1:
    raise RuntimeError(f'terrain test namespace anchor matched {text.count(namespace_anchor)}')
text = text.replace(namespace_anchor, namespace_replacement, 1)
end_anchor = '''    for(const sim::MaterialParticle& item:environment.material_particles())
        require(item.position.y+item.radius>=environment.ground_height_at(item.position.x)-0.005f,"material tunneled below terrain");
    return EXIT_SUCCESS;
'''
end_replacement = '''    for(const sim::MaterialParticle& item:environment.material_particles())
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
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.80f,12.0f,0.22f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x-0.25f,10.0f,0.18f);
    sim::EnvironmentTestAccess::deposit_world(escape,escape_root.x+0.55f,1.0f,0.25f);
    sim::EnvironmentTestAccess::refresh_material_metrics(escape,1.0f/60.0f);
    require(escape.burial_depth()>0.05f,"partial burial scenario produced no burial depth");
    require(escape.free_space_direction()>0.0f,"partial burial did not identify the open escape side");

    sim::Environment trapped(sim::CreatureBlueprint::quadruped(),0xB091EDu);
    trapped.set_course(sim::CourseStage::moving_hazards,0.90f);
    const Vec2 trapped_root=sim::EnvironmentTestAccess::root_position(trapped);
    constexpr std::array<float,6> burial_offsets{-0.90f,-0.55f,-0.20f,0.20f,0.55f,0.90f};
    for(float offset:burial_offsets)
        sim::EnvironmentTestAccess::deposit_world(trapped,trapped_root.x+offset,12.0f,0.20f);
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
if text.count(end_anchor) != 1:
    raise RuntimeError(f'terrain test end anchor matched {text.count(end_anchor)}')
terrain.write_text(text.replace(end_anchor, end_replacement, 1), encoding='utf-8')
Path(__file__).unlink()
