from pathlib import Path

path = Path("tools/write_missioncache_v071.py")
text = path.read_text(encoding="utf-8")
text = text.replace(
    "- Workflow run: `{run_id}`;",
    "- Linux validation workflow: `30718528145`; Windows package and release workflow: `{run_id}`;",
)
text = text.replace(
    '        f"- Workflow run: `{run_id}`\\n"',
    '        "- Linux validation workflow: `30718528145`\\n"\n'
    '        f"- Windows package and release workflow: `{run_id}`\\n"',
)
path.write_text(text, encoding="utf-8")
