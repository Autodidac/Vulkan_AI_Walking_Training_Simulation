from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


def patch_static_foot_contact_plane() -> None:
    text = read("src/simulation.cpp")
    old = '''            support.position.y = ground_height_at(support.position.x) + support.radius;
            support.previous = support.position;
            support.grounded = true;'''
    new = '''            support.position.y = ground_height_at(support.position.x)
                + ground_contact_offset(true, support.radius);
            support.previous = support.position;
            support.grounded = true;'''
    text = replace_once(text, old, new,
        "static press semantic foot contact plane")
    write("src/simulation.cpp", text)


def remove_obsolete_rigid_foot_test() -> None:
    text = read("tests/core_tests.cpp")
    message = '"humanoid feet are not two rigid heel-toe contact plates"'
    message_position = text.find(message)
    if message_position < 0:
        raise RuntimeError("obsolete rigid heel-toe test message is missing")
    start = text.rfind("    require(", 0, message_position)
    finish = text.find(");\n", message_position)
    if start < 0 or finish < 0:
        raise RuntimeError("obsolete rigid heel-toe require block is malformed")
    # Round 11 already adds stronger checks for forward heel-ball-toe geometry,
    # an actual ball-to-toe hinge, no rigid heel-to-toe brace, and live toe
    # motion. The old rigid-plate assertion directly contradicts that design.
    text = text[:start] + text[finish + 3:]
    write("tests/core_tests.cpp", text)


def main() -> None:
    patch_static_foot_contact_plane()
    remove_obsolete_rigid_foot_test()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
