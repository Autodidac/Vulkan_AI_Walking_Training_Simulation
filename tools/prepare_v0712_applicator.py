from pathlib import Path

TOOLS = Path(__file__).resolve().parent

# Normalize the applicator to the current v0.7.11 curriculum source before materialization.
path = TOOLS / "apply_v0712_runtime_fix.py"
text = path.read_text(encoding="utf-8")
old = '''    old = """        case sim::CourseStage::duck_press:
            return metrics.evaluation_valid
                && metrics.evaluation_invalid_runs == 0u
                && metrics.evaluation_duck_recoveries >= 2.0f
                && metrics.evaluation_duck_seconds >= 1.25f
                && metrics.evaluation_longest_stance >= 2.5f
                && metrics.evaluation_survival >= 9.0f
                && metrics.evaluation_max_joint_speed <= 10.0f;"""
'''
new = '''    old = """        case sim::CourseStage::duck_press:
            return metrics.evaluation_duck_recoveries >= 2.0f
                && metrics.evaluation_duck_seconds >= 1.25f
                && metrics.evaluation_longest_stance >= 2.5f
                && metrics.evaluation_survival >= 9.0f
                && metrics.evaluation_max_joint_speed <= 10.0f;"""
'''
if text.count(old) != 1:
    raise RuntimeError(f"expected one obsolete crouch applicator block, found {text.count(old)}")
text = text.replace(old, new, 1)

# The current acceptance struct stores worst_spin before shortest_stance.
round4 = TOOLS / "apply_v0712_round4.py"
round4_text = round4.read_text(encoding="utf-8")
round4_text = round4_text.replace(
    '"            float worst_spin{};\\n        };",',
    '"            float worst_spin{};\\n"\n'
    '        "            float shortest_stance{ std::numeric_limits<float>::max() };\\n"\n'
    '        "        };",',
    1,
)
round4_text = round4_text.replace(
    '        "            float lowest_upright{ 1.0f };\\n"',
    '        "            float shortest_stance{ std::numeric_limits<float>::max() };\\n"\n'
    '        "            float lowest_upright{ 1.0f };\\n"',
    1,
)
round4_text = round4_text.replace(
    '''    old = ''' + "'''" + '''                result.shortest_stance = std::min(result.shortest_stance,
                    environment.longest_stable_stance_seconds());
                result.worst_spin = std::max(result.worst_spin,
                    environment.uncontrolled_spin_turns());''' + "'''" + '''
''',
    '''    old = ''' + "'''" + '''                result.worst_spin = std::max(result.worst_spin,
                    environment.uncontrolled_spin_turns());
                result.shortest_stance = std::min(result.shortest_stance,
                    environment.longest_stable_stance_seconds());''' + "'''" + '''
''',
    1,
)
round4.write_text(round4_text, encoding="utf-8")

# Static crouch and crouch-walk both call the compact teacher; only the first
# occurrence belongs to the static press correction.
round5 = TOOLS / "apply_v0712_round5.py"
round5_text = round5.read_text(encoding="utf-8")
old_round5 = '''    text = replace_once(text,
        "            : compact_support_teacher_action(environment, pressure);",
        "            : compact_support_teacher_action(environment, pressure * 0.35f);",
        "bounded compact crouch teacher")'''
new_round5 = '''    text = text.replace(
        "            : compact_support_teacher_action(environment, pressure);",
        "            : compact_support_teacher_action(environment, pressure * 0.35f);",
        1)'''
if round5_text.count(old_round5) != 1:
    raise RuntimeError(
        f"expected one round-five compact teacher edit, found {round5_text.count(old_round5)}")
round5.write_text(round5_text.replace(old_round5, new_round5, 1), encoding="utf-8")

# Keep the articulated-foot test data in one array and iterate by reference so
# GCC warnings-as-errors does not reject a copied heavyweight blueprint.
round11 = TOOLS / "apply_v0712_round11.py"
round11_text = round11.read_text(encoding="utf-8")
old_round11 = '''    for (const sim::CreatureBlueprint rig : {
            sim::CreatureBlueprint::chicken(),
            sim::CreatureBlueprint::biped(),
            sim::CreatureBlueprint::humanoid() })
    {
        require(rig.support_seed_count() == 6u'''
new_round11 = '''    const std::array articulated_rigs{
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid()
    };
    for (const sim::CreatureBlueprint& rig : articulated_rigs)
    {
        require(rig.support_seed_count() == 6u'''
if round11_text.count(old_round11) != 1:
    raise RuntimeError(
        f"expected one copied articulated-rig loop, found {round11_text.count(old_round11)}")
round11.write_text(round11_text.replace(old_round11, new_round11, 1), encoding="utf-8")

old_main = '''    patch_acceptance()
    patch_docs()
    trigger = ROOT / "WORK_v0712.tmp"'''
new_main = '''    patch_acceptance()
    patch_docs()
    for script_name in (
        "apply_v0712_round2.py",
        "apply_v0712_round3.py",
        "apply_v0712_round4.py",
        "apply_v0712_round5.py",
        "apply_v0712_round6.py",
        "apply_v0712_round7.py",
        "apply_v0712_round8.py",
        "apply_v0712_round9.py",
        "apply_v0712_round10.py",
        "apply_v0712_round11.py",
        "apply_v0712_round12.py",
        "apply_v0712_round13.py",
        "apply_v0712_round14.py",
    ):
        script = ROOT / "tools" / script_name
        namespace = {
            "__name__": script_name.removesuffix(".py"),
            "__file__": str(script),
        }
        exec(compile(script.read_text(encoding="utf-8"), str(script), "exec"), namespace)
        namespace["main"]()
    trigger = ROOT / "WORK_v0712.tmp"'''
if text.count(old_main) != 1:
    raise RuntimeError(f"expected one applicator main tail, found {text.count(old_main)}")
path.write_text(text.replace(old_main, new_main, 1), encoding="utf-8")
Path(__file__).unlink()
