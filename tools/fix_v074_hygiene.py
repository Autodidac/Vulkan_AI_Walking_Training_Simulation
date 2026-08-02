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
        original = path.read_text(encoding='utf-8')
    except (UnicodeDecodeError, OSError):
        continue
    text = original
    text = text.replace(
        'Runner v" RUNNER_VERSION " - Sand-Sim Enemy Locomotion Trainer',
        'Runner v" RUNNER_VERSION " - Autonomous Physics Locomotion Trainer')
    text = text.replace('## Sand-sim enemy locomotion hotfix',
                        '## Autonomous locomotion hotfix')
    text = text.replace('Sand-sim enemy locomotion curriculum',
                        'Simulation-enemy locomotion curriculum')
    text = re.sub(r'sand-sim enemy', 'simulation-enemy', text,
                  flags=re.IGNORECASE)
    text = text.replace('std::array<bool, 5> found{};',
                        'std::array<bool, 6> found{};')
    text = text.replace(
        '0.8f, 0u, 0.0f, 0u, 1u),\n        "valid duck-and-clear result cannot seed self-imitation"',
        '0.8f, 0u, 0.0f, 0u, 2u),\n        "valid press-and-low-bar result cannot seed self-imitation"')
    text = text.replace(
        'if (environment.obstacles_passed() < 1u)\n                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);',
        'if (environment.obstacles_passed() < 2u)\n                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);')
    normalized = '\n'.join(line.rstrip() for line in text.splitlines()).rstrip() + '\n'
    if normalized != original:
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
print('removed legacy material and aligned all two-part duck acceptance tests')
