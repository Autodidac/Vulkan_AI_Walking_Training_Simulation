from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MISSION = ROOT / "missioncache.md"
STALE_TRIGGER = ROOT / ".github/workflows/trigger-v0.1.3.txt"

if not STALE_TRIGGER.is_file():
    raise RuntimeError("expected stale v0.1.3 trigger file is missing")
if STALE_TRIGGER.read_text(encoding="utf-8").strip() != "Trigger the inspectable Windows validation for v0.1.3.":
    raise RuntimeError("stale trigger contents changed; refusing broad cleanup")
STALE_TRIGGER.unlink()

text = MISSION.read_text(encoding="utf-8")
anchor = """**Release-audit hygiene finding:** clean PR release run `31020437322`, Linux job `92355450216`, stopped before compilation because the repository still contained `.github/workflows/trigger-v0.1.3.txt`, a one-line historical workflow trigger from release v0.1.3. It is not product source, not referenced by the current workflows, and is explicitly forbidden by the v0.7.15 release audit. Remove that exact stale file, retain all current validation/release workflows, and rerun the full clean package gate.

"""
addition = anchor + """**Release-audit hygiene correction:** removed only the verified obsolete `.github/workflows/trigger-v0.1.3.txt` artifact. No current workflow, source, test, asset, or release configuration was changed. The repository must now pass the same trigger-file scan before compilation and packaging.

"""
if text.count(anchor) != 1:
    raise RuntimeError("release-audit finding anchor missing or duplicated")
text = text.replace(anchor, addition, 1)
MISSION.write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")
