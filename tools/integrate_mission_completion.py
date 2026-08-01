from pathlib import Path

def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")

def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")

def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}: {old[:120]!r}")
    write(path, text.replace(old, new, 1))

def insert_before(path: str, marker: str, addition: str) -> None:
    text = read(path)
    count = text.count(marker)
    if count != 1:
        raise RuntimeError(f"{path}: expected one marker, found {count}: {marker[:120]!r}")
    write(path, text.replace(marker, addition + marker, 1))

replace_once("src/simulation.hpp",
'''        [[nodiscard]] static CreatureBlueprint quadruped();
        [[nodiscard]] static CreatureBlueprint monoped();''',
'''        [[nodiscard]] static CreatureBlueprint quadruped();
        [[nodiscard]] static CreatureBlueprint crawler4();
        [[nodiscard]] static CreatureBlueprint hexapod();
        [[nodiscard]] static CreatureBlueprint monoped();''')

replace_once("src/simulation.hpp",
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

    [[nodiscard]] inline bool ground_clearance_hazard(CourseFeatureKind kind) noexcept
    {
        return kind == CourseFeatureKind::rock || kind == CourseFeatureKind::hurdle;
    }

    [[nodiscard]] inline float hazard_approach_weight(float distance_ahead) noexcept
    {
        if (distance_ahead <= -0.20f || distance_ahead >= 2.60f)
            return 0.0f;
        if (distance_ahead <= 0.45f)
            return 1.0f;
        return clamp((2.60f - distance_ahead) / 2.15f, 0.0f, 1.0f);
    }

    [[nodiscard]] inline bool hazard_quiver_motion(float distance_ahead, float root_speed,
        float lifted_foot_clearance, float target_clearance, float action_energy) noexcept
    {
        return hazard_approach_weight(distance_ahead) > 0.35f
            && std::abs(root_speed) < 0.16f
            && lifted_foot_clearance < target_clearance * 0.55f
            && action_energy > 0.075f;
    }
''')

replace_once("src/simulation.hpp",
'''        wheel_sliding,
        body_rolling
    };''',
'''        wheel_sliding,
        body_rolling,
        hazard_quiver
    };''')
replace_once("src/simulation.hpp",
'''        case InvalidMotion::body_rolling: return "HEAD / TAIL / BODY ROLLING";''',
'''        case InvalidMotion::body_rolling: return "HEAD / TAIL / BODY ROLLING";
        case InvalidMotion::hazard_quiver: return "HAZARD QUIVER / NO LEG LIFT";''')

replace_once("src/simulation.hpp",
'''        [[nodiscard]] float body_rolling_seconds() const noexcept { return body_rolling_seconds_; }
        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }''',
'''        [[nodiscard]] float body_rolling_seconds() const noexcept { return body_rolling_seconds_; }
        [[nodiscard]] float hazard_stall_seconds() const noexcept { return hazard_stall_seconds_; }
        [[nodiscard]] float obstacle_lift_clearance() const noexcept { return obstacle_lift_clearance_; }
        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }''')
replace_once("src/simulation.hpp",
'''        [[nodiscard]] float contact_cluster_horizontal_speed(std::uint16_t contact_node,
            float dt) const noexcept;
        [[nodiscard]] bool knee_before_foot_fault() const noexcept;''',
'''        [[nodiscard]] float contact_cluster_horizontal_speed(std::uint16_t contact_node,
            float dt) const noexcept;
        [[nodiscard]] float contact_cluster_clearance(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] bool knee_before_foot_fault() const noexcept;''')
replace_once("src/simulation.hpp",
'''        float torso_turn_speed_{};
        float stance_slip_speed_{};
        bool non_foot_grounded_{};''',
'''        float torso_turn_speed_{};
        float stance_slip_speed_{};
        float hazard_stall_seconds_{};
        float obstacle_approach_weight_{};
        float obstacle_lift_clearance_{};
        float obstacle_clearance_target_{ 0.20f };
        bool non_foot_grounded_{};''')

replace_once("src/simulation.cpp",
'''        void calibrate_quadruped_stable_defaults(CreatureBlueprint& rig) noexcept
        {
            // The quadruped is the stable reference because its roughly one-metre
            // driven arms, symmetric travel, and moderate correction speed do not
            // launch the body. Preserve that effective endpoint displacement on
            // every body instead of copying a raw strength onto longer limbs.
            constexpr std::array<float, action_count> travel_degrees{ 22.0f, 30.0f, 22.0f, 30.0f };
            constexpr std::array<float, action_count> reference_linear_gain{ 0.0525f, 0.0575f, 0.0525f, 0.0575f };
            for (std::size_t index = 0; index < action_count; ++index)
            {
                const MotorConstraint& motor = rig.motors[index];
                const float driven_arm = motor.pivot < rig.nodes.size() && motor.c < rig.nodes.size()
                    ? length(rig.nodes[motor.c] - rig.nodes[motor.pivot]) : 1.0f;
                const float normalized_strength = clamp(
                    reference_linear_gain[index] / std::max(0.75f, driven_arm), 0.035f, 0.058f);
                rig.calibrate_motor(index, travel_degrees[index], travel_degrees[index], normalized_strength);
            }
        }''',
'''        void calibrate_grounded_defaults(CreatureBlueprint& rig,
            float major_travel_degrees, float minor_travel_degrees,
            float major_linear_gain, float minor_linear_gain) noexcept
        {
            for (std::size_t index = 0; index < action_count; ++index)
            {
                const MotorConstraint& motor = rig.motors[index];
                const bool minor_joint = (index & 1u) != 0u;
                const float travel = minor_joint ? minor_travel_degrees : major_travel_degrees;
                const float linear_gain = minor_joint ? minor_linear_gain : major_linear_gain;
                const float driven_arm = motor.pivot < rig.nodes.size() && motor.c < rig.nodes.size()
                    ? length(rig.nodes[motor.c] - rig.nodes[motor.pivot]) : 1.0f;
                const float normalized_strength = clamp(
                    linear_gain / std::max(0.75f, driven_arm), 0.032f, 0.056f);
                rig.calibrate_motor(index, travel, travel, normalized_strength);
            }
        }

        void calibrate_obstacle_legs(CreatureBlueprint& rig, float travel = 46.0f) noexcept
        {
            for (std::size_t index = 0; index < action_count; ++index)
                rig.calibrate_motor(index, travel, travel, 0.043f);
        }''')

replace_once("src/simulation.cpp",
'''        calibrate_quadruped_stable_defaults(result);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::biped()''',
'''        calibrate_grounded_defaults(result, 34.0f, 56.0f, 0.044f, 0.050f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::biped()''')
replace_once("src/simulation.cpp",
'''        calibrate_quadruped_stable_defaults(result);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::humanoid()''',
'''        calibrate_grounded_defaults(result, 36.0f, 58.0f, 0.045f, 0.051f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::humanoid()''')
replace_once("src/simulation.cpp",
'''        calibrate_quadruped_stable_defaults(result);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::quadruped()''',
'''        calibrate_grounded_defaults(result, 36.0f, 58.0f, 0.045f, 0.051f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::quadruped()''')
replace_once("src/simulation.cpp",
'''        result.rebuild_rest_lengths();
        calibrate_quadruped_stable_defaults(result);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::monoped()''',
'''        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 34.0f, 50.0f, 0.046f, 0.052f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::crawler4()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.05f }, { 1.05f, 2.12f }, { 1.82f, 2.42f },
            { -0.72f, 0.30f }, { -0.20f, 0.28f },
            { 1.22f, 0.28f }, { 1.72f, 0.30f },
            { -0.48f, 2.10f }, { 1.52f, 2.16f }
        };
        result.radii = { 0.29f, 0.29f, 0.24f, 0.15f, 0.15f, 0.15f, 0.15f, 0.18f, 0.18f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.92f },
            { 7, 0, 0.0f, 0.98f }, { 1, 8, 0.0f, 0.98f },
            { 7, 3, 0.0f, 0.97f }, { 0, 4, 0.0f, 0.97f },
            { 1, 5, 0.0f, 0.97f }, { 8, 6, 0.0f, 0.97f },
            { 3, 4, 0.0f, 0.42f }, { 5, 6, 0.0f, 0.42f }
        };
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 2;
        result.left_contact_node = 3;
        result.right_contact_node = 5;
        result.motors = {
            MotorConstraint{ 0, 7, 3 }, MotorConstraint{ 1, 0, 4 },
            MotorConstraint{ 0, 1, 5 }, MotorConstraint{ 1, 8, 6 }
        };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 48.0f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::hexapod()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.08f }, { 0.92f, 2.12f }, { 1.82f, 2.36f },
            { -0.92f, 0.30f }, { -0.42f, 0.27f },
            { 0.42f, 0.27f }, { 0.92f, 0.27f },
            { 1.55f, 0.28f }, { 2.02f, 0.31f },
            { -0.48f, 2.10f }, { 0.45f, 2.15f }, { 1.48f, 2.16f }
        };
        result.radii = {
            0.28f, 0.29f, 0.23f,
            0.14f, 0.14f, 0.14f, 0.14f, 0.14f, 0.14f,
            0.17f, 0.17f, 0.17f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.92f },
            { 9, 0, 0.0f, 0.98f }, { 0, 10, 0.0f, 0.98f }, { 1, 11, 0.0f, 0.98f },
            { 9, 3, 0.0f, 0.96f }, { 9, 4, 0.0f, 0.96f },
            { 10, 5, 0.0f, 0.96f }, { 10, 6, 0.0f, 0.96f },
            { 11, 7, 0.0f, 0.96f }, { 11, 8, 0.0f, 0.96f },
            { 3, 4, 0.0f, 0.42f }, { 4, 5, 0.0f, 0.36f },
            { 5, 6, 0.0f, 0.42f }, { 6, 7, 0.0f, 0.36f }, { 7, 8, 0.0f, 0.42f }
        };
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 2;
        result.left_contact_node = 3;
        result.right_contact_node = 6;
        result.motors = {
            MotorConstraint{ 0, 9, 3 }, MotorConstraint{ 0, 9, 4 },
            MotorConstraint{ 1, 10, 5 }, MotorConstraint{ 0, 1, 11 }
        };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 50.0f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::monoped()''')
replace_once("src/simulation.cpp",
'''        calibrate_quadruped_stable_defaults(result);
        return result;
    }

    void CreatureBlueprint::rebuild_rest_lengths() noexcept''',
'''        calibrate_grounded_defaults(result, 32.0f, 48.0f, 0.043f, 0.049f);
        return result;
    }

    void CreatureBlueprint::rebuild_rest_lengths() noexcept''')

replace_once("src/simulation.cpp",
'''    bool Environment::contact_cluster_contains(std::uint16_t contact_node,
        std::size_t particle_index) const noexcept
    {
        if (!valid_node(contact_node) || particle_index >= particles_.size()
            || particle_index >= blueprint_.nodes.size())
            return false;
        if (particle_index == static_cast<std::size_t>(contact_node))
            return true;

        const float contact_height = blueprint_.nodes[contact_node].y;
        if (blueprint_.nodes[particle_index].y > contact_height + 0.08f)
            return false;
        const auto node = static_cast<std::uint16_t>(particle_index);
        return std::ranges::any_of(blueprint_.bones, [contact_node, node](const DistanceConstraint& bone)
        {
            return (bone.a == contact_node && bone.b == node)
                || (bone.a == node && bone.b == contact_node);
        });
    }''',
'''    bool Environment::contact_cluster_contains(std::uint16_t contact_node,
        std::size_t particle_index) const noexcept
    {
        if (!valid_node(contact_node) || particle_index >= particles_.size()
            || particle_index >= blueprint_.nodes.size())
            return false;

        const float contact_height = blueprint_.nodes[contact_node].y;
        if (blueprint_.nodes[particle_index].y > contact_height + 0.18f)
            return false;

        std::array<bool, 128> visited{};
        std::array<std::uint16_t, 128> queue{};
        std::size_t head = 0;
        std::size_t tail = 0;
        visited[contact_node] = true;
        queue[tail++] = contact_node;
        while (head < tail)
        {
            const std::uint16_t current = queue[head++];
            if (current == particle_index)
                return true;
            for (const DistanceConstraint& bone : blueprint_.bones)
            {
                std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                if (bone.a == current)
                    next = bone.b;
                else if (bone.b == current)
                    next = bone.a;
                if (next >= blueprint_.nodes.size() || visited[next]
                    || blueprint_.nodes[next].y > contact_height + 0.18f)
                    continue;
                visited[next] = true;
                queue[tail++] = next;
            }
        }
        return false;
    }''')

insert_before("src/simulation.cpp",
'''    bool Environment::knee_before_foot_fault() const noexcept
''',
'''    float Environment::contact_cluster_clearance(std::uint16_t contact_node) const noexcept
    {
        float maximum = 0.0f;
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!contact_cluster_contains(contact_node, index))
                continue;
            const Particle& particle = particles_[index];
            maximum = std::max(maximum, particle.position.y
                - ground_height_at(particle.position.x) - particle.radius);
        }
        return maximum;
    }

''')

replace_once("src/simulation.cpp",
'''        torso_turn_speed_ = 0.0f;
        stance_slip_speed_ = 0.0f;
        non_foot_grounded_ = false;''',
'''        torso_turn_speed_ = 0.0f;
        stance_slip_speed_ = 0.0f;
        hazard_stall_seconds_ = 0.0f;
        obstacle_approach_weight_ = 0.0f;
        obstacle_lift_clearance_ = 0.0f;
        obstacle_clearance_target_ = 0.20f;
        non_foot_grounded_ = false;''')

replace_once("src/simulation.cpp", "for (int iteration = 0; iteration < 12; ++iteration)",
             "for (int iteration = 0; iteration < 14; ++iteration)")
replace_once("src/simulation.cpp", "particle.previous += correction * 0.25f;",
             "particle.previous += correction * 0.06f;")
replace_once("src/simulation.cpp", "particle.previous += correction * 0.18f;",
             "particle.previous += correction * 0.05f;")

replace_once("src/simulation.cpp",
'''        if (wheel_sliding_seconds_ > 0.90f)
            invalidate(InvalidMotion::wheel_sliding);

        if (!left && !right)''',
'''        if (wheel_sliding_seconds_ > 0.90f)
            invalidate(InvalidMotion::wheel_sliding);

        float nearest_hazard_dx = std::numeric_limits<float>::infinity();
        float nearest_hazard_target = 0.20f;
        for (const CourseFeature& feature : course_features_)
        {
            if (!ground_clearance_hazard(feature.kind))
                continue;
            const float dx = feature.center.x - root_x;
            if (dx < nearest_hazard_dx && dx >= -0.35f)
            {
                nearest_hazard_dx = dx;
                nearest_hazard_target = course_feature_top(feature)
                    - ground_height_at(feature.center.x) + 0.12f;
            }
        }
        obstacle_approach_weight_ = std::isfinite(nearest_hazard_dx)
            ? hazard_approach_weight(nearest_hazard_dx) : 0.0f;
        obstacle_clearance_target_ = std::max(0.18f, nearest_hazard_target);
        obstacle_lift_clearance_ = std::max(
            contact_cluster_clearance(blueprint_.left_contact_node),
            contact_cluster_clearance(blueprint_.right_contact_node));
        if (std::isfinite(nearest_hazard_dx) && hazard_quiver_motion(nearest_hazard_dx,
            root_speed, obstacle_lift_clearance_, obstacle_clearance_target_, action_energy))
            hazard_stall_seconds_ += dt;
        else
            hazard_stall_seconds_ = std::max(0.0f, hazard_stall_seconds_ - dt * 1.75f);
        if (hazard_stall_seconds_ > 1.35f)
            invalidate(InvalidMotion::hazard_quiver);

        if (!left && !right)''')

replace_once("src/simulation.cpp",
'''        const float swing_reward = single_support && swing_clearance > 0.10f
            ? clamp(swing_clearance, 0.0f, 0.45f) * 0.004f : 0.0f;
''',
'''        const float swing_reward = single_support && swing_clearance > 0.10f
            ? clamp(swing_clearance, 0.0f, 0.55f) * 0.005f : 0.0f;
        const float obstacle_lift_ratio = clamp(
            obstacle_lift_clearance_ / std::max(0.10f, obstacle_clearance_target_), 0.0f, 1.25f);
        const float obstacle_lift_reward = obstacle_approach_weight_
            * obstacle_lift_ratio * (single_support ? 0.020f : 0.007f);
        const float hazard_stall_penalty = obstacle_approach_weight_
            * clamp(hazard_stall_seconds_, 0.0f, 1.5f) * 0.022f;
''')
replace_once("src/simulation.cpp",
'''                + contact * 0.0006f
                + swing_reward
                - std::max(0.0f, -safe_progress) * 0.45f''',
'''                + contact * 0.0006f
                + swing_reward
                + obstacle_lift_reward
                - std::max(0.0f, -safe_progress) * 0.45f''')
replace_once("src/simulation.cpp",
'''                - stance_slip_penalty
                - wheel_penalty;''',
'''                - stance_slip_penalty
                - wheel_penalty
                - hazard_stall_penalty;''')

replace_once("src/app.cpp",
'''        enum class RigPreset : std::uint8_t { humanoid, biped, chicken, quadruped, monoped, custom };''',
'''        enum class RigPreset : std::uint8_t {
            humanoid, biped, chicken, quadruped, crawler4, hexapod, monoped, custom
        };''')
replace_once("src/app.cpp",
'''            case RigPreset::quadruped: return "QUADRUPED";
            case RigPreset::monoped: return "MONOPED";''',
'''            case RigPreset::quadruped: return "QUADRUPED";
            case RigPreset::crawler4: return "FOUR-LEG CRAWLER";
            case RigPreset::hexapod: return "SIX-LEG HEXAPOD";
            case RigPreset::monoped: return "MONOPED";''')
replace_once("src/app.cpp",
'''            case RigPreset::quadruped:
                return { "REAR HIP", "REAR KNEE", "FRONT SHOULDER", "FRONT KNEE" };
            case RigPreset::monoped:''',
'''            case RigPreset::quadruped:
                return { "REAR HIP", "REAR KNEE", "FRONT SHOULDER", "FRONT KNEE" };
            case RigPreset::crawler4:
                return { "REAR LEG", "MID-REAR LEG", "MID-FRONT LEG", "FRONT LEG" };
            case RigPreset::hexapod:
                return { "REAR PAIR A", "REAR PAIR B", "MID PAIR", "FRONT PAIR" };
            case RigPreset::monoped:''')
replace_once("src/app.cpp",
'''            case RigPreset::quadruped: blueprint = sim::CreatureBlueprint::quadruped(); break;
            case RigPreset::monoped: blueprint = sim::CreatureBlueprint::monoped(); break;''',
'''            case RigPreset::quadruped: blueprint = sim::CreatureBlueprint::quadruped(); break;
            case RigPreset::crawler4: blueprint = sim::CreatureBlueprint::crawler4(); break;
            case RigPreset::hexapod: blueprint = sim::CreatureBlueprint::hexapod(); break;
            case RigPreset::monoped: blueprint = sim::CreatureBlueprint::monoped(); break;''')

replace_once("src/app.cpp",
'''                const float third = (rect.size.x - 48.0f) / 3.0f;
                if (button({ cursor, { third, 35.0f } }, "HUMANOID", input, rig_preset == RigPreset::humanoid))
                    use_preset(RigPreset::humanoid);
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 35.0f } }, "BIPED", input,
                    rig_preset == RigPreset::biped))
                    use_preset(RigPreset::biped);
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 35.0f } }, "QUADRUPED", input,
                    rig_preset == RigPreset::quadruped))
                    use_preset(RigPreset::quadruped);
                cursor.y += 43.0f;
                const float half = (rect.size.x - 42.0f) * 0.5f;
                if (button({ cursor, { half, 35.0f } }, "CHICKEN", input, rig_preset == RigPreset::chicken))
                    use_preset(RigPreset::chicken);
                if (button({ cursor + Vec2{ half + 6.0f, 0.0f }, { half, 35.0f } }, "MONOPED", input,
                    rig_preset == RigPreset::monoped))
                    use_preset(RigPreset::monoped);
                cursor.y += 48.0f;
''',
'''                const float fourth = (rect.size.x - 54.0f) / 4.0f;
                if (button({ cursor, { fourth, 35.0f } }, "HUMANOID", input, rig_preset == RigPreset::humanoid))
                    use_preset(RigPreset::humanoid);
                if (button({ cursor + Vec2{ fourth + 6.0f, 0.0f }, { fourth, 35.0f } }, "BIPED", input,
                    rig_preset == RigPreset::biped))
                    use_preset(RigPreset::biped);
                if (button({ cursor + Vec2{ (fourth + 6.0f) * 2.0f, 0.0f }, { fourth, 35.0f } }, "QUADRUPED", input,
                    rig_preset == RigPreset::quadruped))
                    use_preset(RigPreset::quadruped);
                if (button({ cursor + Vec2{ (fourth + 6.0f) * 3.0f, 0.0f }, { fourth, 35.0f } }, "4-LEG", input,
                    rig_preset == RigPreset::crawler4))
                    use_preset(RigPreset::crawler4);
                cursor.y += 43.0f;
                const float third = (rect.size.x - 48.0f) / 3.0f;
                if (button({ cursor, { third, 35.0f } }, "CHICKEN", input, rig_preset == RigPreset::chicken))
                    use_preset(RigPreset::chicken);
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 35.0f } }, "6-LEG", input,
                    rig_preset == RigPreset::hexapod))
                    use_preset(RigPreset::hexapod);
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 35.0f } }, "MONOPED", input,
                    rig_preset == RigPreset::monoped))
                    use_preset(RigPreset::monoped);
                cursor.y += 48.0f;
''')
replace_once("src/app.cpp",
'''                std::format("RECOVERY {}   FEET {}/{}   STEPS {}   KNEE FAULTS {}",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.left_supported() ? "L" : "-",
                    environment.right_supported() ? "R" : "-",
                    environment.alternating_steps(), environment.knee_first_faults()),''',
'''                std::format("RECOVERY {}  FEET {}/{}  STEPS {}  LIFT {:.2f} M  STALL {:.1f} S",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.left_supported() ? "L" : "-",
                    environment.right_supported() ? "R" : "-",
                    environment.alternating_steps(), environment.obstacle_lift_clearance(),
                    environment.hazard_stall_seconds()),''')

replace_once("tests/core_tests.cpp",
'''    require(!sim::wheel_sliding_motion(0.45f, true, false, 0.50f),
        "single-support walking is incorrectly classified as wheel sliding");''',
'''    require(!sim::wheel_sliding_motion(0.45f, true, false, 0.50f),
        "single-support walking is incorrectly classified as wheel sliding");
    require(sim::hazard_approach_weight(0.40f) == 1.0f,
        "near obstacle does not activate full leg-lift training");
    require(sim::hazard_quiver_motion(0.50f, 0.02f, 0.03f, 0.40f, 0.20f),
        "high-energy no-lift obstacle quiver is not detected");
    require(!sim::hazard_quiver_motion(0.50f, 0.02f, 0.35f, 0.40f, 0.20f),
        "useful obstacle leg lift is incorrectly classified as quivering");''')
replace_once("tests/core_tests.cpp",
'''    const std::array<sim::CreatureBlueprint, 5> presets{
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid(),
        sim::CreatureBlueprint::quadruped(),
        sim::CreatureBlueprint::monoped()
    };''',
'''    const std::array<sim::CreatureBlueprint, 7> presets{
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid(),
        sim::CreatureBlueprint::quadruped(),
        sim::CreatureBlueprint::crawler4(),
        sim::CreatureBlueprint::hexapod(),
        sim::CreatureBlueprint::monoped()
    };''')
replace_once("tests/core_tests.cpp",
'''            require(motor.strength <= 0.060f, "default joint speed remains too strong");''',
'''            require(motor.strength <= 0.060f, "default joint speed remains too strong");
            require((motor.maximum_angle - motor.minimum_angle) * 180.0f / pi >= 60.0f,
                "preset cannot articulate enough to lift a leg over debris");''')

old_humanoid_calibration = '''    for (std::size_t motor_index = 0; motor_index < humanoid.motors.size(); ++motor_index)
    {
        const sim::MotorConstraint& motor = humanoid.motors[motor_index];
        const float driven_arm = length(humanoid.nodes[motor.c] - humanoid.nodes[motor.pivot]);
        const float expected_linear_gain = (motor_index % 2u) == 0u ? 0.0525f : 0.0575f;
        const float expected_travel = (motor_index % 2u) == 0u ? 22.0f : 30.0f;
        require(std::abs(motor.strength * std::max(0.75f, driven_arm) - expected_linear_gain) < 0.002f,
            "non-quadruped motor does not use the quadruped-stable effective gain");
        require(std::abs((motor.neutral_angle - motor.minimum_angle) * 180.0f / pi - expected_travel) < 0.05f,
            "quadruped-stable backward travel was not applied");
        require(std::abs((motor.maximum_angle - motor.neutral_angle) * 180.0f / pi - expected_travel) < 0.05f,
            "quadruped-stable forward travel was not applied");
    }
'''
new_humanoid_calibration = '''    for (std::size_t motor_index = 0; motor_index < humanoid.motors.size(); ++motor_index)
    {
        const sim::MotorConstraint& motor = humanoid.motors[motor_index];
        const float driven_arm = length(humanoid.nodes[motor.c] - humanoid.nodes[motor.pivot]);
        const float expected_linear_gain = (motor_index % 2u) == 0u ? 0.045f : 0.051f;
        const float expected_travel = (motor_index % 2u) == 0u ? 36.0f : 58.0f;
        require(std::abs(motor.strength * std::max(0.75f, driven_arm) - expected_linear_gain) < 0.003f,
            "humanoid motor does not use the bounded obstacle-capable effective gain");
        require(std::abs((motor.neutral_angle - motor.minimum_angle) * 180.0f / pi - expected_travel) < 0.05f,
            "obstacle-capable backward travel was not applied");
        require(std::abs((motor.maximum_angle - motor.neutral_angle) * 180.0f / pi - expected_travel) < 0.05f,
            "obstacle-capable forward travel was not applied");
    }

    const sim::CreatureBlueprint crawler4 = sim::CreatureBlueprint::crawler4();
    const sim::CreatureBlueprint hexapod = sim::CreatureBlueprint::hexapod();
    require(crawler4.nodes.size() >= 9 && crawler4.bones.size() >= 10,
        "four-legged crawler geometry is incomplete");
    require(hexapod.nodes.size() >= 12 && hexapod.bones.size() >= 16,
        "six-legged hexapod geometry is incomplete");
'''
replace_once("tests/core_tests.cpp", old_humanoid_calibration, new_humanoid_calibration)

insert_before("MISSIONS.md",
'''## Current warning
''',
'''## WALK-LOCO-004 — Obstacle-capable bipeds, quadrupeds, and multi-leg enemies

**Status:** ACTIVE

The biped still fails to establish a usable gait, and the quadruped can stall and quiver at hazards because the old safe motor envelope cannot lift a leg high enough. Add explicit four-legged and six-legged sand-sim enemy bodies and eliminate the high-energy no-lift local optimum.

**Acceptance:**

- Biped and humanoid hips/knees have bounded travel sufficient to clear configured rocks and hurdles.
- Quadruped can articulate a foot above the first debris target without excessive joint strength.
- High-energy obstacle quivering with no useful leg lift is detected, penalized, and eventually invalidated.
- Approaching a rock or hurdle creates a measurable foot-lift objective before collision.
- Four-legged crawler and six-legged hexapod presets are structurally valid, selectable, trainable, and use semantic foot clusters rather than treating extra feet as body contact.
- Deterministic tests cover all built-in presets, leg travel, support clustering, obstacle approach, and quiver rejection.
- No release is prepared until every non-visual `OPEN` or `ACTIVE` mission in this ledger has passing evidence.

''')

replace_once("README.md",
'''The **Rig Lab** remains available for inspecting joints, testing individual motors or groups, selecting A/Pivot/C, changing safe travel limits, and manually correcting geometry.''',
'''The **Rig Lab** remains available for inspecting joints, testing individual motors or groups, selecting A/Pivot/C, changing safe travel limits, and manually correcting geometry. Built-in enemy bodies now include the humanoid and basic bipeds, the original quadruped, a four-legged crawler, and a six-legged hexapod.''')

print("Applied mission-cache locomotion integration")
