from pathlib import Path

path = Path(__file__).with_name("apply_joint_lab_upgrade.py")
text = path.read_text(encoding="utf-8")
old = '''simulation = replace_section(
    simulation,
    "    std::array<float, observation_count> Environment::observation() const noexcept",
    "}\\n",
    observation_block,
    "generic observations") + "}\\n"
'''
new = '''observation_start = simulation.index(
    "    std::array<float, observation_count> Environment::observation() const noexcept")
namespace_end = simulation.rfind("\\n}")
if namespace_end <= observation_start:
    raise RuntimeError("generic observations: namespace end not found")
simulation = simulation[:observation_start] + observation_block + simulation[namespace_end:]
'''
if text.count(old) != 1:
    raise RuntimeError(f"expected one observation replacement, found {text.count(old)}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
print("Joint Lab patcher corrected.")
