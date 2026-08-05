from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MISSION = ROOT / "missioncache.md"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


text = MISSION.read_text(encoding="utf-8")
anchor = """**Remaining v0.7.15 gate:** run the clean script-free PR workflow, install and audit the package, test `run.bat` from an unrelated directory, verify optional artwork/assets and fallback behavior, create checksums and a per-file manifest, merge, publish `v0.7.15`, re-download and byte-compare the release asset, then remove obsolete PRs/branches. Visual appearance remains subject to released screenshot/manual review and must reopen the exact mission if contradicted.\n\n"""
finding = anchor + """**Release-audit hygiene finding:** clean PR release run `31020437322`, Linux job `92355450216`, stopped before compilation because the repository still contained `.github/workflows/trigger-v0.1.3.txt`, a one-line historical workflow trigger from release v0.1.3. It is not product source, not referenced by the current workflows, and is explicitly forbidden by the v0.7.15 release audit. Remove that exact stale file, retain all current validation/release workflows, and rerun the full clean package gate.\n\n"""
text = replace_once(text, anchor, finding, "cache stale trigger finding")
MISSION.write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")

apply_script = '''from pathlib import Path\n\nROOT = Path(__file__).resolve().parents[1]\nMISSION = ROOT / "missioncache.md"\nSTALE_TRIGGER = ROOT / ".github/workflows/trigger-v0.1.3.txt"\n\nif not STALE_TRIGGER.is_file():\n    raise RuntimeError("expected stale v0.1.3 trigger file is missing")\nif STALE_TRIGGER.read_text(encoding="utf-8").strip() != "Trigger the inspectable Windows validation for v0.1.3.":\n    raise RuntimeError("stale trigger contents changed; refusing broad cleanup")\nSTALE_TRIGGER.unlink()\n\ntext = MISSION.read_text(encoding="utf-8")\nanchor = """**Release-audit hygiene finding:** clean PR release run `31020437322`, Linux job `92355450216`, stopped before compilation because the repository still contained `.github/workflows/trigger-v0.1.3.txt`, a one-line historical workflow trigger from release v0.1.3. It is not product source, not referenced by the current workflows, and is explicitly forbidden by the v0.7.15 release audit. Remove that exact stale file, retain all current validation/release workflows, and rerun the full clean package gate.\n\n"""\naddition = anchor + """**Release-audit hygiene correction:** removed only the verified obsolete `.github/workflows/trigger-v0.1.3.txt` artifact. No current workflow, source, test, asset, or release configuration was changed. The repository must now pass the same trigger-file scan before compilation and packaging.\n\n"""\nif text.count(anchor) != 1:\n    raise RuntimeError("release-audit finding anchor missing or duplicated")\ntext = text.replace(anchor, addition, 1)\nMISSION.write_text(text.replace("\\r\\n", "\\n").rstrip() + "\\n", encoding="utf-8")\n'''
(ROOT / "tools/apply_v0715_refinement.py").write_text(apply_script, encoding="utf-8")
