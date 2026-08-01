from pathlib import Path

root = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise RuntimeError(f"expected {label} not found")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    root / "CMakeLists.txt",
    "set(CMAKE_CXX_SCAN_FOR_MODULES ON)",
    "if(MSVC)\n    set(CMAKE_CXX_SCAN_FOR_MODULES ON)\nelse()\n    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)\nendif()",
    "CMAKE_CXX_SCAN_FOR_MODULES setting",
)
replace_once(
    root / "src/ppo.hpp",
    "#include <string_view>\n#include <vector>",
    "#include <string_view>\n#include <thread>\n#include <vector>",
    "ppo include block",
)
replace_once(
    root / "tests/runtime_pipeline_tests.cpp",
    "            neutral.step(zero);\n            arms.step(arm_action);",
    "            static_cast<void>(neutral.step(zero));\n            static_cast<void>(arms.step(arm_action));",
    "arm action test calls",
)

core = root / "tests/core_tests.cpp"
replace_once(
    core,
    "        for (std::size_t motor_index = 0; motor_index < preset.motors.size(); ++motor_index)",
    "        for (std::size_t motor_index = 0; motor_index < preset.active_motor_count; ++motor_index)",
    "preset active motor loop",
)
replace_once(
    core,
    '''    require(humanoid.nodes.size() == 11, "human-calibrated rig should include passive heel/toe feet");
    require(std::abs(humanoid.nodes[0].y - 2.8127f) < 0.01f, "uploaded humanoid pelvis calibration not applied");
    require(humanoid.nodes.size() == 11, "humanoid passive heel/toe feet were not created");
    require(humanoid.bones.size() == 12, "humanoid feet are not structurally connected");
    for (std::size_t motor_index = 0; motor_index < humanoid.motors.size(); ++motor_index)
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
    }''',
    '''    require(humanoid.nodes.size() >= 17,
        "human-calibrated rig should include passive heel/toe feet and articulated arms");
    require(std::abs(humanoid.nodes[0].y - 2.8127f) < 0.01f,
        "uploaded humanoid pelvis calibration not applied");
    require(humanoid.bones.size() >= 17,
        "humanoid feet or arms are not structurally connected");
    require(humanoid.active_motor_count == sim::action_count,
        "humanoid does not expose independent shoulder and elbow motors");
    for (std::size_t motor_index = 0; motor_index < humanoid.active_motor_count; ++motor_index)
    {
        const sim::MotorConstraint& motor = humanoid.motors[motor_index];
        require(motor.enabled, "humanoid active motor is disabled");
        if (motor_index < 4u)
        {
            const float driven_arm = length(humanoid.nodes[motor.c] - humanoid.nodes[motor.pivot]);
            const float expected_linear_gain = (motor_index % 2u) == 0u ? 0.045f : 0.051f;
            const float expected_travel = (motor_index % 2u) == 0u ? 36.0f : 58.0f;
            require(std::abs(motor.strength * std::max(0.75f, driven_arm) - expected_linear_gain) < 0.003f,
                "humanoid leg motor does not use the bounded obstacle-capable effective gain");
            require(std::abs((motor.neutral_angle - motor.minimum_angle) * 180.0f / pi
                - expected_travel) < 0.05f, "obstacle-capable backward leg travel was not applied");
            require(std::abs((motor.maximum_angle - motor.neutral_angle) * 180.0f / pi
                - expected_travel) < 0.05f, "obstacle-capable forward leg travel was not applied");
        }
        else
        {
            require((motor.maximum_angle - motor.minimum_angle) * 180.0f / pi >= 180.0f,
                "humanoid arm motor lacks useful acrobatic travel");
            require(motor.strength <= 0.040f,
                "humanoid arm motor is too strong for balance and controlled flips");
        }
    }''',
    "humanoid structure and motor assertions",
)

print("Applied v0.7 integration fixups.")
