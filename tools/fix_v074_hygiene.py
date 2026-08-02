from pathlib import Path

root = Path(__file__).resolve().parents[1]
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
    workflow.write_text(text, encoding='utf-8', newline='\n')

for path in root.rglob('*'):
    if not path.is_file() or '.git' in path.parts or path == Path(__file__):
        continue
    try:
        text = path.read_text(encoding='utf-8')
    except (UnicodeDecodeError, OSError):
        continue
    normalized = '\n'.join(line.rstrip() for line in text.splitlines()).rstrip() + '\n'
    if normalized != text:
        path.write_text(normalized, encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('normalized Runner v0.7.4 text files and restored the split-token brand gate')
