from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


header = Path("src/simulation.hpp")
text = header.read_text(encoding="utf-8")
text = replace_once(
    text,
    """        projectile
    };
""",
    """        projectile
    };
""",
    "course feature enum",
)
text = replace_once(
    text,
    """        Vec2 velocity{};
    };

    [[nodiscard]] inline float course_feature_observation_size(
""",
    """        Vec2 velocity{};
        int marker_sequence{ -1 };
    };

    [[nodiscard]] inline float course_feature_half_width(const CourseFeature& feature) noexcept
    {
        switch (feature.kind)
        {
        case CourseFeatureKind::moving_hazard:
        case CourseFeatureKind::rock:
        case CourseFeatureKind::projectile:
            return feature.radius;
        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
            return feature.half_extent.x;
        }
        return 0.0f;
    }

    [[nodiscard]] inline float course_feature_top(const CourseFeature& feature) noexcept
    {
        switch (feature.kind)
        {
        case CourseFeatureKind::moving_hazard:
        case CourseFeatureKind::rock:
        case CourseFeatureKind::projectile:
            return feature.center.y + feature.radius;
        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
            return feature.center.y + feature.half_extent.y;
        }
        return feature.center.y;
    }

    [[nodiscard]] inline bool knee_crosses_before_foot(float knee_front_x,
        float foot_front_x, float foot_top_y, const CourseFeature& feature) noexcept
    {
        if (feature.kind != CourseFeatureKind::rock
            && feature.kind != CourseFeatureKind::hurdle)
            return false;
        const float obstacle_front = feature.center.x + course_feature_half_width(feature);
        return knee_front_x > feature.center.x
            && foot_front_x < obstacle_front - 0.02f
            && foot_top_y < course_feature_top(feature) + 0.08f;
    }

    [[nodiscard]] inline float gait_progress_multiplier(std::uint32_t alternating_steps,
        bool single_support, float swing_clearance) noexcept
    {
        if (alternating_steps == 0)
            return single_support && swing_clearance > 0.10f ? 0.12f : 0.0f;
        const float established = clamp(0.30f + static_cast<float>(alternating_steps) * 0.10f,
            0.30f, 1.0f);
        const float swing_bonus = single_support && swing_clearance > 0.10f ? 0.12f : 0.0f;
        return clamp(established + swing_bonus, 0.0f, 1.0f);
    }

    [[nodiscard]] inline bool wheel_sliding_motion(float root_speed, bool left_supported,
        bool right_supported, float stance_slip_speed) noexcept
    {
        return left_supported && right_supported
            && std::abs(root_speed) > 0.22f && stance_slip_speed > 0.18f;
    }

    [[nodiscard]] inline float course_feature_observation_size(
""",
    "gait and obstacle-order helpers",
)
text = replace_once(
    text,
    """        micro_motion
    };
""",
    """        micro_motion,
        wheel_sliding
    };
""",
    "wheel-sliding invalid motion",
)
text = replace_once(
    text,
    """        case InvalidMotion::micro_motion: return "MICRO-MOTION EXPLOIT";
""",
    """        case InvalidMotion::micro_motion: return "MICRO-MOTION EXPLOIT";
        case InvalidMotion::wheel_sliding: return "WHEEL-SLIDING EXPLOIT";
""",
    "wheel-sliding diagnostic name",
)
text = replace_once(
    text,
    """        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }
        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }
""",
    """        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }
        [[nodiscard]] std::uint32_t knee_first_faults() const noexcept { return knee_first_faults_; }
        [[nodiscard]] float stance_slip_speed() const noexcept { return stance_slip_speed_; }
        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }
""",
    "gait diagnostics getters",
)
text = replace_once(
    text,
    """        [[nodiscard]] bool contact_supported(std::uint16_t contact_node) const noexcept;

        CreatureBlueprint blueprint_{};
""",
    """        [[nodiscard]] bool contact_supported(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] float contact_cluster_front_x(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] float contact_cluster_top_y(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] float contact_cluster_horizontal_speed(std::uint16_t contact_node,
            float dt) const noexcept;
        [[nodiscard]] bool knee_before_foot_fault() const noexcept;

        CreatureBlueprint blueprint_{};
""",
    "gait helper declarations",
)
text = replace_once(
    text,
    """        std::uint32_t alternating_steps_{};
        int last_contact_side_{};
""",
    """        std::uint32_t alternating_steps_{};
        std::uint32_t knee_first_faults_{};
        float wheel_sliding_seconds_{};
        float stance_slip_speed_{};
        bool knee_first_this_step_{};
        int last_contact_side_{};
""",
    "gait state",
)
header.write_text(text, encoding="utf-8")

source = Path("src/simulation.cpp")
text = source.read_text(encoding="utf-8")
text = replace_once(
    text,
    """                    kind, { x, ground + radius }, {}, radius, { treadmill_velocity, 0.0f }
""",
    """                    kind, { x, ground + radius }, {}, radius, { treadmill_velocity, 0.0f }, sequence
""",
    "rock marker sequence",
)
text = replace_once(
    text,
    """                    { treadmill_velocity, 0.0f }
                });
                break;
            }
            case CourseFeatureKind::overhead_bar:
""",
    """                    { treadmill_velocity, 0.0f }, sequence
                });
                break;
            }
            case CourseFeatureKind::overhead_bar:
""",
    "hurdle marker sequence",
)
text = replace_once(
    text,
    """                    { treadmill_velocity, 0.0f }
                });
                break;
            }
            case CourseFeatureKind::moving_hazard:
""",
    """                    { treadmill_velocity, 0.0f }, sequence
                });
                break;
            }
            case CourseFeatureKind::moving_hazard:
""",
    "overhead marker sequence",
)
text = replace_once(
    text,
    """                    { treadmill_velocity + oscillation * 0.35f, 0.0f }
                });
""",
    """                    { treadmill_velocity + oscillation * 0.35f, 0.0f }, sequence
                });
""",
    "moving hazard marker sequence",
)
text = replace_once(
    text,
    """                    { treadmill_velocity - throw_speed, (1.0f - throw_phase * 2.0f) * 2.4f }
                });
""",
    """                    { treadmill_velocity - throw_speed, (1.0f - throw_phase * 2.0f) * 2.4f }, sequence
                });
""",
    "projectile marker sequence",
)
text = replace_once(
    text,
    """    bool Environment::contact_supported(std::uint16_t contact_node) const noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (particles_[index].grounded && contact_cluster_contains(contact_node, index))
                return true;
        }
        return false;
    }

    void Environment::solve_ground(float dt) noexcept
""",
    """    bool Environment::contact_supported(std::uint16_t contact_node) const noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (particles_[index].grounded && contact_cluster_contains(contact_node, index))
                return true;
        }
        return false;
    }

    float Environment::contact_cluster_front_x(std::uint16_t contact_node) const noexcept
    {
        float front = -std::numeric_limits<float>::infinity();
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (contact_cluster_contains(contact_node, index))
                front = std::max(front, particles_[index].position.x + particles_[index].radius);
        }
        return std::isfinite(front) ? front : 0.0f;
    }

    float Environment::contact_cluster_top_y(std::uint16_t contact_node) const noexcept
    {
        float top = -std::numeric_limits<float>::infinity();
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (contact_cluster_contains(contact_node, index))
                top = std::max(top, particles_[index].position.y + particles_[index].radius);
        }
        return std::isfinite(top) ? top : 0.0f;
    }

    float Environment::contact_cluster_horizontal_speed(std::uint16_t contact_node,
        float dt) const noexcept
    {
        float accumulated = 0.0f;
        std::size_t count = 0;
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (!particles_[index].grounded || !contact_cluster_contains(contact_node, index))
                continue;
            accumulated += std::abs((particles_[index].position.x - particles_[index].previous.x)
                / std::max(dt, 1.0e-5f));
            ++count;
        }
        return count == 0 ? 0.0f : accumulated / static_cast<float>(count);
    }

    bool Environment::knee_before_foot_fault() const noexcept
    {
        constexpr std::array<std::size_t, 2> knee_motors{ 1u, 3u };
        constexpr std::array<bool, 2> left_side{ true, false };
        for (std::size_t side = 0; side < knee_motors.size(); ++side)
        {
            const MotorConstraint& knee_motor = blueprint_.motors[knee_motors[side]];
            if (!knee_motor.enabled || !valid_node(knee_motor.pivot))
                continue;
            const std::uint16_t foot = left_side[side]
                ? blueprint_.left_contact_node : blueprint_.right_contact_node;
            const float knee_front = particles_[knee_motor.pivot].position.x
                + particles_[knee_motor.pivot].radius;
            const float foot_front = contact_cluster_front_x(foot);
            const float foot_top = contact_cluster_top_y(foot);
            for (const CourseFeature& feature : course_features_)
            {
                if (knee_crosses_before_foot(knee_front, foot_front, foot_top, feature))
                    return true;
            }
        }
        return false;
    }

    void Environment::solve_ground(float dt) noexcept
""",
    "gait helper implementations",
)
text = replace_once(
    text,
    """        alternating_steps_ = 0;
        last_contact_side_ = 0;
""",
    """        alternating_steps_ = 0;
        knee_first_faults_ = 0;
        wheel_sliding_seconds_ = 0.0f;
        stance_slip_speed_ = 0.0f;
        knee_first_this_step_ = false;
        last_contact_side_ = 0;
""",
    "reset gait exploit state",
)
text = replace_once(
    text,
    """        previous_left_grounded_ = left;
        previous_right_grounded_ = right;

        if (!left && !right)
""",
    """        previous_left_grounded_ = left;
        previous_right_grounded_ = right;

        const float left_slip = left
            ? contact_cluster_horizontal_speed(blueprint_.left_contact_node, dt) : 0.0f;
        const float right_slip = right
            ? contact_cluster_horizontal_speed(blueprint_.right_contact_node, dt) : 0.0f;
        stance_slip_speed_ = left_slip + right_slip;
        const float root_speed = valid_node(blueprint_.root_node)
            ? (particles_[blueprint_.root_node].position.x
                - particles_[blueprint_.root_node].previous.x) / std::max(dt, 1.0e-5f)
            : 0.0f;
        if (course_stage_ != CourseStage::balance
            && wheel_sliding_motion(root_speed, left, right, stance_slip_speed_))
            wheel_sliding_seconds_ += dt;
        else
            wheel_sliding_seconds_ = std::max(0.0f, wheel_sliding_seconds_ - dt * 1.5f);
        if (wheel_sliding_seconds_ > 0.90f)
            invalidate(InvalidMotion::wheel_sliding);

        if (!left && !right)
""",
    "wheel-sliding gait gate",
)
text = replace_once(
    text,
    """        if (collided_this_step_)
            collision_count_ += 1.0f;

        elapsed_seconds_ += dt;
""",
    """        knee_first_this_step_ = knee_before_foot_fault();
        if (knee_first_this_step_)
            ++knee_first_faults_;
        if (collided_this_step_)
            collision_count_ += 1.0f;

        elapsed_seconds_ += dt;
""",
    "knee-first fault capture",
)
text = replace_once(
    text,
    """        const float left_contact = contact_supported(blueprint_.left_contact_node) ? 1.0f : 0.0f;
        const float right_contact = contact_supported(blueprint_.right_contact_node) ? 1.0f : 0.0f;
        const float contact = left_contact + right_contact;
        const float gait = clamp(0.25f + static_cast<float>(alternating_steps_) * 0.12f, 0.25f, 1.0f);
        const float safe_progress = clamp(frame_progress, -0.015f, 0.065f);
        const float collision_penalty = collided_this_step_ ? 0.025f : 0.0f;
""",
    """        const bool left_supported = contact_supported(blueprint_.left_contact_node);
        const bool right_supported = contact_supported(blueprint_.right_contact_node);
        const float left_contact = left_supported ? 1.0f : 0.0f;
        const float right_contact = right_supported ? 1.0f : 0.0f;
        const float contact = left_contact + right_contact;
        const bool single_support = left_supported != right_supported;
        const std::uint16_t swing_foot = left_supported
            ? blueprint_.right_contact_node : blueprint_.left_contact_node;
        const float swing_clearance = single_support && valid_node(swing_foot)
            ? particles_[swing_foot].position.y
                - ground_height_at(particles_[swing_foot].position.x)
                - particles_[swing_foot].radius
            : 0.0f;
        const float gait = gait_progress_multiplier(alternating_steps_,
            single_support, swing_clearance);
        const float safe_progress = clamp(frame_progress, -0.015f, 0.065f);
        const float collision_penalty = collided_this_step_ ? 0.025f : 0.0f;
        const float knee_first_penalty = knee_first_this_step_ ? 0.11f : 0.0f;
        const float stance_slip_penalty = clamp(stance_slip_speed_ - 0.08f, 0.0f, 4.0f) * 0.012f;
        const float wheel_penalty = wheel_sliding_motion(raw_speed,
            left_supported, right_supported, stance_slip_speed_) ? 0.055f : 0.0f;
        const float swing_reward = single_support && swing_clearance > 0.10f
            ? clamp(swing_clearance, 0.0f, 0.45f) * 0.004f : 0.0f;
""",
    "real-step reward inputs",
)
text = replace_once(
    text,
    """            last_reward_ = std::max(0.0f, safe_progress) * 1.65f * gait
                + std::max(0.0f, upright) * 0.012f
                + contact * 0.0012f
                - std::max(0.0f, -safe_progress) * 0.45f
                - action_energy * 0.0010f
                - collision_penalty;
""",
    """            last_reward_ = std::max(0.0f, safe_progress) * 1.65f * gait
                + std::max(0.0f, upright) * 0.012f
                + contact * 0.0006f
                + swing_reward
                - std::max(0.0f, -safe_progress) * 0.45f
                - action_energy * 0.0010f
                - collision_penalty
                - knee_first_penalty
                - stance_slip_penalty
                - wheel_penalty;
""",
    "real-step reward gate",
)
source.write_text(text, encoding="utf-8")

app = Path("src/app.cpp")
text = app.read_text(encoding="utf-8")
text = replace_once(
    text,
    """                std::format("RECOVERY {}   {}/{}   FEET {}/{}",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.recovery_successes(), environment.recovery_events(),
                    environment.left_supported() ? "L" : "-",
                    environment.right_supported() ? "R" : "-"),
                1.06f, environment.recovering() ? yellow : muted, overlay_width);
""",
    """                std::format("RECOVERY {}   FEET {}/{}   STEPS {}   KNEE FAULTS {}",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.left_supported() ? "L" : "-",
                    environment.right_supported() ? "R" : "-",
                    environment.alternating_steps(), environment.knee_first_faults()),
                1.02f, environment.recovering() ? yellow : muted, overlay_width);
""",
    "live gait telemetry",
)
app.write_text(text, encoding="utf-8")

tests = Path("tests/core_tests.cpp")
text = tests.read_text(encoding="utf-8")
anchor = """    require(sim::course_marker_distance_m(sim::course_safe_runway_markers) >= 24.0f,
        "course does not provide enough safe runway before the first obstacle marker");
"""
replacement = anchor + """    sim::CourseFeature rock_order{};
    rock_order.kind = sim::CourseFeatureKind::rock;
    rock_order.center = { 1.0f, 0.25f };
    rock_order.radius = 0.25f;
    require(sim::knee_crosses_before_foot(1.12f, 0.92f, 0.34f, rock_order),
        "knee-first rock traversal is not detected");
    require(!sim::knee_crosses_before_foot(1.12f, 1.32f, 0.34f, rock_order),
        "foot-first rock traversal is incorrectly penalized");
    require(sim::gait_progress_multiplier(0, false, 0.0f) == 0.0f,
        "sliding without a real step still receives walking progress credit");
    require(sim::gait_progress_multiplier(2, true, 0.18f)
            > sim::gait_progress_multiplier(0, true, 0.18f),
        "alternating lifted-foot gait does not receive stronger progress credit");
    require(sim::wheel_sliding_motion(0.45f, true, true, 0.50f),
        "double-supported wheel-like sliding is not detected");
    require(!sim::wheel_sliding_motion(0.45f, true, false, 0.50f),
        "single-support walking is incorrectly classified as wheel sliding");
"""
text = replace_once(text, anchor, replacement, "gait semantic tests")
tests.write_text(text, encoding="utf-8")

missions = Path("MISSIONS.md")
text = missions.read_text(encoding="utf-8")
mission = """## WALK-GAIT-002 — Real stepping instead of wheel sliding

**Status:** ACTIVE

Forward reward must represent foot-led alternating walking, not a body sliding across planted contacts. A knee may not clear a rock or hurdle before its corresponding foot. Sustained double-supported sliding is an invalid gait exploit.

**Acceptance:**

- Zero-step sliding receives no positive forward-progress multiplier.
- Alternating contact plus visible swing-foot clearance earns the strongest gait multiplier.
- Grounded foot-cluster slip is measured and penalized.
- Sustained double-supported root motion with slipping feet terminates as wheel sliding.
- Knee-before-foot traversal over rocks or hurdles receives a strong per-step penalty and increments telemetry.
- Foot-first traversal is not penalized.
- Full Windows/Vulkan build, deterministic gait tests, diagnostics, package, and exact-source evidence pass.

"""
if "## WALK-GAIT-002" not in text:
    marker = "## Current warning\n"
    if marker not in text:
        raise SystemExit("mission ledger: Current warning anchor not found")
    text = text.replace(marker, mission + marker, 1)
missions.write_text(text, encoding="utf-8")

readme = Path("README.md")
text = readme.read_text(encoding="utf-8")
needle = "Course markers and obstacles now share one eight-metre schedule. Each lesson starts with three clear markers of safe runway, then advanced training cycles rocks, hurdles, overhead bars, moving hazards, and thrown projectiles at consecutive markers. The virtual course moves quickly enough to reach those events in practical training time while every feature remains anchored to course coordinates rather than following the actor.\n"
replacement = needle + "\nWalking reward now requires foot-led gait evidence. Sliding forward with both feet planted receives no startup progress credit, grounded foot slip is penalized, sustained wheel-like motion is a hard invalid gate, and a knee crossing a rock or hurdle before its corresponding foot receives an explicit penalty and visible fault count.\n"
text = replace_once(text, needle, replacement, "README gait semantics note")
readme.write_text(text, encoding="utf-8")
