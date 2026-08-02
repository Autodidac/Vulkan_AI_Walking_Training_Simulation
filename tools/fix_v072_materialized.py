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
    header_path = Path("src/simulation.hpp")
    header = header_path.read_text(encoding="utf-8")
    header = replace_once(
        header,
        """        [[nodiscard]] float uprightness() const noexcept { return torso_uprightness(); }
        [[nodiscard]] bool left_supported() const noexcept
""",
        """        [[nodiscard]] float uprightness() const noexcept { return torso_uprightness(); }
        [[nodiscard]] bool current_display_posture_valid() const noexcept;
        [[nodiscard]] bool left_supported() const noexcept
""",
        "current display-posture declaration",
    )
    header_path.write_text(header, encoding="utf-8")

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

    strict_display = """    bool Environment::current_display_posture_valid() const noexcept
    {
        if (!valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node)
            || !valid_node(blueprint_.left_contact_node)
            || !valid_node(blueprint_.right_contact_node))
            return false;

        const bool supported = contact_supported(blueprint_.left_contact_node)
            || contact_supported(blueprint_.right_contact_node);
        if (!supported || non_foot_ground_contact())
            return false;

        const Vec2 root = particles_[blueprint_.root_node].position;
        const Vec2 torso = particles_[blueprint_.torso_node].position;
        const Vec2 head = particles_[blueprint_.head_node].position;
        const float rest_torso_length = length(
            blueprint_.nodes[blueprint_.torso_node]
                - blueprint_.nodes[blueprint_.root_node]);
        const float rest_head_length = length(
            blueprint_.nodes[blueprint_.head_node]
                - blueprint_.nodes[blueprint_.root_node]);
        const float torso_length = length(torso - root);
        const float head_length = length(head - root);
        const float torso_ground = ground_height_at(torso.x);
        const float head_ground = ground_height_at(head.x);

        return torso_uprightness() >= 0.55f
            && torso_length >= rest_torso_length * 0.58f
            && head_length >= rest_head_length * 0.55f
            && torso.y - torso_ground >= 0.55f
            && head.y - head_ground >= 0.80f;
    }

"""
    current_display = """    bool Environment::current_display_posture_valid() const noexcept
    {
        if (!valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node)
            || !valid_node(blueprint_.left_contact_node)
            || !valid_node(blueprint_.right_contact_node))
            return false;

        const bool supported = contact_supported(blueprint_.left_contact_node)
            || contact_supported(blueprint_.right_contact_node);
        if (!supported || non_foot_grounded_)
            return false;

        const Vec2 root = particles_[blueprint_.root_node].position;
        const float torso_length = length(
            particles_[blueprint_.torso_node].position - root);
        const float head_length = length(
            particles_[blueprint_.head_node].position - root);
        const float rest_torso_length = length(
            blueprint_.nodes[blueprint_.torso_node]
                - blueprint_.nodes[blueprint_.root_node]);
        const float rest_head_length = length(
            blueprint_.nodes[blueprint_.head_node]
                - blueprint_.nodes[blueprint_.root_node]);

        // Historical lesson evidence may remain latched, but the body displayed
        // now must still have intact torso and head geometry. Ducking preserves
        // these segment lengths; the collapsed screenshot pose does not.
        return torso_length >= rest_torso_length * 0.58f
            && head_length >= rest_head_length * 0.55f;
    }

"""
    if current_display not in simulation:
        if strict_display in simulation:
            simulation = simulation.replace(strict_display, current_display, 1)
        else:
            anchor = """    void Environment::invalidate(InvalidMotion reason) noexcept
    {
"""
            if anchor not in simulation:
                raise RuntimeError("missing current display-posture implementation anchor")
            simulation = simulation.replace(anchor, current_display + anchor, 1)
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
    policy = replace_once(
        policy,
        """        if (!qualification.valid || environment.non_foot_grounded())
            return false;
""",
        """        if (!qualification.valid
            || !environment.current_display_posture_valid())
            return false;
""",
        "fresh display-sample posture gate",
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
    tests = replace_once(
        tests,
        """        sim::EnvironmentTestAccess::collapse_upper_body(assisted_stance);
        require(!rl::stage_display_sample_eligible(
                sim::CourseStage::balance, assisted_stance),
            \"collapsed current frame is still published as a valid sample\");
""",
        """        sim::EnvironmentTestAccess::collapse_upper_body(assisted_stance);
        require(!assisted_stance.current_display_posture_valid(),
            \"fresh geometric posture check accepts a collapsed current body\");
        require(!rl::stage_display_sample_eligible(
                sim::CourseStage::balance, assisted_stance),
            \"collapsed current frame is still published as a valid sample\");
""",
        "fresh collapsed-frame regression assertion",
    )
    tests_path.write_text(tests, encoding="utf-8")

    obsolete_script = Path("tools/apply_v072_sim_regression.py")
    if obsolete_script.exists():
        obsolete_script.unlink()
    shutil.rmtree("tools/__pycache__", ignore_errors=True)

    print("Applied idempotent v0.7.2 materialized-source corrections")


if __name__ == "__main__":
    main()
