from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


cmake = Path("CMakeLists.txt")
text = cmake.read_text(encoding="utf-8")
text = replace_once(
    text,
    "project(EpochRunner VERSION 0.6.1 LANGUAGES CXX)",
    "project(EpochRunner VERSION 0.6.2 LANGUAGES CXX)",
    "CMake project version",
)
cmake.write_text(text, encoding="utf-8")

vcpkg = Path("vcpkg.json")
text = vcpkg.read_text(encoding="utf-8")
text = replace_once(text, '"version-semver": "0.6.1"', '"version-semver": "0.6.2"', "vcpkg version")
vcpkg.write_text(text, encoding="utf-8")

header = Path("src/simulation.hpp")
text = header.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    [[nodiscard]] inline bool qualifies_alternating_step(int previous_side, int strike_side,
        float seconds_since_previous, float root_displacement) noexcept
    {
        return previous_side != 0 && strike_side != 0 && strike_side != previous_side
            && seconds_since_previous >= 0.12f && std::abs(root_displacement) >= 0.025f;
    }

    struct Particle
""",
    """    [[nodiscard]] inline bool qualifies_alternating_step(int previous_side, int strike_side,
        float seconds_since_previous, float root_displacement) noexcept
    {
        return previous_side != 0 && strike_side != 0 && strike_side != previous_side
            && seconds_since_previous >= 0.12f && std::abs(root_displacement) >= 0.025f;
    }

    [[nodiscard]] inline float ground_velocity_retention(bool traction_contact,
        float vertical_speed) noexcept
    {
        if (!traction_contact)
            return 0.96f;
        return std::abs(vertical_speed) < 1.5f ? 0.42f : 0.72f;
    }

    [[nodiscard]] inline int first_course_feature_sequence(float root_x, float course_progress,
        float spacing = 5.5f, float lead_distance = 4.5f) noexcept
    {
        return static_cast<int>(std::ceil((root_x + course_progress + lead_distance) / spacing));
    }

    [[nodiscard]] inline float course_feature_world_x(int sequence, float course_progress,
        float spacing = 5.5f) noexcept
    {
        return static_cast<float>(sequence) * spacing - course_progress;
    }

    struct Particle
""",
    "ground contact and course anchoring helpers",
)
text = replace_once(
    text,
    """        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }
        [[nodiscard]] InvalidMotion invalid_reason() const noexcept { return invalid_reason_; }
        [[nodiscard]] float uprightness() const noexcept { return torso_uprightness(); }

    private:
""",
    """        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }
        [[nodiscard]] InvalidMotion invalid_reason() const noexcept { return invalid_reason_; }
        [[nodiscard]] float uprightness() const noexcept { return torso_uprightness(); }
        [[nodiscard]] bool left_supported() const noexcept
        {
            return contact_supported(blueprint_.left_contact_node);
        }
        [[nodiscard]] bool right_supported() const noexcept
        {
            return contact_supported(blueprint_.right_contact_node);
        }

    private:
""",
    "public foot-support diagnostics",
)
text = replace_once(
    text,
    """        [[nodiscard]] float torso_uprightness() const noexcept;
        [[nodiscard]] float random_unit() noexcept;
        [[nodiscard]] bool valid_node(std::uint16_t index) const noexcept;

        CreatureBlueprint blueprint_{};
""",
    """        [[nodiscard]] float torso_uprightness() const noexcept;
        [[nodiscard]] float random_unit() noexcept;
        [[nodiscard]] bool valid_node(std::uint16_t index) const noexcept;
        [[nodiscard]] bool contact_cluster_contains(std::uint16_t contact_node,
            std::size_t particle_index) const noexcept;
        [[nodiscard]] bool contact_supported(std::uint16_t contact_node) const noexcept;

        CreatureBlueprint blueprint_{};
""",
    "foot contact cluster declarations",
)
header.write_text(text, encoding="utf-8")

source = Path("src/simulation.cpp")
text = source.read_text(encoding="utf-8")
text = replace_once(
    text,
    """        constexpr float spacing = 5.5f;
        const float progress = course_progress();
        const int first_sequence = static_cast<int>(std::floor(progress / spacing));
        const float phase = std::fmod(progress, spacing);
        const float treadmill_velocity = -course_speed();
""",
    """        constexpr float spacing = 5.5f;
        const float progress = course_progress();
        const int first_sequence = first_course_feature_sequence(root_x, progress, spacing);
        const float treadmill_velocity = -course_speed();
""",
    "course feature sequence selection",
)
text = replace_once(
    text,
    """            const float variation = variation_for(sequence);
            const float x = root_x + 4.5f + static_cast<float>(offset) * spacing - phase;
            const float ground = ground_height_at(x);
""",
    """            const float variation = variation_for(sequence);
            const float x = course_feature_world_x(sequence, progress, spacing);
            const float ground = ground_height_at(x);
""",
    "course feature world anchoring",
)
text = replace_once(
    text,
    """    void Environment::solve_ground(float dt) noexcept
    {
        for (Particle& particle : particles_)
        {
            particle.grounded = false;
            const float minimum_y = ground_height_at(particle.position.x) + particle.radius;
            if (particle.position.y < minimum_y)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;
                const Vec2 velocity = (particle.position - particle.previous) / dt;
                const float friction = std::abs(velocity.y) < 1.5f ? 0.72f : 0.90f;
                particle.previous.x = particle.position.x - velocity.x * dt * friction;
                if (velocity.y < 0.0f)
                    particle.previous.y = particle.position.y + velocity.y * dt * 0.05f;
            }
        }
    }
""",
    """    bool Environment::contact_cluster_contains(std::uint16_t contact_node,
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
    }

    bool Environment::contact_supported(std::uint16_t contact_node) const noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (particles_[index].grounded && contact_cluster_contains(contact_node, index))
                return true;
        }
        return false;
    }

    void Environment::solve_ground(float dt) noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            Particle& particle = particles_[index];
            particle.grounded = false;
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
        }
    }
""",
    "contact-aware ground traction",
)
text = replace_once(
    text,
    """        const bool left = valid_node(blueprint_.left_contact_node) && particles_[blueprint_.left_contact_node].grounded;
        const bool right = valid_node(blueprint_.right_contact_node) && particles_[blueprint_.right_contact_node].grounded;
""",
    """        const bool left = contact_supported(blueprint_.left_contact_node);
        const bool right = contact_supported(blueprint_.right_contact_node);
""",
    "gait support clusters",
)
text = replace_once(
    text,
    """        const bool supported = particles_[blueprint_.left_contact_node].grounded
            || particles_[blueprint_.right_contact_node].grounded;
""",
    """        const bool supported = contact_supported(blueprint_.left_contact_node)
            || contact_supported(blueprint_.right_contact_node);
""",
    "recovery support clusters",
)
text = replace_once(
    text,
    """        const float left_contact = particles_[blueprint_.left_contact_node].grounded ? 1.0f : 0.0f;
        const float right_contact = particles_[blueprint_.right_contact_node].grounded ? 1.0f : 0.0f;
""",
    """        const float left_contact = contact_supported(blueprint_.left_contact_node) ? 1.0f : 0.0f;
        const float right_contact = contact_supported(blueprint_.right_contact_node) ? 1.0f : 0.0f;
""",
    "reward support clusters",
)
text = replace_once(
    text,
    """        result[12] = particles_[blueprint_.left_contact_node].grounded ? 1.0f : 0.0f;
        result[13] = particles_[blueprint_.right_contact_node].grounded ? 1.0f : 0.0f;
""",
    """        result[12] = contact_supported(blueprint_.left_contact_node) ? 1.0f : 0.0f;
        result[13] = contact_supported(blueprint_.right_contact_node) ? 1.0f : 0.0f;
""",
    "policy support observations",
)
source.write_text(text, encoding="utf-8")

app = Path("src/app.cpp")
text = app.read_text(encoding="utf-8")
text = replace_once(text, '"epochrunner-v050-autosave.eppo"', '"epochrunner-v062-autosave.eppo"', "policy autosave compatibility")
text = replace_once(text, '"epochrunner-v050-evolved.epochrig"', '"epochrunner-v062-evolved.epochrig"', "rig autosave compatibility")
text = replace_once(text, '"epochrunner-v050-autonomy.state"', '"epochrunner-v062-autonomy.state"', "state autosave compatibility")
text = replace_once(
    text,
    """                std::format("RECOVERY {}   {}/{} SUCCESS",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.recovery_successes(), environment.recovery_events()),
""",
    """                std::format("RECOVERY {}   {}/{} SUCCESS   FEET {}/{}",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.recovery_successes(), environment.recovery_events(),
                    environment.left_supported() ? "L" : "-",
                    environment.right_supported() ? "R" : "-"),
""",
    "live foot-contact telemetry",
)
app.write_text(text, encoding="utf-8")

tests = Path("tests/core_tests.cpp")
text = tests.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    require(std::abs(sim::course_feature_observation_size(hurdle_feature) - 0.42f) < 0.0001f,
        "rectangular obstacle extent is incorrect in policy observations");

    const std::array<sim::CreatureBlueprint, 5> presets{
""",
    """    require(std::abs(sim::course_feature_observation_size(hurdle_feature) - 0.42f) < 0.0001f,
        "rectangular obstacle extent is incorrect in policy observations");

    require(sim::ground_velocity_retention(true, 0.0f)
        < sim::ground_velocity_retention(false, 0.0f),
        "feet do not receive more ground traction than head, tail, or body nodes");
    require(sim::ground_velocity_retention(false, 0.0f) >= 0.95f,
        "non-foot body contact can still pin the creature to the ground");
    const int anchored_sequence = sim::first_course_feature_sequence(1.0f, 3.0f);
    const float anchored_x = sim::course_feature_world_x(anchored_sequence, 3.0f);
    const float advanced_x = sim::course_feature_world_x(anchored_sequence, 4.0f);
    require(std::abs((advanced_x - anchored_x) + 1.0f) < 0.0001f,
        "course debris does not advance in world space solely from course progress");

    const std::array<sim::CreatureBlueprint, 5> presets{
""",
    "traction and course anchoring tests",
)
text = replace_once(
    text,
    """    for (std::size_t motor_index = 0; motor_index < humanoid.motors.size(); ++motor_index)
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

    for (std::size_t stage_index = 0; stage_index < sim::course_stage_count; ++stage_index)
""",
    """    for (std::size_t motor_index = 0; motor_index < humanoid.motors.size(); ++motor_index)
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

    {
        sim::Environment biped_support{ humanoid, 0xFEE7u };
        biped_support.set_course(sim::CourseStage::balance, 0.25f);
        const std::array<float, sim::action_count> zero_actions{};
        bool support_observed = false;
        for (int frame = 0; frame < 60; ++frame)
        {
            const sim::StepResult result = biped_support.step(zero_actions);
            support_observed = support_observed
                || biped_support.left_supported() || biped_support.right_supported();
            if (result.terminated)
                break;
        }
        require(support_observed,
            "passive biped heel/toe nodes never became valid support contacts");
        require(biped_support.invalid_reason() != sim::InvalidMotion::sustained_flight,
            "grounded passive biped feet were still classified as flying");
    }

    for (std::size_t stage_index = 0; stage_index < sim::course_stage_count; ++stage_index)
""",
    "biped support regression test",
)
tests.write_text(text, encoding="utf-8")

readme = Path("README.md")
text = readme.read_text(encoding="utf-8")
text = replace_once(
    text,
    "EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, vcpkg manifest mode, and a compact PPO controller. Version 0.6.1 completes radial obstacle observations and closes the harmless-contact recovery reward exploit while retaining the autonomous curriculum introduced in v0.6.0.\n",
    "EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, vcpkg manifest mode, and a compact PPO controller. Version 0.6.2 restores real biped foot support and traction, prevents head/tail/body contacts from pinning the actor, and keeps rocks and other course debris anchored to the moving course instead of the creature.\n\n## Biped traction and world anchoring hotfix\n\nPassive heel/toe triangles now count as the left or right support cluster used by observations, gait validation, airborne checks, rewards, and recovery. Only those designated foot clusters receive strong traction; incidental head, tail, or torso contact slides instead of acting like an unintended brake. Procedural rocks and hazards use stable sequence/world coordinates and no longer inherit root translation. Version-specific autosaves prevent incompatible v0.6.1 controllers from silently resuming under the corrected contact model.\n",
    "README v0.6.2 summary",
)
readme.write_text(text, encoding="utf-8")

missions = Path("MISSIONS.md")
text = missions.read_text(encoding="utf-8")
mission = """## WALK-PHYS-001 — Biped support, traction, and world-anchored debris

**Status:** ACTIVE

Passive heel/toe geometry must participate in semantic left/right support. Designated feet require usable traction, while incidental head, tail, and body contacts must not pin the creature. Procedural rocks, hazards, and debris must remain in course/world coordinates and may not inherit actor translation.

**Acceptance:**

- Passive biped heel/toe contacts drive support observations, gait validation, airborne checks, rewards, and recovery.
- Foot contact retains substantially less horizontal velocity than incidental body contact.
- Head, tail, and torso ground contact slide rather than becoming unintended brakes.
- A course feature's world position depends on its stable sequence and course progress, never root position.
- Incompatible v0.6.1 autosaves are not resumed automatically.
- Full Windows/Vulkan build, deterministic tests, Vulkan diagnostic, package, checksum, and exact-source evidence pass.

"""
if "## WALK-PHYS-001" not in text:
    marker = "## Current warning\n"
    if marker not in text:
        raise SystemExit("mission ledger: Current warning anchor not found")
    text = text.replace(marker, mission + marker, 1)
missions.write_text(text, encoding="utf-8")

legacy_workflow = Path(".github/workflows/v050-core-diagnostic.yml")
if legacy_workflow.exists():
    legacy_workflow.unlink()
