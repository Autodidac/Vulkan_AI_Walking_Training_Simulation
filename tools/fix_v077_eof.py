from pathlib import Path

for name in ('missioncache.md', 'RELEASE_NOTES_v0.7.7.md'):
    path = Path(name)
    path.write_text(path.read_text(encoding='utf-8').rstrip() + '\n',
                    encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('normalized v0.7.7 text file endings')
