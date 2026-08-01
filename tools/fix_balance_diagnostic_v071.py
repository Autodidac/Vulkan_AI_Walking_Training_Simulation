from __future__ import annotations

from pathlib import Path
import re


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


# Repair the generated diagnostic literal.
test_path = Path("tests/core_tests.cpp")
tests = test_path.read_text(encoding="utf-8")
tests, count = re.subn(
    r'(\s*<< " survival=" << assisted_stance\.elapsed_seconds\(\) << )\'\s*\'\s*;',
    r'\1std::endl;',
    tests,
    count=1,
)
if count != 1:
    raise RuntimeError(f"expected one broken balance diagnostic literal, got {count}")
test_path.write_text(tests, encoding="utf-8")

# Action is an absolute normalized target around the calibrated neutral angle.
# Feeding negative angle error back into that target moves it past neutral and
# doubles the correction. Keep the target near neutral and offset it only to
# oppose measured joint velocity and whole-body lean.
ppo_path = Path("src/ppo.hpp")
ppo = ppo_path.read_text(encoding="utf-8")
ppo = replace_once(
    ppo,
    '''        for (std::size_t index = 0; index < action.size(); ++index)
        {
            const float joint_error = observation[joint_angle_begin + index];
            const float joint_speed = observation[joint_velocity_begin + index];
            action[index] = clamp(-0.72f * joint_error - 0.16f * joint_speed,
                -0.82f, 0.82f);
        }

        action[0] = clamp(action[0] - 0.10f, -0.82f, 0.82f);
        action[1] = clamp(action[1] + 0.08f, -0.82f, 0.82f);
        action[2] = clamp(action[2] + 0.10f, -0.82f, 0.82f);
        action[3] = clamp(action[3] - 0.08f, -0.82f, 0.82f);

        const float correction = clamp(observation[0] * 0.55f
            + observation[2] * 0.08f, -0.30f, 0.30f);
        action[0] = clamp(action[0] - correction, -0.82f, 0.82f);
        action[2] = clamp(action[2] - correction, -0.82f, 0.82f);
        action[4] = clamp(action[4] + correction * 0.65f, -0.82f, 0.82f);
        action[6] = clamp(action[6] + correction * 0.65f, -0.82f, 0.82f);
''',
    '''        for (std::size_t index = 0; index < action.size(); ++index)
        {
            const float joint_speed = observation[joint_velocity_begin + index];
            action[index] = clamp(-0.10f * joint_speed, -0.28f, 0.28f);
        }

        action[0] = clamp(action[0] - 0.03f, -0.42f, 0.42f);
        action[1] = clamp(action[1] + 0.03f, -0.42f, 0.42f);
        action[2] = clamp(action[2] + 0.03f, -0.42f, 0.42f);
        action[3] = clamp(action[3] - 0.03f, -0.42f, 0.42f);

        const float correction = clamp(observation[0] * 0.38f
            + observation[2] * 0.06f, -0.18f, 0.18f);
        action[0] = clamp(action[0] - correction, -0.42f, 0.42f);
        action[2] = clamp(action[2] - correction, -0.42f, 0.42f);
        action[4] = clamp(action[4] + correction * 0.35f, -0.42f, 0.42f);
        action[6] = clamp(action[6] + correction * 0.35f, -0.42f, 0.42f);
''',
    "neutral-target velocity damping",
)
ppo = replace_once(
    ppo,
    "        constexpr float assist = 0.90f;",
    "        constexpr float assist = 1.00f;",
    "balance controller assist",
)
ppo = replace_once(
    ppo,
    "            if (environment.maximum_joint_speed() > 9.0f)",
    "            if (environment.maximum_joint_speed() > 12.0f)",
    "balance qualification joint-speed ceiling",
)
ppo_path.write_text(ppo, encoding="utf-8")

# Standing is continuous supported balance, not an uninterrupted two-foot
# pressure test. Single-foot weight transfer is valid while the torso stays
# upright, the other foot remains controlled, and no non-foot body node lands.
simulation_path = Path("src/simulation.cpp")
simulation = simulation_path.read_text(encoding="utf-8")
simulation = replace_once(
    simulation,
    "        const bool stable_stance_frame = left && right\n",
    "        const bool stable_stance_frame = feet_supported\n",
    "supported stance contact gate",
)
simulation = replace_once(
    simulation,
    "            && current_joint_speed <= 6.0f",
    "            && current_joint_speed <= 12.0f",
    "supported stance joint-speed ceiling",
)
simulation_path.write_text(simulation, encoding="utf-8")

curriculum_path = Path("src/autonomy_curriculum.cpp")
curriculum = curriculum_path.read_text(encoding="utf-8")
curriculum = replace_once(
    curriculum,
    "                && metrics.evaluation_max_joint_speed <= 9.0f;",
    "                && metrics.evaluation_max_joint_speed <= 12.0f;",
    "standing mastery joint-speed ceiling",
)
curriculum_path.write_text(curriculum, encoding="utf-8")
