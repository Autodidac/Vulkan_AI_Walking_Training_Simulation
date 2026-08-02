from __future__ import annotations

from pathlib import Path
import shutil


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise RuntimeError(f"missing {label} anchor")
    return text.replace(old, new, 1)


def main() -> None:
    simulation_path = Path("src/simulation.cpp")
    simulation = simulation_path.read_text(encoding="utf-8")
    simulation = replace_once(
        simulation,
        "{ ankle_position.x + 0.055f, ankle_position.y - 0.095f }",
        "{ ankle_position.x + 0.055f, ankle_position.y - 0.205f }",
        "passive foot vertical separation",
    )
    simulation = replace_once(
        simulation,
        """                // The ankle remains the final articulated leg joint.  A short
                // passive ankle-to-foot link feeds a separate contact plate;
                // the lower-leg endpoint itself is never a semantic foot.
                rig.bones.push_back({ ankle, foot, 0.0f, 0.98f });
                rig.bones.push_back({ foot, heel, 0.0f, 0.96f });
                rig.bones.push_back({ foot, toe, 0.0f, 0.96f });
                rig.bones.push_back({ heel, toe, 0.0f, 0.90f });
""",
        """                // The ankle remains the final articulated leg joint. A braced
                // passive adapter feeds a separate contact plate; the lower-leg
                // endpoint itself is never a semantic foot or traction contact.
                rig.bones.push_back({ ankle, foot, 0.0f, 0.98f });
                rig.bones.push_back({ ankle, heel, 0.0f, 0.94f });
                rig.bones.push_back({ ankle, toe, 0.0f, 0.94f });
                rig.bones.push_back({ foot, heel, 0.0f, 0.96f });
                rig.bones.push_back({ foot, toe, 0.0f, 0.96f });
                rig.bones.push_back({ heel, toe, 0.0f, 0.90f });
""",
        "braced passive foot",
    )
    simulation_path.write_text(simulation, encoding="utf-8")

    policy_path = Path("src/ppo.hpp")
    policy = policy_path.read_text(encoding="utf-8")
    policy = replace_once(
        policy,
        """            const auto observation = environment.observation();
    const bool overhead_bar = std::abs(observation[30]) < 0.05f
        && observation[32] > 0.01f;
    const float observed_weight = overhead_bar
        ? sim::duck_obstacle_approach_weight(observation[29] * 6.0f) : 0.0f;
    const float obstacle_weight = std::max(
        environment.duck_obstacle_weight(), observed_weight);
    const float assist = 0.56f + obstacle_weight * 0.34f;
""",
        """            const auto observation = environment.observation();
            const bool overhead_bar = std::abs(observation[30]) < 0.05f
                && observation[32] > 0.01f;
            const float observed_weight = overhead_bar
                ? sim::duck_obstacle_approach_weight(observation[29] * 6.0f) : 0.0f;
            const float obstacle_weight = std::max(
                environment.duck_obstacle_weight(), observed_weight);
            const float assist = 0.56f + obstacle_weight * 0.34f;
""",
        "duck-assistance formatting",
    )
    policy_path.write_text(policy, encoding="utf-8")

    tests_path = Path("tests/core_tests.cpp")
    tests = tests_path.read_text(encoding="utf-8")
    tests = replace_once(
        tests,
        """    require(humanoid.additional_left_contact_nodes.size() == 2u
            && humanoid.additional_right_contact_nodes.size() == 2u,
        \"dedicated foot plates do not include heel and toe contacts\");
""",
        """    require(humanoid.additional_left_contact_nodes.size() == 2u
            && humanoid.additional_right_contact_nodes.size() == 2u,
        \"dedicated foot plates do not include heel and toe contacts\");
    require(humanoid.nodes[humanoid.motors[1].c].y
            - humanoid.nodes[humanoid.left_contact_node].y >= 0.18f
            && humanoid.nodes[humanoid.motors[3].c].y
                - humanoid.nodes[humanoid.right_contact_node].y >= 0.18f,
        \"passive foot adapter leaves an ankle on the contact plane\");
""",
        "ankle-clearance regression test",
    )
    tests_path.write_text(tests, encoding="utf-8")

    obsolete_script = Path("tools/apply_v072_sim_regression.py")
    if obsolete_script.exists():
        obsolete_script.unlink()
    shutil.rmtree("tools/__pycache__", ignore_errors=True)

    print("Applied idempotent v0.7.2 materialized-source corrections")


if __name__ == "__main__":
    main()
