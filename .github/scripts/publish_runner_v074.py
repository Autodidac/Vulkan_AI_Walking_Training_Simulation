from __future__ import annotations

from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import zipfile

ROOT = Path.cwd()
REPO = os.environ["GITHUB_REPOSITORY"]
RUN_ID = "30745394278"
ARTIFACT_NAME = "Runner-v0.7.4-windows-x64"
TAG = "v0.7.4"
PR_NUMBER = "27"
HEAD_SHA = "c6ef7668175d7529a062d577d47e18b96a4b2448"
LINUX_JOB = "91490018837"
WINDOWS_JOB = "91490104548"


def run(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    print("+", " ".join(args), flush=True)
    return subprocess.run(args, check=check, text=True)


def output(*args: str) -> str:
    return subprocess.check_output(args, text=True).strip()


def artifact_metadata() -> tuple[str, str]:
    payload = json.loads(output(
        "gh", "api", f"repos/{REPO}/actions/runs/{RUN_ID}/artifacts"))
    for artifact in payload.get("artifacts", []):
        if artifact.get("name") == ARTIFACT_NAME and not artifact.get("expired", False):
            return str(artifact["id"]), str(artifact.get("digest", "unavailable"))
    raise RuntimeError("validated Windows artifact was not found")


def patch_prepublication_ledger(artifact_id: str, artifact_digest: str) -> None:
    path = ROOT / "missioncache.md"
    text = path.read_text(encoding="utf-8")
    text = re.sub(
        r"\*\*Release state:\*\* IN PROGRESS[^\n]*",
        "**Release state:** PACKAGE VERIFIED — v0.7.4 publication and final asset audit in progress",
        text,
        count=1,
    )
    section_start = text.index("## v0.7.4 rebrand and duck-training correction")
    section_end = text.index("## v0.7.3 live-runtime correction")
    section = text[section_start:section_end]
    section = re.sub(r"(?m)^\*\*Status:\*\* IN PROGRESS$",
                     "**Status:** PACKAGE VERIFIED", section)
    section = section.replace(
        "### WALK-LIVE-066 — Screenshot-level packaged-runtime acceptance\n**Status:** PACKAGE VERIFIED",
        "### WALK-LIVE-066 — Screenshot-level packaged-runtime acceptance\n**Status:** PACKAGE VERIFIED — AWAITING LIVE CONFIRMATION",
    )
    text = text[:section_start] + section + text[section_end:]
    text = text.replace(
        "### WALK-STATS-038 — Rig lifetime and cumulative runtime totals\n**Status:** REOPENED BY FULL CONVERSATION AUDIT",
        "### WALK-STATS-038 — Rig lifetime and cumulative runtime totals\n**Status:** PACKAGE VERIFIED — COMPLETED BY WALK-STATS-062",
    )
    evidence = f'''

### WALK-REL-067 — Runner v0.7.4 final package and publication
**Status:** PACKAGE VERIFIED — PUBLICATION IN PROGRESS

- exact validated source: `{HEAD_SHA}`;
- pull request: `#{PR_NUMBER}`;
- Linux and Windows validation run: `{RUN_ID}`;
- Linux deterministic build/test job: `{LINUX_JOB}` — passed;
- Windows full application/package job: `{WINDOWS_JOB}` — passed;
- Windows build and all tests: passed;
- build-tree version, Vulkan, and package diagnostics from unrelated CWD: passed;
- installed executable and installed `run.bat` diagnostics: passed;
- independent archive extraction and manifest audit: passed;
- package: `{ARTIFACT_NAME}.zip`;
- Actions artifact ID: `{artifact_id}`;
- Actions artifact digest: `{artifact_digest}`;
- repository and package scans for the former project word and obsolete trainer title: passed;
- contradictory live packaged-runtime evidence will reopen the affected mission.
'''
    if "### WALK-REL-067 — Runner v0.7.4 final package and publication" not in text:
        text += evidence
    path.write_text(text, encoding="utf-8", newline="\n")

    notes = ROOT / "RELEASE_NOTES_v0.7.4.md"
    note_text = notes.read_text(encoding="utf-8")
    validation = (
        "- Passed Linux GCC 14 and full Windows Server 2025 MSVC build, test, "
        "Vulkan/package diagnostics, installed launcher, checksum, and independent extraction audit."
    )
    if validation not in note_text:
        notes.write_text(note_text.rstrip() + "\n" + validation + "\n",
                         encoding="utf-8", newline="\n")


def remove_temporary_files() -> None:
    for relative in (
        ".github/workflows/validate-runner-v074.yml",
        ".github/workflows/publish-runner-v074.yml",
        ".github/scripts/publish_runner_v074.py",
        "tools/repair_app_from_passing_source.py",
        "tools/apply_runner_v074.py",
        "tools/fix_v074_live_failures.py",
        "tools/complete_v074_missions.py",
        "tools/reconcile_runner_missions.py",
        "tools/fix_v074_hygiene.py",
        "tools/finalize_v074_quality.py",
    ):
        (ROOT / relative).unlink(missing_ok=True)


def commit_and_push(message: str) -> str:
    run("git", "config", "user.name", "github-actions[bot]")
    run("git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com")
    run("git", "add", "-A")
    status = subprocess.run(["git", "diff", "--cached", "--quiet"])
    if status.returncode != 0:
        run("git", "commit", "-m", message)
        run("git", "push", "origin", "HEAD:main")
    return output("git", "rev-parse", "HEAD")


def scan_clean_tree(root: Path) -> None:
    old_brand = ("ep" + "och").encode().lower()
    old_title = ("sand-sim " + "enemy").encode().lower()
    for file in root.rglob("*"):
        if not file.is_file() or ".git" in file.parts:
            continue
        if old_brand.decode() in file.name.lower():
            raise RuntimeError(f"former project word remains in filename: {file}")
        data = file.read_bytes().lower()
        if old_brand in data:
            raise RuntimeError(f"former project word remains in file: {file}")
        if old_title in data:
            raise RuntimeError(f"obsolete trainer title remains in file: {file}")


def write_assets(stage: Path, artifact: Path) -> tuple[Path, Path, Path]:
    archive = artifact / f"{ARTIFACT_NAME}.zip"
    checksum = artifact / f"{ARTIFACT_NAME}.zip.sha256"
    manifest = artifact / f"{ARTIFACT_NAME}.manifest.sha256"
    scan_clean_tree(stage)
    lines: list[str] = []
    for file in sorted(p for p in stage.rglob("*") if p.is_file()):
        digest = hashlib.sha256(file.read_bytes()).hexdigest().upper()
        lines.append(f"{digest}  {file.relative_to(stage).as_posix()}")
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    archive.unlink(missing_ok=True)
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as destination:
        for file in sorted(p for p in stage.rglob("*") if p.is_file()):
            destination.write(file, file.relative_to(stage).as_posix())
    checksum.write_text(hashlib.sha256(archive.read_bytes()).hexdigest().upper() + "\n",
                        encoding="utf-8")
    return archive, checksum, manifest


def download_stage() -> tuple[Path, Path]:
    artifact = ROOT / "artifact"
    shutil.rmtree(artifact, ignore_errors=True)
    artifact.mkdir()
    run("gh", "run", "download", RUN_ID, "--repo", REPO,
        "--name", ARTIFACT_NAME, "--dir", str(artifact))
    archive = artifact / f"{ARTIFACT_NAME}.zip"
    if not archive.is_file():
        raise RuntimeError("validated package archive is missing")
    stage = ROOT / "release-stage"
    shutil.rmtree(stage, ignore_errors=True)
    stage.mkdir()
    with zipfile.ZipFile(archive) as source:
        source.extractall(stage)
    for relative in (
        "Runner.exe", "run.bat", "missioncache.md", "RELEASE_NOTES_v0.7.4.md",
        "shaders/flat.vert.spv", "shaders/flat.frag.spv", "assets/chicken.ppm",
    ):
        if not (stage / relative).is_file():
            raise RuntimeError(f"missing package file: {relative}")
    return artifact, stage


def audit_assets(assets: tuple[Path, Path, Path], directory: Path) -> None:
    shutil.rmtree(directory, ignore_errors=True)
    directory.mkdir()
    run("gh", "release", "download", TAG, "--repo", REPO, "--dir", str(directory))
    for expected in assets:
        actual = directory / expected.name
        if not actual.is_file() or actual.read_bytes() != expected.read_bytes():
            raise RuntimeError(f"published asset mismatch: {expected.name}")
    archive = directory / f"{ARTIFACT_NAME}.zip"
    checksum = (directory / f"{ARTIFACT_NAME}.zip.sha256").read_text(
        encoding="utf-8").strip().upper()
    if hashlib.sha256(archive.read_bytes()).hexdigest().upper() != checksum:
        raise RuntimeError("published archive checksum audit failed")
    audit = ROOT / "published-audit"
    shutil.rmtree(audit, ignore_errors=True)
    audit.mkdir()
    with zipfile.ZipFile(archive) as source:
        source.extractall(audit)
    scan_clean_tree(audit)
    actual_lines: list[str] = []
    for file in sorted(p for p in audit.rglob("*") if p.is_file()):
        digest = hashlib.sha256(file.read_bytes()).hexdigest().upper()
        actual_lines.append(f"{digest}  {file.relative_to(audit).as_posix()}")
    expected_lines = (directory / f"{ARTIFACT_NAME}.manifest.sha256").read_text(
        encoding="utf-8").splitlines()
    if actual_lines != expected_lines:
        raise RuntimeError("published archive manifest audit failed")


def patch_final_evidence() -> None:
    path = ROOT / "missioncache.md"
    text = path.read_text(encoding="utf-8")
    text = text.replace(
        "**Release state:** PACKAGE VERIFIED — v0.7.4 publication and final asset audit in progress",
        "**Release state:** PUBLISHED — v0.7.4 assets independently audited; awaiting Adam's live packaged-runtime confirmation",
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
        "- published release asset re-download, byte comparison, SHA-256, manifest, extraction, and branding audit: passed;\n"
        "- open pull requests after cleanup: `0`;\n"
        "- remaining branches after cleanup: `main`.\n"
    )
    rel = text.index("### WALK-REL-067 — Runner v0.7.4 final package and publication")
    before, section = text[:rel], text[rel:]
    if "- publication workflow run:" not in section:
        section = section.replace(marker, marker + evidence, 1)
    path.write_text(before + section, encoding="utf-8", newline="\n")


def repository_cleanup() -> None:
    run("git", "push", "origin", "--delete", "agent/runner-v074-brand-duck-press", check=False)
    open_prs = json.loads(output("gh", "pr", "list", "--repo", REPO,
                                 "--state", "open", "--json", "number"))
    if open_prs:
        raise RuntimeError(f"open pull requests remain: {open_prs}")
    branches = json.loads(output("gh", "api", f"repos/{REPO}/branches?per_page=100"))
    names = sorted(item["name"] for item in branches)
    if names != ["main"]:
        raise RuntimeError(f"unexpected branches remain: {names}")


def main() -> None:
    release_created = False
    try:
        existing = subprocess.run(["gh", "release", "view", TAG, "--repo", REPO],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if existing.returncode == 0:
            raise RuntimeError(f"{TAG} already exists; refusing to overwrite")
        artifact_id, artifact_digest = artifact_metadata()
        patch_prepublication_ledger(artifact_id, artifact_digest)
        remove_temporary_files()
        scan_clean_tree(ROOT)
        target = commit_and_push("Finalize clean Runner v0.7.4 release source")
        artifact, stage = download_stage()
        shutil.copy2(ROOT / "missioncache.md", stage / "missioncache.md")
        shutil.copy2(ROOT / "RELEASE_NOTES_v0.7.4.md", stage / "RELEASE_NOTES_v0.7.4.md")
        assets = write_assets(stage, artifact)
        run("gh", "release", "create", TAG, *(str(asset) for asset in assets),
            "--repo", REPO, "--target", target, "--title", "Runner v0.7.4",
            "--notes-file", "RELEASE_NOTES_v0.7.4.md")
        release_created = True
        audit_assets(assets, ROOT / "published-preliminary")
        repository_cleanup()
        patch_final_evidence()
        shutil.copy2(ROOT / "missioncache.md", stage / "missioncache.md")
        assets = write_assets(stage, artifact)
        run("gh", "release", "upload", TAG, *(str(asset) for asset in assets),
            "--repo", REPO, "--clobber")
        audit_assets(assets, ROOT / "published-final")
        commit_and_push("Record published Runner v0.7.4 release evidence")
    except Exception:
        if release_created:
            run("gh", "release", "delete", TAG, "--repo", REPO,
                "--yes", "--cleanup-tag", check=False)
        raise


if __name__ == "__main__":
    main()
