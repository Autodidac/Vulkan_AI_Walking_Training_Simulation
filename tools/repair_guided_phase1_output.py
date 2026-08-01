from pathlib import Path

path = Path("src/simulation.cpp")
text = path.read_text(encoding="utf-8")
old = '''        result.left_contact_node = 3;
        result.right_contact_node = 6;
        result.additional_left_contact_nodes = { 6, 7 };
        result.additional_right_contact_nodes = { 4, 5, 8 };
'''
new = '''        result.left_contact_node = 3;
        result.right_contact_node = 4;
        result.additional_left_contact_nodes = { 5, 7 };
        result.additional_right_contact_nodes = { 6, 8 };
'''
if text.count(old) != 1:
    raise RuntimeError("Expected generated hexapod support grouping exactly once")
path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
print("Repaired hexapod alternating support groups")
