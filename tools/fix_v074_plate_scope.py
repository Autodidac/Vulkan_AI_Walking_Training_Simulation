from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / 'src/simulation.cpp'
text = path.read_text(encoding='utf-8')
old = '''    void Environment::separate_support_clusters() noexcept
    {
        auto support_nodes = [&](bool left)
'''
new = '''    void Environment::separate_support_clusters() noexcept
    {
        auto rigid_contact_plate = [&](std::uint16_t primary,
            const std::vector<std::uint16_t>& additional) noexcept
        {
            return valid_node(primary) && !additional.empty()
                && std::ranges::all_of(additional,
                    [&](std::uint16_t node)
                    {
                        return valid_node(node)
                            && direct_bone(blueprint_, primary, node);
                    });
        };
        if (!rigid_contact_plate(blueprint_.left_contact_node,
                blueprint_.additional_left_contact_nodes)
            || !rigid_contact_plate(blueprint_.right_contact_node,
                blueprint_.additional_right_contact_nodes))
            return;

        auto support_nodes = [&](bool left)
'''
if new not in text:
    if old not in text:
        raise SystemExit('support separation function was not found')
    path.write_text(text.replace(old, new, 1), encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('scoped support separation to true rigid heel-toe plates')
