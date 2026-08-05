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
    anchor = """    enum class InvalidMotion : std::uint8_t
"""
    evidence = """    struct CrouchPostureEvidence
    {
        bool paired_leg_chains{};
        bool horizontal_body{};
        bool feet_supported{};
        bool non_foot_grounded{};
        float pelvis_drop{};
        float left_knee_flex{};
        float right_knee_flex{};
        float torso_pitch{};
        float support_margin{ -1.0f };
    };

    [[nodiscard]] inline bool crouch_posture_qualified(
        const CrouchPostureEvidence& evidence) noexcept
    {
        if (!evidence.feet_supported || evidence.non_foot_grounded)
            return false;
        if (evidence.paired_leg_chains)
        {
            return evidence.pelvis_drop >= 0.30f
                && evidence.left_knee_flex >= 0.16f
                && evidence.right_knee_flex >= 0.16f
                && evidence.torso_pitch <= 0.55f
                && evidence.support_margin >= -0.08f;
        }
        if (evidence.horizontal_body)
        {
            return evidence.pelvis_drop >= 0.18f
                && evidence.torso_pitch <= 0.75f
                && evidence.support_margin >= -0.15f;
        }
        return evidence.pelvis_drop >= 0.22f
            && evidence.torso_pitch <= 0.65f
            && evidence.support_margin >= -0.10f;
    }

"""
    text = replace_once(text, anchor, evidence + anchor,
        "crouch posture evidence contract")
    text = replace_once(text,
        """        duck_body_contact,
        buried_no_escape
""",
        """        duck_body_contact,
        buried_no_escape,
        duck_hip_hinge
""",
        "hip-hinge invalid motion enum")
    text = replace_once(text,
        """        case InvalidMotion::buried_no_escape: return "BURIED / NO ESCAPE SPACE";
""",
        """        case InvalidMotion::buried_no_escape: return "BURIED / NO ESCAPE SPACE";
        case InvalidMotion::duck_hip_hinge: return "HIP HINGE - NOT A CROUCH";
""",
        "hip-hinge invalid motion label")
    text = replace_once(text,
        """        [[nodiscard]] float duck_clearance_margin() const noexcept
        {
            return duck_clearance_margin_;
        }
""",
        """        [[nodiscard]] float duck_clearance_margin() const noexcept
        {
            return duck_clearance_margin_;
        }
        [[nodiscard]] CrouchPostureEvidence current_crouch_posture() const noexcept;
        [[nodiscard]] bool crouch_posture_valid() const noexcept
        {
            return crouch_posture_qualified(current_crouch_posture());
        }
        [[nodiscard]] float longest_valid_crouch_seconds() const noexcept
        {
            return longest_valid_crouch_seconds_;
        }
""",
        "public crouch posture telemetry")
    text = replace_once(text,
        """        [[nodiscard]] float torso_roll_angle() const noexcept;
""",
        """        [[nodiscard]] float torso_roll_angle() const noexcept;
""",
        "torso roll declaration anchor")
    text = replace_once(text,
        """        float duck_body_contact_seconds_{};
        float duck_press_max_penetration_{};
""",
        """        float duck_body_contact_seconds_{};
        float duck_posture_failure_seconds_{};
        float current_valid_crouch_seconds_{};
        float longest_valid_crouch_seconds_{};
        float duck_press_max_penetration_{};
""",
        "crouch posture state")
    write("src/simulation.hpp", text)


def patch_simulation_source() -> None:
    text = read("src/simulation.cpp")
    text = replace_once(text,
        """        duck_body_contact_seconds_ = 0.0f;
        duck_press_max_penetration_ = 0.0f;
""",
        """        duck_body_contact_seconds_ = 0.0f;
        duck_posture_failure_seconds_ = 0.0f;
        current_valid_crouch_seconds_ = 0.0f;
        longest_valid_crouch_seconds_ = 0.0f;
        duck_press_max_penetration_ = 0.0f;
""",
        "reset crouch posture state")

    start = text.index("    void Environment::stabilize_duck_posture() noexcept\n")
    end = text.index("    bool Environment::articulated_toe_motor(bool left,\n", start)
    replacement = """    void Environment::stabilize_duck_posture() noexcept
    {
        if (course_stage_ != CourseStage::duck_press
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node))
            return;

        Vec2 rest_support{};
        std::size_t support_count = 0u;
        auto accumulate_rest_support = [&](std::size_t node)
        {
            if (node >= blueprint_.nodes.size() || node >= particles_.size())
                return;
            rest_support += blueprint_.nodes[node];
            ++support_count;
        };
        accumulate_rest_support(blueprint_.left_contact_node);
        accumulate_rest_support(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate_rest_support(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate_rest_support(node);
        if (support_count == 0u)
            return;
        rest_support /= static_cast<float>(support_count);

        const float rest_head_top = blueprint_.nodes[blueprint_.head_node].y
            + particles_[blueprint_.head_node].radius;
        const DuckPressProfile profile = duck_press_profile(
            elapsed_seconds_, course_difficulty_, rest_head_top);
        const float rest_height = std::max(0.65f, rest_head_top - rest_support.y);
        const float requested_drop = clamp(rest_head_top - profile.bottom_y,
            0.0f, rest_height * 0.48f);
        const bool recovery_guide = duck_press_contact_seen_
            && requested_drop <= 0.001f;
        const bool settle_guide = !duck_press_contact_seen_
            && requested_drop <= 0.001f;
        const float phase_strength = (recovery_guide || settle_guide)
            ? 1.0f : clamp(requested_drop / 0.48f, 0.0f, 1.0f);

        auto pin_support = [&](std::size_t node)
        {
            if (node >= particles_.size() || node >= blueprint_.nodes.size())
                return;
            Particle& support = particles_[node];
            const float authored_x = blueprint_.nodes[node].x;
            support.position.x = lerp(support.position.x, authored_x, 0.72f);
            support.position.y = ground_height_at(support.position.x)
                + ground_contact_offset(true, support.radius);
            support.previous = support.position;
            support.grounded = true;
        };
        pin_support(blueprint_.left_contact_node);
        pin_support(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            pin_support(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            pin_support(node);

        Vec2 current_support{};
        auto accumulate_current_support = [&](std::size_t node)
        {
            if (node < particles_.size())
                current_support += particles_[node].position;
        };
        accumulate_current_support(blueprint_.left_contact_node);
        accumulate_current_support(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate_current_support(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate_current_support(node);
        current_support /= static_cast<float>(support_count);

        if (blueprint_.paired_leg_chains())
        {
            const std::uint16_t left_knee = blueprint_.motors[1].pivot;
            const std::uint16_t right_knee = blueprint_.motors[3].pivot;
            const std::uint16_t left_ankle = blueprint_.motors[1].c;
            const std::uint16_t right_ankle = blueprint_.motors[3].c;
            const float guide_strength = 0.28f + phase_strength * 0.36f;

            for (std::size_t node = 0; node < particles_.size(); ++node)
            {
                if (node >= blueprint_.nodes.size() || blueprint_.is_support_seed(node))
                    continue;
                const Vec2 rest_offset = blueprint_.nodes[node] - rest_support;
                Vec2 target = current_support + rest_offset;
                if (node == left_knee || node == right_knee)
                {
                    target.y -= requested_drop * 0.48f;
                    const float direction = node == left_knee ? -1.0f : 1.0f;
                    target.x += direction * requested_drop * 0.18f;
                }
                else if (node == left_ankle || node == right_ankle)
                {
                    target.y -= requested_drop * 0.04f;
                }
                else
                {
                    // Translate the pelvis and complete upper body downward as
                    // one unit. This preserves the authored torso axis instead
                    // of shrinking it into the forward bow seen in v0.7.14.
                    target.y -= requested_drop;
                }
                const float floor = ground_height_at(target.x)
                    + particles_[node].radius + 0.08f;
                target.y = std::max(target.y, floor);
                if (!recovery_guide && node == blueprint_.head_node)
                    target.y = std::min(target.y,
                        profile.bottom_y - particles_[node].radius - 0.035f);

                Vec2 correction = target - particles_[node].position;
                const float magnitude = length(correction);
                constexpr float maximum_step = 0.22f;
                if (magnitude > maximum_step && magnitude > 1.0e-6f)
                    correction *= maximum_step / magnitude;
                const Vec2 applied = correction * guide_strength;
                particles_[node].position += applied;
                particles_[node].previous += applied * 0.94f;
            }
            return;
        }

        const float vertical_scale = clamp(
            (rest_height - requested_drop) / rest_height, 0.52f, 1.0f);
        const float horizontal_scale = 1.0f + (1.0f - vertical_scale) * 0.12f;
        for (std::size_t node = 0; node < particles_.size(); ++node)
        {
            if (node >= blueprint_.nodes.size() || blueprint_.is_support_seed(node))
                continue;
            const Vec2 rest_offset = blueprint_.nodes[node] - rest_support;
            Vec2 target = current_support + Vec2{
                rest_offset.x * horizontal_scale,
                rest_offset.y * vertical_scale
            };
            const float floor = ground_height_at(target.x)
                + particles_[node].radius + 0.14f;
            if (!recovery_guide)
                target.y = std::min(target.y,
                    profile.bottom_y - particles_[node].radius - 0.035f);
            target.y = std::max(target.y, floor);
            Vec2 correction = target - particles_[node].position;
            const float magnitude = length(correction);
            constexpr float maximum_step = 0.60f;
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const Vec2 applied = correction * phase_strength;
            particles_[node].position += applied;
            particles_[node].previous += applied;
        }
    }

"""
    text = text[:start] + replacement + text[end:]

    anchor = """    float Environment::torso_roll_angle() const noexcept
"""
    implementation = """    CrouchPostureEvidence Environment::current_crouch_posture() const noexcept
    {
        CrouchPostureEvidence evidence{};
        evidence.paired_leg_chains = blueprint_.paired_leg_chains();
        evidence.horizontal_body = blueprint_.horizontal_body_plan();
        const bool left = left_supported();
        const bool right = right_supported();
        evidence.feet_supported = evidence.paired_leg_chains
            ? left && right : left || right;
        evidence.non_foot_grounded = non_foot_ground_contact();
        evidence.torso_pitch = std::abs(torso_roll_angle());

        Vec2 rest_support{};
        Vec2 current_support{};
        std::size_t support_count = 0u;
        float minimum_support_x = std::numeric_limits<float>::infinity();
        float maximum_support_x = -std::numeric_limits<float>::infinity();
        auto accumulate = [&](std::size_t node)
        {
            if (node >= blueprint_.nodes.size() || node >= particles_.size())
                return;
            rest_support += blueprint_.nodes[node];
            current_support += particles_[node].position;
            minimum_support_x = std::min(minimum_support_x,
                particles_[node].position.x - particles_[node].radius);
            maximum_support_x = std::max(maximum_support_x,
                particles_[node].position.x + particles_[node].radius);
            ++support_count;
        };
        accumulate(blueprint_.left_contact_node);
        accumulate(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate(node);
        if (support_count > 0u && valid_node(blueprint_.root_node))
        {
            rest_support /= static_cast<float>(support_count);
            current_support /= static_cast<float>(support_count);
            const float rest_height = blueprint_.nodes[blueprint_.root_node].y
                - rest_support.y;
            const float current_height = particles_[blueprint_.root_node].position.y
                - current_support.y;
            evidence.pelvis_drop = std::max(0.0f, rest_height - current_height);

            double weighted_x = 0.0;
            double total_mass = 0.0;
            for (const Particle& particle : particles_)
            {
                const double mass = 1.0 / static_cast<double>(
                    std::max(particle.inverse_mass, 1.0e-5f));
                weighted_x += static_cast<double>(particle.position.x) * mass;
                total_mass += mass;
            }
            if (total_mass > 1.0e-9
                && std::isfinite(minimum_support_x)
                && std::isfinite(maximum_support_x))
            {
                const float center_of_mass_x = static_cast<float>(weighted_x / total_mass);
                evidence.support_margin = std::min(
                    center_of_mass_x - minimum_support_x,
                    maximum_support_x - center_of_mass_x);
            }
        }

        if (evidence.paired_leg_chains)
        {
            const MotorConstraint& left_knee = blueprint_.motors[1];
            const MotorConstraint& right_knee = blueprint_.motors[3];
            evidence.left_knee_flex = std::max(0.0f, wrap_angle(
                joint_angle(left_knee) - left_knee.neutral_angle));
            evidence.right_knee_flex = std::max(0.0f, wrap_angle(
                right_knee.neutral_angle - joint_angle(right_knee)));
        }
        else
        {
            evidence.left_knee_flex = evidence.pelvis_drop;
            evidence.right_knee_flex = evidence.pelvis_drop;
        }
        return evidence;
    }

"""
    text = replace_once(text, anchor, implementation + anchor,
        "crouch posture evidence implementation")

    old = """        const bool generic_duck = feet_supported
            && current_uprightness > 0.60f && duck_depth_ >= 0.48f;
        const bool press_duck = course_stage_ == CourseStage::duck_press
            && feet_supported && !non_foot_grounded_
            && duck_obstacle_weight_ >= 0.64f
            && duck_clearance_margin_ >= -0.10f
            && current_uprightness > 0.20f;
        duck_active_ = generic_duck || press_duck;
"""
    new = """        const CrouchPostureEvidence crouch_posture = current_crouch_posture();
        const bool physical_crouch = crouch_posture_qualified(crouch_posture);
        const bool generic_duck = physical_crouch
            && current_uprightness > 0.60f && duck_depth_ >= 0.48f;
        const bool press_duck = course_stage_ == CourseStage::duck_press
            && physical_crouch
            && duck_obstacle_weight_ >= 0.64f
            && duck_clearance_margin_ >= -0.10f
            && current_uprightness > 0.45f;
        duck_active_ = generic_duck || press_duck;

        const bool crouch_challenge = (course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::crouch_walk)
            && duck_obstacle_weight_ >= 0.72f
            && duck_depth_ >= 0.30f
            && feet_supported && !non_foot_grounded_;
        duck_posture_failure_seconds_ = crouch_challenge && !physical_crouch
            ? duck_posture_failure_seconds_ + dt
            : std::max(0.0f, duck_posture_failure_seconds_ - dt * 2.0f);
        if (duck_posture_failure_seconds_ > 1.10f)
            invalidate(InvalidMotion::duck_hip_hinge);
"""
    text = replace_once(text, old, new,
        "strict crouch activation and hip-hinge rejection")

    old = """        if (duck_active_ && !non_foot_grounded_)
            duck_seconds_ += dt;
"""
    new = """        if (duck_active_ && !non_foot_grounded_)
        {
            duck_seconds_ += dt;
            current_valid_crouch_seconds_ += dt;
            longest_valid_crouch_seconds_ = std::max(
                longest_valid_crouch_seconds_, current_valid_crouch_seconds_);
        }
        else
        {
            current_valid_crouch_seconds_ = 0.0f;
        }
"""
    text = replace_once(text, old, new,
        "valid crouch duration evidence")

    old = """                && body_integrity_valid()
                && current_uprightness >= 0.50f
                && !duck_press_completed_)
"""
    new = """                && body_integrity_valid()
                && current_uprightness >= 0.78f
                && head_height_ratio >= 0.82f
                && std::abs(torso_angle) <= 0.40f
                && stance_slip_speed_ <= 0.16f
                && !duck_press_completed_)
"""
    text = replace_once(text, old, new,
        "controlled standing recovery gate")
    write("src/simulation.cpp", text)


def patch_ppo() -> None:
    text = read("src/ppo.hpp")
    text = replace_once(text,
        """    inline constexpr std::uint32_t training_semantics_version = 0x0007'1501u;
""",
        """    inline constexpr std::uint32_t training_semantics_version = 0x0007'1502u;
""",
        "squat training semantics bump")

    old = """            const float left_flex = std::max(0.0f,
                0.5f * (-action[0] + action[1]));
            const float right_flex = std::max(0.0f,
                0.5f * (action[2] - action[3]));
            const float shared_flex = 0.5f * (left_flex + right_flex);
            constexpr float chain_strength = 0.78f;
            action[0] = lerp(action[0], -shared_flex, chain_strength);
            action[1] = lerp(action[1], shared_flex, chain_strength);
            action[2] = lerp(action[2], shared_flex, chain_strength);
            action[3] = lerp(action[3], -shared_flex, chain_strength);
"""
    new = """            const float shared_hip_flex = 0.5f
                * (std::max(0.0f, -action[0]) + std::max(0.0f, action[2]));
            const float shared_knee_flex = 0.5f
                * (std::max(0.0f, action[1]) + std::max(0.0f, -action[3]));
            constexpr float chain_strength = 0.88f;
            action[0] = lerp(action[0], -shared_hip_flex, chain_strength);
            action[1] = lerp(action[1], shared_knee_flex, chain_strength);
            action[2] = lerp(action[2], shared_hip_flex, chain_strength);
            action[3] = lerp(action[3], -shared_knee_flex, chain_strength);
"""
    text = replace_once(text, old, new,
        "independent bilateral hip and knee crouch synergy")

    old = """            const float hip_flex = std::max(0.06f,
                0.18f * pressure - span_brake);
            const float knee_flex = 0.46f * pressure;
"""
    new = """            const sim::CrouchPostureEvidence posture =
                environment.current_crouch_posture();
            const float drop_deficit = clamp(
                (0.42f - posture.pelvis_drop) / 0.42f, 0.0f, 1.0f);
            const float hip_flex = std::max(0.025f,
                0.10f * pressure - span_brake);
            const float knee_flex = (0.60f + drop_deficit * 0.10f) * pressure;
"""
    text = replace_once(text, old, new,
        "knee-led squat teacher")

    text = replace_once(text,
        """        excessive_rotation = 1u << 8u
""",
        """        excessive_rotation = 1u << 8u,
        invalid_crouch_posture = 1u << 9u
""",
        "crouch evidence failure bit")
    text = replace_once(text,
        """        if ((mask & evidence_bit(MotionEvidenceFailure::no_stable_stance)) != 0u)
""",
        """        if ((mask & evidence_bit(MotionEvidenceFailure::invalid_crouch_posture)) != 0u)
            return "HIP HINGE - NOT A CROUCH";
        if ((mask & evidence_bit(MotionEvidenceFailure::no_stable_stance)) != 0u)
""",
        "crouch evidence failure label")
    text = replace_once(text,
        """            if (environment.non_foot_grounded()
                || (!environment.left_supported() && !environment.right_supported()))
                rejection |= evidence_bit(MotionEvidenceFailure::body_contact);
""",
        """            if (environment.non_foot_grounded()
                || (!environment.left_supported() && !environment.right_supported()))
                rejection |= evidence_bit(MotionEvidenceFailure::body_contact);
            if (environment.longest_valid_crouch_seconds() < 0.55f)
                rejection |= evidence_bit(MotionEvidenceFailure::invalid_crouch_posture);
""",
        "strict static crouch qualification")
    text = replace_once(text,
        """            if (environment.gait_cycles() < 4u
                || environment.crouch_walk_seconds() < 2.0f
""",
        """            if (environment.longest_valid_crouch_seconds() < 0.30f)
                rejection |= evidence_bit(MotionEvidenceFailure::invalid_crouch_posture);
            if (environment.gait_cycles() < 4u
                || environment.crouch_walk_seconds() < 2.0f
""",
        "strict crouch-walk qualification")
    write("src/ppo.hpp", text)


def patch_app_state() -> None:
    text = read("src/app.cpp")
    text = replace_once(text,
        """        std::filesystem::path autosave_policy_path{ "runner-v0715-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "runner-v0715-evolved.rig" };
        std::filesystem::path autosave_state_path{ "runner-v0715-autonomy.state" };
""",
        """        std::filesystem::path autosave_policy_path{ "runner-v0715-squat-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "runner-v0715-squat-evolved.rig" };
        std::filesystem::path autosave_state_path{ "runner-v0715-squat-autonomy.state" };
""",
        "squat semantics autosave isolation")
    write("src/app.cpp", text)


def patch_tests() -> None:
    text = read("tests/core_tests.cpp")
    anchor = """        static void set_duck_pressure(Environment& environment, float pressure) noexcept
        {
            environment.duck_obstacle_weight_ = pressure;
        }

"""
    helpers = anchor + """        static bool hip_hinge_is_rejected(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            auto pin = [&](std::size_t node)
            {
                if (node >= environment.particles_.size())
                    return;
                Particle& particle = environment.particles_[node];
                particle.position.y = environment.ground_height_at(particle.position.x)
                    + ground_contact_offset(true, particle.radius);
                particle.previous = particle.position;
                particle.grounded = true;
            };
            pin(environment.blueprint_.left_contact_node);
            pin(environment.blueprint_.right_contact_node);
            for (const std::uint16_t node : environment.blueprint_.additional_left_contact_nodes)
                pin(node);
            for (const std::uint16_t node : environment.blueprint_.additional_right_contact_nodes)
                pin(node);
            const Vec2 root = environment.particles_[environment.blueprint_.root_node].position;
            environment.particles_[environment.blueprint_.torso_node].position =
                root + Vec2{ 1.05f, 0.42f };
            environment.particles_[environment.blueprint_.head_node].position =
                root + Vec2{ 1.72f, 0.58f };
            environment.particles_[environment.blueprint_.torso_node].previous =
                environment.particles_[environment.blueprint_.torso_node].position;
            environment.particles_[environment.blueprint_.head_node].previous =
                environment.particles_[environment.blueprint_.head_node].position;
            return !environment.crouch_posture_valid();
        }

        static bool guided_squat_is_valid(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            environment.elapsed_seconds_ = 6.0f;
            environment.duck_press_contact_seen_ = true;
            for (int iteration = 0; iteration < 48; ++iteration)
            {
                environment.stabilize_duck_posture();
                environment.solve_ground(1.0f / 60.0f);
            }
            const CrouchPostureEvidence evidence =
                environment.current_crouch_posture();
            return crouch_posture_qualified(evidence)
                && evidence.pelvis_drop >= 0.30f
                && evidence.left_knee_flex >= 0.16f
                && evidence.right_knee_flex >= 0.16f
                && evidence.torso_pitch <= 0.55f;
        }

"""
    text = replace_once(text, anchor, helpers,
        "crouch adversarial test access")

    anchor = """    require(!sim::duck_ground_contact_allowed(true, true)
            && sim::duck_ground_contact_allowed(true, false)
            && sim::duck_ground_contact_allowed(false, true),
        "foot-only duck contact rule is not strict");
"""
    tests = anchor + """    sim::CrouchPostureEvidence hinge{};
    hinge.paired_leg_chains = true;
    hinge.feet_supported = true;
    hinge.pelvis_drop = 0.08f;
    hinge.left_knee_flex = 0.03f;
    hinge.right_knee_flex = 0.02f;
    hinge.torso_pitch = 1.10f;
    hinge.support_margin = 0.12f;
    require(!sim::crouch_posture_qualified(hinge),
        "forward hip hinge is accepted as a crouch");
    sim::CrouchPostureEvidence squat{};
    squat.paired_leg_chains = true;
    squat.feet_supported = true;
    squat.pelvis_drop = 0.44f;
    squat.left_knee_flex = 0.32f;
    squat.right_knee_flex = 0.31f;
    squat.torso_pitch = 0.20f;
    squat.support_margin = 0.14f;
    require(sim::crouch_posture_qualified(squat),
        "bilateral pelvis-down squat cannot satisfy crouch evidence");
"""
    text = replace_once(text, anchor, tests,
        "pure squat and hip-hinge acceptance tests")

    anchor = """    sim::Environment crouch_humanoid(humanoid_rig, 141);
"""
    tests = """    sim::Environment hinge_humanoid(humanoid_rig, 139);
    require(sim::EnvironmentTestAccess::hip_hinge_is_rejected(hinge_humanoid),
        "live humanoid forward bow passes the physical crouch gate");
    sim::Environment guided_squat(humanoid_rig, 140);
    require(sim::EnvironmentTestAccess::guided_squat_is_valid(guided_squat),
        "authored crouch guide cannot produce a pelvis-down bilateral squat");

""" + anchor
    text = replace_once(text, anchor, tests,
        "physical crouch posture tests")
    write("tests/core_tests.cpp", text)


def patch_documents() -> None:
    text = read("missioncache.md")
    text = replace_once(text,
        """### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow
**Status:** OPEN — SCREENSHOT REOPENED
""",
        """### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow
**Status:** IMPLEMENTED — DETERMINISTIC, CROSS-PLATFORM, AND SCREENSHOT VALIDATION REQUIRED
""",
        "crouch mission implementation status")
    write("missioncache.md", text)

    text = read("CHANGELOG.md")
    anchor = """## Runner v0.7.15 — structural evolution completion
"""
    addition = """## Runner v0.7.15 — real crouch correction

- Replaced head-clearance-only duck evidence with pelvis drop, bilateral knee flexion, bounded torso pitch, center-of-mass support, feet-only contact, held crouch, and upright recovery evidence.
- Reworked the paired-leg crouch guide to lower the pelvis and upper body as a unit while driving knees into a squat instead of shrinking the torso into a forward bow.
- Added explicit hip-hinge rejection, adversarial posture tests, and isolated autosaves for the corrected training semantics.

""" + anchor
    text = replace_once(text, anchor, addition,
        "crouch correction changelog")
    write("CHANGELOG.md", text)


def main() -> None:
    patch_simulation_header()
    patch_simulation_source()
    patch_ppo()
    patch_app_state()
    patch_tests()
    patch_documents()


if __name__ == "__main__":
    main()
