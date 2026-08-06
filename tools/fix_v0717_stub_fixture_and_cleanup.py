#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def cache() -> None:
    text = read("missioncache.md")
    marker = "# Runner v0.7.18 equipment, carry, and target curriculum"
    finding = '''## v0.7.17 eleventh Linux validation finding — obsolete articulated-foot fixture and branch cleanup

Clean run `31095364734` passed repository and optional-art audits, GCC 14 compilation, the focused v0.7.17 eye-test suite, all eight six-seed Stand cases, all eight four-seed static crouch/hold/recover cases, the complete 24/24 live acceptance matrix, deformable terrain, SandHybrid integration, runtime pipeline, and the warmed concurrency benchmark. The only failing assertion still expected the superseded 17-node heel/ball/toe humanoid even though v0.7.17 intentionally uses the existing terminal ankle as one compact physical support stub and renders the forward boot as optional side-view art.

The fixture must verify the 13-node/15-bone articulated humanoid, eight arm/leg motors, terminal semantic support stubs, empty extra-contact lists, bounded support radii, and retained uploaded pelvis calibration. It must not require deleted heel/ball/toe collision nodes. The publisher must also remove the superseded `agent/v0717-eye-test-batch` branch and every accidental `agent/v0717-*copy*`/finalizer branch after the release assets are re-downloaded and verified. No production threshold or runtime behavior changes. Full Linux, Windows, package, optional-art fallback, archive, publication, re-download, and branch cleanup gates remain mandatory.

'''
    if "## v0.7.17 eleventh Linux validation finding" not in text:
        if marker not in text:
            raise RuntimeError("v0.7.18 carry-forward marker missing")
        text = text.replace(marker, finding + marker, 1)
    write("missioncache.md", text)


def implement() -> None:
    tests = read("tests/core_tests.cpp")
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
    tests, count = pattern.subn(replacement, tests, count=1)
    if count != 1:
        raise RuntimeError(f"legacy humanoid articulated-foot block: expected one match, found {count}")
    tests = tests.replace(
        '"passive biped heel/toe nodes never became valid support contacts"',
        '"terminal biped support stubs never became valid support contacts"',
    )
    write("tests/core_tests.cpp", tests)

    workflow = read(".github/workflows/runner-v0717-release.yml")
    old_cleanup = '''      - name: Remove completed release branch
        shell: bash
        run: |
          encoded=${RELEASE_BRANCH//\\//%2F}
          gh api --method DELETE "repos/$GITHUB_REPOSITORY/git/refs/heads/$encoded" || true
        env:
          RELEASE_BRANCH: agent/v0717-eye-test-feet-gait-batch25
'''
    branches = [
        "agent/v0717-eye-test-batch",
        "agent/v0717-eye-test-feet-gait-batch25",
        "agent/v0717-finalize-release",
        "agent/v0717-eye-test-feet-gait-batch25-copy",
        *[f"agent/v0717-eye-test-feet-gait-batch25-copy{i}" for i in range(2, 12)],
    ]
    quoted = "\n".join(f'            "{branch}"' for branch in branches)
    new_cleanup = f'''      - name: Remove completed and obsolete v0.7.17 branches
        shell: bash
        run: |
          set -euo pipefail
          branches=(
{quoted}
          )
          for branch in "${{branches[@]}}"; do
            encoded=${{branch//\\//%2F}}
            gh api --method DELETE "repos/$GITHUB_REPOSITORY/git/refs/heads/$encoded" || true
          done
'''
    workflow = replace_once(workflow, old_cleanup, new_cleanup,
        "release branch cleanup block")
    write(".github/workflows/runner-v0717-release.yml", workflow)


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {"cache", "implement"}:
        print("usage: fix_v0717_stub_fixture_and_cleanup.py cache|implement", file=sys.stderr)
        return 2
    cache() if sys.argv[1] == "cache" else implement()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
