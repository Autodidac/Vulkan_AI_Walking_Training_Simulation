from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}: {old[:160]!r}")
    write(path, text.replace(old, new, 1))


def set_mission_status(text: str, mission_id: str, status: str) -> str:
    heading = f"## {mission_id}"
    start = text.find(heading)
    if start < 0:
        raise RuntimeError(f"mission not found: {mission_id}")
    marker = "**Status:**"
    status_start = text.find(marker, start)
    next_heading = text.find("\n## ", start + len(heading))
    if status_start < 0 or (next_heading >= 0 and status_start > next_heading):
        raise RuntimeError(f"status not found for mission: {mission_id}")
    line_end = text.find("\n", status_start)
    return text[:status_start] + f"**Status:** {status}" + text[line_end:]


replace_once(
    "CMakeLists.txt",
    "project(EpochRunner VERSION 0.6.3 LANGUAGES CXX)",
    "project(EpochRunner VERSION 0.6.5 LANGUAGES CXX)")

readme = read("README.md")
old_intro = (
    "EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, "
    "vcpkg manifest mode, and a compact PPO controller. Version 0.6.3 retargets the "
    "curriculum toward a grounded sand-simulation enemy: large readable telemetry, long "
    "flat patrol zones, separated sand mounds, flat early obstacle pads, and hard rejection "
    "of head, tail, or body rolling."
)
new_intro = (
    "EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, "
    "vcpkg manifest mode, and a compact PPO controller. Version 0.6.5 completes the guided "
    "sand-simulation enemy pass: true four-leg and six-leg support semantics, longer safe "
    "training runway, startup-only rolling grace, strict mature rolling rejection, zero-motion "
    "episode reset, automatic best-result self-imitation, and a real worker-rollout picture-in-picture."
)
if readme.count(old_intro) != 1:
    raise RuntimeError("README release introduction did not match")
readme = readme.replace(old_intro, new_intro, 1)

insert_marker = "## Sand-sim enemy locomotion hotfix\n"
insert_at = readme.find(insert_marker)
if insert_at < 0:
    raise RuntimeError("README insertion marker missing")
release_section = """## Guided multi-leg training release

Version 0.6.5 replaces the old two-contact quadruped approximation with a true four-leg body and gives the four-leg crawler and six-leg hexapod explicit multi-foot support groups. Near and far legs are staggered for readable side-view alignment, while every semantic foot receives the same flat-sole traction and anti-pivot-rolling treatment.

The first obstacle is now held beyond a forty-metre safe runway so a fresh policy can establish balance and gait before debris arrives. Rolling gates allow a brief startup settling window, then become strict. Episodes that produce no translation, no new gait step, and no useful foot lift reset promptly instead of consuming most of a rollout.

The trainer automatically records clean frames from its best valid stepped result and uses them as a small decaying actor-only imitation prior. Invalid body-contact and orange-foot rolling frames are excluded. The live view also includes a compact upper-right picture-in-picture showing an actual exploratory worker rollout rather than a duplicate deterministic replay.

The former foot-before-knee restriction is no longer a rigid ordering rule. Natural bent-knee lead and useful raised-foot clearance are allowed; only an obvious low-foot body-first shove receives a mild shaping penalty.

"""
readme = readme[:insert_at] + release_section + readme[insert_at:]
readme = readme.replace(
    "Each lesson starts with three clear markers of safe runway",
    "Each lesson starts with five clear markers forming a forty-metre safe runway",
    1)
readme = readme.replace(
    "and a knee crossing a rock or hurdle before its corresponding foot receives an explicit penalty and visible fault count.",
    "while only an egregious low-foot body-first obstacle shove receives a mild shaping penalty and visible fault count.",
    1)
readme = readme.replace(
    "the original quadruped, a four-legged crawler, and a six-legged hexapod.",
    "a true four-legged quadruped, a separate four-legged crawler, and a six-legged hexapod.",
    1)
write("README.md", readme)

missions = read("MISSIONS.md")
for mission in (
    "WALK-PHYS-001",
    "WALK-UI-002",
    "WALK-GAIT-002",
    "WALK-SAND-001",
    "WALK-ROLL-003",
    "WALK-HAZARD-003",
    "WALK-LOCO-004",
    "WALK-IDLE-005",
    "WALK-GUIDE-006",
):
    missions = set_mission_status(missions, mission, "VERIFIED")

# These two are implemented and packaged, but their final acceptance is inherently visual.
missions = set_mission_status(missions, "WALK-UI-003", "IMPLEMENTED — USER VISUAL REVIEW")
missions = set_mission_status(missions, "WALK-PIP-007", "IMPLEMENTED — USER VISUAL REVIEW")

warning_start = missions.find("## Current warning")
if warning_start < 0:
    raise RuntimeError("Current warning section missing")
missions = missions[:warning_start] + """## v0.6.5 release closure

All non-visual locomotion missions introduced or reopened after v0.6.3 have passing deterministic tests, full Windows SDL3/Vulkan/EpochGui build evidence, and Vulkan diagnostics. The release includes true four-leg and six-leg support semantics, flat semantic feet, mature anti-rolling gates, a longer obstacle runway, zero-motion reset, automatic best-result imitation, relaxed joint-clearance guidance, and actual training picture-in-picture publication.

`WALK-UI-003` and `WALK-PIP-007` remain explicitly marked for user visual review because compilation cannot prove readability or preferred placement. They no longer conceal unfinished implementation work and do not block the requested v0.6.5 package.

The coroutine, ownership, asynchronous persistence, and ThreadSanitizer missions remain tracked for the separate v0.7 runtime pipeline in `V070_MASTER_PLAN.md`; they were never silently deleted or misrepresented as part of this locomotion release.
"""
write("MISSIONS.md", missions)

print("Prepared EpochRunner v0.6.5 release source and closed locomotion missions")
