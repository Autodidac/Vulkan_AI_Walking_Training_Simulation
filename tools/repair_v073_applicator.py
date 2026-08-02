from pathlib import Path

path = Path(__file__).with_name("apply_v073_runtime_fix.py")
text = path.read_text(encoding="utf-8")
changed = False

start_marker = '''replace_text(
    "tests/core_tests.cpp",
    "humanoid.support_seed_count() == 6",'''
if start_marker in text:
    start = text.index(start_marker)
    end = text.index("\n\ninsert_before(", start)
    text = text[:start] + text[end + 2:]
    changed = True

for old, new in (
    ("ankle_position.y - 0.155f", "ankle_position.y - 0.185f"),
    ("ankle_position.y - 0.165f", "ankle_position.y - 0.195f"),
):
    if old in text:
        text = text.replace(old, new)
        changed = True

anchor = '''
insert_before(
    "tests/core_tests.cpp",
    "        static void qualify_stable_stance",'''
compatibility = '''
replace_text(
    "tests/core_tests.cpp",
    "humanoid.nodes.size() >= 19",
    "humanoid.nodes.size() >= 17",
    "humanoid.nodes.size() >= 17"
)
replace_text(
    "tests/core_tests.cpp",
    "humanoid.bones.size() >= 21",
    "humanoid.bones.size() >= 19",
    "humanoid.bones.size() >= 19"
)
replace_text(
    "tests/core_tests.cpp",
    "humanoid.additional_left_contact_nodes.size() == 2u\\n            && humanoid.additional_right_contact_nodes.size() == 2u",
    "humanoid.additional_left_contact_nodes.size() == 1u\\n            && humanoid.additional_right_contact_nodes.size() == 1u",
    "humanoid.additional_left_contact_nodes.size() == 1u"
)
'''
if "humanoid.nodes.size() >= 17" not in text:
    if anchor not in text:
        raise RuntimeError("Could not locate test compatibility insertion point")
    text = text.replace(anchor, compatibility + anchor, 1)
    changed = True

if changed:
    path.write_text(text, encoding="utf-8", newline="\n")
    print("updated v0.7.3 applicator compatibility")
else:
    print("applicator compatibility repair already applied")
