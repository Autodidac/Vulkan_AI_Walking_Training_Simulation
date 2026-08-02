from pathlib import Path

root = Path(__file__).resolve().parents[1]

header_path = root / "src/simulation.hpp"
header = header_path.read_text(encoding="utf-8")
old_decl = "        void stabilize_passive_appendages() noexcept;\n        void solve_motor"
new_decl = "        void stabilize_passive_appendages() noexcept;\n        void stabilize_balance_posture() noexcept;\n        void solve_motor"
if new_decl not in header:
    if old_decl not in header:
        raise RuntimeError("balance posture declaration point missing")
    header_path.write_text(header.replace(old_decl, new_decl, 1), encoding="utf-8", newline="\n")

source_path = root / "src/simulation.cpp"
source = source_path.read_text(encoding="utf-8")
marker = "    void Environment::solve_motor"
function = '''    void Environment::stabilize_balance_posture() noexcept
    {
        if (course_stage_ != CourseStage::balance
            || !valid_node(blueprint_.root_node)
            || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node))
            return;

        Vec2 rest_support{};
        Vec2 current_support{};
        std::size_t support_count = 0;
        auto accumulate = [&](std::size_t index)
        {
            if (index >= blueprint_.nodes.size() || index >= particles_.size())
                return;
            rest_support += blueprint_.nodes[index];
            current_support += particles_[index].position;
            ++support_count;
        };
        accumulate(blueprint_.left_contact_node);
        accumulate(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            accumulate(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            accumulate(node);
        if (support_count == 0u
            || (!contact_supported(blueprint_.left_contact_node)
                && !contact_supported(blueprint_.right_contact_node)))
            return;

        rest_support /= static_cast<float>(support_count);
        current_support /= static_cast<float>(support_count);
        auto guide = [&](std::uint16_t node, float strength, float maximum_step)
        {
            const Vec2 target = current_support + (blueprint_.nodes[node] - rest_support);
            Vec2 correction = target - particles_[node].position;
            const float magnitude = length(correction);
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const Vec2 applied = correction * strength;
            particles_[node].position += applied;
            particles_[node].previous += applied * 0.88f;
        };

        // The feet remain the authority. This only keeps the calibrated body
        // stack above their live support center while PPO learns a real stance.
        guide(blueprint_.root_node, 0.16f, 0.035f);
        guide(blueprint_.torso_node, 0.12f, 0.030f);
        guide(blueprint_.head_node, 0.08f, 0.025f);
    }

'''
if "void Environment::stabilize_balance_posture() noexcept" not in source:
    if marker not in source:
        raise RuntimeError("balance posture implementation point missing")
    source = source.replace(marker, function + marker, 1)

old_call = "                solve_motor(blueprint_.motors[index], applied_actions[index]);\n            stabilize_passive_appendages();\n            solve_ground(dt);"
new_call = "                solve_motor(blueprint_.motors[index], applied_actions[index]);\n            stabilize_balance_posture();\n            stabilize_passive_appendages();\n            solve_ground(dt);"
if new_call not in source:
    if old_call not in source:
        raise RuntimeError("balance posture solver call point missing")
    source = source.replace(old_call, new_call, 1)

source_path.write_text(source, encoding="utf-8", newline="\n")
print("materialized feet-anchored balance posture tutor")
