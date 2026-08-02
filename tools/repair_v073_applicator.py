from pathlib import Path

path = Path(__file__).with_name("apply_v073_runtime_fix.py")
text = path.read_text(encoding="utf-8")
start_marker = '''replace_text(
    "tests/core_tests.cpp",
    "humanoid.support_seed_count() == 6",'''
if start_marker in text:
    start = text.index(start_marker)
    end = text.index("\n\ninsert_before(", start)
    text = text[:start] + text[end + 2:]
    path.write_text(text, encoding="utf-8", newline="\n")
    print("removed optional legacy support-count replacement")
else:
    print("applicator compatibility repair already applied")
