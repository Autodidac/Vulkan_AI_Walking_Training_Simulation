from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


def patch_articulated_feet_and_rig_geometry() -> None:
    text = read("src/simulation.cpp")
    start = text.index("        void add_passive_feet(CreatureBlueprint& rig")
    end = text.index("        void calibrate_grounded_defaults", start)
    replacement = r'''        void add_passive_feet(CreatureBlueprint& rig, float heel_reach = 0.20f,
            float toe_reach = 0.34f) noexcept
        {
            auto add_foot = [&](std::uint16_t ankle)
            {
                std::array<std::uint16_t, 3> result{};
                if (ankle >= rig.nodes.size() || rig.nodes.size() > 122)
                    return result;

                const Vec2 ankle_position = rig.nodes[ankle];
                const float rear_radius = ankle < rig.radii.size()
                    ? clamp(rig.radii[ankle] * 0.48f, 0.070f, 0.092f) : 0.080f;
                const float toe_radius = clamp(rear_radius * 0.86f, 0.060f, 0.080f);

                // Runner is a side-view simulation. Both feet point forward in
                // +X; mirroring one foot outward created the split stance seen
                // in the packaged preview.
                const auto heel = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({
                    ankle_position.x - heel_reach * 0.62f,
                    ankle_position.y - 0.205f
                });
                rig.radii.push_back(rear_radius);

                const auto ball = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({
                    ankle_position.x + toe_reach * 0.30f,
                    ankle_position.y - 0.212f
                });
                rig.radii.push_back(rear_radius);

                const auto toe = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({
                    ankle_position.x + toe_reach,
                    ankle_position.y - 0.195f
                });
                rig.radii.push_back(toe_radius);

                // The rear foot is a stable ankle/heel/ball triangle. The toe
                // is a separate segment hinged at the ball, so it can lift for
                // clearance and plantar-flex against the ground for push-off.
                rig.bones.push_back({ ankle, heel, 0.0f, 1.0f });
                rig.bones.push_back({ ankle, ball, 0.0f, 1.0f });
                rig.bones.push_back({ heel, ball, 0.0f, 1.0f });
                rig.bones.push_back({ ball, toe, 0.0f, 0.98f });
                result = { heel, ball, toe };
                return result;
            };

            const auto left = add_foot(rig.left_contact_node);
            const auto right = add_foot(rig.right_contact_node);
            if (left[0] != 0u && right[0] != 0u)
            {
                rig.left_contact_node = left[0];
                rig.right_contact_node = right[0];
                rig.additional_left_contact_nodes = { left[1], left[2] };
                rig.additional_right_contact_nodes = { right[1], right[2] };
            }
        }

'''
    text = text[:start] + replacement + text[end:]

    text = replace_once(text,
        '''            { -0.36f, 1.52f }, { -0.46f, 0.26f },
            { 0.36f, 1.52f }, { 0.46f, 0.26f }''',
        '''            { -0.20f, 1.52f }, { -0.16f, 0.26f },
            { 0.20f, 1.52f }, { 0.16f, 0.26f }''',
        "biped compact neutral stance")
    text = replace_once(text,
        '''            { -0.3443f, 1.5514f }, { -0.4200f, 0.2500f },
            { 0.3400f, 1.6200f }, { 0.4200f, 0.2500f },''',
        '''            { -0.2100f, 1.5514f }, { -0.1700f, 0.2500f },
            { 0.2100f, 1.6200f }, { 0.1700f, 0.2500f },''',
        "humanoid compact neutral stance")
    text = replace_once(text,
        '''            { -0.42f, 1.42f }, { -0.58f, 0.28f },
            { 0.42f, 1.42f }, { 0.58f, 0.28f },''',
        '''            { -0.25f, 1.42f }, { -0.20f, 0.28f },
            { 0.25f, 1.42f }, { 0.20f, 0.28f },''',
        "chicken compact neutral stance")

    monoped_start = text.index("    CreatureBlueprint CreatureBlueprint::monoped()")
    monoped_end = text.index("    void CreatureBlueprint::rebuild_rest_lengths() noexcept", monoped_start)
    monoped = text[monoped_start:monoped_end]
    monoped = replace_once(monoped,
        "        add_passive_feet(result);\n",
        "",
        "monoped duplicate passive feet")
    text = text[:monoped_start] + monoped + text[monoped_end:]
    write("src/simulation.cpp", text)


def patch_toe_motor_runtime() -> None:
    text = read("src/simulation.hpp")
    text = replace_once(text,
        "        void stabilize_duck_posture() noexcept;\n"
        "        void solve_motor(const MotorConstraint& motor, float action) noexcept;",
        "        void stabilize_duck_posture() noexcept;\n"
        "        [[nodiscard]] bool articulated_toe_motor(bool left,\n"
        "            MotorConstraint& motor) const noexcept;\n"
        "        void solve_articulated_toes(\n"
        "            std::span<const float, action_count> actions) noexcept;\n"
        "        void solve_motor(const MotorConstraint& motor, float action) noexcept;",
        "articulated toe declarations")
    write("src/simulation.hpp", text)

    text = read("src/simulation.cpp")
    insertion = r'''    bool Environment::articulated_toe_motor(bool left,
        MotorConstraint& motor) const noexcept
    {
        const std::uint16_t heel = left
            ? blueprint_.left_contact_node : blueprint_.right_contact_node;
        const auto& extra = left
            ? blueprint_.additional_left_contact_nodes
            : blueprint_.additional_right_contact_nodes;
        if (extra.size() < 2u)
            return false;
        const std::uint16_t ball = extra[0];
        const std::uint16_t toe = extra[1];
        if (!valid_node(heel) || !valid_node(ball) || !valid_node(toe))
            return false;

        std::uint16_t ankle = std::numeric_limits<std::uint16_t>::max();
        for (const DistanceConstraint& bone : blueprint_.bones)
        {
            std::uint16_t candidate = std::numeric_limits<std::uint16_t>::max();
            if (bone.a == ball)
                candidate = bone.b;
            else if (bone.b == ball)
                candidate = bone.a;
            if (candidate == heel || candidate == toe
                || candidate >= blueprint_.nodes.size())
                continue;
            if (direct_bone(blueprint_, candidate, heel))
            {
                ankle = candidate;
                break;
            }
        }
        if (ankle >= blueprint_.nodes.size())
            return false;

        motor = MotorConstraint{ ankle, ball, toe };
        motor.neutral_angle = signed_angle(
            blueprint_.nodes[ankle] - blueprint_.nodes[ball],
            blueprint_.nodes[toe] - blueprint_.nodes[ball]);
        motor.minimum_angle = motor.neutral_angle - degrees_to_radians(42.0f);
        motor.maximum_angle = motor.neutral_angle + degrees_to_radians(36.0f);
        motor.strength = 0.034f;
        motor.enabled = true;
        return true;
    }

    void Environment::solve_articulated_toes(
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
                toe_action = supported ? -0.06f : 0.28f;
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

'''
    marker = "    void Environment::solve_motor(const MotorConstraint& motor, float action) noexcept\n"
    text = replace_once(text, marker, insertion + marker,
        "articulated toe implementation")
    text = replace_once(text,
        '''            for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
                solve_motor(blueprint_.motors[index], applied_actions[index]);
            stabilize_balance_posture();''',
        '''            for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
                solve_motor(blueprint_.motors[index], applied_actions[index]);
            solve_articulated_toes(applied_actions);
            stabilize_balance_posture();''',
        "toe motor solver call")

    old_retention = '''                float retention = ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact && stage_uses_deformable_terrain(course_stage_))
                    retention = std::lerp(0.24f, 0.015f, firmness);'''
    new_retention = '''                float retention = ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact && stage_uses_deformable_terrain(course_stage_))
                    retention = std::lerp(0.24f, 0.015f, firmness);
                if (blueprint_.is_support_seed(index))
                {
                    const float stance_retention = (course_stage_ == CourseStage::balance
                            || course_stage_ == CourseStage::duck_press)
                        ? 0.004f : 0.024f;
                    retention = std::min(retention, stance_retention);
                }'''
    text = replace_once(text, old_retention, new_retention,
        "semantic foot traction")
    write("src/simulation.cpp", text)


def patch_coordinated_joint_learning() -> None:
    text = read("src/ppo.hpp")
    start = text.index("    [[nodiscard]] inline std::size_t motor_discovery_lane_count(\n")
    end = text.index("    [[nodiscard]] inline float motor_action_for_target_angle(\n", start)
    replacement = r'''    [[nodiscard]] inline std::size_t motor_discovery_lane_count(
        const sim::CreatureBlueprint& rig) noexcept
    {
        return std::min<std::size_t>(2u * rig.active_motor_count + 8u, 28u);
    }

    [[nodiscard]] inline MotorDiscoveryProbe motor_discovery_probe(
        const sim::Environment& environment, std::size_t environment_index,
        std::uint64_t update, std::size_t rollout_step) noexcept
    {
        MotorDiscoveryProbe probe{};
        const std::size_t active = environment.blueprint().active_motor_count;
        const std::size_t lane_count = motor_discovery_lane_count(environment.blueprint());
        if (active == 0u || environment_index >= lane_count || update >= 480u)
            return probe;
        const std::size_t half_cycle = (rollout_step / 24u) & 1u;
        const float progress = clamp(static_cast<float>(update) / 480.0f, 0.0f, 1.0f);
        const float amplitude = lerp(0.20f, 0.48f, progress);
        const std::size_t lane = environment_index % lane_count;
        if (half_cycle != 0u)
        {
            probe.weight = 0.88f;
            return probe;
        }
        if (lane < active)
            probe.action[lane] = amplitude;
        else if (lane < active * 2u)
            probe.action[lane - active] = -amplitude;
        else
        {
            const std::size_t pattern = lane - active * 2u;
            if (pattern == 0u || pattern == 1u)
            {
                const float sign = pattern == 0u ? 1.0f : -1.0f;
                for (std::size_t index = 0; index < active; ++index)
                    probe.action[index] = amplitude * sign;
            }
            else if (pattern == 2u)
            {
                for (std::size_t index = 0; index < active; ++index)
                    probe.action[index] = ((index / 2u) & 1u) == 0u
                        ? amplitude : -amplitude;
            }
            else if (pattern == 3u)
            {
                for (std::size_t index = 0; index < active; ++index)
                    probe.action[index] = (index & 1u) == 0u
                        ? amplitude : -amplitude;
            }
            else if (active >= 4u)
            {
                // Anatomy-aware simultaneous lanes: left chain, right chain,
                // bilateral crouch, and bilateral extension. These explicitly
                // teach that hip and knee joints may move in one policy step.
                if (pattern == 4u || pattern == 6u)
                {
                    probe.action[0] = -amplitude;
                    probe.action[1] = amplitude;
                }
                if (pattern == 5u || pattern == 6u)
                {
                    probe.action[2] = amplitude;
                    probe.action[3] = -amplitude;
                }
                if (pattern == 7u)
                {
                    probe.action[0] = amplitude;
                    probe.action[1] = -amplitude;
                    probe.action[2] = -amplitude;
                    probe.action[3] = amplitude;
                }
            }
        }
        probe.weight = 0.88f;
        return probe;
    }

'''
    text = text[:start] + replacement + text[end:]

    start = text.index("    [[nodiscard]] inline std::array<float, sim::action_count> bilateral_joint_synergy_action(\n")
    end = text.index("    [[nodiscard]] inline std::array<float, sim::action_count> duck_teacher_action(\n", start)
    synergy = r'''    [[nodiscard]] inline std::array<float, sim::action_count> bilateral_joint_synergy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> action,
        sim::CourseStage stage) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        if (!rig.paired_leg_chains())
        {
            for (float& value : action)
                value = clamp(value, -1.0f, 1.0f);
            return action;
        }

        if (stage == sim::CourseStage::duck_press)
        {
            const float left_flex = std::max(0.0f,
                0.5f * (-action[0] + action[1]));
            const float right_flex = std::max(0.0f,
                0.5f * (action[2] - action[3]));
            const float shared_flex = 0.5f * (left_flex + right_flex);
            constexpr float chain_strength = 0.78f;
            action[0] = lerp(action[0], -shared_flex, chain_strength);
            action[1] = lerp(action[1], shared_flex, chain_strength);
            action[2] = lerp(action[2], shared_flex, chain_strength);
            action[3] = lerp(action[3], -shared_flex, chain_strength);
        }
        else if (stage != sim::CourseStage::balance)
        {
            const float pair_strength = stage == sim::CourseStage::crouch_walk
                ? 0.18f : 0.10f;
            const float hip_mirror = 0.5f * (action[0] - action[2]);
            const float knee_mirror = 0.5f * (action[1] - action[3]);
            action[0] = lerp(action[0], hip_mirror, pair_strength);
            action[2] = lerp(action[2], -hip_mirror, pair_strength);
            action[1] = lerp(action[1], knee_mirror, pair_strength);
            action[3] = lerp(action[3], -knee_mirror, pair_strength);
        }

        if (rig.active_motor_count >= 8u)
        {
            const float arm_pair_strength = sim::stage_allows_controlled_flips(stage)
                ? 0.24f : 0.06f;
            const float shoulder = 0.5f * (action[4] - action[6]);
            const float elbow = 0.5f * (action[5] - action[7]);
            action[4] = lerp(action[4], shoulder, arm_pair_strength);
            action[6] = lerp(action[6], -shoulder, arm_pair_strength);
            action[5] = lerp(action[5], elbow, arm_pair_strength);
            action[7] = lerp(action[7], -elbow, arm_pair_strength);
        }

        if (rig.active_motor_count >= 8u
            && environment.longest_stable_stance_seconds() < 1.0f
            && !sim::stage_allows_controlled_flips(stage))
        {
            for (std::size_t index = 4; index < 8; ++index)
                action[index] *= 0.08f;
        }
        for (float& value : action)
            value = clamp(value, -1.0f, 1.0f);
        return action;
    }

'''
    text = text[:start] + synergy + text[end:]

    start = text.index("    [[nodiscard]] inline std::array<float, sim::action_count> duck_teacher_action(\n")
    end = text.index("    [[nodiscard]] inline std::array<float, sim::action_count> crouch_walk_teacher_action(\n", start)
    duck = r'''    [[nodiscard]] inline std::array<float, sim::action_count> duck_teacher_action(
        const sim::Environment& environment) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        float pressure = environment.duck_press_completed()
            ? 0.0f : environment.duck_obstacle_weight();
        if (environment.duck_active())
            pressure *= 0.55f;
        auto action = rig.paired_leg_chains()
            ? balance_teacher_action(environment)
            : compact_support_teacher_action(environment, pressure * 0.48f);
        if (rig.paired_leg_chains() && !environment.duck_press_completed())
        {
            const float span_ratio = environment.primary_support_span_ratio();
            const float span_brake = clamp((span_ratio - 1.02f) * 0.34f,
                0.0f, 0.24f);
            const float hip_flex = std::max(0.06f,
                0.18f * pressure - span_brake);
            const float knee_flex = 0.46f * pressure;
            action[0] = clamp(action[0] - hip_flex, -0.62f, 0.62f);
            action[1] = clamp(action[1] + knee_flex, -0.82f, 0.82f);
            action[2] = clamp(action[2] + hip_flex, -0.62f, 0.62f);
            action[3] = clamp(action[3] - knee_flex, -0.82f, 0.82f);
        }
        for (std::size_t index = 4; index < rig.active_motor_count; ++index)
            action[index] = 0.0f;
        return bilateral_joint_synergy_action(environment, action,
            sim::CourseStage::duck_press);
    }

'''
    text = text[:start] + duck + text[end:]

    text = text.replace(
        "            const float leg_assist = 0.58f + pressure * 0.20f;",
        "            const float leg_assist = 0.76f + pressure * 0.16f;",
        1)
    write("src/ppo.hpp", text)


def patch_tests_and_docs() -> None:
    text = read("tests/core_tests.cpp")
    marker = '''        static void collapse_upper_body(Environment& environment) noexcept
'''
    helper = r'''        static bool articulated_toes_move(Environment& environment) noexcept
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

'''
    text = replace_once(text, marker, helper + marker,
        "articulated toe test access")

    insert_after = '''    require(press_retract.retracting && press_retract.vertical_velocity > 0.0f,
        "duck press does not retract after the hold");
'''
    tests = r'''    auto articulated_forward_foot = [](const sim::CreatureBlueprint& rig,
        bool left)
    {
        const std::uint16_t heel = left ? rig.left_contact_node : rig.right_contact_node;
        const auto& extra = left
            ? rig.additional_left_contact_nodes : rig.additional_right_contact_nodes;
        if (heel >= rig.nodes.size() || extra.size() < 2u
            || extra[0] >= rig.nodes.size() || extra[1] >= rig.nodes.size())
            return false;
        const std::uint16_t ball = extra[0];
        const std::uint16_t toe = extra[1];
        const bool toe_hinge = std::ranges::any_of(rig.bones,
            [ball, toe](const sim::DistanceConstraint& bone)
            {
                return (bone.a == ball && bone.b == toe)
                    || (bone.a == toe && bone.b == ball);
            });
        const bool rigid_heel_to_toe = std::ranges::any_of(rig.bones,
            [heel, toe](const sim::DistanceConstraint& bone)
            {
                return (bone.a == heel && bone.b == toe)
                    || (bone.a == toe && bone.b == heel);
            });
        return rig.nodes[heel].x < rig.nodes[ball].x
            && rig.nodes[ball].x < rig.nodes[toe].x
            && toe_hinge && !rigid_heel_to_toe;
    };
    for (const sim::CreatureBlueprint rig : {
            sim::CreatureBlueprint::chicken(),
            sim::CreatureBlueprint::biped(),
            sim::CreatureBlueprint::humanoid() })
    {
        require(rig.support_seed_count() == 6u
                && articulated_forward_foot(rig, true)
                && articulated_forward_foot(rig, false),
            "paired rig lacks forward articulated heel-ball-toe feet");
    }
    sim::Environment toe_environment(sim::CreatureBlueprint::biped(), 79u);
    toe_environment.set_course(sim::CourseStage::duck_press, 0.25f);
    require(sim::EnvironmentTestAccess::articulated_toes_move(toe_environment),
        "coordinated leg action does not actuate both toe hinges");
    const sim::Environment discovery_environment(sim::CreatureBlueprint::biped(), 83u);
    const std::size_t crouch_lane = 2u
        * discovery_environment.blueprint().active_motor_count + 6u;
    const rl::MotorDiscoveryProbe crouch_probe = rl::motor_discovery_probe(
        discovery_environment, crouch_lane, 120u, 0u);
    require(crouch_probe.action[0] < 0.0f && crouch_probe.action[1] > 0.0f
            && crouch_probe.action[2] > 0.0f && crouch_probe.action[3] < 0.0f,
        "motor discovery does not explore simultaneous bilateral hip-knee flexion");
'''
    text = replace_once(text, insert_after, insert_after + tests,
        "foot and synergy regression tests")
    write("tests/core_tests.cpp", text)

    cache = read("missioncache.md")
    if "WALK-FOOT-124" not in cache:
        cache = cache.rstrip() + r'''

### WALK-FOOT-124 — Articulated forward heel-ball-toe feet
**Status:** IN VALIDATION

Chicken, biped, and humanoid feet point forward in the side view and use a rigid rear foot plus a ball-to-toe hinge. Grounded toes plantar-flex for push-off; swing/crouch toes dorsiflex for clearance and stability. The monoped keeps its existing authored heel/toe motors instead of receiving duplicate feet.

### WALK-SYNERGY-125 — Discover and execute simultaneous joint chains
**Status:** IN VALIDATION

Motor discovery includes left-chain, right-chain, bilateral crouch, and bilateral extension probes. Static crouch couples same-side hip, knee, and toe motion in one policy step, prioritizes hip flexion, and brakes excessive support-span widening instead of teaching a split.

### WALK-PREVIEW-126 — Stop uncontrolled preview foot sliding
**Status:** IN VALIDATION

Semantic heel, ball, and toe contacts receive stance traction while retaining ordinary controlled sliding outside the static lessons. All seven named Stand/Crouch package gates must remain valid; friction-only shuffling still receives no gait credit.
'''
    write("missioncache.md", cache)

    changelog = read("CHANGELOG.md")
    anchor = "- Isolated corrected training with v0.7.12 semantics and autosave names."
    additions = """- Rebuilt paired feet as forward-facing articulated heel-ball-toe chains with automatic toe stabilization and push-off.
- Added anatomy-aware simultaneous hip/knee discovery lanes and strong same-side crouch coordination.
- Narrowed the authored biped, humanoid, and chicken neutral stance and removed duplicate monoped feet.
- Added stance traction to semantic foot contacts to stop uncontrolled preview skating without crediting friction-only gait.
"""
    if "forward-facing articulated heel-ball-toe" not in changelog:
        changelog = replace_once(changelog, anchor, anchor + "\n" + additions.rstrip(),
            "v0.7.12 changelog foot anchor")
    write("CHANGELOG.md", changelog)


def main() -> None:
    patch_articulated_feet_and_rig_geometry()
    patch_toe_motor_runtime()
    patch_coordinated_joint_learning()
    patch_tests_and_docs()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
