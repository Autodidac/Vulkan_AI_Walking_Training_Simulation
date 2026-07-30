from pathlib import Path
import re

def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")

def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")

sim_path = "src/simulation.cpp"
sim = read(sim_path)

helper = r"""
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
    anchor = """        [[nodiscard]] bool direct_bone(const CreatureBlueprint& rig, std::uint16_t a, std::uint16_t b) noexcept
        {
            return std::ranges::any_of(rig.bones, [a, b](const DistanceConstraint& bone)
            {
                return (bone.a == a && bone.b == b) || (bone.a == b && bone.b == a);
            });
        }
"""
    if anchor not in sim:
        raise SystemExit("simulation helper anchor not found")
    sim = sim.replace(anchor, anchor + helper, 1)

pattern = re.compile(
    r'        result\.calibrate_motor\(0,[^\n]*\);\n'
    r'        result\.calibrate_motor\(1,[^\n]*\);\n'
    r'        result\.calibrate_motor\(2,[^\n]*\);\n'
    r'        result\.calibrate_motor\(3,[^\n]*\);'
)
sim, replacements = pattern.subn("        calibrate_quadruped_stable_defaults(result);", sim)
if replacements != 5:
    raise SystemExit(f"expected five preset calibration blocks, replaced {replacements}")
write(sim_path, sim)

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
addition = r"""    for (std::size_t motor_index = 0; motor_index < humanoid.motors.size(); ++motor_index)
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
write(test_path, tests)

readme_path = "README.md"
readme = read(readme_path)
note = """
### Quadruped-stable defaults

Every built-in body now derives its motor defaults from the stable quadruped profile. Hips/shoulders use a symmetric 22-degree envelope and knees use 30 degrees. Power is normalized by driven-limb length, so a longer humanoid leg receives the same effective endpoint correction as the quadruped instead of being launched by the same raw strength.
"""
if "### Quadruped-stable defaults" not in readme:
    marker = "## Human-calibrated defaults"
    if marker in readme:
        readme = readme.replace(marker, note + "\n" + marker, 1)
    else:
        readme += "\n" + note
write(readme_path, readme)

app_path = "src/app.cpp"
app = read(app_path)
if "QUADRUPED-STABLE MOTOR PROFILE" not in app:
    old = '                autonomy.rollout_threads, autonomy.environment_count), 1.12f, muted);\n'
    new = old + '            cursor.y += 23.0f;\n            add_text(canvas, cursor, "QUADRUPED-STABLE MOTOR PROFILE / LENGTH-NORMALIZED POWER", 1.04f, muted);\n'
    if old in app:
        app = app.replace(old, new, 1)
write(app_path, app)
