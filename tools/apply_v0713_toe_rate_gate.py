from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def patch_version_and_state() -> None:
    replace_once(
        ROOT / "CMakeLists.txt",
        "project(Runner VERSION 0.7.12 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.13 LANGUAGES CXX)",
        "CMake version",
    )
    replace_once(
        ROOT / "src" / "ppo.hpp",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1200u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'1300u;",
        "training semantics",
    )
    app = ROOT / "src" / "app.cpp"
    text = app.read_text(encoding="utf-8")
    for old, new in (
        ("runner-v0712-autosave.eppo", "runner-v0713-autosave.eppo"),
        ("runner-v0712-evolved.rig", "runner-v0713-evolved.rig"),
        ("runner-v0712-autonomy.state", "runner-v0713-autonomy.state"),
    ):
        if text.count(old) != 1:
            raise RuntimeError(f"autosave isolation: expected one {old}")
        text = text.replace(old, new, 1)
    app.write_text(text, encoding="utf-8")


def patch_header() -> None:
    path = ROOT / "src" / "simulation.hpp"
    replace_once(
        path,
        """    [[nodiscard]] inline float motor_target_angle(const MotorConstraint& motor, float action) noexcept
    {
        action = clamp(action, -1.0f, 1.0f);
        const float negative_span = std::max(0.0f, motor.neutral_angle - motor.minimum_angle);
        const float positive_span = std::max(0.0f, motor.maximum_angle - motor.neutral_angle);
        const float target = action < 0.0f
            ? motor.neutral_angle + action * negative_span
            : motor.neutral_angle + action * positive_span;
        return clamp(target, motor.minimum_angle, motor.maximum_angle);
    }
""",
        """    [[nodiscard]] inline float motor_target_angle(const MotorConstraint& motor, float action) noexcept
    {
        action = clamp(action, -1.0f, 1.0f);
        const float negative_span = std::max(0.0f, motor.neutral_angle - motor.minimum_angle);
        const float positive_span = std::max(0.0f, motor.maximum_angle - motor.neutral_angle);
        const float target = action < 0.0f
            ? motor.neutral_angle + action * negative_span
            : motor.neutral_angle + action * positive_span;
        return clamp(target, motor.minimum_angle, motor.maximum_angle);
    }

    [[nodiscard]] inline float toe_command_slew_rate(bool supported,
        CourseStage stage) noexcept
    {
        if (stage == CourseStage::balance)
            return 0.55f;
        if (stage == CourseStage::duck_press)
            return 0.80f;
        if (supported)
            return stage_allows_powered_airtime(stage) ? 1.55f : 1.25f;
        return stage_allows_powered_airtime(stage) ? 2.20f : 1.80f;
    }

    [[nodiscard]] inline float rate_limited_toe_command(float previous,
        float desired, float dt, bool supported, CourseStage stage) noexcept
    {
        desired = clamp(desired, -1.0f, 1.0f);
        if (std::abs(desired) < 0.055f)
            desired = 0.0f;
        const float maximum_delta = toe_command_slew_rate(supported, stage)
            * clamp(dt, 1.0f / 240.0f, 1.0f / 30.0f);
        float next = previous + clamp(desired - previous,
            -maximum_delta, maximum_delta);
        if (std::abs(next) < 0.025f && desired == 0.0f)
            next = 0.0f;
        return clamp(next, -1.0f, 1.0f);
    }

    [[nodiscard]] inline float toe_angular_rate_limit(bool supported,
        CourseStage stage) noexcept
    {
        constexpr float radians_per_degree = pi / 180.0f;
        if (stage == CourseStage::balance)
            return 38.0f * radians_per_degree;
        if (stage == CourseStage::duck_press)
            return 58.0f * radians_per_degree;
        if (supported)
            return (stage_allows_powered_airtime(stage) ? 112.0f : 88.0f)
                * radians_per_degree;
        return (stage_allows_powered_airtime(stage) ? 168.0f : 138.0f)
            * radians_per_degree;
    }
""",
        "toe rate helpers",
    )
    replace_once(
        path,
        """        [[nodiscard]] bool articulated_toe_motor(bool left,
            MotorConstraint& motor) const noexcept;
        void solve_articulated_toes(
            std::span<const float, action_count> actions) noexcept;
        void solve_motor(const MotorConstraint& motor, float action) noexcept;
""",
        """        [[nodiscard]] bool articulated_toe_motor(bool left,
            MotorConstraint& motor) const noexcept;
        void update_articulated_toe_commands(
            std::span<const float, action_count> actions, float dt) noexcept;
        void solve_articulated_toes() noexcept;
        void limit_articulated_toe_rates(float dt) noexcept;
        void solve_motor(const MotorConstraint& motor, float action) noexcept;
""",
        "toe method declarations",
    )
    replace_once(
        path,
        """        std::array<float, action_count> previous_angles_{};
        std::array<float, action_count> angular_velocities_{};
        std::array<float, action_count> previous_applied_actions_{};
""",
        """        std::array<float, action_count> previous_angles_{};
        std::array<float, action_count> angular_velocities_{};
        std::array<float, action_count> previous_applied_actions_{};
        std::array<float, 2> articulated_toe_commands_{};
        std::array<float, 2> previous_articulated_toe_angles_{};
""",
        "toe state",
    )


def patch_simulation() -> None:
    path = ROOT / "src" / "simulation.cpp"
    replace_once(
        path,
        """        previous_angles_.fill(0.0f);
        angular_velocities_.fill(0.0f);
        previous_applied_actions_.fill(0.0f);
        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
            previous_angles_[index] = joint_angle(blueprint_.motors[index]);
""",
        """        previous_angles_.fill(0.0f);
        angular_velocities_.fill(0.0f);
        previous_applied_actions_.fill(0.0f);
        articulated_toe_commands_.fill(0.0f);
        previous_articulated_toe_angles_.fill(0.0f);
        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
            previous_angles_[index] = joint_angle(blueprint_.motors[index]);
        for (std::size_t side = 0; side < previous_articulated_toe_angles_.size(); ++side)
        {
            MotorConstraint toe_motor{};
            if (articulated_toe_motor(side == 0u, toe_motor))
                previous_articulated_toe_angles_[side] = joint_angle(toe_motor);
        }
""",
        "reset toe state",
    )
    replace_once(
        path,
        """    void Environment::solve_articulated_toes(
        std::span<const float, action_count> actions) noexcept
    {
        auto solve_side = [&](bool left, std::size_t hip_index,
            std::size_t knee_index)
        {
            MotorConstraint toe_motor{};
            if (!articulated_toe_motor(left, toe_motor))
                return;

            const std::uint16_t heel = left
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            const bool supported = contact_supported(heel);
            const float hip = hip_index < blueprint_.active_motor_count
                ? actions[hip_index] : 0.0f;
            const float knee = knee_index < blueprint_.active_motor_count
                ? actions[knee_index] : 0.0f;
            const float chain_effort = clamp(
                0.5f * (std::abs(hip) + std::abs(knee)), 0.0f, 1.0f);

            float toe_action = 0.0f;
            if (course_stage_ == CourseStage::balance)
            {
                // Keep the new hinge neutral during Stand. Toe actuation begins
                // only when a lesson actually asks for flexion or propulsion.
                toe_action = 0.0f;
            }
            else if (course_stage_ == CourseStage::duck_press)
            {
                // Dorsiflex with the hip/knee chain instead of spreading the
                // feet apart to gain head clearance.
                toe_action = 0.16f + chain_effort * 0.24f;
            }
            else if (stage_requires_forward_gait(course_stage_)
                || stage_allows_powered_airtime(course_stage_))
            {
                // A grounded toe plantar-flexes for push-off; a swing toe lifts
                // for clearance. Both motions are coupled to the same-side leg
                // chain and therefore happen in the same policy step.
                toe_action = supported
                    ? -(0.30f + chain_effort * 0.42f)
                    : 0.46f + chain_effort * 0.18f;
            }
            solve_motor(toe_motor, clamp(toe_action, -0.90f, 0.80f));
        };

        solve_side(true, 0u, 1u);
        solve_side(false, 2u, 3u);
    }
""",
        """    void Environment::update_articulated_toe_commands(
        std::span<const float, action_count> actions, float dt) noexcept
    {
        auto update_side = [&](bool left, std::size_t side,
            std::size_t hip_index, std::size_t knee_index)
        {
            MotorConstraint toe_motor{};
            if (!articulated_toe_motor(left, toe_motor))
            {
                articulated_toe_commands_[side] = 0.0f;
                return;
            }

            const std::uint16_t heel = left
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            const bool supported = contact_supported(heel);
            const float hip = hip_index < blueprint_.active_motor_count
                ? actions[hip_index] : 0.0f;
            const float knee = knee_index < blueprint_.active_motor_count
                ? actions[knee_index] : 0.0f;
            const float chain_effort = clamp(
                0.5f * (std::abs(hip) + std::abs(knee)), 0.0f, 1.0f);

            float desired = 0.0f;
            if (course_stage_ == CourseStage::duck_press)
            {
                desired = 0.16f + chain_effort * 0.24f;
            }
            else if (stage_requires_forward_gait(course_stage_)
                || stage_allows_powered_airtime(course_stage_))
            {
                desired = supported
                    ? -(0.30f + chain_effort * 0.42f)
                    : 0.46f + chain_effort * 0.18f;
            }
            articulated_toe_commands_[side] = rate_limited_toe_command(
                articulated_toe_commands_[side],
                clamp(desired, -0.90f, 0.80f), dt, supported, course_stage_);
        };

        update_side(true, 0u, 0u, 1u);
        update_side(false, 1u, 2u, 3u);
    }

    void Environment::solve_articulated_toes() noexcept
    {
        for (std::size_t side = 0; side < articulated_toe_commands_.size(); ++side)
        {
            MotorConstraint toe_motor{};
            if (articulated_toe_motor(side == 0u, toe_motor))
                solve_motor(toe_motor, articulated_toe_commands_[side]);
        }
    }

    void Environment::limit_articulated_toe_rates(float dt) noexcept
    {
        for (std::size_t side = 0; side < previous_articulated_toe_angles_.size(); ++side)
        {
            const bool left = side == 0u;
            MotorConstraint toe_motor{};
            if (!articulated_toe_motor(left, toe_motor))
                continue;
            const std::uint16_t heel = left
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            const bool supported = contact_supported(heel);
            const float current = joint_angle(toe_motor);
            const float prior = previous_articulated_toe_angles_[side];
            const float maximum_delta = toe_angular_rate_limit(
                supported, course_stage_) * dt;
            const float bounded_delta = clamp(wrap_angle(current - prior),
                -maximum_delta, maximum_delta);
            const float bounded = wrap_angle(prior + bounded_delta);
            const float correction = wrap_angle(bounded - current);
            if (std::abs(correction) > 1.0e-6f
                && toe_motor.pivot < particles_.size()
                && toe_motor.c < particles_.size())
            {
                Particle& toe = particles_[toe_motor.c];
                const Vec2 pivot = particles_[toe_motor.pivot].position;
                const Vec2 corrected = pivot + rotate(toe.position - pivot, correction);
                const Vec2 translation = corrected - toe.position;
                toe.position = corrected;
                toe.previous += translation;

                const float minimum_y = ground_height_at(toe.position.x)
                    + ground_contact_offset(true, toe.radius);
                if (toe.position.y < minimum_y)
                {
                    const float lift = minimum_y - toe.position.y;
                    toe.position.y += lift;
                    toe.previous.y += lift;
                }
                toe.grounded = toe.position.y <= minimum_y + 0.0025f;
            }
            previous_articulated_toe_angles_[side] = joint_angle(toe_motor);
        }
    }
""",
        "toe solver refactor",
    )
    replace_once(
        path,
        """        update_materials(dt);
        rebuild_course_features();
        for (int iteration = 0; iteration < 14; ++iteration)
""",
        """        update_materials(dt);
        rebuild_course_features();
        update_articulated_toe_commands(applied_actions, dt);
        for (int iteration = 0; iteration < 14; ++iteration)
""",
        "toe command update",
    )
    replace_once(
        path,
        """            solve_articulated_toes(applied_actions);
            stabilize_balance_posture();
""",
        """            solve_articulated_toes();
            stabilize_balance_posture();
""",
        "toe solver call",
    )
    replace_once(
        path,
        """            solve_ground(dt);
        }
        apply_support_pressure(dt);
""",
        """            solve_ground(dt);
        }
        // The iterative solver can otherwise reverse the toe between
        // stabilization and push-off every frame. Bound the final physical
        // hinge travel while preserving the contact and propulsion roles.
        limit_articulated_toe_rates(dt);
        apply_support_pressure(dt);
""",
        "physical toe rate gate",
    )


def patch_tests() -> None:
    path = ROOT / "tests" / "core_tests.cpp"
    replace_once(
        path,
        """        static bool articulated_toes_move(Environment& environment) noexcept
        {
            MotorConstraint left{};
            MotorConstraint right{};
            if (!environment.articulated_toe_motor(true, left)
                || !environment.articulated_toe_motor(false, right))
                return false;
            const float left_before = environment.joint_angle(left);
            const float right_before = environment.joint_angle(right);
            std::array<float, action_count> crouch{};
            crouch[0] = -0.45f;
            crouch[1] = 0.65f;
            crouch[2] = 0.45f;
            crouch[3] = -0.65f;
            for (int iteration = 0; iteration < 24; ++iteration)
                environment.solve_articulated_toes(crouch);
            return std::abs(wrap_angle(environment.joint_angle(left) - left_before)) > 0.01f
                && std::abs(wrap_angle(environment.joint_angle(right) - right_before)) > 0.01f;
        }
""",
        """        static bool articulated_toes_move(Environment& environment) noexcept
        {
            MotorConstraint left{};
            MotorConstraint right{};
            if (!environment.articulated_toe_motor(true, left)
                || !environment.articulated_toe_motor(false, right))
                return false;
            const float left_before = environment.joint_angle(left);
            const float right_before = environment.joint_angle(right);
            std::array<float, action_count> crouch{};
            crouch[0] = -0.45f;
            crouch[1] = 0.65f;
            crouch[2] = 0.45f;
            crouch[3] = -0.65f;
            for (int frame = 0; frame < 48; ++frame)
            {
                environment.update_articulated_toe_commands(crouch, 1.0f / 60.0f);
                for (int iteration = 0; iteration < 14; ++iteration)
                    environment.solve_articulated_toes();
                environment.limit_articulated_toe_rates(1.0f / 60.0f);
            }
            return std::abs(wrap_angle(environment.joint_angle(left) - left_before)) > 0.01f
                && std::abs(wrap_angle(environment.joint_angle(right) - right_before)) > 0.01f;
        }

        static bool articulated_toe_rate_is_bounded(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.60f);
            MotorConstraint left{};
            if (!environment.articulated_toe_motor(true, left))
                return false;
            std::array<float, action_count> action{};
            float previous = environment.joint_angle(left);
            constexpr float dt = 1.0f / 60.0f;
            for (int frame = 0; frame < 180; ++frame)
            {
                const float sign = (frame & 1) == 0 ? 1.0f : -1.0f;
                action[0] = sign;
                action[1] = -sign;
                environment.update_articulated_toe_commands(action, dt);
                for (int iteration = 0; iteration < 14; ++iteration)
                    environment.solve_articulated_toes();
                environment.limit_articulated_toe_rates(dt);
                const float current = environment.joint_angle(left);
                const float delta = std::abs(wrap_angle(current - previous));
                const bool supported = environment.contact_supported(
                    environment.blueprint_.left_contact_node);
                if (delta > toe_angular_rate_limit(supported,
                        environment.course_stage_) * dt + 0.0002f)
                    return false;
                previous = current;
            }
            return true;
        }
""",
        "toe test access",
    )
    marker = """    require(sim::EnvironmentTestAccess::articulated_toes_move(toe_environment),
        "coordinated leg action does not actuate both toe hinges");
"""
    replacement = marker + """    sim::Environment rate_limited_toes(sim::CreatureBlueprint::humanoid(), 181u);
    require(sim::EnvironmentTestAccess::articulated_toe_rate_is_bounded(
            rate_limited_toes),
        "articulated toe hinge can chatter faster than its stance/swing rate gate");
    require(std::abs(sim::rate_limited_toe_command(0.0f, 1.0f,
                1.0f / 60.0f, true, sim::CourseStage::uneven))
            <= sim::toe_command_slew_rate(true, sim::CourseStage::uneven)
                / 60.0f + 0.000001f,
        "toe command slew gate permits an instantaneous stabilization snap");
"""
    replace_once(path, marker, replacement, "toe regression assertions")


def patch_docs() -> None:
    changelog = ROOT / "CHANGELOG.md"
    text = changelog.read_text(encoding="utf-8")
    marker = "## [0.7.12] - 2026-08-04\n"
    entry = """## [0.7.13] - 2026-08-04

### Fixed

- Added stance/swing-specific slew limits to articulated toe commands so balance corrections cannot reverse every frame.
- Added a physical angular-velocity gate to the toe hinge after iterative solving, preserving propulsion while removing visible chatter.
- Added a command dead zone and adversarial alternating-input regressions for natural toe motion.
- Isolated v0.7.13 policy, rig, and autonomy state from earlier toe-control semantics.

"""
    if text.count(marker) != 1:
        raise RuntimeError("changelog marker missing")
    changelog.write_text(text.replace(marker, entry + marker, 1), encoding="utf-8")

    mission = ROOT / "missioncache.md"
    text = mission.read_text(encoding="utf-8")
    if text.count("**Target:** Runner v0.7.12") != 1:
        raise RuntimeError("mission target missing")
    text = text.replace("**Target:** Runner v0.7.12", "**Target:** Runner v0.7.13", 1)
    old_state = "**Release state:** PUBLISHED — Runner v0.7.12 articulated feet, coordinated joint control, zero-slip all-rig Stand, all-rig Crouch progression, UI cleanup, release assets, checksum, manifest, released executable, branch cleanup, and PR audit verified."
    new_state = "**Release state:** IMPLEMENTING — packaged v0.7.12 runtime evidence reopened toe-motion naturalness; v0.7.13 gates toe command slew and physical hinge angular velocity before publication."
    if text.count(old_state) != 1:
        raise RuntimeError("mission release state missing")
    text = text.replace(old_state, new_state, 1)
    text += """

## v0.7.13 toe-motion naturalness correction

### WALK-TOE-RATE-127 — Gate toe stabilization and push-off rate
**Status:** IMPLEMENTED — VALIDATION REQUIRED

The articulated toe remains available for stance stabilization, crouch dorsiflexion, swing clearance, and forward push-off. Its command passes through a dead zone and stage/contact-specific slew limiter, and the physical hinge stays below an explicit stance/swing angular-rate ceiling even under alternating frame-by-frame policy input. The correction must preserve all seven Stand and static Crouch gates and must not reintroduce preview sliding.

### WALK-STATE-128 — Isolate corrected toe-control semantics
**Status:** IMPLEMENTED — VALIDATION REQUIRED

Runner v0.7.13 uses training semantics `0x0007'1300` and `runner-v0713-*` policy, rig, and autonomy-state paths so learned v0.7.12 toe chatter cannot silently resume.
"""
    mission.write_text(text, encoding="utf-8")

    readme = ROOT / "README.md"
    text = readme.read_text(encoding="utf-8")
    if text.count("Runner 0.7.10") != 1:
        raise RuntimeError("README version line missing")
    readme.write_text(text.replace("Runner 0.7.10", "Runner 0.7.13", 1), encoding="utf-8")


def main() -> None:
    patch_version_and_state()
    patch_header()
    patch_simulation()
    patch_tests()
    patch_docs()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
