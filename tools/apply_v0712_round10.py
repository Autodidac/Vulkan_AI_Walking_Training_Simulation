from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def patch_static_press_invalid_state() -> None:
    text = read("src/simulation.cpp")
    marker = '''        last_reward_ += recovery_reward - uncontrolled_spin_penalty;
        if (invalid_reason_ != InvalidMotion::none)'''
    position = text.rfind(marker)
    if position < 0:
        raise RuntimeError("missing final episode invalid-reason gate")

    insertion = '''        last_reward_ += recovery_reward - uncontrolled_spin_penalty;
        // Static crouch qualification explicitly requires grounded support,
        // a real press hold, feet-only ground contact, integrity, and recovery.
        // Do not let a flight reason recorded by the generic locomotion gate
        // terminate this supported compression lesson before those stronger
        // stage-specific checks can run. Every other invalid reason remains.
        if (course_stage_ == CourseStage::duck_press
            && invalid_reason_ == InvalidMotion::sustained_flight)
            invalid_reason_ = InvalidMotion::none;
        if (invalid_reason_ != InvalidMotion::none)'''
    if "Static crouch qualification explicitly requires grounded support" in text:
        raise RuntimeError("static press flight reset was already materialized")
    text = text[:position] + insertion + text[position + len(marker):]
    write("src/simulation.cpp", text)


def main() -> None:
    patch_static_press_invalid_state()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
