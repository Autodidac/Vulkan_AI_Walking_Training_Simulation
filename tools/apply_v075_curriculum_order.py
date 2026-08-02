from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def save(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace(path: str, old: str, new: str) -> None:
    text = load(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"missing target in {path}: {old[:140]!r}")
    save(path, text.replace(old, new, 1))


replace(
    "src/simulation.hpp",
    '''    enum class CourseStage : std::uint8_t
    {
        balance,
        duck_press,
        ramps,
        uneven,
        hurdles,
        duck_bars,
        moving_hazards
    };''',
    '''    enum class CourseStage : std::uint8_t
    {
        balance,
        uneven,
        duck_press,
        ramps,
        hurdles,
        duck_bars,
        moving_hazards
    };''',
)

replace(
    "src/simulation.hpp",
    '''        case CourseStage::balance: return "1. STAND";
        case CourseStage::duck_press: return "2. CROUCH WALK / UNEVEN AVOID";
        case CourseStage::ramps: return "3. JUMP / LAND";
        case CourseStage::uneven: return "4. WALK / RUN";
        case CourseStage::hurdles: return "5. MOVING LOW BAR / HURDLE";
        case CourseStage::duck_bars: return "6. CONTROLLED FLIPS";
        case CourseStage::moving_hazards: return "7. MIXED GOAL COURSE";''',
    '''        case CourseStage::balance: return "1. STAND";
        case CourseStage::uneven: return "2. WALK / RUN";
        case CourseStage::duck_press: return "3. CROUCH WALK / UNEVEN AVOID";
        case CourseStage::ramps: return "4. JUMP / LAND";
        case CourseStage::hurdles: return "5. MOVING LOW BAR / HURDLE";
        case CourseStage::duck_bars: return "6. CONTROLLED FLIPS";
        case CourseStage::moving_hazards: return "7. MIXED GOAL COURSE";''',
)

replace(
    "tests/core_tests.cpp",
    '''    require(sim::course_stage_name(sim::CourseStage::balance) == "1. STAND"
        && sim::course_stage_name(sim::CourseStage::duck_press) == "2. CROUCH WALK / UNEVEN AVOID"
        && sim::course_stage_name(sim::CourseStage::ramps) == "3. JUMP / LAND"
        && sim::course_stage_name(sim::CourseStage::uneven) == "4. WALK / RUN"
        && sim::course_stage_name(sim::CourseStage::hurdles) == "5. MOVING LOW BAR / HURDLE"
        && sim::course_stage_name(sim::CourseStage::duck_bars) == "6. CONTROLLED FLIPS"
        && sim::course_stage_name(sim::CourseStage::moving_hazards) == "7. MIXED GOAL COURSE",
        "skill curriculum is not ordered by prerequisite");''',
    '''    require(sim::course_stage_name(sim::CourseStage::balance) == "1. STAND"
        && sim::course_stage_name(sim::CourseStage::uneven) == "2. WALK / RUN"
        && sim::course_stage_name(sim::CourseStage::duck_press) == "3. CROUCH WALK / UNEVEN AVOID"
        && sim::course_stage_name(sim::CourseStage::ramps) == "4. JUMP / LAND"
        && sim::course_stage_name(sim::CourseStage::hurdles) == "5. MOVING LOW BAR / HURDLE"
        && sim::course_stage_name(sim::CourseStage::duck_bars) == "6. CONTROLLED FLIPS"
        && sim::course_stage_name(sim::CourseStage::moving_hazards) == "7. MIXED GOAL COURSE"
        && static_cast<std::uint8_t>(sim::CourseStage::balance)
            < static_cast<std::uint8_t>(sim::CourseStage::uneven)
        && static_cast<std::uint8_t>(sim::CourseStage::uneven)
            < static_cast<std::uint8_t>(sim::CourseStage::duck_press),
        "walking and running do not precede crouch walking in the curriculum");''',
)

# Reordering serialized enum values must invalidate any unreleased duck-first state.
replace(
    "src/ppo.hpp",
    "inline constexpr std::uint32_t training_semantics_version = 0x0007'0500u;",
    "inline constexpr std::uint32_t training_semantics_version = 0x0007'0501u;",
)
replace("src/autonomy_persistence.cpp", 'output << "RUNAUTONOMY 8\\n";',
        'output << "RUNAUTONOMY 9\\n";')
replace("src/autonomy_persistence.cpp", 'magic != "RUNAUTONOMY" || version != 8',
        'magic != "RUNAUTONOMY" || version != 9')

mission = load("missioncache.md")
entry = '''

### WALK-CURRICULUM-073 — Walking and running before crouch walking
**Status:** IN PROGRESS

The prerequisite order is now stand, then ordinary walking/running, then foot-only crouch walking and low-obstacle avoidance. A rig such as the chicken may not use successful static ducking to skip gait mastery. The learned walking controller carries into the crouch lesson, where it is extended rather than replaced by shoulder-folding behavior. The reordered stage encoding invalidates unreleased duck-first checkpoints and autonomy state.
'''
if "### WALK-CURRICULUM-073" not in mission:
    mission += entry
save("missioncache.md", mission)

notes = load("RELEASE_NOTES_v0.7.5.md")
line = "- Reorders the curriculum to stand, walk/run, then crouch-walk and duck obstacles, so the chicken and other rigs must learn ordinary gait before combining gait with ducking.\n"
if line not in notes:
    notes = notes.replace("# Runner v0.7.5\n\n", "# Runner v0.7.5\n\n" + line, 1)
save("RELEASE_NOTES_v0.7.5.md", notes)

Path(__file__).unlink()
print("reordered curriculum: stand -> walk/run -> crouch walk")
