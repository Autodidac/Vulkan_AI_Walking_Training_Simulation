from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
def x(p,o,n):
 q=R/p; t=q.read_text(); c=t.count(o)
 if c!=1: raise RuntimeError(f'{p}: {c} matches')
 q.write_text(t.replace(o,n,1))
p=R/'src/simulation.cpp'; t=p.read_text()
pat=r"    void Environment::solve_ground\(float dt\) noexcept\n    \{.*?\n    \}\n\n    void Environment::solve_course"
new='''    void Environment::solve_ground(float dt) noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            Particle& particle = particles_[index];
            particle.grounded = false;
            const bool traction_contact = contact_cluster_contains(
                blueprint_.left_contact_node, index)
                || contact_cluster_contains(blueprint_.right_contact_node, index);
            const float firmness = terrain_firmness_at(particle.position.x);
            const float looseness = terrain_looseness_at(particle.position.x);
            const float burial_allowance = stage_uses_deformable_terrain(course_stage_)
                ? (traction_contact ? (1.0f - firmness) * 0.055f
                    : std::min(particle.radius * 0.78f,
                        (1.0f - firmness + looseness * 0.45f) * 0.18f))
                : 0.0f;
            const float minimum_y = ground_height_at(particle.position.x)
                + ground_contact_offset(traction_contact, particle.radius) - burial_allowance;
            if (particle.position.y <= minimum_y + 0.0025f)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;
                const Vec2 velocity = (particle.position - particle.previous) / dt;
                float retention = ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact && stage_uses_deformable_terrain(course_stage_))
                    retention = std::lerp(0.24f, 0.015f, firmness);
                particle.previous.x = particle.position.x - velocity.x * retention * dt;
                if (traction_contact)
                    particle.previous.y = particle.position.y;
                else if (velocity.y < 0.0f)
                    particle.previous.y = particle.position.y + velocity.y * dt * 0.05f;
            }
        }
    }

    void Environment::solve_course'''
t,c=re.subn(pat,new,t,count=1,flags=re.S)
if c!=1: raise RuntimeError(f'solve ground: {c}')
p.write_text(t)
x('src/simulation.cpp','''        recovery_events_ = 0;
        recovery_successes_ = 0;
        invalid_reason_ = InvalidMotion::none;
        rebuild_course_features();
''','''        recovery_events_ = 0;
        recovery_successes_ = 0;
        terrain_.reset(random_state_ ^ 0xa5a5a5a5a5a5a5a5ULL, course_difficulty_);
        material_particles_.clear();
        next_material_event_seconds_ = 1.50f;
        material_event_sequence_ = 0u;
        terrain_firmness_ = 1.0f;
        terrain_looseness_ = 0.0f;
        burial_depth_ = 0.0f;
        previous_burial_depth_ = 0.0f;
        buried_no_escape_seconds_ = 0.0f;
        free_space_direction_ = 0.0f;
        incoming_material_velocity_ = {};
        incoming_time_to_impact_ = 10.0f;
        incoming_material_density_ = 0.0f;
        obstruction_mask_ = 0u;
        invalid_reason_ = InvalidMotion::none;
        rebuild_course_features();
''')
x('src/simulation.cpp','''        collided_this_step_ = false;
        duck_press_contact_this_step_ = false;
        duck_press_max_penetration_ = 0.0f;
        rebuild_course_features();
''','''        collided_this_step_ = false;
        duck_press_contact_this_step_ = false;
        duck_press_max_penetration_ = 0.0f;
        update_materials(dt);
        rebuild_course_features();
''')
x('src/simulation.cpp','''            solve_course();
            separate_support_clusters();
        }
        if (elapsed_seconds_ >= 8.00f && !body_integrity_valid())
''','''            solve_course();
            separate_support_clusters();
        }
        apply_support_pressure(dt);
        if (stage_uses_deformable_terrain(course_stage_))
            terrain_.step(dt);
        update_material_metrics(dt);
        if (elapsed_seconds_ >= 8.00f && !body_integrity_valid())
''')
x('src/simulation.cpp','''        const float body_contact_penalty = non_foot_grounded_
            ? (head_ground_contact() ? 0.16f : 0.08f) : 0.0f;

        const float forward_gait_reward = std::max(0.0f, safe_progress) * 1.65f * gait;
''','''        const float body_contact_penalty = non_foot_grounded_
            ? (head_ground_contact() ? 0.16f : 0.08f) : 0.0f;
        const float burial_penalty = burial_depth_ * 0.22f
            + static_cast<float>(obstruction_mask_ != 0u) * 0.025f;
        const float burial_change = previous_burial_depth_ - burial_depth_;
        const float escape_alignment = free_space_direction_ == 0.0f ? 0.0f
            : std::max(0.0f, raw_speed * free_space_direction_);
        const float escape_reward = burial_depth_ > 0.03f
            ? std::max(0.0f, burial_change) * 0.25f + escape_alignment * 0.012f
            : 0.0f;

        const float forward_gait_reward = std::max(0.0f, safe_progress) * 1.65f * gait;
''')
x('src/simulation.cpp','''                + spin_landing_reward + obstacle_lift_reward + pass_reward
                - backward_penalty - unearned_progress_penalty
''','''                + spin_landing_reward + obstacle_lift_reward + pass_reward
                + escape_reward
                - backward_penalty - unearned_progress_penalty - burial_penalty
''')
x('src/simulation.cpp','        static_assert(observation_count == 40);\n','        static_assert(observation_count == 50);\n')
x('src/simulation.cpp','''        result[38] = std::sin(gait_phase);
        result[39] = std::cos(gait_phase);
        return result;
''','''        result[38] = std::sin(gait_phase);
        result[39] = std::cos(gait_phase);
        result[40] = terrain_firmness_;
        result[41] = terrain_looseness_;
        result[42] = clamp(burial_depth_ / 0.80f, 0.0f, 2.0f);
        result[43] = free_space_direction_;
        result[44] = clamp(incoming_material_velocity_.x / 6.0f, -2.0f, 2.0f);
        result[45] = clamp(incoming_material_velocity_.y / 6.0f, -2.0f, 2.0f);
        result[46] = clamp(incoming_time_to_impact_ / 4.0f, 0.0f, 2.5f);
        result[47] = clamp(incoming_material_density_, 0.0f, 1.0f);
        result[48] = static_cast<float>(obstruction_mask_) / 7.0f;
        result[49] = clamp(terrain_.slope_at(root.x + course_progress()), -2.0f, 2.0f);
        return result;
''')
Path(__file__).unlink()
