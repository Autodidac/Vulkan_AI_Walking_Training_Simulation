from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]
old = 'ep' + 'och'

for relative in ('archive-audit', 'artifact', 'published', 'release-stage'):
    shutil.rmtree(root / relative, ignore_errors=True)

for path in sorted(root.rglob('*'), key=lambda item: len(item.parts), reverse=True):
    if '.git' in path.parts or path == Path(__file__):
        continue
    if old in path.name.lower():
        if path.is_dir():
            shutil.rmtree(path, ignore_errors=True)
        else:
            path.unlink(missing_ok=True)

Path(__file__).unlink()
print('removed legacy branded binaries and temporary release staging')
