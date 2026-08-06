#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / "tests" / "core_tests.cpp"
text = path.read_text(encoding="utf-8")

pattern = re.compile(
    r'    require\(humanoid\.nodes\.size\(\) >= 17,\n'
    r'        "human-calibrated rig should include passive heel/toe feet and articulated arms"\);\n'
    r'    require\(std::abs\(humanoid\.nodes\[0\]\.y - 2\.8127f\) < 0\.01f,\n'
    r'        "uploaded humanoid pelvis calibration not applied"\);\n'
    r'    require\(humanoid\.bones\.size\(\) >= 19,\n'
    r'        "humanoid feet or arms are not structurally connected"\);\n'
    r'    require\(humanoid\.active_motor_count == sim::action_count,\n'
    r'        "humanoid does not expose independent shoulder and elbow motors"\);\n'
    r'    require\(humanoid\.left_contact_node != humanoid\.motors\[1\]\.c\n'
    r'            && humanoid\.right_contact_node != humanoid\.motors\[3\]\.c,\n'
    r'        "semantic feet are still the lower-leg motor endpoints"\);\n'
    r'    require\(humanoid\.additional_left_contact_nodes\.size\(\) == 2u\n'
    r'            && humanoid\.additional_right_contact_nodes\.size\(\) == 2u,\n'
    r'        "articulated foot does not include heel, ball, and toe contacts"\);\n'
    r'    require\(humanoid\.nodes\[humanoid\.motors\[1\]\.c\]\.y\n'
    r'            - humanoid\.nodes\[humanoid\.left_contact_node\]\.y >= 0\.18f\n'
    r'            && humanoid\.nodes\[humanoid\.motors\[3\]\.c\]\.y\n'
    r'                - humanoid\.nodes\[humanoid\.right_contact_node\]\.y >= 0\.18f,\n'
    r'        "passive foot adapter leaves an ankle on the contact plane"\);\n'
    r'    require\(std::ranges::none_of\(humanoid\.motors,\n'
    r'            \[&humanoid\]\(const sim::MotorConstraint& motor\)\n'
    r'            \{\n'
    r'                return motor\.c == humanoid\.left_contact_node\n'
    r'                    \|\| motor\.c == humanoid\.right_contact_node;\n'
    r'            \}\),\n'
    r'        "a policy motor still terminates directly on a semantic foot contact"\);\n',
    re.S,
)
replacement = '''    require(humanoid.nodes.size() == 13u,
        "human-calibrated rig does not retain the compact articulated body and arms");
    require(std::abs(humanoid.nodes[0].y - 2.8127f) < 0.01f,
        "uploaded humanoid pelvis calibration not applied");
    require(humanoid.bones.size() == 15u,
        "humanoid legs or articulated arms are not structurally connected");
    require(humanoid.active_motor_count == sim::action_count,
        "humanoid does not expose independent shoulder and elbow motors");
    require(humanoid.left_contact_node == humanoid.motors[1].c
            && humanoid.right_contact_node == humanoid.motors[3].c,
        "terminal lower-leg joints are not the physical support stubs");
    require(humanoid.additional_left_contact_nodes.empty()
            && humanoid.additional_right_contact_nodes.empty(),
        "terrain-hostile heel/ball/toe collision contacts remain on the humanoid");
    require(humanoid.left_contact_node < humanoid.radii.size()
            && humanoid.right_contact_node < humanoid.radii.size()
            && humanoid.radii[humanoid.left_contact_node] >= 0.104f
            && humanoid.radii[humanoid.right_contact_node] >= 0.104f
            && humanoid.radii[humanoid.left_contact_node] <= 0.1121f
            && humanoid.radii[humanoid.right_contact_node] <= 0.1121f,
        "humanoid terminal support stubs are not compact and terrain-conforming");
'''
text, count = pattern.subn(replacement, text, count=1)
if count != 1:
    raise RuntimeError(f"legacy humanoid articulated-foot block: expected one match, found {count}")
text = text.replace(
    '"passive biped heel/toe nodes never became valid support contacts"',
    '"terminal biped support stubs never became valid support contacts"',
)
path.write_text(text, encoding="utf-8", newline="\n")
