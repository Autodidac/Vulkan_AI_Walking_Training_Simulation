from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
p=R/'src/simulation.cpp'; t=p.read_text()
pat=r"    float Environment::ground_height_at\(float x\) const noexcept\n    \{.*?\n    \}\n\n    void Environment::rebuild_course_features"
new='''    float Environment::ground_height_at(float x) const noexcept
    {
        if (!stage_uses_deformable_terrain(course_stage_))
            return 0.0f;
        return terrain_.height_at(x + course_progress());
    }

    float Environment::terrain_firmness_at(float x) const noexcept
    {
        return stage_uses_deformable_terrain(course_stage_)
            ? terrain_.firmness_at(x + course_progress()) : 1.0f;
    }

    float Environment::terrain_looseness_at(float x) const noexcept
    {
        return stage_uses_deformable_terrain(course_stage_)
            ? terrain_.looseness_at(x + course_progress()) : 0.0f;
    }

    void Environment::update_materials(float dt) noexcept
    {
        if (course_stage_ != CourseStage::moving_hazards)
        {
            material_particles_.clear();
            return;
        }
        const float root_x = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        const float interval = std::lerp(4.20f, 2.20f, course_difficulty_);
        while (elapsed_seconds_ >= next_material_event_seconds_)
        {
            ++material_event_sequence_;
            if (material_particles_.size() > 72u)
                std::erase_if(material_particles_, [](const MaterialParticle& item) { return !item.active; });
            const float spawn_x = root_x + 3.2f + random_unit() * 3.0f
                + (random_unit() - 0.5f) * 1.4f;
            if ((material_event_sequence_ % 4u) == 0u)
            {
                const MaterialKind kind = (material_event_sequence_ % 8u) == 0u
                    ? MaterialKind::rock : MaterialKind::debris;
                material_particles_.push_back({ kind,
                    { spawn_x, 5.6f + random_unit() * 2.2f },
                    { -0.55f - course_difficulty_ * 1.1f, -0.35f - random_unit() * 0.60f },
                    kind == MaterialKind::rock ? 0.23f : 0.17f,
                    kind == MaterialKind::rock ? 0.92f : 0.70f, true });
            }
            else
            {
                constexpr std::size_t burst_count = 10u;
                for (std::size_t index = 0; index < burst_count; ++index)
                {
                    const float spread = (static_cast<float>(index)
                        - static_cast<float>(burst_count - 1u) * 0.5f) * 0.13f;
                    material_particles_.push_back({ MaterialKind::sand,
                        { spawn_x + spread, 5.2f + random_unit() * 1.8f },
                        { -0.25f - random_unit() * 0.45f, -0.20f - random_unit() * 0.35f },
                        0.055f + random_unit() * 0.025f, 0.42f, true });
                }
            }
            next_material_event_seconds_ += interval;
        }
        const float treadmill = course_speed();
        for (MaterialParticle& item : material_particles_)
        {
            if (!item.active)
                continue;
            item.velocity.y -= 13.0f * dt;
            item.position += item.velocity * dt;
            item.position.x -= treadmill * dt;
            const float ground = ground_height_at(item.position.x);
            if (item.position.y - item.radius > ground)
                continue;
            item.position.y = ground + item.radius;
            if (item.kind == MaterialKind::sand)
            {
                terrain_.deposit(item.position.x + course_progress(),
                    std::clamp(item.radius * item.radius * 2.8f, 0.004f, 0.025f), 0.18f);
                item.active = false;
            }
            else
            {
                item.velocity.y = std::abs(item.velocity.y) * 0.16f;
                item.velocity.x *= 0.72f;
                if (std::abs(item.velocity.x) < 0.08f && std::abs(item.velocity.y) < 0.08f)
                {
                    terrain_.deposit(item.position.x + course_progress(), item.radius * 0.12f, item.density);
                    item.active = false;
                }
            }
        }
        std::erase_if(material_particles_, [root_x](const MaterialParticle& item)
        {
            return !item.active || item.position.x < root_x - 12.0f
                || item.position.y < -3.0f || item.position.y > 18.0f;
        });
    }

    void Environment::append_material_features() noexcept
    {
        int marker = -1000;
        for (const MaterialParticle& item : material_particles_)
        {
            if (!item.active)
                continue;
            course_features_.push_back({ item.kind == MaterialKind::sand
                    ? CourseFeatureKind::projectile : CourseFeatureKind::moving_hazard,
                item.position, {}, item.radius, item.velocity, marker-- });
        }
    }

    void Environment::apply_support_pressure(float dt) noexcept
    {
        if (!stage_uses_deformable_terrain(course_stage_))
            return;
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!particles_[index].grounded || !blueprint_.is_support_seed(index))
                continue;
            const Particle& particle = particles_[index];
            const float slip = std::abs((particle.position.x - particle.previous.x)
                / std::max(dt, 1.0e-5f));
            const float load = std::clamp(1.0f / std::max(particle.inverse_mass, 0.15f), 0.5f, 3.5f);
            terrain_.apply_pressure(particle.position.x + course_progress(), load, slip, dt);
        }
    }

    void Environment::update_material_metrics(float dt) noexcept
    {
        if (!valid_node(blueprint_.root_node))
            return;
        const Particle& root = particles_[blueprint_.root_node];
        terrain_firmness_ = terrain_firmness_at(root.position.x);
        terrain_looseness_ = terrain_looseness_at(root.position.x);
        const float prior_burial = burial_depth_;
        burial_depth_ = 0.0f;
        obstruction_mask_ = 0u;
        auto measure = [&](std::uint16_t node, std::uint8_t mask)
        {
            if (!valid_node(node))
                return;
            const Particle& particle = particles_[node];
            const float depth = ground_height_at(particle.position.x)
                - (particle.position.y - particle.radius);
            burial_depth_ = std::max(burial_depth_, std::max(0.0f, depth));
            if (depth > particle.radius * 0.38f)
                obstruction_mask_ = static_cast<std::uint8_t>(obstruction_mask_ | mask);
        };
        measure(blueprint_.head_node, 0x1u);
        measure(blueprint_.torso_node, 0x2u);
        measure(blueprint_.left_contact_node, 0x4u);
        measure(blueprint_.right_contact_node, 0x4u);
        float left_density = 0.0f;
        float right_density = 0.0f;
        incoming_time_to_impact_ = 10.0f;
        incoming_material_velocity_ = {};
        incoming_material_density_ = 0.0f;
        for (const MaterialParticle& item : material_particles_)
        {
            if (!item.active)
                continue;
            const Vec2 delta = item.position - root.position;
            const float distance = length(delta);
            if (delta.x < 0.0f && distance < 3.5f)
                left_density += item.density;
            else if (delta.x >= 0.0f && distance < 3.5f)
                right_density += item.density;
            const Vec2 relative = item.velocity - Vec2{ forward_speed_, 0.0f };
            const float closing = -dot(normalized(delta, { 0.0f, 1.0f }), relative);
            if (closing <= 0.05f)
                continue;
            const float time = distance / std::max(closing, 0.05f);
            if (time < incoming_time_to_impact_)
            {
                incoming_time_to_impact_ = time;
                incoming_material_velocity_ = relative;
                incoming_material_density_ = item.density;
            }
        }
        const float center_ground = ground_height_at(root.position.x);
        const float left_cost = left_density + std::max(0.0f,
            ground_height_at(root.position.x - 1.25f) - center_ground) * 3.0f;
        const float right_cost = right_density + std::max(0.0f,
            ground_height_at(root.position.x + 1.25f) - center_ground) * 3.0f;
        const float delta_cost = left_cost - right_cost;
        free_space_direction_ = std::abs(delta_cost) < 0.08f
            ? 0.0f : (delta_cost > 0.0f ? 1.0f : -1.0f);
        const bool trapped = burial_depth_ > 0.32f
            && (obstruction_mask_ & 0x3u) == 0x3u
            && (left_density + right_density) > 1.4f;
        buried_no_escape_seconds_ = trapped
            ? buried_no_escape_seconds_ + dt
            : std::max(0.0f, buried_no_escape_seconds_ - dt * 2.0f);
        if (buried_no_escape_seconds_ > 2.25f)
            invalidate(InvalidMotion::buried_no_escape);
        previous_burial_depth_ = prior_burial;
    }

    void Environment::rebuild_course_features'''
t,c=re.subn(pat,new,t,count=1,flags=re.S)
if c!=1: raise RuntimeError(f'ground replacement: {c}')
old='''            }
        }
    }

    void Environment::reset(std::uint64_t seed)
'''
new2='''            }
        }
        append_material_features();
    }

    void Environment::reset(std::uint64_t seed)
'''
if t.count(old)!=1: raise RuntimeError('append location')
t=t.replace(old,new2,1)
p.write_text(t)
Path(__file__).unlink()
