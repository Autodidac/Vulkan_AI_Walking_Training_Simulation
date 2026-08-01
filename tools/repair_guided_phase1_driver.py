from pathlib import Path

path = Path("tools/patch_guided_training_phase1.py")
text = path.read_text(encoding="utf-8")
needle = '''    ''' + "'''" + '''            output << "S "'''
replacement = '''    r''' + "'''" + '''            output << "S "'''
if text.count(needle) != 2:
    raise RuntimeError(f"Expected two rig-semantic multiline literals, found {text.count(needle)}")
path.write_text(text.replace(needle, replacement), encoding="utf-8", newline="\n")
print("Repaired guided phase-one newline literals")
