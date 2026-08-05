from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def patch_simulation_header() -> None:
    text = read("src/simulation.hpp")
    anchor = """    struct Particle
    {
"""
    foot_contract = """    enum class FootContactPhase : std::uint8_t
    {
        airborne,
        heel_strike,
        flat,
        toe_off
    };

    [[nodiscard]] inline std::string_view foot_contact_phase_name(
        FootContactPhase phase) noexcept
    {
        switch (phase)
        {
        case FootContactPhase::airborne: return "AIR";
        case FootContactPhase::heel_strike: return "HEEL";
        case FootContactPhase::flat: return "FLAT";
        case FootContactPhase::toe_off: return "TOE";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] inline FootContactPhase classify_foot_contact_phase(
        bool heel, bool ball, bool toe) noexcept
    {
        if (!heel && !ball && !toe)
            return FootContactPhase::airborne;
        if (heel && !toe)
            return FootContactPhase::heel_strike;
        if (toe && !heel)
            return FootContactPhase::toe_off;
        return FootContactPhase::flat;
    }

"""
    text = replace_once(text, anchor, foot_contract + anchor,
        "foot contact phase contract")

    anchor = """    [[nodiscard]] inline float ground_velocity_retention(bool traction_contact,
        float vertical_speed) noexcept
    {
        static_cast<void>(vertical_speed);
        return traction_contact ? 0.0f : 0.985f;
    }
"""
    addition = anchor + """
    [[nodiscard]] inline float foot_friction_retention(float horizontal_speed,
        float firmness, float looseness, bool static_lesson,
        bool toe_contact) noexcept
    {
        firmness = clamp(firmness, 0.0f, 1.0f);
        looseness = clamp(looseness, 0.0f, 1.0f);
        const float static_limit = std::max(0.035f,
            0.08f + firmness * 0.18f - looseness * 0.06f);
        if (std::abs(horizontal_speed) <= static_limit)
            return 0.0f;
        float retention = 0.30f - firmness * 0.22f + looseness * 0.10f;
        if (static_lesson)
            retention *= 0.35f;
        if (toe_contact)
            retention = std::max(retention, 0.060f);
        return clamp(retention, 0.0f, 0.42f);
    }

    [[nodiscard]] inline bool qualifies_crossing_step(int previous_side,
        int strike_side, float seconds_since_previous, float root_displacement,
        float swing_air_seconds, float swing_clearance, bool swing_crossed,
        bool crossing_required) noexcept
    {
        return (!crossing_required || swing_crossed)
            && qualifies_supported_step(previous_side, strike_side,
                seconds_since_previous, root_displacement,
                swing_air_seconds, swing_clearance);
    }
"""
    text = replace_once(text, anchor, addition,
        "physical foot friction and crossing-step helpers")

    text = replace_once(text,
        """        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }
""",
        """        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }
        [[nodiscard]] std::uint32_t limb_crossings() const noexcept { return limb_crossings_; }
        [[nodiscard]] std::uint32_t heel_strikes() const noexcept { return heel_strike_count_; }
        [[nodiscard]] std::uint32_t toe_offs() const noexcept { return toe_off_count_; }
        [[nodiscard]] FootContactPhase left_foot_phase() const noexcept
        {
            return left_foot_phase_;
        }
        [[nodiscard]] FootContactPhase right_foot_phase() const noexcept
        {
            return right_foot_phase_;
        }
""",
        "gait and foot phase telemetry")
    text = replace_once(text,
        """        [[nodiscard]] float contact_cluster_horizontal_speed(std::uint16_t contact_node,
            float dt) const noexcept;
""",
        """        [[nodiscard]] float contact_cluster_horizontal_speed(std::uint16_t contact_node,
            float dt) const noexcept;
        [[nodiscard]] float contact_cluster_center_x(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] FootContactPhase detect_foot_contact_phase(bool left) const noexcept;
""",
        "foot phase helper declarations")
    text = replace_once(text,
        """        float left_swing_seconds_{};
        float right_swing_seconds_{};
        float left_swing_clearance_{};
        float right_swing_clearance_{};
""",
        """        float left_swing_seconds_{};
        float right_swing_seconds_{};
        float left_swing_clearance_{};
        float right_swing_clearance_{};
        bool left_swing_crossed_{};
        bool right_swing_crossed_{};
        std::uint32_t limb_crossings_{};
        std::uint32_t heel_strike_count_{};
        std::uint32_t toe_off_count_{};
        FootContactPhase left_foot_phase_{ FootContactPhase::airborne };
        FootContactPhase right_foot_phase_{ FootContactPhase::airborne };
""",
        "gait crossing and foot phase state")
    write("src/simulation.hpp", text)


def patch_simulation_source() -> None:
    text = read("src/simulation.cpp")
    text = replace_once(text,
        """    void Environment::separate_support_clusters() noexcept
    {
        std::array<std::uint16_t, 32> supports{};
""",
        """    void Environment::separate_support_clusters() noexcept
    {
        // Side-view locomotion requires the near and far legs to pass through
        // the same screen-space lane. Preserve the fused-foot guard for static
        // lessons, but never push a legitimate swing foot away from the stance
        // foot during walking, crouch-walking, hurdles, or the mixed course.
        if (stage_requires_forward_gait(course_stage_))
            return;
        std::array<std::uint16_t, 32> supports{};
""",
        "stage-aware support separation")

    text = replace_once(text,
        """        left_swing_seconds_ = 0.0f;
        right_swing_seconds_ = 0.0f;
        left_swing_clearance_ = 0.0f;
        right_swing_clearance_ = 0.0f;
""",
        """        left_swing_seconds_ = 0.0f;
        right_swing_seconds_ = 0.0f;
        left_swing_clearance_ = 0.0f;
        right_swing_clearance_ = 0.0f;
        left_swing_crossed_ = false;
        right_swing_crossed_ = false;
        limb_crossings_ = 0u;
        heel_strike_count_ = 0u;
        toe_off_count_ = 0u;
        left_foot_phase_ = FootContactPhase::airborne;
        right_foot_phase_ = FootContactPhase::airborne;
""",
        "reset crossing and foot phase state")

    anchor = """    float Environment::contact_cluster_clearance(std::uint16_t contact_node) const noexcept
"""
    implementation = """    float Environment::contact_cluster_center_x(
        std::uint16_t contact_node) const noexcept
    {
        float accumulated = 0.0f;
        std::size_t count = 0u;
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!contact_cluster_contains(contact_node, index))
                continue;
            accumulated += particles_[index].position.x;
            ++count;
        }
        return count == 0u ? 0.0f : accumulated / static_cast<float>(count);
    }

    FootContactPhase Environment::detect_foot_contact_phase(bool left) const noexcept
    {
        const std::uint16_t heel = left
            ? blueprint_.left_contact_node : blueprint_.right_contact_node;
        const auto& extra = left
            ? blueprint_.additional_left_contact_nodes
            : blueprint_.additional_right_contact_nodes;
        const bool heel_grounded = valid_node(heel) && particles_[heel].grounded;
        if (extra.size() < 2u)
            return heel_grounded ? FootContactPhase::flat : FootContactPhase::airborne;
        const std::uint16_t ball = extra[0];
        const std::uint16_t toe = extra[1];
        const bool ball_grounded = valid_node(ball) && particles_[ball].grounded;
        const bool toe_grounded = valid_node(toe) && particles_[toe].grounded;
        return classify_foot_contact_phase(
            heel_grounded, ball_grounded, toe_grounded);
    }

"""
    text = replace_once(text, anchor, implementation + anchor,
        "foot center and phase implementation")

    old = """                float retention = ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact && stage_uses_deformable_terrain(course_stage_))
                    retention = std::lerp(0.24f, 0.015f, firmness);
                if (blueprint_.is_support_seed(index))
                {
                    const float stance_retention = (course_stage_ == CourseStage::balance
                            || course_stage_ == CourseStage::duck_press)
                        ? 0.004f : 0.024f;
                    retention = std::min(retention, stance_retention);
                }
"""
    new = """                float retention = ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact)
                {
                    const bool left_toe = blueprint_.additional_left_contact_nodes.size() >= 2u
                        && index == blueprint_.additional_left_contact_nodes[1];
                    const bool right_toe = blueprint_.additional_right_contact_nodes.size() >= 2u
                        && index == blueprint_.additional_right_contact_nodes[1];
                    const bool static_lesson = course_stage_ == CourseStage::balance
                        || course_stage_ == CourseStage::duck_press;
                    retention = foot_friction_retention(velocity.x,
                        firmness, looseness, static_lesson,
                        left_toe || right_toe);
                }
"""
    text = replace_once(text, old, new,
        "terrain-aware static and dynamic foot friction")

    old = """        const float left_clearance = contact_cluster_clearance(blueprint_.left_contact_node);
        const float right_clearance = contact_cluster_clearance(blueprint_.right_contact_node);
        if (!left)
        {
            left_swing_seconds_ += dt;
            left_swing_clearance_ = std::max(left_swing_clearance_, left_clearance);
        }
        if (!right)
        {
            right_swing_seconds_ += dt;
            right_swing_clearance_ = std::max(right_swing_clearance_, right_clearance);
        }
"""
    new = """        const float left_clearance = contact_cluster_clearance(blueprint_.left_contact_node);
        const float right_clearance = contact_cluster_clearance(blueprint_.right_contact_node);
        const float left_center = contact_cluster_center_x(blueprint_.left_contact_node);
        const float right_center = contact_cluster_center_x(blueprint_.right_contact_node);
        if (!left && previous_left_grounded_)
            left_swing_crossed_ = false;
        if (!right && previous_right_grounded_)
            right_swing_crossed_ = false;
        if (!left)
        {
            left_swing_seconds_ += dt;
            left_swing_clearance_ = std::max(left_swing_clearance_, left_clearance);
            left_swing_crossed_ = left_swing_crossed_
                || (left_clearance >= 0.065f && left_center > right_center + 0.035f);
        }
        if (!right)
        {
            right_swing_seconds_ += dt;
            right_swing_clearance_ = std::max(right_swing_clearance_, right_clearance);
            right_swing_crossed_ = right_swing_crossed_
                || (right_clearance >= 0.065f && right_center > left_center + 0.035f);
        }

        const FootContactPhase next_left_phase = detect_foot_contact_phase(true);
        const FootContactPhase next_right_phase = detect_foot_contact_phase(false);
        if (next_left_phase == FootContactPhase::heel_strike
            && left_foot_phase_ != FootContactPhase::heel_strike)
            ++heel_strike_count_;
        if (next_right_phase == FootContactPhase::heel_strike
            && right_foot_phase_ != FootContactPhase::heel_strike)
            ++heel_strike_count_;
        if (next_left_phase == FootContactPhase::toe_off
            && left_foot_phase_ != FootContactPhase::toe_off)
            ++toe_off_count_;
        if (next_right_phase == FootContactPhase::toe_off
            && right_foot_phase_ != FootContactPhase::toe_off)
            ++toe_off_count_;
        left_foot_phase_ = next_left_phase;
        right_foot_phase_ = next_right_phase;
"""
    text = replace_once(text, old, new,
        "crossing swing and foot contact phase tracking")

    old = """            else if (qualifies_supported_step(last_contact_side_, strike_side,
                elapsed_seconds_ - last_step_time_, root_x - last_step_x_,
                swing_air_seconds, swing_clearance))
            {
                ++alternating_steps_;
"""
    new = """            else
            {
                const bool swing_crossed = new_left
                    ? left_swing_crossed_ : right_swing_crossed_;
                const bool crossing_required = blueprint_.paired_leg_chains();
                if (!qualifies_crossing_step(last_contact_side_, strike_side,
                    elapsed_seconds_ - last_step_time_, root_x - last_step_x_,
                    swing_air_seconds, swing_clearance,
                    swing_crossed, crossing_required))
                    goto step_not_qualified;
                ++alternating_steps_;
                if (crossing_required)
                    ++limb_crossings_;
"""
    text = replace_once(text, old, new,
        "crossing-required paired step qualification")
    old = """                last_step_x_ = root_x;
            }
        }
        if (left)
"""
    new = """                last_step_x_ = root_x;
                if (new_left)
                    left_swing_crossed_ = false;
                else
                    right_swing_crossed_ = false;
            }
        }
step_not_qualified:
        if (left)
"""
    text = replace_once(text, old, new,
        "crossing step completion label")
    write("src/simulation.cpp", text)


def patch_ppo() -> None:
    text = read("src/ppo.hpp")
    text = replace_once(text,
        """    inline constexpr std::uint32_t training_semantics_version = 0x0007'1502u;
""",
        """    inline constexpr std::uint32_t training_semantics_version = 0x0007'1503u;
""",
        "crossing gait semantics bump")

    anchor = """    [[nodiscard]] inline std::array<float, sim::action_count> crouch_walk_teacher_action(
"""
    teacher = """    [[nodiscard]] inline std::array<float, sim::action_count> walking_teacher_action(
        const sim::Environment& environment) noexcept
    {
        auto action = balance_teacher_action(environment);
        const sim::CreatureBlueprint& rig = environment.blueprint();
        if (!rig.paired_leg_chains())
            return action;
        const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.12f;
        const float swing = std::sin(phase);
        action[0] = clamp(action[0] + 0.42f * swing, -0.82f, 0.82f);
        action[1] = clamp(action[1] + 0.34f * std::max(0.0f, swing), -0.88f, 0.88f);
        action[2] = clamp(action[2] - 0.42f * swing, -0.82f, 0.82f);
        action[3] = clamp(action[3] - 0.34f * std::max(0.0f, -swing), -0.88f, 0.88f);
        return bilateral_joint_synergy_action(environment, action,
            sim::CourseStage::uneven);
    }

"""
    text = replace_once(text, anchor, teacher + anchor,
        "alternating crossing gait teacher")

    anchor = """        else if (stage == sim::CourseStage::crouch_walk)
"""
    insertion = """        else if (stage == sim::CourseStage::uneven)
        {
            const auto teacher = walking_teacher_action(environment);
            for (std::size_t index = 0; index < std::min<std::size_t>(4u, active); ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.34f);
            for (std::size_t index = 4; index < active; ++index)
                policy_action[index] = lerp(policy_action[index], teacher[index], 0.42f);
        }
        else if (stage == sim::CourseStage::crouch_walk)
"""
    text = replace_once(text, anchor, insertion,
        "walking teacher integration")

    text = replace_once(text,
        """            if (environment.gait_cycles() < 4u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
""",
        """            if (environment.gait_cycles() < 4u
                || (environment.blueprint().paired_leg_chains()
                    && environment.limb_crossings() < 4u))
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
""",
        "uneven crossing qualification")
    text = replace_once(text,
        """            if (environment.gait_cycles() < 4u
                || environment.crouch_walk_seconds() < 2.0f
""",
        """            if (environment.gait_cycles() < 4u
                || (environment.blueprint().paired_leg_chains()
                    && environment.limb_crossings() < 4u)
                || environment.crouch_walk_seconds() < 2.0f
""",
        "crouch-walk crossing qualification")
    text = replace_once(text,
        """            if (environment.alternating_steps() < 3u
                || environment.obstacles_passed() < 1u
""",
        """            if (environment.alternating_steps() < 3u
                || (environment.blueprint().paired_leg_chains()
                    && environment.limb_crossings() < 3u)
                || environment.obstacles_passed() < 1u
""",
        "hurdle crossing qualification")
    write("src/ppo.hpp", text)


def patch_app() -> None:
    text = read("src/app.cpp")
    text = replace_once(text,
        """        std::filesystem::path autosave_policy_path{ "runner-v0715-squat-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "runner-v0715-squat-evolved.rig" };
        std::filesystem::path autosave_state_path{ "runner-v0715-squat-autonomy.state" };
""",
        """        std::filesystem::path autosave_policy_path{ "runner-v0715-gait-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "runner-v0715-gait-evolved.rig" };
        std::filesystem::path autosave_state_path{ "runner-v0715-gait-autonomy.state" };
""",
        "crossing gait autosave isolation")

    old = """            for (const sim::DistanceConstraint& bone : rig.bones)
            {
                if (bone.a >= particles.size() || bone.b >= particles.size())
                    continue;
                const float radius_a = bone.a < rig.radii.size() ? rig.radii[bone.a] : 0.15f;
                const float radius_b = bone.b < rig.radii.size() ? rig.radii[bone.b] : 0.15f;
                const float radius = std::max(0.055f, std::min(radius_a, radius_b) * 0.55f) * scale;
                canvas.capsule(point(bone.a), point(bone.b), radius, body, 16);
            }
"""
    new = """            auto leg_side = [&](std::size_t index) noexcept
            {
                if (!rig.paired_leg_chains())
                    return 0;
                if (rig.is_left_support_seed(index)
                    || index == rig.motors[0].c || index == rig.motors[1].c)
                    return -1;
                if (rig.is_right_support_seed(index)
                    || index == rig.motors[2].c || index == rig.motors[3].c)
                    return 1;
                return 0;
            };
            for (const sim::DistanceConstraint& bone : rig.bones)
            {
                if (bone.a >= particles.size() || bone.b >= particles.size())
                    continue;
                const float radius_a = bone.a < rig.radii.size() ? rig.radii[bone.a] : 0.15f;
                const float radius_b = bone.b < rig.radii.size() ? rig.radii[bone.b] : 0.15f;
                const float radius = std::max(0.055f, std::min(radius_a, radius_b) * 0.55f) * scale;
                const int side = leg_side(bone.a) != 0 ? leg_side(bone.a) : leg_side(bone.b);
                const Color color = side < 0 ? rgb(0x765033)
                    : side > 0 ? leg : body;
                canvas.capsule(point(bone.a), point(bone.b), radius, color, 16);
            }
"""
    text = replace_once(text, old, new,
        "near and far side-view leg rendering")

    text = replace_once(text,
        """                Color color = index == rig.head_node ? body_light : body;
                const bool primary_foot = rig.is_support_seed(index);
""",
        """                Color color = index == rig.head_node ? body_light : body;
                const int side = leg_side(index);
                if (side < 0)
                    color = rgb(0x765033);
                else if (side > 0)
                    color = leg;
                const bool primary_foot = rig.is_support_seed(index);
""",
        "near and far leg node rendering")
    text = replace_once(text,
        """                    color = leg;
                    const Vec2 center = point(index);
""",
        """                    color = side < 0 ? rgb(0x765033) : leg;
                    const Vec2 center = point(index);
""",
        "near and far foot rendering")

    old = """            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 119.0f },
                std::format("STEPS {}  DUCK {:.1f} S  JUMP {}/{}  FLIP {:.1f}  SPIN {:.1f}  PASSED {}",
                    environment.alternating_steps(), environment.duck_seconds(),
                    environment.powered_jumps(), environment.landed_jumps(),
                    environment.maximum_flip_turns(), environment.uncontrolled_spin_turns(),
                    environment.obstacles_passed()),
                1.02f, environment.recovering() ? yellow : muted, overlay_width);
"""
    new = """            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 119.0f },
                std::format("STEPS {}  CROSS {}  HEEL {}  TOE {}  SLIP {:.2f}",
                    environment.alternating_steps(), environment.limb_crossings(),
                    environment.heel_strikes(), environment.toe_offs(),
                    environment.stance_slip_speed()),
                1.02f, environment.recovering() ? yellow : muted, overlay_width);
            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, 147.0f },
                std::format("L {}  R {}  DUCK {:.1f} S  JUMP {}/{}  PASSED {}",
                    sim::foot_contact_phase_name(environment.left_foot_phase()),
                    sim::foot_contact_phase_name(environment.right_foot_phase()),
                    environment.duck_seconds(), environment.powered_jumps(),
                    environment.landed_jumps(), environment.obstacles_passed()),
                0.96f, muted, overlay_width);
"""
    text = replace_once(text, old, new,
        "live foot phase and crossing telemetry")

    old = """                add_text(canvas, screen(index) + Vec2{ 10.0f, -8.0f }, std::to_string(index), 1.05f, white);
"""
    new = """                add_text(canvas, screen(index) + Vec2{ 10.0f, -8.0f }, std::to_string(index), 1.05f, white);
                std::string_view foot_label{};
                if (index == blueprint.left_contact_node) foot_label = "L HEEL";
                else if (index == blueprint.right_contact_node) foot_label = "R HEEL";
                else if (blueprint.additional_left_contact_nodes.size() >= 1u
                    && index == blueprint.additional_left_contact_nodes[0]) foot_label = "L BALL";
                else if (blueprint.additional_left_contact_nodes.size() >= 2u
                    && index == blueprint.additional_left_contact_nodes[1]) foot_label = "L TOE";
                else if (blueprint.additional_right_contact_nodes.size() >= 1u
                    && index == blueprint.additional_right_contact_nodes[0]) foot_label = "R BALL";
                else if (blueprint.additional_right_contact_nodes.size() >= 2u
                    && index == blueprint.additional_right_contact_nodes[1]) foot_label = "R TOE";
                if (!foot_label.empty())
                    add_text(canvas, screen(index) + Vec2{ 10.0f, 10.0f }, foot_label, 0.82f, yellow);
"""
    text = replace_once(text, old, new,
        "rig lab heel ball toe labels")
    write("src/app.cpp", text)


def patch_tests() -> None:
    text = read("tests/core_tests.cpp")
    anchor = """        static void separate_supports(Environment& environment) noexcept
        {
            environment.separate_support_clusters();
        }

"""
    addition = anchor + """        static bool moving_stage_allows_leg_crossing(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.45f);
            if (!environment.valid_node(environment.blueprint_.left_contact_node)
                || !environment.valid_node(environment.blueprint_.right_contact_node))
                return false;
            const float left_before = environment.particles_[
                environment.blueprint_.left_contact_node].position.x;
            const float right_before = environment.particles_[
                environment.blueprint_.right_contact_node].position.x;
            const float left_shift = right_before - left_before + 0.36f;
            const float right_shift = left_before - right_before - 0.36f;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (environment.blueprint_.is_left_support_seed(index))
                {
                    environment.particles_[index].position.x += left_shift;
                    environment.particles_[index].previous.x += left_shift;
                }
                if (environment.blueprint_.is_right_support_seed(index))
                {
                    environment.particles_[index].position.x += right_shift;
                    environment.particles_[index].previous.x += right_shift;
                }
            }
            const float crossed_gap = primary_support_gap(environment);
            environment.separate_support_clusters();
            return crossed_gap < 0.0f
                && primary_support_gap(environment) < 0.0f;
        }

"""
    text = replace_once(text, anchor, addition,
        "moving-stage crossing test access")

    anchor = """    require(sim::qualifies_supported_step(-1, 1, 0.30f, 0.08f, 0.16f, 0.12f),
        "real lifted swing and landing is rejected as a walking step");
"""
    addition = anchor + """    require(!sim::qualifies_crossing_step(-1, 1, 0.30f, 0.08f,
            0.16f, 0.12f, false, true)
            && sim::qualifies_crossing_step(-1, 1, 0.30f, 0.08f,
                0.16f, 0.12f, true, true)
            && sim::qualifies_crossing_step(-1, 1, 0.30f, 0.08f,
                0.16f, 0.12f, false, false),
        "paired gait crossing is either optional or incorrectly forced on nonpaired rigs");
    require(sim::classify_foot_contact_phase(false, false, false)
                == sim::FootContactPhase::airborne
            && sim::classify_foot_contact_phase(true, false, false)
                == sim::FootContactPhase::heel_strike
            && sim::classify_foot_contact_phase(true, true, true)
                == sim::FootContactPhase::flat
            && sim::classify_foot_contact_phase(false, true, true)
                == sim::FootContactPhase::toe_off,
        "heel, flat-foot, toe-off, and airborne phases are not distinct");
    require(sim::foot_friction_retention(0.04f, 1.0f, 0.0f, false, false) == 0.0f,
        "loaded low-speed foot does not enter static friction");
    require(sim::foot_friction_retention(0.45f, 1.0f, 0.0f, false, false)
            < sim::foot_friction_retention(0.45f, 0.25f, 0.75f, false, false),
        "firm ground does not provide more dynamic traction than loose ground");
    require(sim::foot_friction_retention(0.45f, 1.0f, 0.0f, true, false)
            < sim::foot_friction_retention(0.45f, 1.0f, 0.0f, false, false),
        "static lessons do not apply stronger planted-foot friction");
"""
    text = replace_once(text, anchor, addition,
        "crossing, foot phase, and friction tests")

    anchor = """    sim::Environment fused_feet(sim::CreatureBlueprint::humanoid(), 19);
"""
    addition = """    sim::Environment crossing_feet(sim::CreatureBlueprint::humanoid(), 18);
    require(sim::EnvironmentTestAccess::moving_stage_allows_leg_crossing(crossing_feet),
        "support separation prevents one side-view leg from passing the other");

""" + anchor
    text = replace_once(text, anchor, addition,
        "physical side-view crossing test")

    anchor = """    const auto crouch_teacher = rl::duck_teacher_action(crouch_humanoid);
"""
    addition = """    const auto walk_teacher = rl::walking_teacher_action(neutral_humanoid);
    require(walk_teacher[0] * walk_teacher[2] < 0.0f
            || walk_teacher[1] * walk_teacher[3] < 0.0f,
        "walking teacher does not alternate the near and far leg chains");

""" + anchor
    text = replace_once(text, anchor, addition,
        "alternating walking teacher test")
    write("tests/core_tests.cpp", text)


def patch_documents() -> None:
    text = read("missioncache.md")
    text = replace_once(text,
        """### WALK-SIDEGAIT-141 — Normal side-view limb crossing and alternating steps
**Status:** OPEN — SCREENSHOT REOPENED
""",
        """### WALK-SIDEGAIT-141 — Normal side-view limb crossing and alternating steps
**Status:** IMPLEMENTED — DETERMINISTIC, CROSS-PLATFORM, AND SCREENSHOT VALIDATION REQUIRED
""",
        "side gait mission implementation status")
    text = replace_once(text,
        """### WALK-FEET-142 — Proper forward articulated feet and physical traction
**Status:** OPEN — SCREENSHOT REOPENED
""",
        """### WALK-FEET-142 — Proper forward articulated feet and physical traction
**Status:** IMPLEMENTED — DETERMINISTIC, CROSS-PLATFORM, AND SCREENSHOT VALIDATION REQUIRED
""",
        "feet and traction mission implementation status")
    write("missioncache.md", text)

    text = read("CHANGELOG.md")
    anchor = """## Runner v0.7.15 — real crouch correction
"""
    addition = """## Runner v0.7.15 — side-view gait and traction

- Allowed near/far legs to cross during locomotion while retaining fused-foot separation in static lessons.
- Required paired-leg step credit to include a genuine lifted swing crossing before the next strike.
- Added terrain-aware static/dynamic foot friction, heel/flat/toe-off phases, crossing and slip telemetry, and readable near/far leg rendering.
- Added an alternating gait teacher and deterministic crossing, contact-phase, and firm-versus-loose traction tests.

""" + anchor
    text = replace_once(text, anchor, addition,
        "side gait and traction changelog")
    write("CHANGELOG.md", text)


def main() -> None:
    patch_simulation_header()
    patch_simulation_source()
    patch_ppo()
    patch_app()
    patch_tests()
    patch_documents()


if __name__ == "__main__":
    main()
