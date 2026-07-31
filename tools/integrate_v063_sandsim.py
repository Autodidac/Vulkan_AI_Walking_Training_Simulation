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


def replace_all(path: str, old: str, new: str, expected: int) -> None:
    text = read(path)
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"{path}: expected {expected} matches, found {count}: {old[:120]!r}")
    write(path, text.replace(old, new))


replace_once("CMakeLists.txt", "project(EpochRunner VERSION 0.6.2 LANGUAGES CXX)",
             "project(EpochRunner VERSION 0.6.3 LANGUAGES CXX)")

replace_once("src/main.cpp",
             '"EpochRunner v" EPOCHRUNNER_VERSION " - Autonomous Vulkan Locomotion Lab",\n        1760,\n        1040,',
             '"EpochRunner v" EPOCHRUNNER_VERSION " - Sand-Sim Enemy Locomotion Trainer",\n        1900,\n        1180,')
replace_once("src/main.cpp", "SDL_SetWindowMinimumSize(window, 1100, 760);",
             "SDL_SetWindowMinimumSize(window, 1280, 820);")

replace_once("src/app.cpp", "constexpr float ui_font_scale = 1.55f;",
             "constexpr float ui_font_scale = 2.05f;")
replace_all("src/app.cpp", "float maximum_width, float minimum_scale = 0.92f) noexcept",
            "float maximum_width, float minimum_scale = 1.05f) noexcept", 1)
replace_all("src/app.cpp", "float maximum_width, float minimum_scale = 0.92f)\n",
            "float maximum_width, float minimum_scale = 1.05f)\n", 1)
replace_once("src/app.cpp", "&& scale > 0.95f)", "&& scale > 1.05f)")
replace_once("src/app.cpp", 'std::filesystem::path autosave_policy_path{ "epochrunner-v062-autosave.eppo" };',
             'std::filesystem::path autosave_policy_path{ "epochrunner-v063-autosave.eppo" };')
replace_once("src/app.cpp", 'std::filesystem::path autosave_rig_path{ "epochrunner-v062-evolved.epochrig" };',
             'std::filesystem::path autosave_rig_path{ "epochrunner-v063-evolved.epochrig" };')
replace_once("src/app.cpp", 'std::filesystem::path autosave_state_path{ "epochrunner-v062-autonomy.state" };',
             'std::filesystem::path autosave_state_path{ "epochrunner-v063-autonomy.state" };')
replace_once("src/app.cpp", '"AUTONOMOUS LOCOMOTION LAB"', '"SAND-SIM ENEMY LOCOMOTION LAB"')
replace_once("src/app.cpp", '"AUTONOMOUS TRAINER"', '"SAND-SIM ENEMY TRAINER"')
replace_once("src/app.cpp",
'''                const Rect sign{ top + Vec2{ -43.0f, -22.0f }, { 86.0f, 21.0f } };
                add_rounded_rect(canvas, sign, 4.0f, rgb(0x102431, 0.94f), accent, 1.0f);
                add_text(canvas, sign.position + Vec2{ 5.0f, 5.0f },
                    std::format("{:.0f} M / {:.3f} MI", distance, distance / 1609.344f),
                    0.76f, white);''',
'''                const Rect sign{ top + Vec2{ -62.0f, -28.0f }, { 124.0f, 28.0f } };
                add_rounded_rect(canvas, sign, 5.0f, rgb(0x102431, 0.96f), accent, 1.0f);
                add_text(canvas, sign.position + Vec2{ 7.0f, 6.0f },
                    std::format("{:.0f} M / {:.3f} MI", distance, distance / 1609.344f),
                    1.02f, white);''')
replace_once("src/app.cpp",
'''                add_text(canvas, feature_screen + Vec2{ -42.0f, -36.0f },
                    sim::course_feature_name(feature.kind), 0.82f,
                    feature.kind == sim::CourseFeatureKind::projectile ? yellow : muted);''',
'''                add_text(canvas, feature_screen + Vec2{ -58.0f, -42.0f },
                    std::format("HAZARD: {}", sim::course_feature_name(feature.kind)), 1.00f,
                    feature.kind == sim::CourseFeatureKind::projectile ? yellow : danger);''')
replace_once("src/app.cpp",
             '"REAL FEET / SOFT START / AUTOMATIC CHECKPOINTS AND RIG EVOLUTION"',
             '"GROUND-CONTACT ENEMY / SOFT START / CHECKPOINTED RIG EVOLUTION"')
replace_once("src/app.cpp",
             '"A NEW VERIFIED BEST IS APPLIED AT THE NEXT LIVE RUN"',
             '"NO ROLLING / NO BODY-SURFING / HAZARDS NEVER PAY REWARD"')
replace_once("src/app.cpp",
             '"LIVE BEST CONTROLLER   v" EPOCHRUNNER_VERSION "   BACKGROUND TRAINING ACTIVE"',
             '"LIVE SAND-SIM ENEMY CONTROLLER   v" EPOCHRUNNER_VERSION "   BACKGROUND TRAINING ACTIVE"')
replace_once("src/app.cpp",
             "const float panel_width = std::clamp(content.size.x * 0.40f, 500.0f, 590.0f);",
             "const float panel_width = std::clamp(content.size.x * 0.42f, 650.0f, 720.0f);")
replace_once("src/app.cpp",
             "const float panel_width = std::clamp(content.size.x * 0.40f, 540.0f, 640.0f);",
             "const float panel_width = std::clamp(content.size.x * 0.42f, 680.0f, 760.0f);")
replace_once("src/app.cpp", "if (content.size.x < 760.0f || content.size.y < 560.0f)",
             "if (content.size.x < 1080.0f || content.size.y < 640.0f)")
replace_once("src/app.cpp", "0.98f, muted, usable_width, 4.0f);",
             "1.06f, muted, usable_width, 4.0f);")
replace_once("src/app.cpp", "0.98f, muted, usable_width, 4.0f);",
             "1.06f, muted, usable_width, 4.0f);")
replace_once("src/app.cpp", "0.96f, muted, overlay_width, 0.82f);",
             "1.05f, muted, overlay_width, 1.00f);")

replace_once("src/simulation.hpp", 'case CourseStage::balance: return "BALANCE";',
             'case CourseStage::balance: return "SPAWN STANCE";')
replace_once("src/simulation.hpp", 'case CourseStage::walk: return "FLAT WALK";',
             'case CourseStage::walk: return "FLAT SAND PATROL";')
replace_once("src/simulation.hpp", 'case CourseStage::ramps: return "RAMPS";',
             'case CourseStage::ramps: return "SAND MOUNDS";')
replace_once("src/simulation.hpp", 'case CourseStage::uneven: return "UNEVEN TERRAIN";',
             'case CourseStage::uneven: return "LOOSE / DEFORMED SAND";')
replace_once("src/simulation.hpp", 'case CourseStage::hurdles: return "HURDLES";',
             'case CourseStage::hurdles: return "FLAT DEBRIS";')
replace_once("src/simulation.hpp", 'case CourseStage::duck_bars: return "DUCK UNDER BARS";',
             'case CourseStage::duck_bars: return "LOW-CLEARANCE DEBRIS";')
replace_once("src/simulation.hpp", 'case CourseStage::moving_hazards: return "MOVING HAZARDS";',
             'case CourseStage::moving_hazards: return "COMBAT TRAVERSAL";')
replace_once("src/simulation.hpp",
'''    [[nodiscard]] inline bool wheel_sliding_motion(float root_speed, bool left_supported,
        bool right_supported, float stance_slip_speed) noexcept
    {
        return left_supported && right_supported
            && std::abs(root_speed) > 0.22f && stance_slip_speed > 0.18f;
    }
''',
'''    [[nodiscard]] inline bool wheel_sliding_motion(float root_speed, bool left_supported,
        bool right_supported, float stance_slip_speed) noexcept
    {
        return left_supported && right_supported
            && std::abs(root_speed) > 0.22f && stance_slip_speed > 0.18f;
    }

    [[nodiscard]] inline bool rolling_body_motion(float root_speed, float torso_turn_speed,
        float uprightness, bool feet_supported, bool non_foot_grounded) noexcept
    {
        return non_foot_grounded
            && (std::abs(torso_turn_speed) > 0.45f || uprightness < 0.55f)
            && (!feet_supported || std::abs(root_speed) > 0.08f);
    }

    inline constexpr float terrain_cycle_length_m = 56.0f;

    [[nodiscard]] inline bool course_zone_is_flat(float course_distance) noexcept
    {
        float local = std::fmod(std::max(0.0f, course_distance), terrain_cycle_length_m);
        if (local < 0.0f)
            local += terrain_cycle_length_m;
        return local < 28.0f || local >= 44.0f;
    }

    [[nodiscard]] inline bool obstacles_require_flat_zone(CourseStage stage,
        float difficulty) noexcept
    {
        return stage != CourseStage::moving_hazards || difficulty < 0.70f;
    }
''')
replace_once("src/simulation.hpp", "        wheel_sliding\n    };",
             "        wheel_sliding,\n        body_rolling\n    };")
replace_once("src/simulation.hpp", 'case InvalidMotion::wheel_sliding: return "WHEEL-SLIDING EXPLOIT";',
             'case InvalidMotion::wheel_sliding: return "WHEEL-SLIDING EXPLOIT";\n        case InvalidMotion::body_rolling: return "HEAD / TAIL / BODY ROLLING";')
replace_once("src/simulation.hpp",
'''    [[nodiscard]] inline bool recovery_should_start(bool collided,
        float uprightness, bool geometric_fall, bool hard_fall) noexcept
    {
        constexpr float destabilized_collision_uprightness = 0.88f;
        constexpr float independent_recovery_uprightness = 0.72f;
        return !hard_fall && (
            (collided && uprightness < destabilized_collision_uprightness)
            || uprightness < independent_recovery_uprightness
            || geometric_fall);
    }''',
'''    [[nodiscard]] inline bool recovery_should_start(bool collided,
        float uprightness, bool geometric_fall, bool hard_fall) noexcept
    {
        static_cast<void>(collided);
        constexpr float independent_recovery_uprightness = 0.72f;
        return !hard_fall
            && (uprightness < independent_recovery_uprightness || geometric_fall);
    }''')
replace_once("src/simulation.hpp", "            return 0.96f;", "            return 0.985f;")
replace_once("src/simulation.hpp",
'''    [[nodiscard]] inline int first_course_feature_sequence(float root_x, float course_progress,
        float spacing = course_marker_spacing_m, float lead_distance = 4.5f) noexcept
    {
        return static_cast<int>(std::ceil((root_x + course_progress + lead_distance) / spacing));
    }''',
'''    [[nodiscard]] inline int first_course_feature_sequence(float root_x, float course_progress,
        float spacing = course_marker_spacing_m, float trailing_distance = 6.0f) noexcept
    {
        return static_cast<int>(std::ceil(
            (root_x + course_progress - trailing_distance) / spacing));
    }''')
replace_once("src/simulation.hpp",
'''        [[nodiscard]] float course_speed() const noexcept
        {
            return course_stage_ == CourseStage::balance ? 0.0f : 0.70f + course_difficulty_ * 0.90f;
        }''',
'''        [[nodiscard]] float course_speed() const noexcept
        {
            if (course_stage_ == CourseStage::balance)
                return 0.0f;
            if (static_cast<std::uint8_t>(course_stage_)
                < static_cast<std::uint8_t>(CourseStage::hurdles))
                return 0.68f + course_difficulty_ * 0.72f;
            return 1.05f + course_difficulty_ * 0.82f;
        }''')
replace_once("src/simulation.hpp",
'''        [[nodiscard]] float stance_slip_speed() const noexcept { return stance_slip_speed_; }
        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }''',
'''        [[nodiscard]] float stance_slip_speed() const noexcept { return stance_slip_speed_; }
        [[nodiscard]] bool non_foot_grounded() const noexcept { return non_foot_grounded_; }
        [[nodiscard]] float body_rolling_seconds() const noexcept { return body_rolling_seconds_; }
        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }''')
replace_once("src/simulation.hpp",
'''        [[nodiscard]] bool contact_supported(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] float contact_cluster_front_x(std::uint16_t contact_node) const noexcept;''',
'''        [[nodiscard]] bool contact_supported(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] bool non_foot_ground_contact() const noexcept;
        [[nodiscard]] bool head_ground_contact() const noexcept;
        [[nodiscard]] float torso_roll_angle() const noexcept;
        [[nodiscard]] float contact_cluster_front_x(std::uint16_t contact_node) const noexcept;''')
replace_once("src/simulation.hpp",
'''        float wheel_sliding_seconds_{};
        float stance_slip_speed_{};
        bool knee_first_this_step_{};''',
'''        float wheel_sliding_seconds_{};
        float body_rolling_seconds_{};
        float head_contact_seconds_{};
        float previous_torso_angle_{};
        float torso_turn_speed_{};
        float stance_slip_speed_{};
        bool non_foot_grounded_{};
        bool knee_first_this_step_{};''')

old_ground = '''    float Environment::ground_height_at(float x) const noexcept
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
    }'''
new_ground = '''    float Environment::ground_height_at(float x) const noexcept
    {
        if (course_stage_ == CourseStage::balance || course_stage_ == CourseStage::walk)
            return 0.0f;

        const float course_x = std::max(0.0f, x + course_progress());
        const float local = std::fmod(course_x, terrain_cycle_length_m);
        const float amplitude = 0.18f + course_difficulty_ * 0.42f;
        float height = 0.0f;

        if (local >= 28.0f && local < 34.0f)
        {
            const float t = (local - 28.0f) / 6.0f;
            const float smooth = t * t * (3.0f - 2.0f * t);
            height = amplitude * smooth;
        }
        else if (local >= 34.0f && local < 38.0f)
        {
            height = amplitude;
        }
        else if (local >= 38.0f && local < 44.0f)
        {
            const float t = (local - 38.0f) / 6.0f;
            const float smooth = t * t * (3.0f - 2.0f * t);
            height = amplitude * (1.0f - smooth);
        }

        if (course_stage_ >= CourseStage::uneven && !course_zone_is_flat(course_x))
        {
            const float roughness = course_difficulty_ * 0.065f;
            height += std::sin(course_x * 0.83f) * roughness;
            height += std::sin(course_x * 2.17f + 0.7f) * roughness * 0.42f;
        }
        return std::max(-0.06f, height);
    }'''
replace_once("src/simulation.cpp", old_ground, new_ground)
replace_once("src/simulation.cpp",
'''        if (course_stage_ == CourseStage::balance || course_stage_ == CourseStage::walk)
            return;''',
'''        if (static_cast<std::uint8_t>(course_stage_)
            < static_cast<std::uint8_t>(CourseStage::hurdles))
            return;''')
replace_once("src/simulation.cpp",
'''            const int sequence = first_sequence + offset;
            if (sequence < course_safe_runway_markers)
                continue;

            const float variation = variation_for(sequence);''',
'''            const int sequence = first_sequence + offset;
            if (sequence < course_safe_runway_markers)
                continue;
            const float marker_distance = course_marker_distance_m(sequence);
            if (obstacles_require_flat_zone(course_stage_, course_difficulty_)
                && !course_zone_is_flat(marker_distance))
                continue;

            const float variation = variation_for(sequence);''')
replace_once("src/simulation.cpp",
'''            const float radius = index < blueprint_.radii.size() ? blueprint_.radii[index] : 0.15f;
            particles_.push_back({ position, position, index == blueprint_.head_node ? 0.65f : 1.0f, radius, false });''',
'''            const float radius = index < blueprint_.radii.size() ? blueprint_.radii[index] : 0.15f;
            const bool contact_semantic = index == blueprint_.left_contact_node
                || index == blueprint_.right_contact_node;
            const std::size_t degree = static_cast<std::size_t>(std::ranges::count_if(
                blueprint_.bones, [index](const DistanceConstraint& bone)
                {
                    return bone.a == index || bone.b == index;
                }));
            float inverse_mass = 1.0f;
            if (index == blueprint_.head_node)
                inverse_mass = 1.25f;
            else if (degree == 1u && !contact_semantic && index != blueprint_.root_node
                && index != blueprint_.torso_node)
                inverse_mass = 1.18f;
            particles_.push_back({ position, position, inverse_mass, radius, false });''')
replace_once("src/simulation.cpp",
'''        previous_pelvis_ = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position : Vec2{};
        previous_angles_.fill(0.0f);''',
'''        previous_pelvis_ = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position : Vec2{};
        previous_torso_angle_ = torso_roll_angle();
        previous_angles_.fill(0.0f);''')
replace_once("src/simulation.cpp",
'''        wheel_sliding_seconds_ = 0.0f;
        stance_slip_speed_ = 0.0f;
        knee_first_this_step_ = false;''',
'''        wheel_sliding_seconds_ = 0.0f;
        body_rolling_seconds_ = 0.0f;
        head_contact_seconds_ = 0.0f;
        torso_turn_speed_ = 0.0f;
        stance_slip_speed_ = 0.0f;
        non_foot_grounded_ = false;
        knee_first_this_step_ = false;''')
replace_once("src/simulation.cpp",
'''    bool Environment::contact_supported(std::uint16_t contact_node) const noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (particles_[index].grounded && contact_cluster_contains(contact_node, index))
                return true;
        }
        return false;
    }
''',
'''    bool Environment::contact_supported(std::uint16_t contact_node) const noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (particles_[index].grounded && contact_cluster_contains(contact_node, index))
                return true;
        }
        return false;
    }

    bool Environment::non_foot_ground_contact() const noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!particles_[index].grounded)
                continue;
            const bool left_foot = contact_cluster_contains(blueprint_.left_contact_node, index);
            const bool right_foot = contact_cluster_contains(blueprint_.right_contact_node, index);
            if (!left_foot && !right_foot)
                return true;
        }
        return false;
    }

    bool Environment::head_ground_contact() const noexcept
    {
        return valid_node(blueprint_.head_node) && particles_[blueprint_.head_node].grounded;
    }

    float Environment::torso_roll_angle() const noexcept
    {
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node))
            return 0.0f;
        const Vec2 current = particles_[blueprint_.torso_node].position
            - particles_[blueprint_.root_node].position;
        const Vec2 desired = blueprint_.nodes[blueprint_.torso_node]
            - blueprint_.nodes[blueprint_.root_node];
        return signed_angle(desired, current);
    }
''')
replace_once("src/simulation.cpp",
'''        const float root_speed = valid_node(blueprint_.root_node)
            ? (particles_[blueprint_.root_node].position.x
                - particles_[blueprint_.root_node].previous.x) / std::max(dt, 1.0e-5f)
            : 0.0f;
        if (course_stage_ != CourseStage::balance
            && wheel_sliding_motion(root_speed, left, right, stance_slip_speed_))''',
'''        const float root_speed = valid_node(blueprint_.root_node)
            ? (particles_[blueprint_.root_node].position.x
                - particles_[blueprint_.root_node].previous.x) / std::max(dt, 1.0e-5f)
            : 0.0f;
        const float torso_angle = torso_roll_angle();
        torso_turn_speed_ = wrap_angle(torso_angle - previous_torso_angle_)
            / std::max(dt, 1.0e-5f);
        previous_torso_angle_ = torso_angle;
        non_foot_grounded_ = non_foot_ground_contact();
        const bool feet_supported = left || right;
        if (rolling_body_motion(root_speed, torso_turn_speed_, torso_uprightness(),
            feet_supported, non_foot_grounded_))
            body_rolling_seconds_ += dt;
        else
            body_rolling_seconds_ = std::max(0.0f, body_rolling_seconds_ - dt * 2.0f);
        if (head_ground_contact())
            head_contact_seconds_ += dt;
        else
            head_contact_seconds_ = std::max(0.0f, head_contact_seconds_ - dt * 3.0f);
        const float rolling_limit = course_stage_ == CourseStage::balance ? 0.55f : 0.32f;
        if (body_rolling_seconds_ > rolling_limit || head_contact_seconds_ > 0.24f)
            invalidate(InvalidMotion::body_rolling);

        if (course_stage_ != CourseStage::balance
            && wheel_sliding_motion(root_speed, left, right, stance_slip_speed_))''')
replace_once("src/simulation.cpp",
'''        if (recovery_active_)
        {
            const float improvement = upright - recovery_best_upright_;
            if (improvement > 0.0f)
                recovery_reward += improvement * 0.10f;
            recovery_best_upright_ = std::max(recovery_best_upright_, upright);
            const float recovery_time = elapsed_seconds_ - recovery_started_seconds_;
            if (upright >= 0.90f && supported && !geometric_fall && recovery_time >= 0.12f)
            {
                recovery_active_ = false;
                ++recovery_successes_;
                recovery_reward += 0.14f;
            }
            else if (hard_fall || recovery_time > 3.0f)
            {
                recovery_active_ = false;
                recovery_reward -= 0.10f;
            }
        }''',
'''        if (recovery_active_)
        {
            recovery_best_upright_ = std::max(recovery_best_upright_, upright);
            const float recovery_time = elapsed_seconds_ - recovery_started_seconds_;
            if (upright >= 0.90f && supported && !geometric_fall && recovery_time >= 0.12f)
            {
                recovery_active_ = false;
                ++recovery_successes_;
            }
            else if (hard_fall || recovery_time > 3.0f)
            {
                recovery_active_ = false;
                recovery_reward -= 0.12f;
            }
        }''')
replace_once("src/simulation.cpp",
'''        const float gait = gait_progress_multiplier(alternating_steps_,
            single_support, swing_clearance);
        const float safe_progress = clamp(frame_progress, -0.015f, 0.065f);
        const float collision_penalty = collided_this_step_ ? 0.025f : 0.0f;''',
'''        const float gait = non_foot_grounded_ ? 0.0f
            : gait_progress_multiplier(alternating_steps_, single_support, swing_clearance);
        const float safe_progress = clamp(frame_progress, -0.015f, 0.065f);
        const float collision_penalty = collided_this_step_ ? 0.070f : 0.0f;''')
replace_once("src/simulation.cpp",
'''        const float swing_reward = single_support && swing_clearance > 0.10f
            ? clamp(swing_clearance, 0.0f, 0.45f) * 0.004f : 0.0f;

        if (course_stage_ == CourseStage::balance)''',
'''        const float swing_reward = single_support && swing_clearance > 0.10f
            ? clamp(swing_clearance, 0.0f, 0.45f) * 0.004f : 0.0f;
        const float body_contact_penalty = non_foot_grounded_
            ? (head_ground_contact() ? 0.16f : 0.08f) : 0.0f;

        if (course_stage_ == CourseStage::balance)''')
replace_once("src/simulation.cpp",
'''                - std::abs(distance_travelled_) * 0.0015f
                - action_energy * 0.0012f;''',
'''                - std::abs(distance_travelled_) * 0.0015f
                - action_energy * 0.0012f
                - body_contact_penalty;''')
replace_once("src/simulation.cpp",
'''                - stance_slip_penalty
                - wheel_penalty;''',
'''                - stance_slip_penalty
                - wheel_penalty
                - body_contact_penalty;''')
replace_once("src/simulation.cpp",
             "const float timeout = course_stage_ == CourseStage::balance ? 12.0f : 20.0f;",
             "const float timeout = course_stage_ == CourseStage::balance ? 12.0f\n            : static_cast<std::uint8_t>(course_stage_) >= static_cast<std::uint8_t>(CourseStage::hurdles)\n                ? 32.0f : 24.0f;")
replace_once("src/simulation.cpp",
             "result[17] = recovery_active_ ? clamp(torso.y, -1.0f, 1.0f) : 1.0f;",
             "result[17] = non_foot_grounded_ ? -1.0f\n            : recovery_active_ ? clamp(torso.y, -1.0f, 1.0f) : 1.0f;")

replace_once("src/autonomy_curriculum.cpp",
'''        case sim::CourseStage::walk:
            return metrics.evaluation_distance >= 3.0f && metrics.evaluation_stride_events >= 3.0f;
        case sim::CourseStage::ramps:
            return metrics.evaluation_distance >= 3.5f && metrics.evaluation_survival >= 7.0f;
        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 4.0f && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::hurdles:
            return metrics.evaluation_distance >= 5.0f && metrics.evaluation_collisions <= 3.0f;
        case sim::CourseStage::duck_bars:
            return metrics.evaluation_distance >= 5.0f && metrics.evaluation_collisions <= 3.0f;
        case sim::CourseStage::moving_hazards:
            return metrics.evaluation_distance >= 6.0f && metrics.evaluation_collisions <= 4.0f;''',
'''        case sim::CourseStage::walk:
            return metrics.evaluation_distance >= 4.0f && metrics.evaluation_stride_events >= 4.0f;
        case sim::CourseStage::ramps:
            return metrics.evaluation_distance >= 5.0f && metrics.evaluation_survival >= 8.0f;
        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 6.0f && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::hurdles:
            return metrics.evaluation_distance >= 7.0f && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::duck_bars:
            return metrics.evaluation_distance >= 8.0f && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::moving_hazards:
            return metrics.evaluation_distance >= 9.0f && metrics.evaluation_collisions <= 2.0f;''')
replace_once("src/autonomy_curriculum.cpp",
             '"INVALID RUN REJECTED - {} OF 6 FAILED WALKING GATES"',
             '"INVALID RUN REJECTED - {} FAILED GROUNDED-ENEMY GATES"')
replace_once("src/autonomy_curriculum.cpp",
'''        constexpr std::size_t agents = 4;
        constexpr int maximum_steps = 600;
        std::array<float, agents> scores{};
        std::array<std::jthread, agents> evaluators{};
        const sim::CourseStage stage = stage_;
        const float difficulty = difficulty_;''',
'''        constexpr std::size_t agents = 4;
        const sim::CourseStage stage = stage_;
        const float difficulty = difficulty_;
        const int maximum_steps = static_cast<std::uint8_t>(stage)
            >= static_cast<std::uint8_t>(sim::CourseStage::hurdles) ? 1500 : 900;
        std::array<float, agents> scores{};
        std::array<std::jthread, agents> evaluators{};''')
replace_once("src/autonomy_curriculum.cpp",
             "const bool gait_valid = stage == sim::CourseStage::balance || environment.alternating_steps() >= 2;",
             "const bool gait_valid = stage == sim::CourseStage::balance || environment.alternating_steps() >= 3;")
replace_once("src/autonomy_curriculum.cpp",
'''                scores[agent] = reward + environment.distance_travelled() * 0.60f
                    + environment.elapsed_seconds() * 0.04f
                    + static_cast<float>(environment.alternating_steps()) * 0.02f
                    - environment.collision_count() * 0.18f
                    - environment.airborne_ratio() * 0.70f;''',
'''                scores[agent] = reward + environment.distance_travelled() * 0.75f
                    + environment.elapsed_seconds() * 0.03f
                    + static_cast<float>(environment.alternating_steps()) * 0.03f
                    - environment.collision_count() * 0.30f
                    - environment.airborne_ratio() * 1.00f
                    - environment.body_rolling_seconds() * 2.00f;''')

replace_once("tests/core_tests.cpp",
'''    require(sim::recovery_should_start(true, 0.80f, false, false),
        "destabilizing collision did not start recovery");''',
'''    require(!sim::recovery_should_start(true, 0.80f, false, false),
        "ordinary obstacle contact still opens a rewardable recovery event");''')
replace_once("tests/core_tests.cpp",
'''    require(!sim::wheel_sliding_motion(0.45f, true, false, 0.50f),
        "single-support walking is incorrectly classified as wheel sliding");''',
'''    require(!sim::wheel_sliding_motion(0.45f, true, false, 0.50f),
        "single-support walking is incorrectly classified as wheel sliding");
    require(sim::rolling_body_motion(0.20f, 0.70f, 0.35f, false, true),
        "head, tail, or body rolling is not detected");
    require(!sim::rolling_body_motion(0.20f, 0.70f, 0.95f, true, false),
        "normal foot-supported walking is incorrectly classified as rolling");
    require(sim::course_zone_is_flat(24.0f) && sim::course_zone_is_flat(48.0f),
        "long flat sand-sim patrol zones are missing");
    require(!sim::course_zone_is_flat(32.0f) && !sim::course_zone_is_flat(40.0f),
        "sand mounds are not separated from flat patrol zones");
    require(sim::obstacles_require_flat_zone(sim::CourseStage::hurdles, 1.0f),
        "early debris training can place obstacles on hills");
    require(!sim::obstacles_require_flat_zone(sim::CourseStage::moving_hazards, 0.75f),
        "advanced combat traversal never combines hazards with terrain");
    require(sim::first_course_feature_sequence(0.0f, 29.9f) <= 3,
        "a contacted obstacle is culled like a pickup before it passes behind the actor");''')
replace_once("tests/core_tests.cpp", "const float initial_height = procedural.ground_height_at(7.5f);",
             "const float initial_height = procedural.ground_height_at(29.0f);")
replace_once("tests/core_tests.cpp",
             "require(std::abs(procedural.ground_height_at(7.5f) - initial_height) > 0.001f,",
             "require(std::abs(procedural.ground_height_at(29.0f) - initial_height) > 0.001f,")
replace_once("tests/core_tests.cpp",
             "procedural.set_course(sim::CourseStage::moving_hazards, 0.65f);",
             "procedural.set_course(sim::CourseStage::moving_hazards, 0.75f);")
replace_once("tests/core_tests.cpp",
'''    rl::PpoTrainer trainer{ humanoid, 16 };''',
'''    {
        sim::Environment flat_obstacles{ humanoid, 0xF1A7u };
        flat_obstacles.set_course(sim::CourseStage::hurdles, 0.45f);
        require(!flat_obstacles.course_features().empty(),
            "flat debris lesson has no obstacles");
        for (const sim::CourseFeature& feature : flat_obstacles.course_features())
        {
            require(sim::course_zone_is_flat(sim::course_marker_distance_m(feature.marker_sequence)),
                "early obstacle curriculum placed debris on a hill or slope");
        }
    }

    rl::PpoTrainer trainer{ humanoid, 16 };''')

replace_once("README.md",
'''EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, vcpkg manifest mode, and a compact PPO controller. Version 0.6.2 restores real biped foot support and traction, prevents head/tail/body contacts from pinning the actor, and keeps rocks and other course debris anchored to the moving course instead of the creature.
''',
'''EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, vcpkg manifest mode, and a compact PPO controller. Version 0.6.3 retargets the curriculum toward a grounded sand-simulation enemy: large readable telemetry, long flat patrol zones, separated sand mounds, flat early obstacle pads, and hard rejection of head, tail, or body rolling.

## Sand-sim enemy locomotion hotfix

The trainer now starts with spawn stance and flat sand patrol, then introduces isolated sand mounds and loose/deformed terrain before debris. Early rocks, hurdles, and low bars are generated only on flat zones. Terrain-plus-hazard combinations remain locked to the later combat-traversal lesson at higher difficulty.

Head, tail, torso, and other non-foot ground contacts no longer provide a locomotion path. Sustained body rolling is a hard invalid gate, body-ground motion receives no gait multiplier, head/body contact receives an immediate penalty, and new versioned autosaves prevent the old rolling controller from resuming.

Course features remain physical hazards after contact and are retained until they pass behind the actor instead of disappearing at approach distance like collectible upgrades. Obstacle contact no longer opens a positive recovery-reward opportunity; collisions are strictly costly while recovery remains a survival state.

The bitmap UI now uses a substantially larger global scale, larger minimum fitted text, larger marker signs and hazard labels, wider panels, a larger default window, and sand-sim-specific labels.
''')
replace_once("README.md",
             "Obstacle impacts and large balance errors start a timed recovery objective. Policies receive extra reward for restoring upright supported balance and a penalty when recovery times out.",
             "Large balance errors start a timed recovery state. Obstacle impacts are penalized and never grant a recovery bonus; failed recovery still receives an additional penalty.")
replace_once("README.md",
'''- fails to produce alternating foot contacts after the balance lesson.''',
'''- fails to produce alternating foot contacts after the balance lesson;
- travels on its head, tail, torso, knees, or other non-foot body contacts.''')

replace_once("MISSIONS.md",
'''## WALK-PHYS-001 — Biped support, traction, and world-anchored debris

**Status:** VERIFIED''',
'''## WALK-PHYS-001 — Biped support, traction, and world-anchored debris

**Status:** ACTIVE''')
replace_once("MISSIONS.md",
'''## WALK-UI-002 — Readable responsive telemetry

**Status:** VERIFIED''',
'''## WALK-UI-002 — Readable responsive telemetry

**Status:** ACTIVE''')
replace_once("MISSIONS.md",
'''## WALK-GAIT-002 — Real stepping instead of wheel sliding

**Status:** VERIFIED''',
'''## WALK-GAIT-002 — Real stepping instead of wheel sliding

**Status:** ACTIVE''')
replace_once("MISSIONS.md",
'''## Current warning

EpochRunner v0.6.2 passed the full Windows/Vulkan build, biped support and traction tests, real-step gait and knee-before-foot tests, world-anchored mile-marker obstacle schedule tests, responsive UI compilation, concurrency benchmark, runtime diagnostics, and package gate. Remaining ACTIVE and OPEN missions carry forward unchanged.
''',
'''## WALK-SAND-001 — Sand-sim enemy locomotion curriculum

**Status:** ACTIVE

Retarget the curriculum from a generic treadmill demonstration to a grounded enemy controller suitable for later integration into a cellular sand simulation.

**Acceptance:**

- Spawn stance and flat patrol precede terrain and hazards.
- Long flat sections separate sand mounds and loose/deformed patches.
- Early debris and low-clearance hazards appear only on flat ground.
- Terrain-plus-hazard combinations unlock only in later combat traversal at higher difficulty.
- Deterministic evaluation actually runs long enough to encounter the first hazard.

## WALK-ROLL-003 — Head, tail, and body rolling are invalid locomotion

**Status:** ACTIVE

Non-foot body contact may slide during a fall but may not become a movement strategy.

**Acceptance:**

- Head contact cannot remain grounded long enough to propel the rig.
- Tail, torso, knee, and other non-foot ground contacts are detected semantically.
- Sustained body-ground rotation terminates as `HEAD / TAIL / BODY ROLLING`.
- Body-ground motion receives no gait progress multiplier and receives a strong penalty.
- A new autosave namespace prevents the v0.6.2 rolling policy from resuming.

## WALK-HAZARD-003 — Obstacles are hazards, never pickups or rewards

**Status:** ACTIVE

- Obstacles remain present through contact and are culled only after passing behind the actor.
- Ordinary obstacle contact cannot open a positive recovery-reward loop.
- Collision penalties exceed any incidental contact benefit.
- Hazard labels communicate danger rather than collectible/reward semantics.

## WALK-UI-003 — User-verified readable typography

**Status:** ACTIVE

The previous UI mission was incorrectly closed from compilation evidence without a visual acceptance pass. Increase all bitmap text substantially, enlarge minimum fitted text, marker signs, hazard labels, panels, and the default window. This mission remains active until the packaged application is visually confirmed readable by the user.

## Current warning

The user visually rejected the v0.6.2 typography and confirmed that rolling and pickup-like obstacle behavior remained. Those missions are reopened. v0.6.3 may record build and deterministic-test evidence, but visual readability remains `ACTIVE` until user confirmation. All other ACTIVE and OPEN missions carry forward unchanged.
''')

print("Applied EpochRunner v0.6.3 sand-sim enemy locomotion integration")
