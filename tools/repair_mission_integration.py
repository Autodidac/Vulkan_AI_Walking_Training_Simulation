from pathlib import Path

path = Path("tools/integrate_mission_completion.py")
text = path.read_text(encoding="utf-8")
old = '''replace_once("src/simulation.cpp",
''' + "'''" + '''                - stance_slip_penalty
                - wheel_penalty;''' + "'''" + ''',
''' + "'''" + '''                - stance_slip_penalty
                - wheel_penalty
                - hazard_stall_penalty;''' + "'''" + ''')'''
new = '''replace_once("src/simulation.cpp",
''' + "'''" + '''                - stance_slip_penalty
                - wheel_penalty
                - body_contact_penalty;''' + "'''" + ''',
''' + "'''" + '''                - stance_slip_penalty
                - wheel_penalty
                - hazard_stall_penalty
                - body_contact_penalty;''' + "'''" + ''')'''
if text.count(old) != 1:
    raise RuntimeError("Expected exactly one obsolete reward patch")
path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
print("Repaired mission integration reward patch")
