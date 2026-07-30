from pathlib import Path
import re


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


sim_path = "src/simulation.cpp"
sim = read(sim_path)

helpers = r"""
        void add_passive_feet(CreatureBlueprint& rig, float heel_reach = 0.20f,
            float toe_reach = 0.34f) noexcept
        {
            auto add_foot = [&](std::uint16_t ankle)
            {
                if (ankle >= rig.nodes.size() || rig.nodes.size() > 124)
                    return;
                const Vec2 center = rig.nodes[ankle];
                const float radius = ankle < rig.radii.size()
                    ? clamp(rig.radii[ankle] * 0.72f, 0.10f, 0.15f) : 0.12f;
                const auto heel = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({ center.x - heel_reach, center.y - 0.01f });
                rig.radii.push_back(radius);
                const auto toe = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({ center.x + toe_reach, center.y - 0.015f });
                rig.radii.push_back(radius);
                rig.bones.push_back({ ankle, heel, 0.0f, 0.96f });
                rig.bones.push_back({ ankle, toe, 0.0f, 0.96f });
                rig.bones.push_back({ heel, toe, 0.0f, 0.88f });
            };
            add_foot(rig.left_contact_node);
            add_foot(rig.right_contact_node);
        }

        void calibrate_quadruped_stable_defaults(CreatureBlueprint& rig) noexcept
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
        }
"""

if "calibrate_quadruped_stable_defaults" not in sim:
    marker = "    }\n\n    CreatureBlueprint CreatureBlueprint::chicken()"
    if marker not in sim:
        raise SystemExit("anonymous namespace close was not found")
    sim = sim.replace(marker, helpers + "    }\n\n    CreatureBlueprint CreatureBlueprint::chicken()", 1)


def patch_preset_feet(source: str, name: str) -> str:
    start_token = f"    CreatureBlueprint CreatureBlueprint::{name}()"
    start = source.find(start_token)
    if start < 0:
        raise SystemExit(f"{name} preset was not found")
    next_start = source.find("    CreatureBlueprint CreatureBlueprint::", start + len(start_token))
    end = len(source) if next_start < 0 else next_start
    block = source[start:end]
    if "add_passive_feet(result" not in block:
        anchor = "        result.rebuild_rest_lengths();"
        if anchor not in block:
            raise SystemExit(f"{name} rest-length anchor was not found")
        block = block.replace(anchor,
            "        add_passive_feet(result);\n        result.rebuild_rest_lengths();", 1)
    return source[:start] + block + source[end:]


for preset_name in ("chicken", "biped", "humanoid", "monoped"):
    sim = patch_preset_feet(sim, preset_name)

pattern = re.compile(
    r'        result\.calibrate_motor\(0,[^\n]*\);\n'
    r'        result\.calibrate_motor\(1,[^\n]*\);\n'
    r'        result\.calibrate_motor\(2,[^\n]*\);\n'
    r'        result\.calibrate_motor\(3,[^\n]*\);'
)
sim, replacements = pattern.subn("        calibrate_quadruped_stable_defaults(result);", sim)
if replacements != 5:
    raise SystemExit(f"expected five preset calibration blocks, replaced {replacements}")

if "control_ramp" not in sim:
    anchor = """        dt = clamp(dt, 1.0f / 240.0f, 1.0f / 30.0f);
        constexpr Vec2 gravity{ 0.0f, -22.0f };
"""
    replacement = """        dt = clamp(dt, 1.0f / 240.0f, 1.0f / 30.0f);
        // Let every body settle onto its feet before the policy can apply a
        // meaningful impulse, then ease control in rather than launching it.
        const float ramp_t = clamp((elapsed_seconds_ - 0.35f) / 1.25f, 0.0f, 1.0f);
        const float control_ramp = ramp_t * ramp_t * (3.0f - 2.0f * ramp_t);
        std::array<float, action_count> applied_actions{};
        for (std::size_t index = 0; index < action_count; ++index)
            applied_actions[index] = clamp(actions[index], -1.0f, 1.0f) * control_ramp;
        constexpr Vec2 gravity{ 0.0f, -22.0f };
"""
    if anchor not in sim:
        raise SystemExit("environment step soft-start anchor was not found")
    sim = sim.replace(anchor, replacement, 1)

    motor_call = "                solve_motor(blueprint_.motors[index], actions[index]);"
    if motor_call not in sim:
        raise SystemExit("motor application call was not found")
    sim = sim.replace(motor_call,
        "                solve_motor(blueprint_.motors[index], applied_actions[index]);", 1)

    energy = "            const float effective = blueprint_.motors[index].enabled ? actions[index] : 0.0f;"
    if energy not in sim:
        raise SystemExit("action energy line was not found")
    sim = sim.replace(energy,
        "            const float effective = blueprint_.motors[index].enabled ? applied_actions[index] : 0.0f;", 1)

write(sim_path, sim)

network_path = "src/ppo_network.cpp"
network = read(network_path)
network = network.replace(
    "parameters_[layout_.actor_w + index] = random_normal() * 0.01f;",
    "parameters_[layout_.actor_w + index] = random_normal() * 0.0035f;",
    1,
)
network = network.replace(
    "parameters_[layout_.log_std + index] = std::log(0.14f);",
    "parameters_[layout_.log_std + index] = std::log(0.08f);",
    1,
)
write(network_path, network)

test_path = "tests/core_tests.cpp"
tests = read(test_path)
tests = re.sub(
    r'\n\s*require\(std::abs\(humanoid\.motors\[0\]\.strength[^\n]*\n'
    r'\s*require\(std::abs\(humanoid\.motors\[1\]\.strength[^\n]*\n',
    '\n',
    tests,
    count=1,
)
anchor = '    require(std::abs(humanoid.nodes[0].y - 2.8127f) < 0.01f, "uploaded humanoid pelvis calibration not applied");\n'
addition = r"""    require(humanoid.nodes.size() == 11, "humanoid passive heel/toe feet were not created");
    require(humanoid.bones.size() == 12, "humanoid feet are not structurally connected");
    for (std::size_t motor_index = 0; motor_index < humanoid.motors.size(); ++motor_index)
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
"""
if addition not in tests:
    if anchor not in tests:
        raise SystemExit("humanoid test anchor not found")
    tests = tests.replace(anchor, anchor + addition, 1)

exploration_anchor = '    require(trainer.exploration() < 0.20f, "fresh exploration is too aggressive");\n'
exploration_test = '    require(trainer.exploration() <= 0.081f, "fresh policy still applies an aggressive spawn impulse");\n'
if exploration_test not in tests:
    if exploration_anchor not in tests:
        raise SystemExit("fresh exploration test anchor not found")
    tests = tests.replace(exploration_anchor, exploration_anchor + exploration_test, 1)
write(test_path, tests)

readme_path = "README.md"
readme = read(readme_path)
note = """
### Quadruped-stable bodies and real feet

Every built-in body now derives its motor defaults from the stable quadruped profile. Hips/shoulders use a symmetric 22-degree envelope and knees use 30 degrees. Power is normalized by driven-limb length, so a longer humanoid leg receives the same effective endpoint correction as the quadruped instead of being launched by the same raw strength.

Bipeds, humanoids, chickens, and monopeds now have passive heel/toe triangles rather than balancing on one circular contact point. Every episode begins with a short no-control settling period and a smooth motor ramp, while fresh policies start with lower output weights and exploration. This preserves each body shape while copying the quadruped's stable startup behavior.
"""
if "### Quadruped-stable bodies and real feet" not in readme:
    marker = "## Human-calibrated defaults"
    if marker in readme:
        readme = readme.replace(marker, note + "\n" + marker, 1)
    else:
        readme += "\n" + note
write(readme_path, readme)

app_path = "src/app.cpp"
app = read(app_path)
if "QUADRUPED-STABLE MOTORS / REAL FEET / SOFT START" not in app:
    old = '                autonomy.rollout_threads, autonomy.environment_count), 1.12f, muted);\n'
    new = old + '            cursor.y += 23.0f;\n            add_text(canvas, cursor, "QUADRUPED-STABLE MOTORS / REAL FEET / SOFT START", 1.04f, muted);\n'
    if old in app:
        app = app.replace(old, new, 1)
write(app_path, app)
