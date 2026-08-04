from pathlib import Path

path = Path('tools/publish_v0713.ps1')
text = path.read_text(encoding='utf-8')
replacements = (
    (
        "Set-StrictMode -Version Latest\n\n$repo = $env:GITHUB_REPOSITORY",
        "Set-StrictMode -Version Latest\n\n# The workflow patches this script in-memory, then restores the tracked file so git remains clean.\ngit checkout -- tools/publish_v0713.ps1\n\n$repo = $env:GITHUB_REPOSITORY",
    ),
    (
        "$releaseSha = (git rev-parse HEAD).Trim()\n\n# Repack",
        "$releaseSha = (git rev-parse HEAD).Trim()\n$env:RELEASE_SHA = $releaseSha\n\n# Repack",
    ),
    (
        "$finalHash = (Get-FileHash $archive -Algorithm SHA256).Hash\n$finalHash | Set-Content $checksum",
        "$finalHash = (Get-FileHash $archive -Algorithm SHA256).Hash\n$env:FINAL_ZIP_SHA256 = $finalHash\n$finalHash | Set-Content $checksum",
    ),
    (
        "git rm .github/workflows/publish-runner-v0713.yml tools/publish_v0713.ps1",
        "git rm .github/workflows/publish-runner-v0713.yml tools/publish_v0713.ps1 tools/patch_publish_v0713.py",
    ),
)
for old, new in replacements:
    if text.count(old) != 1:
        raise SystemExit(f'publisher patch expected one match, found {text.count(old)}: {old[:60]}')
    text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8')
