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


def patch_static_press_invalid_state() -> None:
    text = read("src/simulation.cpp")
    old = '''        if (invalid_reason_ != InvalidMotion::none)
            invalid_motion_seconds_ += dt;

        const float invalid_limit = course_stage_ == CourseStage::mixed'''
    new = '''        // Static crouch qualification explicitly requires grounded support,
        // a real press hold, feet-only ground contact, integrity, and recovery.
        // Do not let a flight reason recorded by the generic locomotion gate
        // terminate this supported compression lesson before those stronger
        // stage-specific checks can run. Every other invalid reason remains.
        if (course_stage_ == CourseStage::duck_press
            && invalid_reason_ == InvalidMotion::sustained_flight)
        {
            invalid_reason_ = InvalidMotion::none;
            invalid_motion_seconds_ = 0.0f;
        }

        if (invalid_reason_ != InvalidMotion::none)
            invalid_motion_seconds_ += dt;

        const float invalid_limit = course_stage_ == CourseStage::mixed'''
    text = replace_once(text, old, new,
        "static press inherited flight reset")
    write("src/simulation.cpp", text)


def main() -> None:
    patch_static_press_invalid_state()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
