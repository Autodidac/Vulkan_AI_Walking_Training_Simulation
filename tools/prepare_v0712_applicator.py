from pathlib import Path

# Normalize the applicator to the current v0.7.11 curriculum source before materialization.
path = Path(__file__).with_name("apply_v0712_runtime_fix.py")
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
old_main = '''    patch_acceptance()
    patch_docs()
    trigger = ROOT / "WORK_v0712.tmp"'''
new_main = '''    patch_acceptance()
    patch_docs()
    for script_name in (
        "apply_v0712_round2.py",
        "apply_v0712_round3.py",
        "apply_v0712_round4.py",
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
