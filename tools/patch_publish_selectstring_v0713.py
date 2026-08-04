from pathlib import Path

path = Path('tools/publish_v0713.ps1')
text = path.read_text(encoding='utf-8')
replacements = (
    (
        "if (-not (Select-String CMakeLists.txt -SimpleMatch 'project(Runner VERSION 0.7.13 LANGUAGES CXX)')) {",
        "if (-not (Select-String -Path 'CMakeLists.txt' -Pattern 'project(Runner VERSION 0.7.13 LANGUAGES CXX)' -SimpleMatch)) {",
    ),
    (
        "if (-not (Select-String src/simulation.hpp,src/simulation.cpp -SimpleMatch $requiredText)) {",
        "if (-not (Select-String -Path @('src/simulation.hpp','src/simulation.cpp') -Pattern $requiredText -SimpleMatch)) {",
    ),
    (
        "if (-not (Select-String src/app.cpp -SimpleMatch 'runner-v0713-autosave.eppo')) {",
        "if (-not (Select-String -Path 'src/app.cpp' -Pattern 'runner-v0713-autosave.eppo' -SimpleMatch)) {",
    ),
    (
        "git rm .github/workflows/publish-runner-v0713.yml tools/publish_v0713.ps1 tools/patch_publish_v0713.py",
        "git rm .github/workflows/publish-runner-v0713.yml tools/publish_v0713.ps1 tools/patch_publish_v0713.py tools/patch_publish_selectstring_v0713.py",
    ),
)
for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'publisher correction expected one match, found {count}: {old[:80]}')
    text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8')
