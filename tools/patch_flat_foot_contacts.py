from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}: {old[:160]!r}")
    write(path, text.replace(old, new, 1))


replace_once(
    "src/simulation.hpp",
    '''    [[nodiscard]] inline bool rolling_body_motion(float root_speed, float torso_turn_speed,
        float uprightness, bool feet_supported, bool non_foot_grounded) noexcept
    {
        return non_foot_grounded
            && (std::abs(torso_turn_speed) > 0.45f || uprightness < 0.55f)
            && (!feet_supported || std::abs(root_speed) > 0.08f);
    }
''',
    '''    [[nodiscard]] inline bool rolling_body_motion(float root_speed, float torso_turn_speed,
        float uprightness, bool feet_supported, bool non_foot_grounded) noexcept
    {
        return non_foot_grounded
            && (std::abs(torso_turn_speed) > 0.45f || uprightness < 0.55f)
            && (!feet_supported || std::abs(root_speed) > 0.08f);
    }

    [[nodiscard]] inline float ground_contact_offset(bool traction_contact,
        float particle_radius) noexcept
    {
        return traction_contact ? std::min(particle_radius, 0.065f) : particle_radius;
    }

    [[nodiscard]] inline bool foot_pivot_rolling_motion(float root_speed,
        bool left_supported, bool right_supported, float stance_slip_speed,
        float maximum_foot_clearance, float torso_turn_speed) noexcept
    {
        return left_supported && right_supported
            && std::abs(root_speed) > 0.10f
            && stance_slip_speed < 0.065f
            && maximum_foot_clearance < 0.075f
            && std::abs(torso_turn_speed) > 0.20f;
    }
''')

replace_once(
    "src/simulation.hpp",
    '''        wheel_sliding,
        body_rolling,
        hazard_quiver
''',
    '''        wheel_sliding,
        body_rolling,
        foot_pivot_rolling,
        hazard_quiver
''')

replace_once(
    "src/simulation.hpp",
    '''        case InvalidMotion::body_rolling: return "HEAD / TAIL / BODY ROLLING";
        case InvalidMotion::hazard_quiver: return "HAZARD QUIVER / NO LEG LIFT";
''',
    '''        case InvalidMotion::body_rolling: return "HEAD / TAIL / BODY ROLLING";
        case InvalidMotion::foot_pivot_rolling: return "FOOT-NODE ROLLING";
        case InvalidMotion::hazard_quiver: return "HAZARD QUIVER / NO LEG LIFT";
''')

replace_once(
    "src/simulation.hpp",
    '''        [[nodiscard]] float body_rolling_seconds() const noexcept { return body_rolling_seconds_; }
        [[nodiscard]] float hazard_stall_seconds() const noexcept { return hazard_stall_seconds_; }
''',
    '''        [[nodiscard]] float body_rolling_seconds() const noexcept { return body_rolling_seconds_; }
        [[nodiscard]] float foot_pivot_rolling_seconds() const noexcept { return foot_pivot_rolling_seconds_; }
        [[nodiscard]] float hazard_stall_seconds() const noexcept { return hazard_stall_seconds_; }
''')

replace_once(
    "src/simulation.hpp",
    '''        float wheel_sliding_seconds_{};
        float body_rolling_seconds_{};
        float head_contact_seconds_{};
''',
    '''        float wheel_sliding_seconds_{};
        float body_rolling_seconds_{};
        float foot_pivot_rolling_seconds_{};
        float head_contact_seconds_{};
''')

replace_once(
    "src/simulation.cpp",
    '''    [[nodiscard]] inline float ground_velocity_retention(bool traction_contact,
        float vertical_speed) noexcept
    {
        if (!traction_contact)
            return 0.985f;
        return std::abs(vertical_speed) < 1.5f ? 0.42f : 0.72f;
    }
''',
    '''    [[nodiscard]] inline float ground_velocity_retention(bool traction_contact,
        float vertical_speed) noexcept
    {
        static_cast<void>(vertical_speed);
        return traction_contact ? 0.0f : 0.985f;
    }
''')

replace_once(
    "src/simulation.cpp",
    '''        wheel_sliding_seconds_ = 0.0f;
        body_rolling_seconds_ = 0.0f;
        head_contact_seconds_ = 0.0f;
''',
    '''        wheel_sliding_seconds_ = 0.0f;
        body_rolling_seconds_ = 0.0f;
        foot_pivot_rolling_seconds_ = 0.0f;
        head_contact_seconds_ = 0.0f;
''')

replace_once(
    "src/simulation.cpp",
    '''            particle.grounded = false;
            const float minimum_y = ground_height_at(particle.position.x) + particle.radius;
            if (particle.position.y < minimum_y)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;
                const Vec2 velocity = (particle.position - particle.previous) / dt;
                const bool traction_contact = contact_cluster_contains(blueprint_.left_contact_node, index)
                    || contact_cluster_contains(blueprint_.right_contact_node, index);
                float retained_horizontal_speed = velocity.x
                    * ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact && std::abs(retained_horizontal_speed) < 0.03f)
                    retained_horizontal_speed = 0.0f;
                particle.previous.x = particle.position.x - retained_horizontal_speed * dt;
                if (velocity.y < 0.0f)
                    particle.previous.y = particle.position.y + velocity.y * dt * 0.05f;
            }
''',
    '''            particle.grounded = false;
            const bool traction_contact = contact_cluster_contains(blueprint_.left_contact_node, index)
                || contact_cluster_contains(blueprint_.right_contact_node, index);
            const float minimum_y = ground_height_at(particle.position.x)
                + ground_contact_offset(traction_contact, particle.radius);
            if (particle.position.y < minimum_y)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;
                const Vec2 velocity = (particle.position - particle.previous) / dt;
                const float retained_horizontal_speed = velocity.x
                    * ground_velocity_retention(traction_contact, velocity.y);
                particle.previous.x = particle.position.x - retained_horizontal_speed * dt;
                if (traction_contact)
                {
                    particle.previous.x = particle.position.x;
                    particle.previous.y = particle.position.y;
                }
                else if (velocity.y < 0.0f)
                {
                    particle.previous.y = particle.position.y + velocity.y * dt * 0.05f;
                }
            }
''')

replace_once(
    "src/simulation.cpp",
    '''        obstacle_lift_clearance_ = std::max(
            contact_cluster_clearance(blueprint_.left_contact_node),
            contact_cluster_clearance(blueprint_.right_contact_node));
        if (std::isfinite(nearest_hazard_dx) && hazard_quiver_motion(nearest_hazard_dx,
''',
    '''        obstacle_lift_clearance_ = std::max(
            contact_cluster_clearance(blueprint_.left_contact_node),
            contact_cluster_clearance(blueprint_.right_contact_node));
        if (course_stage_ != CourseStage::balance && foot_pivot_rolling_motion(root_speed,
            left, right, stance_slip_speed_, obstacle_lift_clearance_, torso_turn_speed_))
            foot_pivot_rolling_seconds_ += dt;
        else
            foot_pivot_rolling_seconds_ = std::max(0.0f, foot_pivot_rolling_seconds_ - dt * 2.5f);
        if (foot_pivot_rolling_seconds_ > 0.42f)
            invalidate(InvalidMotion::foot_pivot_rolling);

        if (std::isfinite(nearest_hazard_dx) && hazard_quiver_motion(nearest_hazard_dx,
''')

replace_once(
    "src/app.cpp",
    '''        std::filesystem::path autosave_policy_path{ "epochrunner-v063-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "epochrunner-v063-evolved.epochrig" };
        std::filesystem::path autosave_state_path{ "epochrunner-v063-autonomy.state" };
''',
    '''        std::filesystem::path autosave_policy_path{ "epochrunner-v064-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "epochrunner-v064-evolved.epochrig" };
        std::filesystem::path autosave_state_path{ "epochrunner-v064-autonomy.state" };
''')

replace_once(
    "src/app.cpp",
    '''                Color color = index == rig.head_node ? body_light : body;
                if (index == rig.left_contact_node || index == rig.right_contact_node)
                    color = leg;
                canvas.circle(point(index), radius, color, 22);
''',
    '''                Color color = index == rig.head_node ? body_light : body;
                const bool primary_foot = index == rig.left_contact_node || index == rig.right_contact_node;
                if (primary_foot)
                {
                    color = leg;
                    const Vec2 center = point(index);
                    canvas.capsule(center - Vec2{ radius * 0.82f, 0.0f },
                        center + Vec2{ radius * 0.82f, 0.0f }, radius * 0.44f, color, 16);
                }
                else
                {
                    canvas.circle(point(index), radius, color, 22);
                }
''')

replace_once(
    "src/app.cpp",
    '''                std::format("RECOVERY {}  FEET {}/{}  STEPS {}  LIFT {:.2f} M  STALL {:.1f} S",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.left_supported() ? "L" : "-",
                    environment.right_supported() ? "R" : "-",
                    environment.alternating_steps(), environment.obstacle_lift_clearance(),
                    environment.hazard_stall_seconds()),
''',
    '''                std::format("RECOVERY {}  FEET {}/{}  STEPS {}  LIFT {:.2f} M  FOOT-ROLL {:.1f} S",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.left_supported() ? "L" : "-",
                    environment.right_supported() ? "R" : "-",
                    environment.alternating_steps(), environment.obstacle_lift_clearance(),
                    environment.foot_pivot_rolling_seconds()),
''')

replace_once(
    "tests/core_tests.cpp",
    '''    require(sim::ground_velocity_retention(true, 0.0f)
        < sim::ground_velocity_retention(false, 0.0f),
        "feet do not receive more ground traction than head, tail, or body nodes");
''',
    '''    require(sim::ground_velocity_retention(true, 0.0f)
        < sim::ground_velocity_retention(false, 0.0f),
        "feet do not receive more ground traction than head, tail, or body nodes");
    require(sim::ground_velocity_retention(true, 0.0f) == 0.0f,
        "grounded orange foot nodes still retain wheel-like horizontal velocity");
    require(std::abs(sim::ground_contact_offset(true, 0.20f) - 0.065f) < 0.0001f,
        "semantic feet still collide with the ground as rolling circles");
''')

replace_once(
    "tests/core_tests.cpp",
    '''    require(!sim::rolling_body_motion(0.20f, 0.70f, 0.95f, true, false),
        "normal foot-supported walking is incorrectly classified as rolling");
''',
    '''    require(!sim::rolling_body_motion(0.20f, 0.70f, 0.95f, true, false),
        "normal foot-supported walking is incorrectly classified as rolling");
    require(sim::foot_pivot_rolling_motion(0.24f, true, true, 0.01f, 0.02f, 0.50f),
        "double-supported rolling around stationary orange foot nodes is not detected");
    require(!sim::foot_pivot_rolling_motion(0.24f, true, false, 0.01f, 0.18f, 0.50f),
        "single-support lifted-foot walking is incorrectly rejected as foot-node rolling");
''')

replace_once(
    "MISSIONS.md",
    '''- Four-legged crawler and six-legged hexapod presets are structurally valid, selectable, trainable, and use semantic foot clusters rather than treating extra feet as body contact.
- Deterministic tests cover all built-in presets, leg travel, support clustering, obstacle approach, and quiver rejection.
''',
    '''- Four-legged crawler and six-legged hexapod presets are structurally valid, selectable, trainable, and use semantic foot clusters rather than treating extra feet as body contact.
- Orange semantic foot nodes use flat-sole ground contact and cannot retain wheel-like horizontal velocity while grounded.
- Sustained double-supported pivoting around stationary orange foot nodes is invalidated as `FOOT-NODE ROLLING`.
- The UI renders primary orange foot contacts as flat soles rather than circular wheels.
- Deterministic tests cover all built-in presets, leg travel, support clustering, obstacle approach, quiver rejection, flat-foot contact, and foot-pivot rolling rejection.
''')

print("Applied flat-foot anti-rolling integration")
