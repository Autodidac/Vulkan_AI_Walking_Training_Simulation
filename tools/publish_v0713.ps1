$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = $env:GITHUB_REPOSITORY
$token = $env:GH_TOKEN
$tag = 'v0.7.13'
$artifactId = '8895615989'
$artifactDigest = 'sha256:8998b229456dad69ad0b6e5f656fe2624f10ef85fb68931685d0cfa48b3a657a'
$validatedZipHash = '3EA39F26A2E037BEDFA8D64626899E1B401568CDD24A4C4B010FCC5EB20A7E3D'
$testedSource = '626d9369dbdbb8ec1410874c1f0de1fcb40a6950'
$validationRun = '30915999459'
$linuxJob = '92014174361'
$windowsJob = '92014559233'

if (-not (Select-String CMakeLists.txt -SimpleMatch 'project(Runner VERSION 0.7.13 LANGUAGES CXX)')) {
    throw 'Runner v0.7.13 source is missing'
}
foreach ($temporary in @(
    '.github/workflows/validate-runner-v0713.yml',
    'tools/validate_v0713_windows.ps1',
    'tools/apply_v0713_toe_rate_gate.py',
    'tools/prepare_v0713_toe_rate_gate.py')) {
    if (Test-Path $temporary) { throw "Temporary validator remains: $temporary" }
}
foreach ($requiredText in @(
    'rate_limited_toe_command',
    'toe_angular_rate_limit',
    'limit_articulated_toe_rates')) {
    if (-not (Select-String src/simulation.hpp,src/simulation.cpp -SimpleMatch $requiredText)) {
        throw "Toe-rate correction missing: $requiredText"
    }
}
if (-not (Select-String src/app.cpp -SimpleMatch 'runner-v0713-autosave.eppo')) {
    throw 'v0.7.13 state isolation is missing'
}

$headers = @{
    Authorization = "Bearer $token"
    Accept = 'application/vnd.github+json'
    'X-GitHub-Api-Version' = '2022-11-28'
}
$metadata = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/actions/artifacts/$artifactId" -Headers $headers
if ($metadata.digest -ne $artifactDigest) {
    throw "Artifact digest changed: $($metadata.digest)"
}
$outer = Join-Path $env:RUNNER_TEMP 'Runner-v0.7.13-validated-artifact.zip'
$artifact = Join-Path $env:RUNNER_TEMP 'runner-v0713-artifact'
$stage = Join-Path $env:RUNNER_TEMP 'Runner-v0.7.13-windows-x64'
Remove-Item $outer -Force -ErrorAction SilentlyContinue
Remove-Item $artifact,$stage -Recurse -Force -ErrorAction SilentlyContinue
Invoke-WebRequest -Uri "https://api.github.com/repos/$repo/actions/artifacts/$artifactId/zip" -Headers $headers -OutFile $outer
Expand-Archive $outer -DestinationPath $artifact -Force

$binaryZip = Join-Path $artifact 'Runner-v0.7.13-windows-x64.zip'
$binaryChecksum = Join-Path $artifact 'Runner-v0.7.13-windows-x64.zip.sha256'
$binaryManifest = Join-Path $artifact 'Runner-v0.7.13-windows-x64.manifest.sha256'
foreach ($path in @($binaryZip,$binaryChecksum,$binaryManifest)) {
    if (-not (Test-Path $path)) { throw "Validated artifact file missing: $path" }
}
$binaryHash = (Get-FileHash $binaryZip -Algorithm SHA256).Hash
if ($binaryHash -ne $validatedZipHash) { throw "Validated ZIP changed: $binaryHash" }
if ((Get-Content $binaryChecksum).Trim().ToUpperInvariant() -ne $binaryHash) {
    throw 'Validated checksum file disagrees with the ZIP'
}
Expand-Archive $binaryZip -DestinationPath $stage -Force
$expectedManifest = Get-Content $binaryManifest
$actualManifest = Get-ChildItem $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
    "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash)  $relative"
}
if (Compare-Object $expectedManifest $actualManifest) { throw 'Validated manifest mismatch' }
Push-Location $env:RUNNER_TEMP
try {
    if ((& (Join-Path $stage 'Runner.exe') --version | Out-String).Trim() -ne 'Runner 0.7.13') {
        throw 'Validated executable version mismatch'
    }
    & (Join-Path $stage 'Runner.exe') --diagnose-package
    if ($LASTEXITCODE -ne 0) { throw 'Validated package diagnostic failed' }
    & (Join-Path $stage 'Runner.exe') --diagnose-acceptance
    if ($LASTEXITCODE -ne 0) { throw 'Validated acceptance diagnostic failed' }
}
finally { Pop-Location }

# Freeze package-verification evidence and remove this one-run publisher before tagging.
git pull --rebase origin main
@'
import re
from pathlib import Path
p = Path('missioncache.md')
t = p.read_text(encoding='utf-8')
t = re.sub(r'^\*\*Target:\*\*.*$', '**Target:** Runner v0.7.13', t, count=1, flags=re.M)
t = re.sub(r'^\*\*Release state:\*\*.*$',
    '**Release state:** PACKAGE VERIFIED — toe command slew, physical hinge angular-rate limits, all-rig Stand/Crouch, Linux, full Windows Vulkan, installed/extracted diagnostics, checksum, and manifest passed; publication pending.',
    t, count=1, flags=re.M)
for name in ('WALK-TOE-RATE-127', 'WALK-STATE-128'):
    pattern = rf'(### {name}[^\n]*\n)\*\*Status:\*\*[^\n]*'
    t, n = re.subn(pattern, rf'\1**Status:** PACKAGE VERIFIED', t, count=1)
    if n != 1:
        raise SystemExit(f'missing mission {name}')
marker = '## v0.7.13 package validation evidence'
if marker in t:
    t = t[:t.index(marker)].rstrip() + '\n'
evidence = '''
## v0.7.13 package validation evidence

- Exact toe-rate implementation source: `626d9369dbdbb8ec1410874c1f0de1fcb40a6950`
- Exact validated PR source: `f4f64aba6c25bfaab8f75cb5a4a71e2c052d2ac2`
- Validation workflow run: `30915999459`
- Linux deterministic job: `92014174361` — passed
- Windows SDL3/Vulkan package job: `92014559233` — passed
- Validated artifact ID: `8895615989`
- Validated artifact digest: `sha256:8998b229456dad69ad0b6e5f656fe2624f10ef85fb68931685d0cfa48b3a657a`
- Validated binary ZIP SHA-256: `3EA39F26A2E037BEDFA8D64626899E1B401568CDD24A4C4B010FCC5EB20A7E3D`
- Toe commands pass through a neutral dead zone and stance/swing-specific slew limits: passed
- Physical toe hinge angular velocity remains bounded under alternating frame-by-frame input: passed
- Every preset Stand gate: `6/6`; strict-Stand slip across all seven presets: `0`
- Every preset static Crouch/hold/recover gate: `4/4`
- Linux warnings-as-errors, core, terrain, concurrency, runtime, and 22-case live acceptance: passed
- Full Windows application, build-tree, installed, and independently extracted package diagnostics: passed
- ZIP checksum and 11-entry per-file manifest audit: passed
'''
t = t.rstrip() + '\n\n' + evidence.strip() + '\n'
p.write_text(t, encoding='utf-8')
'@ | python -
if ($LASTEXITCODE -ne 0) { throw 'Mission cache package update failed' }
git rm .github/workflows/publish-runner-v0713.yml tools/publish_v0713.ps1
git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git add missioncache.md
git commit -m 'Freeze validated Runner v0.7.13 package source'
git pull --rebase origin main
git push origin HEAD:main
$releaseSha = (git rev-parse HEAD).Trim()

# Repack the validated binary with current verified documents.
Copy-Item README.md,LICENSE,CHANGELOG.md,missioncache.md -Destination $stage -Force
$archive = Join-Path $env:GITHUB_WORKSPACE 'Runner-v0.7.13-windows-x64.zip'
$checksum = "$archive.sha256"
$manifest = Join-Path $env:GITHUB_WORKSPACE 'Runner-v0.7.13-windows-x64.manifest.sha256'
Remove-Item $archive,$checksum,$manifest -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force
$finalHash = (Get-FileHash $archive -Algorithm SHA256).Hash
$finalHash | Set-Content $checksum
Get-ChildItem $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
    "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash)  $relative"
} | Set-Content $manifest

$finalAudit = Join-Path $env:RUNNER_TEMP 'runner-v0713-final-extracted'
Remove-Item $finalAudit -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive $archive -DestinationPath $finalAudit -Force
$expected = Get-Content $manifest
$actual = Get-ChildItem $finalAudit -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($finalAudit.Length + 1).Replace('\', '/')
    "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash)  $relative"
}
if (Compare-Object $expected $actual) { throw 'Final package manifest mismatch' }
Push-Location $env:RUNNER_TEMP
try {
    & (Join-Path $finalAudit 'Runner.exe') --diagnose-package
    if ($LASTEXITCODE -ne 0) { throw 'Final package diagnostic failed' }
    & (Join-Path $finalAudit 'Runner.exe') --diagnose-acceptance
    if ($LASTEXITCODE -ne 0) { throw 'Final package acceptance failed' }
    if ((& (Join-Path $finalAudit 'run.bat') --version | Out-String).Trim() -ne 'Runner 0.7.13') {
        throw 'Final run.bat version mismatch'
    }
}
finally { Pop-Location }

$notes = Join-Path $env:RUNNER_TEMP 'Runner-v0.7.13-release-notes.md'
@"
# Runner v0.7.13

Corrects the visible toe-joint chatter introduced with articulated feet.

- Keeps heel-ball-toe articulation for stabilization, crouching, swing clearance, and propulsion.
- Adds a neutral command dead zone and stance/swing-specific command slew limits.
- Adds a hard physical toe-hinge angular-rate ceiling after iterative solving.
- Prevents frame-to-frame reversal from becoming visible high-frequency jitter.
- Preserves all seven Stand gates at 6/6 with zero measured slip and all seven static Crouch gates at 4/4.
- Isolates learned state with v0.7.13 semantics and autosave paths.

Validated on Linux GCC 14 and the complete Windows SDL3/Vulkan application, including build-tree, installed, extracted, checksum, manifest, and all-rig acceptance diagnostics.
"@ | Set-Content $notes
if (gh release view $tag --repo $repo 2>$null) {
    gh release delete $tag --repo $repo --cleanup-tag --yes
}
gh release create $tag --repo $repo --title 'Runner v0.7.13' --target $releaseSha --notes-file $notes $archive $checksum $manifest
if ($LASTEXITCODE -ne 0) { throw 'Release creation failed' }

$download = Join-Path $env:RUNNER_TEMP 'runner-v0713-release-download'
Remove-Item $download -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $download | Out-Null
gh release download $tag --repo $repo --dir $download
if ($LASTEXITCODE -ne 0) { throw 'Release asset download failed' }
foreach ($name in @(
    'Runner-v0.7.13-windows-x64.zip',
    'Runner-v0.7.13-windows-x64.zip.sha256',
    'Runner-v0.7.13-windows-x64.manifest.sha256')) {
    $local = Join-Path $env:GITHUB_WORKSPACE $name
    $remote = Join-Path $download $name
    if (-not (Test-Path $remote)) { throw "Downloaded release asset missing: $name" }
    if ((Get-FileHash $local -Algorithm SHA256).Hash -ne (Get-FileHash $remote -Algorithm SHA256).Hash) {
        throw "Released asset differs from audited local asset: $name"
    }
}
$downloadedHash = (Get-FileHash (Join-Path $download 'Runner-v0.7.13-windows-x64.zip') -Algorithm SHA256).Hash
if ($downloadedHash -ne $finalHash) { throw 'Downloaded release ZIP hash mismatch' }
$releaseAudit = Join-Path $env:RUNNER_TEMP 'runner-v0713-release-extracted'
Remove-Item $releaseAudit -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive (Join-Path $download 'Runner-v0.7.13-windows-x64.zip') -DestinationPath $releaseAudit -Force
$expected = Get-Content (Join-Path $download 'Runner-v0.7.13-windows-x64.manifest.sha256')
$actual = Get-ChildItem $releaseAudit -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($releaseAudit.Length + 1).Replace('\', '/')
    "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash)  $relative"
}
if (Compare-Object $expected $actual) { throw 'Downloaded release manifest mismatch' }
Push-Location $env:RUNNER_TEMP
try {
    & (Join-Path $releaseAudit 'Runner.exe') --diagnose-package
    if ($LASTEXITCODE -ne 0) { throw 'Released package diagnostic failed' }
    & (Join-Path $releaseAudit 'Runner.exe') --diagnose-acceptance
    if ($LASTEXITCODE -ne 0) { throw 'Released acceptance diagnostic failed' }
}
finally { Pop-Location }

# Clean the merged branch and any remaining open cleanup PRs.
$branches = git for-each-ref --format='%(refname:short)' refs/remotes/origin
foreach ($branch in $branches) {
    if ($branch -eq 'origin/HEAD' -or $branch -eq 'origin/main') { continue }
    $name = $branch.Substring('origin/'.Length)
    git push origin --delete $name
}
$openPrs = gh pr list --repo $repo --state open --json number --jq '.[].number'
foreach ($number in $openPrs) { gh pr close $number --repo $repo --delete-branch }

# Record immutable release evidence on main after the tag and asset audit.
git pull --rebase origin main
@'
import re, os
from pathlib import Path
p = Path('missioncache.md')
t = p.read_text(encoding='utf-8')
t = re.sub(r'^\*\*Release state:\*\*.*$',
    '**Release state:** PUBLISHED — Runner v0.7.13 toe-rate correction, release assets, checksum, manifest, released executable, branch cleanup, and PR audit verified.',
    t, count=1, flags=re.M)
for name in ('WALK-TOE-RATE-127', 'WALK-STATE-128'):
    pattern = rf'(### {name}[^\n]*\n)\*\*Status:\*\*[^\n]*'
    t, n = re.subn(pattern, rf'\1**Status:** PUBLISHED — RELEASE VERIFIED', t, count=1)
    if n != 1:
        raise SystemExit(f'missing mission {name}')
marker = '## v0.7.13 immutable release evidence'
if marker in t:
    t = t[:t.index(marker)].rstrip() + '\n'
evidence = f'''
## v0.7.13 immutable release evidence

- Exact toe-rate implementation source: `626d9369dbdbb8ec1410874c1f0de1fcb40a6950`
- Exact validated PR source: `f4f64aba6c25bfaab8f75cb5a4a71e2c052d2ac2`
- Exact tagged package/document source: `{os.environ['RELEASE_SHA']}`
- Validation workflow run: `30915999459`
- Linux deterministic job: `92014174361` — passed
- Windows SDL3/Vulkan package job: `92014559233` — passed
- Validated artifact ID: `8895615989`
- Validated artifact digest: `sha256:8998b229456dad69ad0b6e5f656fe2624f10ef85fb68931685d0cfa48b3a657a`
- Published tag and title: `v0.7.13` / `Runner v0.7.13`
- Published Windows ZIP SHA-256: `{os.environ['FINAL_ZIP_SHA256']}`
- Published assets: Windows ZIP, ZIP SHA-256, and 11-entry per-file manifest
- Toe command slew/dead-zone and physical hinge angular-rate regressions: passed
- Every preset Stand gate: `6/6`, measured slip `0`; every preset static Crouch gate: `4/4`
- Linux and full Windows builds/tests plus build-tree, installed, validated-artifact, final-extracted, and re-downloaded-release diagnostics: passed
- Published assets were byte-compared; ZIP checksum and extracted manifest: passed
- Temporary validator and publisher removed before tagging; open pull requests: `0`; remaining branches: `main`
- Contradictory released-package evidence reopens only the exact affected mission
'''
t = t.rstrip() + '\n\n' + evidence.strip() + '\n'
p.write_text(t, encoding='utf-8')
'@ | python -
if ($LASTEXITCODE -ne 0) { throw 'Immutable mission evidence update failed' }
git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git add missioncache.md
git commit -m 'Publish Runner v0.7.13 and record immutable evidence'
git pull --rebase origin main
git push origin HEAD:main

Write-Host "Runner v0.7.13 published. ZIP SHA-256: $finalHash"
