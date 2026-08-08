#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tools" / "apply_v0724_structural_metrics_icon.py"


def replace_between_fixed(text: str, start: str, end: str,
    replacement: str, label: str) -> str:
    first = text.find(start)
    if first < 0:
        raise RuntimeError(f"{label}: start marker not found")
    last = text.find(end, first)
    if last < 0:
        raise RuntimeError(f"{label}: end marker not found")
    if replacement.endswith(end):
        replacement = replacement[:-len(end)]
    return text[:first] + replacement + text[last:]


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


source_text = SOURCE.read_text(encoding="utf-8")
cut = source_text.find("\ndef patch_docs()")
if cut < 0:
    raise RuntimeError("Broken migration prefix marker was not found")
namespace: dict[str, object] = {
    "__name__": "runner_v0724_migration_prefix",
    "__file__": str(SOURCE),
}
exec(compile(source_text[:cut], str(SOURCE), "exec"), namespace)
namespace["replace_between"] = replace_between_fixed

for function_name in (
    "patch_simulation_header",
    "patch_simulation_source",
    "patch_ppo_header",
    "patch_ppo_trainer",
    "patch_autonomy_curriculum",
    "patch_training_explainer",
    "patch_app",
    "patch_existing_tests",
    "patch_cmake",
):
    function = namespace.get(function_name)
    if not callable(function):
        raise RuntimeError(f"Migration function missing: {function_name}")
    function()

simulation_path = ROOT / "src" / "simulation.cpp"
simulation = simulation_path.read_text(encoding="utf-8")
simulation = simulation.replace(
    "constexpr int projection_passes = 12;",
    "constexpr int projection_passes = 18;",
)
old_projection = '''        project_structure_rigid(dt);
        const float structural_error = maximum_bone_length_error_ratio();
        if (elapsed_seconds_ >= 0.50f
            && (!std::isfinite(structural_error) || structural_error > 0.025f))
            invalidate(InvalidMotion::structural_compression);
        apply_support_pressure(dt);
'''
new_projection = '''        apply_support_pressure(dt);
        project_structure_rigid(dt);
        const float structural_error = maximum_bone_length_error_ratio();
        if (elapsed_seconds_ >= 0.50f
            && (!std::isfinite(structural_error) || structural_error > 0.025f))
            invalidate(InvalidMotion::structural_compression);
'''
simulation = replace_once(
    simulation, old_projection, new_projection,
    "post-pressure rigid projection",
)
simulation_path.write_text(simulation, encoding="utf-8", newline="\n")

cmake_path = ROOT / "CMakeLists.txt"
cmake = cmake_path.read_text(encoding="utf-8")
cmake = replace_once(
    cmake,
    '''    target_compile_definitions(RunnerV0724StructuralMetricsIconTests PRIVATE
        RUNNER_GENERATED_ASSET_DIRECTORY="${RUNNER_GENERATED_ASSET_DIR}")
''',
    '''    target_compile_definitions(RunnerV0724StructuralMetricsIconTests PRIVATE
        RUNNER_GENERATED_ASSET_DIRECTORY="${RUNNER_GENERATED_ASSET_DIR}"
        RUNNER_SOURCE_ICON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/runner_icon_source.rgba.zlib.b64")
''',
    "source icon test definition",
)
cmake_path.write_text(cmake, encoding="utf-8", newline="\n")

readme_path = ROOT / "README.md"
readme = readme_path.read_text(encoding="utf-8")
readme = readme.replace(
    "Runner 0.7.23 is a combined",
    "Runner 0.7.24 is a combined",
    1,
)
readme = replace_once(
    readme,
    "- [`docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md`](docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md) documents the true rounded-outline and center-preservation contract.\n",
    "- [`docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md`](docs/RUNNER_V0723_GRAY_FRAME_HOTFIX.md) documents the true rounded-outline and center-preservation contract.\n"
    "- [`docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md`](docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md) documents rigid bones, stage-qualified totals, mastery-aware completion, and the exact screenshot icon source.\n",
    "README v0.7.24 document link",
)
readme = replace_once(
    readme,
    "## v0.7.23 true rounded-outline rendering hotfix\n",
    '''## v0.7.24 structural integrity and truthful telemetry

- Uses the exact selected gameplay screenshot crop as the canonical application icon source.
- Treats every load-bearing distance constraint as a rigid fixed-length bone.
- Performs a final post-contact structural projection and rejects excessive residual bone-length error.
- Restricts automatic refinement to motor strength and joint range; anatomy and stiffness remain fixed.
- Counts completed rollouts as passed only when they satisfy the current stage checks.
- Separates training work, repeat tests, and mastery passes so zero mastery cannot show 100% completion.
- Renames high-volume totals to simulated runs, passed/failed stage checks, and features cleared.
- Uses smaller procedural joints and limbs by default; optional overlay art remains explicitly opt-in.
- Isolates v0.7.24 autosaves and training semantics from older compressible-rig state.

## v0.7.23 true rounded-outline rendering hotfix
''',
    "README v0.7.24 section",
)
readme = readme.replace(
    "- Automatic training tunes motor strength, joint range, and stiffness without changing the character's anatomy.\n",
    "- Automatic training tunes motor strength and joint range without changing anatomy, rest lengths, or structural stiffness.\n",
)
readme_path.write_text(readme, encoding="utf-8", newline="\n")

changelog_path = ROOT / "CHANGELOG.md"
changelog = changelog_path.read_text(encoding="utf-8")
if not changelog.startswith("## 0.7.24\n"):
    changelog = '''## 0.7.24

- Replaced the synthetic application icon with generated assets derived from the exact committed gameplay screenshot crop.
- Normalized all structural bones to fixed length and added a final post-contact rigid projection.
- Added explicit structural-compression rejection and removed automatic stiffness mutation.
- Changed rollout totals to stage-qualified pass/fail accounting.
- Split lesson completion into training work and mastery evidence so zero mastery cannot report 100%.
- Renamed totals to simulated runs, passed/failed stage checks, and features cleared.
- Reduced default procedural joint/limb bulk and kept optional overlay art disabled by default.
- Bumped training semantics and isolated v0.7.24 autosaves.

''' + changelog
changelog_path.write_text(changelog, encoding="utf-8", newline="\n")

print("Runner v0.7.24 direct source migration applied")
