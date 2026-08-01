from pathlib import Path

path = Path("tools/patch_flat_foot_contacts.py")
text = path.read_text(encoding="utf-8")
needle = '''replace_once(
    "src/simulation.cpp",
    ''' + "'''" + '''    [[nodiscard]] inline float ground_velocity_retention(bool traction_contact,'''
replacement = '''replace_once(
    "src/simulation.hpp",
    ''' + "'''" + '''    [[nodiscard]] inline float ground_velocity_retention(bool traction_contact,'''
if text.count(needle) != 1:
    raise RuntimeError("Expected one ground_velocity_retention patch target")
path.write_text(text.replace(needle, replacement, 1), encoding="utf-8", newline="\n")
print("Repaired flat-foot driver target")
