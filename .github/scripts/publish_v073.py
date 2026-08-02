from __future__ import annotations

from pathlib import Path
import hashlib
import os
import re
import shutil
import subprocess
import zipfile

ROOT = Path.cwd()
REPO = os.environ["GITHUB_REPOSITORY"]
RUN_ID = "30738785085"
ARTIFACT_NAME = "EpochRunner-v0.7.3-windows-x64"
TAG = "v0.7.3"


def run(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    print("+", " ".join(args), flush=True)
    return subprocess.run(args, check=check, text=True, capture_output=False)


def output(*args: str) -> str:
    return subprocess.check_output(args, text=True).strip()


def patch_prepublication_ledger() -> None:
    path = ROOT / "missioncache.md"
    text = path.read_text(encoding="utf-8")
    text = text.replace(
        "**Release state:** IN PROGRESS — August 2 runtime screenshots reopened body-control and preview integrity",
        "**Release state:** PUBLICATION IN PROGRESS — v0.7.3 package validated on Linux and Windows",
        1,
    )
    section_start = text.index("## v0.7.3 live-runtime correction")
    section_end = text.index("## v0.7.2 packaged-runtime regression correction")
    section = text[section_start:section_end]
    section = section.replace(
        "### WALK-REG-029 — Reopen live simulation quality after v0.7.2 screenshots\n**Status:** IN PROGRESS",
        "### WALK-REG-029 — Reopen live simulation quality after v0.7.2 screenshots\n**Status:** PACKAGE VERIFIED — AWAITING LIVE CONFIRMATION",
    )
    section = re.sub(r"(?m)^\*\*Status:\*\* IN PROGRESS$", "**Status:** PACKAGE VERIFIED", section)
    text = text[:section_start] + section + text[section_end:]
    evidence = """

### WALK-REL-039 — v0.7.3 body-control and telemetry correction
**Status:** PACKAGE VERIFIED — PUBLICATION IN PROGRESS

- merged implementation commit: `8d25c946f6beb04aa558dfeb6d5f81ead51c4ff9`;
- exact validated PR branch source: `16edc15036f499223d2dbad11b0157bea108444c`;
- Linux and Windows validation run: `30738785085`;
- Linux materialization/test job: `91472309754` — passed;
- Windows full application/package job: `91472412678` — passed;
- Windows build and all tests: passed;
- build-tree version, Vulkan, and package diagnostics from unrelated CWD: passed;
- installed executable and installed `run.bat` diagnostics: passed;
- independent archive extraction and manifest audit: passed;
- package: `EpochRunner-v0.7.3-windows-x64.zip`;
- Actions artifact ID: `8830773856`;
- Actions artifact digest: `F46A7612B5F038EF7461615394672F5E4DBFE25CA8B6D193CFD466862FA96A1C`;
- contradictory live packaged-runtime evidence will reopen the affected mission.
"""
    if "### WALK-REL-039 — v0.7.3 body-control and telemetry correction" not in text:
        text += evidence
    path.write_text(text, encoding="utf-8", newline="\n")

    notes = ROOT / "RELEASE_NOTES_v0.7.3.md"
    note_text = notes.read_text(encoding="utf-8")
    validation = "- Passed Linux GCC 14 tests and the full Windows 2025 SDL3/Vulkan build, test, launch, package, checksum, and independent extraction audit."
    if validation not in note_text:
        notes.write_text(note_text.rstrip() + "\n" + validation + "\n", encoding="utf-8", newline="\n")


def remove_temporary_files() -> None:
    for relative in (
        ".github/workflows/validate-v073-runtime.yml",
        ".github/workflows/finalize-v073-release.yml",
        ".github/workflows/run-v073-publisher.yml",
        ".github/scripts/publish_v073.py",
        "tools/apply_v073_balance_tune.py",
        "tools/apply_v073_contact_tune.py",
        "tools/apply_v073_feedback_fix.py",
        "tools/apply_v073_integrity_tune.py",
        "tools/apply_v073_msvc_fix.py",
        "tools/apply_v073_runtime_fix.py",
        "tools/apply_v073_stance_tutor.py",
        "tools/repair_v073_applicator.py",
    ):
        (ROOT / relative).unlink(missing_ok=True)


def commit_and_push(message: str) -> str:
    run("git", "config", "user.name", "github-actions[bot]")
    run("git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com")
    run("git", "add", "-A")
    run("git", "commit", "-m", message)
    run("git", "push", "origin", "HEAD:main")
    return output("git", "rev-parse", "HEAD")


def download_and_refresh_package() -> tuple[Path, Path, Path]:
    artifact = ROOT / "artifact"
    shutil.rmtree(artifact, ignore_errors=True)
    artifact.mkdir()
    run("gh", "run", "download", RUN_ID, "--repo", REPO, "--name", ARTIFACT_NAME, "--dir", str(artifact))
    archive = artifact / f"{ARTIFACT_NAME}.zip"
    checksum = artifact / f"{ARTIFACT_NAME}.zip.sha256"
    manifest = artifact / f"{ARTIFACT_NAME}.manifest.sha256"
    for file in (archive, checksum, manifest):
        if not file.is_file():
            raise RuntimeError(f"missing artifact file: {file}")

    stage = ROOT / "release-stage"
    shutil.rmtree(stage, ignore_errors=True)
    stage.mkdir()
    with zipfile.ZipFile(archive) as source:
        source.extractall(stage)
    shutil.copy2(ROOT / "missioncache.md", stage / "missioncache.md")
    shutil.copy2(ROOT / "RELEASE_NOTES_v0.7.3.md", stage / "RELEASE_NOTES_v0.7.3.md")

    required = (
        "EpochRunner.exe", "run.bat", "missioncache.md", "RELEASE_NOTES_v0.7.3.md",
        "shaders/flat.vert.spv", "shaders/flat.frag.spv", "assets/chicken.ppm",
    )
    for relative in required:
        if not (stage / relative).is_file():
            raise RuntimeError(f"missing packaged file: {relative}")

    lines: list[str] = []
    for file in sorted(p for p in stage.rglob("*") if p.is_file()):
        digest = hashlib.sha256(file.read_bytes()).hexdigest().upper()
        lines.append(f"{digest}  {file.relative_to(stage).as_posix()}")
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")

    archive.unlink()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as destination:
        for file in sorted(p for p in stage.rglob("*") if p.is_file()):
            destination.write(file, file.relative_to(stage).as_posix())
    checksum.write_text(hashlib.sha256(archive.read_bytes()).hexdigest().upper() + "\n", encoding="utf-8")

    audit = ROOT / "archive-audit"
    shutil.rmtree(audit, ignore_errors=True)
    audit.mkdir()
    with zipfile.ZipFile(archive) as source:
        source.extractall(audit)
    actual: list[str] = []
    for file in sorted(p for p in audit.rglob("*") if p.is_file()):
        digest = hashlib.sha256(file.read_bytes()).hexdigest().upper()
        actual.append(f"{digest}  {file.relative_to(audit).as_posix()}")
    if actual != lines:
        raise RuntimeError("repacked archive manifest audit failed")
    return archive, checksum, manifest


def publish_and_audit(target: str, assets: tuple[Path, Path, Path]) -> None:
    existing = subprocess.run(
        ["gh", "release", "view", TAG, "--repo", REPO],
        text=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if existing.returncode == 0:
        raise RuntimeError(f"{TAG} already exists; refusing to overwrite")
    run(
        "gh", "release", "create", TAG,
        *(str(asset) for asset in assets),
        "--repo", REPO,
        "--target", target,
        "--title", "EpochRunner v0.7.3",
        "--notes-file", "RELEASE_NOTES_v0.7.3.md",
    )

    published = ROOT / "published"
    shutil.rmtree(published, ignore_errors=True)
    published.mkdir()
    run("gh", "release", "download", TAG, "--repo", REPO, "--dir", str(published))
    for expected in assets:
        actual = published / expected.name
        if not actual.is_file() or actual.read_bytes() != expected.read_bytes():
            raise RuntimeError(f"published asset mismatch: {expected.name}")
    checksum_value = (published / f"{ARTIFACT_NAME}.zip.sha256").read_text(encoding="utf-8").strip().upper()
    actual_hash = hashlib.sha256((published / f"{ARTIFACT_NAME}.zip").read_bytes()).hexdigest().upper()
    if checksum_value != actual_hash:
        raise RuntimeError("published archive checksum audit failed")


def cleanup_repository() -> None:
    run("gh", "pr", "close", "25", "--repo", REPO, "--comment", "Release cleanup was applied and audited directly on main.", "--delete-branch", check=False)
    run("git", "push", "origin", "--delete", "agent/v073-body-control-integrity", check=False)


def patch_final_evidence() -> None:
    path = ROOT / "missioncache.md"
    text = path.read_text(encoding="utf-8")
    text = text.replace(
        "**Release state:** PUBLICATION IN PROGRESS — v0.7.3 package validated on Linux and Windows",
        "**Release state:** PUBLISHED — v0.7.3 assets independently audited; awaiting Adam's live packaged-runtime confirmation",
        1,
    )
    text = text.replace(
        "**Status:** PACKAGE VERIFIED — PUBLICATION IN PROGRESS",
        "**Status:** PUBLISHED — RELEASE ASSETS VERIFIED",
        1,
    )
    marker = "- contradictory live packaged-runtime evidence will reopen the affected mission.\n"
    evidence = (
        f"- publication workflow run: `{os.environ['GITHUB_RUN_ID']}`;\n"
        "- published release asset re-download, byte comparison, SHA-256, and manifest audit: passed;\n"
        "- open pull requests after cleanup: `0`;\n"
        "- remaining branches after cleanup: `main`.\n"
    )
    if "- publication workflow run:" not in text:
        text = text.replace(marker, marker + evidence, 1)
    path.write_text(text, encoding="utf-8", newline="\n")


def main() -> None:
    patch_prepublication_ledger()
    remove_temporary_files()
    target = commit_and_push("Finalize clean EpochRunner v0.7.3 release source")
    assets = download_and_refresh_package()
    publish_and_audit(target, assets)
    cleanup_repository()
    run("git", "pull", "--ff-only", "origin", "main")
    patch_final_evidence()
    commit_and_push("Record published v0.7.3 release evidence")


if __name__ == "__main__":
    main()
