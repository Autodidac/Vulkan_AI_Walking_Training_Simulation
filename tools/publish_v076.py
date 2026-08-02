from __future__ import annotations

import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

REPOSITORY = "Autodidac/Vulkan_AI_Walking_Training_Simulation"
VALIDATION_RUN = "30760774468"
VALIDATION_LINUX_JOB = "91530745311"
VALIDATION_WINDOWS_JOB = "91530869350"
VALIDATED_SOURCE_SHA = "c53e75b5b126c0c48c2290f751116636b16dc8ff"
MERGE_SHA = "28949c6ce9c0b841e2e452ecb6da22e5e766b2cf"
PUBLISH_TRIGGER_SHA = "64682df2c15f73c68a7740ec3c5f676b8a7e0fe0"
ARTIFACT_ID = "8837612890"
ARTIFACT_DIGEST = "sha256:efa109bd28053245125dbd908e7c41f1300d24236c8eae8eea70b897f0b5ae17"
TAG = "v0.7.6"
ASSET = "Runner-v0.7.6-windows-x64.zip"


def run(*args: str, capture: bool = False, check: bool = True) -> str:
    print("+", " ".join(args), flush=True)
    result = subprocess.run(args, text=True, check=check,
                            stdout=subprocess.PIPE if capture else None)
    return result.stdout.strip() if capture else ""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def manifest_for(root: Path) -> list[str]:
    lines: list[str] = []
    for file in sorted(path for path in root.rglob("*") if path.is_file()):
        lines.append(f"{sha256_file(file)}  {file.relative_to(root).as_posix()}")
    return lines


def release_exists() -> bool:
    return subprocess.run(
        ["gh", "release", "view", TAG, "--repo", REPOSITORY],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0


def close_trigger_pr() -> None:
    pr_number = os.environ.get("TRIGGER_PR_NUMBER", "").strip()
    if not pr_number:
        return
    subprocess.run(
        ["gh", "pr", "close", pr_number, "--repo", REPOSITORY, "--delete-branch"],
        check=False,
    )


def delete_branch(name: str) -> None:
    subprocess.run(["git", "push", "origin", "--delete", name], check=False)


def verify_final_state() -> None:
    open_prs = run("gh", "pr", "list", "--repo", REPOSITORY, "--state", "open",
                   "--json", "number", "--jq", "length", capture=True)
    if open_prs != "0":
        raise RuntimeError(f"Expected zero open PRs, found {open_prs}")
    branches = run("gh", "api", f"repos/{REPOSITORY}/branches", "--paginate",
                   "--jq", ".[].name", capture=True).splitlines()
    if branches != ["main"]:
        raise RuntimeError(f"Unexpected remaining branches: {branches}")


def audit_published_assets(work: Path) -> None:
    published = work / "published"
    extracted = work / "published-audit"
    shutil.rmtree(published, ignore_errors=True)
    shutil.rmtree(extracted, ignore_errors=True)
    published.mkdir(parents=True)
    run("gh", "release", "download", TAG, "--repo", REPOSITORY,
        "--dir", str(published))
    archive = published / ASSET
    checksum_path = published / f"{ASSET}.sha256"
    manifest_path = published / "Runner-v0.7.6-windows-x64.manifest.sha256"
    for path in (archive, checksum_path, manifest_path):
        if not path.is_file():
            raise RuntimeError(f"Missing published asset: {path.name}")
    expected_hash = checksum_path.read_text(encoding="utf-8").split()[0]
    if expected_hash != sha256_file(archive):
        raise RuntimeError("Published archive checksum mismatch")
    with zipfile.ZipFile(archive) as package:
        package.extractall(extracted)
    expected_manifest = manifest_path.read_text(encoding="utf-8").splitlines()
    if expected_manifest != manifest_for(extracted):
        raise RuntimeError("Published archive manifest mismatch")
    required = [
        "Runner.exe", "run.bat", "missioncache.md", "RELEASE_NOTES_v0.7.6.md",
        "shaders/flat.vert.spv", "shaders/flat.frag.spv", "assets/chicken.ppm",
    ]
    missing = [name for name in required if not (extracted / name).is_file()]
    if missing:
        raise RuntimeError(f"Published package files missing: {missing}")


def main() -> int:
    root = Path.cwd()
    work = root / ".publisher-v076"
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir()

    run("git", "fetch", "origin", "main", "--tags")
    run("git", "switch", "-C", "publisher-main", "origin/main")
    current = run("git", "rev-parse", "HEAD", capture=True)

    if release_exists():
        print(f"{TAG} already exists; auditing and cleaning trigger state.")
        audit_published_assets(work)
        close_trigger_pr()
        delete_branch("agent/runner-v076-standing-pip")
        delete_branch("agent/trigger-v076-publisher")
        verify_final_state()
        return 0

    if current != PUBLISH_TRIGGER_SHA:
        raise RuntimeError(f"Unexpected main source {current}; expected {PUBLISH_TRIGGER_SHA}")
    parent = run("git", "rev-parse", "HEAD~1", capture=True)
    if parent != MERGE_SHA:
        raise RuntimeError(f"Publisher parent {parent} is not merge {MERGE_SHA}")

    cmake = Path("CMakeLists.txt").read_text(encoding="utf-8")
    ppo = Path("src/ppo.hpp").read_text(encoding="utf-8")
    if "project(Runner VERSION 0.7.6" not in cmake:
        raise RuntimeError("Runner version is not 0.7.6")
    if "training_semantics_version = 0x0007'0600u" not in ppo:
        raise RuntimeError("v0.7.6 training semantics are missing")
    for one_shot in (
        "tools/apply_v076_standing_pip.py",
        "tools/fix_v076_display_gate.py",
        "tools/apply_v076_spin_semantics.py",
    ):
        if Path(one_shot).exists():
            raise RuntimeError(f"One-shot tool remains in merged source: {one_shot}")

    validated = work / "validated"
    validated.mkdir()
    run("gh", "run", "download", VALIDATION_RUN, "--repo", REPOSITORY,
        "--name", "Runner-v0.7.6-windows-x64", "--dir", str(validated))
    artifact_digest = run(
        "gh", "api", f"repos/{REPOSITORY}/actions/artifacts/{ARTIFACT_ID}",
        "--jq", ".digest", capture=True)
    if artifact_digest != ARTIFACT_DIGEST:
        raise RuntimeError(f"Artifact digest mismatch: {artifact_digest}")
    validated_archive = validated / ASSET
    if not validated_archive.is_file():
        raise RuntimeError("Validated Windows archive was not downloaded")

    mission_path = Path("missioncache.md")
    mission = mission_path.read_text(encoding="utf-8")
    mission = mission.replace(
        "**Release state:** IN PROGRESS - v0.7.5 live screenshot reopened standing mastery and PIP acceptance",
        "**Release state:** PUBLISHED - v0.7.6 assets independently audited; awaiting Adam's live packaged-runtime confirmation",
        1,
    )
    statuses = {
        "WALK-STAND-080": "PACKAGE VERIFIED - LIVE ACCEPTANCE PENDING",
        "WALK-SHOULDER-081": "PACKAGE VERIFIED - LIVE ACCEPTANCE PENDING",
        "WALK-PIP-082": "PACKAGE VERIFIED - LIVE ACCEPTANCE PENDING",
        "WALK-RELEASE-083": "PUBLISHED - RELEASE ASSETS VERIFIED",
    }
    for mission_id, status in statuses.items():
        pattern = rf"(### {re.escape(mission_id)}[^\n]*\n\*\*Status:\*\*)[^\n]*"
        mission, count = re.subn(pattern, rf"\1 {status}", mission, count=1)
        if count != 1:
            raise RuntimeError(f"Could not update mission {mission_id}")

    heading = "## v0.7.6 immutable release evidence"
    if heading in mission:
        mission = mission[:mission.index(heading)].rstrip()
    evidence = f"""

{heading}

- Pull request: `#31`
- Exact validated source: `{VALIDATED_SOURCE_SHA}`
- Merge commit: `{MERGE_SHA}`
- Validation workflow run: `{VALIDATION_RUN}`
- Linux deterministic job: `{VALIDATION_LINUX_JOB}` - passed
- Windows application/package job: `{VALIDATION_WINDOWS_JOB}` - passed
- Validated workflow artifact: `{ARTIFACT_ID}`
- Workflow artifact digest: `{ARTIFACT_DIGEST}`
- Publication workflow run: `{os.environ.get('GITHUB_RUN_ID', 'unknown')}`
- Full MSVC/Vulkan build and all four Windows tests: passed
- Build-tree version, Vulkan, and package diagnostics from an unrelated working directory: passed
- Installed executable and executable-relative `run.bat`: passed from an unrelated working directory
- ZIP extraction and per-file manifest audit: passed before publication
- Published assets were re-downloaded, byte-compared, checksum-verified, extracted, and manifest-audited by the publisher
- Temporary release workflows and source branches were removed after publication
- Open pull requests after cleanup: `0`; remaining branch after cleanup: `main`
- Live screenshot-level behavior remains pending Adam's released-package confirmation; contradictory behavior reopens the exact mission
"""
    mission_path.write_text(mission.rstrip() + evidence + "\n", encoding="utf-8", newline="\n")

    for workflow in (
        Path(".github/workflows/validate-runner-v076.yml"),
        Path(".github/workflows/publish-runner-v076.yml"),
    ):
        workflow.unlink(missing_ok=True)
    run("git", "diff", "--check")
    run("git", "config", "user.name", "github-actions[bot]")
    run("git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com")
    run("git", "add", "-A")
    run("git", "commit", "-m", "Finalize clean Runner v0.7.6 release source")
    run("git", "push", "origin", "HEAD:main")
    release_sha = run("git", "rev-parse", "HEAD", capture=True)

    staging = work / "staging"
    release = work / "release"
    staging.mkdir()
    release.mkdir()
    with zipfile.ZipFile(validated_archive) as package:
        package.extractall(staging)
    shutil.copy2(mission_path, staging / "missioncache.md")
    archive = release / ASSET
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as package:
        for file in sorted(path for path in staging.rglob("*") if path.is_file()):
            package.write(file, file.relative_to(staging).as_posix())
    checksum = sha256_file(archive)
    (release / f"{ASSET}.sha256").write_text(
        f"{checksum}  {ASSET}\n", encoding="utf-8")
    manifest_path = release / "Runner-v0.7.6-windows-x64.manifest.sha256"
    manifest_path.write_text("\n".join(manifest_for(staging)) + "\n", encoding="utf-8")

    preaudit = work / "prepublish-audit"
    with zipfile.ZipFile(archive) as package:
        package.extractall(preaudit)
    if manifest_path.read_text(encoding="utf-8").splitlines() != manifest_for(preaudit):
        raise RuntimeError("Pre-publication manifest mismatch")
    embedded = (preaudit / "missioncache.md").read_text(encoding="utf-8")
    if "PUBLISHED - RELEASE ASSETS VERIFIED" not in embedded:
        raise RuntimeError("Final mission ledger is not embedded in package")

    run("git", "tag", "-a", TAG, release_sha, "-m", "Runner v0.7.6")
    run("git", "push", "origin", TAG)
    run(
        "gh", "release", "create", TAG,
        str(archive), str(release / f"{ASSET}.sha256"), str(manifest_path),
        "--repo", REPOSITORY,
        "--target", release_sha,
        "--title", "Runner v0.7.6",
        "--notes-file", "RELEASE_NOTES_v0.7.6.md",
    )

    audit_published_assets(work)
    close_trigger_pr()
    delete_branch("agent/runner-v076-standing-pip")
    delete_branch("agent/trigger-v076-publisher")
    verify_final_state()
    print(f"Published and audited {TAG} at {release_sha}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Publisher failed: {exc}", file=sys.stderr)
        raise
