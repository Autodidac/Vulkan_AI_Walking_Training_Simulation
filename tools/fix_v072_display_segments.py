from __future__ import annotations

from pathlib import Path


def main() -> None:
    path = Path("src/simulation.cpp")
    text = path.read_text(encoding="utf-8")
    old = """        const Vec2 root = particles_[blueprint_.root_node].position;
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
"""
    new = """        const Vec2 root = particles_[blueprint_.root_node].position;
        const Vec2 torso = particles_[blueprint_.torso_node].position;
        const Vec2 head = particles_[blueprint_.head_node].position;
        const float torso_segment = length(torso - root);
        const float head_segment = length(head - torso);
        const float rest_torso_segment = length(
            blueprint_.nodes[blueprint_.torso_node]
                - blueprint_.nodes[blueprint_.root_node]);
        const float rest_head_segment = length(
            blueprint_.nodes[blueprint_.head_node]
                - blueprint_.nodes[blueprint_.torso_node]);

        // Historical lesson evidence may remain latched, but the body displayed
        // now must still have intact direct body segments. A balanced crouch
        // shortens head-to-pelvis distance without collapsing either segment.
        return torso_segment >= rest_torso_segment * 0.58f
            && head_segment >= rest_head_segment * 0.55f;
"""
    if new not in text:
        if old not in text:
            raise RuntimeError("missing head-to-pelvis display geometry")
        text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8")
    print("Applied direct-segment display posture correction")


if __name__ == "__main__":
    main()
