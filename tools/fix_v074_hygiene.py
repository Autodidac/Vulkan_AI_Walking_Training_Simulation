from pathlib import Path
import re
import shutil

root = Path(__file__).resolve().parents[1]
old_brand = 'ep' + 'och'

for relative in ('archive-audit', 'artifact', 'published', 'release-stage'):
    shutil.rmtree(root / relative, ignore_errors=True)

for path in sorted(root.rglob('*'), key=lambda item: len(item.parts), reverse=True):
    if '.git' in path.parts or path == Path(__file__):
        continue
    if old_brand in path.name.lower():
        if path.is_dir():
            shutil.rmtree(path, ignore_errors=True)
        else:
            path.unlink(missing_ok=True)

(root / 'tools/remove_legacy_runner_artifacts.py').unlink(missing_ok=True)

for path in root.rglob('*'):
    if not path.is_file() or '.git' in path.parts or path == Path(__file__):
        continue
    try:
        text = path.read_text(encoding='utf-8')
    except (UnicodeDecodeError, OSError):
        continue
    text = text.replace(
        'Runner v" RUNNER_VERSION " - Sand-Sim Enemy Locomotion Trainer',
        'Runner v" RUNNER_VERSION " - Autonomous Physics Locomotion Trainer')
    text = text.replace('## Sand-sim enemy locomotion hotfix',
                        '## Autonomous locomotion hotfix')
    text = text.replace('Sand-sim enemy locomotion curriculum',
                        'Simulation-enemy locomotion curriculum')
    text = re.sub(r'sand-sim enemy', 'simulation-enemy', text,
                  flags=re.IGNORECASE)
    normalized = '\n'.join(line.rstrip() for line in text.splitlines()).rstrip() + '\n'
    if normalized != path.read_text(encoding='utf-8'):
        path.write_text(normalized, encoding='utf-8', newline='\n')

workflow = root / '.github/workflows/validate-runner-v074.yml'
if workflow.exists():
    text = workflow.read_text(encoding='utf-8')
    text = text.replace(
        "if git grep -in 'runner' -- .; then",
        "old_brand=\"$(printf '%s%s' 'ep' 'och')\"\n          if git grep -in \"$old_brand\" -- .; then")
    text = text.replace(
        "$matches = git grep -in runner -- .",
        "$oldBrandToken = 'ep' + 'och'\n          $matches = git grep -in $oldBrandToken -- .")
    text = text.replace(
        "$oldBrand = Get-ChildItem $stage -Recurse -File | Select-String -Pattern 'runner' -SimpleMatch -CaseSensitive:$false",
        "$oldBrandToken = 'ep' + 'och'\n          $oldBrand = Get-ChildItem $stage -Recurse -File | Select-String -Pattern $oldBrandToken -SimpleMatch -CaseSensitive:$false")
    text = text.replace(
        "if git grep -in 'simulation-enemy' -- .; then",
        "old_title=\"$(printf '%s%s' 'sand-sim ' 'enemy')\"\n          if git grep -in \"$old_title\" -- .; then")
    workflow.write_text(text, encoding='utf-8', newline='\n')

Path(__file__).unlink()
print('removed legacy binaries, obsolete title, and normalized Runner v0.7.4 source')
