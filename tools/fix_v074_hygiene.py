from pathlib import Path

root = Path(__file__).resolve().parents[1]
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
print('normalized Runner v0.7.4 text files')
