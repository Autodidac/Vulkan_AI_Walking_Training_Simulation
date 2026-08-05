from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


tests = read("tests/deformable_terrain_tests.cpp")
old_assertion = """    constexpr float transform_source_x = sim::terrain_sample_x(\n        transform_world_x, transform_progress);\n    static_assert(std::abs(sim::terrain_world_x(transform_source_x,\n        transform_progress) - transform_world_x) < 1.0e-6f);\n"""
new_assertion = """    constexpr float transform_source_x = sim::terrain_sample_x(\n        transform_world_x, transform_progress);\n    constexpr float transform_round_trip_error =\n        sim::terrain_world_x(transform_source_x, transform_progress)\n        - transform_world_x;\n    static_assert(transform_round_trip_error > -1.0e-6f\n        && transform_round_trip_error < 1.0e-6f);\n"""
tests = replace_once(tests, old_assertion, new_assertion,
    "portable constexpr terrain transform assertion")
write("tests/deformable_terrain_tests.cpp", tests)

mission = read("missioncache.md")
anchor = """**MSVC portability finding:** exact-source Windows run `31016262550`, job `92341400943`, built `RunnerCore`, `Runner.exe`, and the other native targets but failed `RunnerDeformableTerrainTests` because MSVC 19.51 does not treat `std::abs(float)` as constexpr in the terrain transform `static_assert`. This is a test-only cross-platform defect. The correction must preserve compile-time transform verification without relying on library constexpr coverage, then rerun Linux, Windows, the complete acceptance matrix, and runtime diagnostics.\n\n"""
addition = anchor + """**MSVC portability correction:** the transform round-trip remains a compile-time assertion, but now compares the signed constexpr error directly against positive and negative epsilon bounds. This avoids implementation-dependent `std::abs` constexpr support without weakening the invariant. Full cross-platform revalidation remains required.\n\n"""
mission = replace_once(mission, anchor, addition,
    "record MSVC constexpr correction")
write("missioncache.md", mission)

changelog = read("CHANGELOG.md")
changelog_anchor = """## Runner v0.7.15 — measured static-friction contacts\n\n"""
changelog_addition = changelog_anchor + """- Kept terrain coordinate round-trip verification compile-time portable across GCC 14 and MSVC 19.51 without depending on `std::abs(float)` constexpr support.\n"""
changelog = replace_once(changelog, changelog_anchor, changelog_addition,
    "record MSVC test portability fix")
write("CHANGELOG.md", changelog)
