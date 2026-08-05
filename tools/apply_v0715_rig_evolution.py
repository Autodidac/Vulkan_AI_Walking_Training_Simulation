from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def patch_curriculum() -> None:
    path = ROOT / "src/autonomy_curriculum.cpp"
    text = path.read_text(encoding="utf-8")
    old = '''                const std::size_t bone_index = static_cast<std::size_t>(
                    generation % candidate.bones.size());
                const sim::DistanceConstraint original = candidate.bones[bone_index];
'''
    new = '''                std::vector<std::size_t> splittable{};
                splittable.reserve(candidate.bones.size());
                for (std::size_t index = 0; index < candidate.bones.size(); ++index)
                {
                    const sim::DistanceConstraint& bone = candidate.bones[index];
                    const bool semantic_contact_edge = candidate.is_support_seed(bone.a)
                        || candidate.is_support_seed(bone.b);
                    if (bone.stiffness < 0.20f || semantic_contact_edge)
                        continue;
                    splittable.push_back(index);
                }
                if (splittable.empty())
                    break;
                const std::size_t bone_index = splittable[static_cast<std::size_t>(
                    generation % splittable.size())];
                const sim::DistanceConstraint original = candidate.bones[bone_index];
'''
    text = replace_once(text, old, new, "protected split-bone candidate selection")
    path.write_text(text, encoding="utf-8")


def patch_tests() -> None:
    path = ROOT / "tests/core_tests.cpp"
    text = path.read_text(encoding="utf-8")
    old = '''        require(mutation.blueprint.valid(),
            "rig evolution published a structurally invalid candidate");
        topology_mutation_seen = topology_mutation_seen || mutation.topology_changed;
        parametric_mutation_seen = parametric_mutation_seen || !mutation.topology_changed;
'''
    new = '''        require(mutation.blueprint.valid(),
            "rig evolution published a structurally invalid candidate");
        const auto articulated_foot_intact = [](const sim::CreatureBlueprint& rig,
            bool left)
        {
            const auto& support_nodes = left
                ? rig.additional_left_contact_nodes
                : rig.additional_right_contact_nodes;
            if (support_nodes.size() < 2u)
                return true;
            const std::uint16_t ball = support_nodes[0];
            const std::uint16_t toe = support_nodes[1];
            return std::ranges::any_of(rig.bones,
                [ball, toe](const sim::DistanceConstraint& bone)
                {
                    return (bone.a == ball && bone.b == toe)
                        || (bone.a == toe && bone.b == ball);
                });
        };
        require(mutation.blueprint.support_seed_count() >= scaffold.support_seed_count(),
            "topology evolution removed a semantic foot contact");
        require(articulated_foot_intact(mutation.blueprint, true)
                && articulated_foot_intact(mutation.blueprint, false),
            "topology evolution split or detached an articulated toe edge");
        topology_mutation_seen = topology_mutation_seen || mutation.topology_changed;
        parametric_mutation_seen = parametric_mutation_seen || !mutation.topology_changed;
'''
    text = replace_once(text, old, new, "topology foot-preservation tests")
    path.write_text(text, encoding="utf-8")


def patch_missioncache() -> None:
    path = ROOT / "missioncache.md"
    text = path.read_text(encoding="utf-8")
    anchor = "- reject disconnected, cyclically invalid, unsupported, nonfinite, or semantically ambiguous candidates.\n"
    addition = anchor + "- protect semantic heel/ball/toe edges and weak visual braces from destructive bone splitting.\n"
    text = replace_once(text, anchor, addition, "foot-safe topology acceptance criterion")
    path.write_text(text, encoding="utf-8")


def main() -> None:
    patch_curriculum()
    patch_tests()
    patch_missioncache()


if __name__ == "__main__":
    main()
