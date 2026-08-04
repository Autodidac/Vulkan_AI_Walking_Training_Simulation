from pathlib import Path

path = Path(__file__).with_name("apply_v0712_round11.py")
text = path.read_text(encoding="utf-8")
old = '''    for (const sim::CreatureBlueprint rig : {
            sim::CreatureBlueprint::chicken(),
            sim::CreatureBlueprint::biped(),
            sim::CreatureBlueprint::humanoid() })
    {
        require(rig.support_seed_count() == 6u'''
new = '''    const std::array articulated_rigs{
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid()
    };
    for (const sim::CreatureBlueprint& rig : articulated_rigs)
    {
        require(rig.support_seed_count() == 6u'''
if text.count(old) != 1:
    raise RuntimeError(f"expected one copied articulated-rig loop, found {text.count(old)}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
Path(__file__).unlink()
