from __future__ import annotations

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

REPOSITORY = "Autodidac/Vulkan_AI_Walking_Training_Simulation"
TAG = "v0.7.6"
EXPECTED_MAIN = "35d67be615a84ba23635803146b05594c7bccdbd"
ASSET = "Runner-v0.7.6-windows-x64.zip"


def run(*args: str, capture: bool = False, check: bool = True) -> str:
    print("+", " ".join(args), flush=True)
    result = subprocess.run(
        args,
        text=True,
        check=check,
        stdout=subprocess.PIPE if capture else None,
    )
    return result.stdout.strip() if capture else ""


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def manifest(root: Path) -> list[str]:
    return [
        f"{digest(path)}  {path.relative_to(root).as_posix()}"
        for path in sorted(item for item in root.rglob("*") if item.is_file())
    ]


def audit_release(work: Path) -> None:
    download = work / "published"
    extracted = work / "extracted"
    download.mkdir(parents=True)
    run("gh", "release", "download", TAG, "--repo", REPOSITORY, "--dir", str(download))

    archive = download / ASSET
    checksum = download / f"{ASSET}.sha256"
    package_manifest = download / "Runner-v0.7.6-windows-x64.manifest.sha256"
    for required in (archive, checksum, package_manifest):
        if not required.is_file():
            raise RuntimeError(f"Missing release asset: {required.name}")

    expected_digest = checksum.read_text(encoding="utf-8").split()[0].lower()
    if digest(archive).lower() != expected_digest:
        raise RuntimeError("Published archive checksum mismatch")

    with zipfile.ZipFile(archive) as package:
        package.extractall(extracted)
    expected_manifest = package_manifest.read_text(encoding="utf-8").splitlines()
    if expected_manifest != manifest(extracted):
        raise RuntimeError("Published archive manifest mismatch")

    required_files = (
        "Runner.exe",
        "run.bat",
        "missioncache.md",
        "RELEASE_NOTES_v0.7.6.md",
        "shaders/flat.vert.spv",
        "shaders/flat.frag.spv",
        "assets/chicken.ppm",
    )
    missing = [name for name in required_files if not (extracted / name).is_file()]
    if missing:
        raise RuntimeError(f"Published archive is missing: {missing}")


def close_trigger_pr() -> None:
    number = os.environ.get("TRIGGER_PR_NUMBER", "").strip()
    if not number:
        raise RuntimeError("TRIGGER_PR_NUMBER is missing")
    run("gh", "pr", "close", number, "--repo", REPOSITORY, "--delete-branch")


def verify_repository_cleanup() -> None:
    open_prs = run(
        "gh", "pr", "list", "--repo", REPOSITORY, "--state", "open",
        "--json", "number", "--jq", "length", capture=True,
    )
    if open_prs != "0":
        raise RuntimeError(f"Expected zero open PRs, found {open_prs}")

    branches = run(
        "gh", "api", f"repos/{REPOSITORY}/branches", "--paginate",
        "--jq", ".[].name", capture=True,
    ).splitlines()
    if branches != ["main"]:
        raise RuntimeError(f"Unexpected branches remain: {branches}")


def main() -> int:
    run("git", "fetch", "origin", "main", "--tags")
    run("git", "switch", "-C", "cleanup-main", "origin/main")
    current = run("git", "rev-parse", "HEAD", capture=True)
    if current != EXPECTED_MAIN:
        raise RuntimeError(f"Unexpected main commit {current}; expected {EXPECTED_MAIN}")

    accidental = Path(".publisher-v076")
    if not accidental.is_dir():
        raise RuntimeError("Accidental publisher staging directory is already absent")
    expected_names = {
        "validated/Runner-v0.7.6-windows-x64.zip",
        "validated/Runner-v0.7.6-windows-x64.zip.sha256",
        "validated/Runner-v0.7.6-windows-x64.manifest.sha256",
    }
    actual_names = {
        path.relative_to(accidental).as_posix()
        for path in accidental.rglob("*")
        if path.is_file()
    }
    if actual_names != expected_names:
        raise RuntimeError(f"Unexpected publisher staging contents: {sorted(actual_names)}")

    shutil.rmtree(accidental)
    run("git", "diff", "--check")
    run("git", "config", "user.name", "github-actions[bot]")
    run("git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com")
    run("git", "add", "-A")
    run("git", "commit", "-m", "Remove accidental Runner v0.7.6 publisher staging files")
    clean_sha = run("git", "rev-parse", "HEAD", capture=True)
    run("git", "push", "origin", "HEAD:main")

    run("git", "tag", "-f", "-a", TAG, clean_sha, "-m", "Runner v0.7.6")
    run("git", "push", "--force", "origin", f"refs/tags/{TAG}")
    peeled = run(
        "git", "ls-remote", "origin", f"refs/tags/{TAG}^{{}}", capture=True,
    ).split()[0]
    if peeled != clean_sha:
        raise RuntimeError(f"Release tag points to {peeled}, expected {clean_sha}")

    with tempfile.TemporaryDirectory(prefix="runner-v076-release-audit-") as directory:
        audit_release(Path(directory))

    release = run(
        "gh", "release", "view", TAG, "--repo", REPOSITORY,
        "--json", "isDraft,isPrerelease,tagName,url", capture=True,
    )
    if '"isDraft":false' not in release or '"isPrerelease":false' not in release:
        raise RuntimeError(f"Release is not final: {release}")

    close_trigger_pr()
    verify_repository_cleanup()
    print(f"Runner {TAG} clean source and tag repaired at {clean_sha}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"Release cleanup failed: {error}", file=sys.stderr)
        raise
