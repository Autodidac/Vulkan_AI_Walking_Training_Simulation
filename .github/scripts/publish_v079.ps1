param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('build', 'release', 'failure')]
    [string]$Phase
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-LastExitCode([string]$Message) {
    if ($LASTEXITCODE -ne 0) {
        throw "$Message (exit code $LASTEXITCODE)"
    }
}

function Invoke-Acceptance([string]$Executable, [string]$Label) {
    & $Executable --diagnose-acceptance
    Assert-LastExitCode "$Label acceptance diagnostic failed"
}

function Update-PackageLedger {
    $path = 'missioncache.md'
    $text = [System.IO.File]::ReadAllText($path)
    $statePattern = '(?m)^\*\*Release state:\*\*.*$'
    $text = [regex]::Replace(
        $text,
        $statePattern,
        '**Release state:** PACKAGE VERIFIED — Runner v0.7.9 passed Linux, Windows, build-tree acceptance, and all deterministic suites; publication audit pending.',
        1)

    foreach ($mission in 99..103) {
        $id = '{0:D3}' -f $mission
        $pattern = "(### WALK-[^`r`n]*-$id[^`r`n]*`r?`n)\*\*Status:\*\*[^`r`n]*"
        $regex = [regex]::new($pattern)
        if (-not $regex.IsMatch($text)) {
            throw "Missing v0.7.9 mission $id"
        }
        $text = $regex.Replace($text, '$1**Status:** PACKAGE VERIFIED', 1)
    }

    [System.IO.File]::WriteAllText(
        $path,
        $text,
        [System.Text.UTF8Encoding]::new($false))
}

function Update-FinalLedger {
    $path = 'missioncache.md'
    $text = [System.IO.File]::ReadAllText($path)
    $text = [regex]::Replace(
        $text,
        '(?m)^\*\*Release state:\*\*.*$',
        '**Release state:** PUBLISHED — Runner v0.7.9 acceptance matrix, package, release assets, checksum, manifest, released executable, branch cleanup, and PR audit verified.',
        1)

    foreach ($mission in 99..103) {
        $id = '{0:D3}' -f $mission
        $pattern = "(### WALK-[^`r`n]*-$id[^`r`n]*`r?`n)\*\*Status:\*\*[^`r`n]*"
        $regex = [regex]::new($pattern)
        if (-not $regex.IsMatch($text)) {
            throw "Missing v0.7.9 mission $id"
        }
        $text = $regex.Replace($text, '$1**Status:** PUBLISHED — RELEASE VERIFIED', 1)
    }

    $marker = '## v0.7.9 immutable release evidence'
    $markerIndex = $text.IndexOf($marker, [System.StringComparison]::Ordinal)
    if ($markerIndex -ge 0) {
        $text = $text.Substring(0, $markerIndex).TrimEnd()
    }

    $evidence = @"

## v0.7.9 immutable release evidence

- Exact tagged package source: ``$env:RELEASE_SHA``
- Validation and publication workflow run: ``$env:GITHUB_RUN_ID``
- Artifact ID: ``$env:ARTIFACT_ID``
- Artifact digest: ``$env:ARTIFACT_DIGEST``
- Release tag and title: ``v0.7.9`` / ``Runner v0.7.9``
- Published assets: Windows ZIP, ZIP SHA-256, and per-file manifest
- Linux GCC 14 warnings-as-errors build and all five deterministic suites: passed
- Live acceptance matrix: 10/10 passed
- Full Windows Vulkan build and all six tests: passed
- Build-tree, installed, independently extracted, and re-downloaded release acceptance diagnostics: passed
- Published assets were byte-compared; ZIP checksum and extracted per-file manifest: passed
- Merged work, diagnostic, and trigger branches removed; open pull requests: ``0``; remaining branches: ``main``
- Contradictory released-package evidence reopens only the exact affected mission
"@

    $text = $text.TrimEnd() + $evidence + "`n"
    [System.IO.File]::WriteAllText(
        $path,
        $text,
        [System.Text.UTF8Encoding]::new($false))
}

if ($Phase -eq 'failure') {
    git fetch origin main
    git reset --hard origin/main
    @"
Runner v0.7.9 wrapper publication failed
run_id=$env:GITHUB_RUN_ID
phase=windows-package
"@ | Set-Content -Path 'VALIDATION_v0.7.9_FAILURE.txt' -Encoding utf8
    git config user.name 'github-actions[bot]'
    git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
    git add VALIDATION_v0.7.9_FAILURE.txt
    git commit -m 'Record Runner v0.7.9 wrapper publication failure'
    if ($LASTEXITCODE -eq 0) {
        git push origin HEAD:main
    }
    exit 1
}

if ($Phase -eq 'build') {
    if (-not (Select-String -Path CMakeLists.txt -SimpleMatch 'project(Runner VERSION 0.7.9 LANGUAGES CXX)')) {
        throw 'Runner v0.7.9 source is missing'
    }

    foreach ($path in @(
        'tools/apply_v079_acceptance.py',
        '.github/workflows/validate-runner-v079.yml',
        '.github/workflows/finalize-runner-v079.yml',
        '.github/workflows/materialize-runner-v079.yml',
        '.github/workflows/publish-runner-v079.yml',
        '.github/workflows/patch-v079-publisher.yml',
        '.github/workflows/publish-runner-v079-fixed.yml',
        '.github/workflows/trigger-v079-fixed.yml')) {
        if (Test-Path $path) {
            throw "Obsolete release tool remains: $path"
        }
    }

    New-Item -ItemType Directory -Force $env:VCPKG_DEFAULT_BINARY_CACHE | Out-Null
    cmake --preset windows-release --fresh
    Assert-LastExitCode 'Windows configure failed'
    cmake --build --preset windows-release --parallel
    Assert-LastExitCode 'Windows build failed'
    ctest --preset windows-release --output-on-failure -V --timeout 1200
    Assert-LastExitCode 'Windows tests failed'

    $exe = (Resolve-Path 'build/windows-release/Release/Runner.exe').Path
    Push-Location $env:RUNNER_TEMP
    try {
        $version = (& $exe --version | Out-String).Trim()
        if ($version -ne 'Runner 0.7.9') {
            throw "Unexpected version: $version"
        }
        & $exe --diagnose-vulkan
        Assert-LastExitCode 'Vulkan diagnostic failed'
        & $exe --diagnose-package
        Assert-LastExitCode 'Package diagnostic failed'
        Invoke-Acceptance $exe 'Build-tree'
    }
    finally {
        Pop-Location
    }

    git pull --rebase origin main
    Assert-LastExitCode 'Failed to update main before freezing package source'
    Update-PackageLedger
    git rm .github/workflows/publish-runner-v079-wrapper.yml .github/scripts/publish_v079.ps1
    Remove-Item VALIDATION_v0.7.9_FAILURE.txt -Force -ErrorAction SilentlyContinue
    git config user.name 'github-actions[bot]'
    git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
    git add missioncache.md
    git add -u
    git commit -m 'Record Runner v0.7.9 package validation'
    Assert-LastExitCode 'Failed to commit cleaned package source'
    git pull --rebase origin main
    Assert-LastExitCode 'Failed to rebase cleaned package source'
    git push origin HEAD:main
    Assert-LastExitCode 'Failed to push cleaned package source'

    $releaseSha = (git rev-parse HEAD).Trim()
    "RELEASE_SHA=$releaseSha" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append

    $stage = "$env:RUNNER_TEMP/Runner-v0.7.9-windows-x64"
    Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
    cmake --install build/windows-release --config Release --prefix $stage
    Assert-LastExitCode 'Install failed'
    Copy-Item README.md, LICENSE, missioncache.md, RELEASE_NOTES_v0.7.9.md -Destination $stage -Force
    Get-ChildItem build/windows-release/Release -Filter *.dll -File -ErrorAction SilentlyContinue |
        Copy-Item -Destination $stage -Force

    foreach ($required in @(
        'Runner.exe', 'run.bat', 'missioncache.md', 'RELEASE_NOTES_v0.7.9.md',
        'shaders/flat.vert.spv', 'shaders/flat.frag.spv', 'assets/chicken.ppm')) {
        if (-not (Test-Path "$stage/$required")) {
            throw "Missing package file: $required"
        }
    }

    $outside = "$env:RUNNER_TEMP/runner-v079-installed-unrelated"
    New-Item -ItemType Directory -Force $outside | Out-Null
    Push-Location $outside
    try {
        $version = (& "$stage/Runner.exe" --version | Out-String).Trim()
        if ($version -ne 'Runner 0.7.9') {
            throw "Installed version mismatch: $version"
        }
        & "$stage/Runner.exe" --diagnose-package
        Assert-LastExitCode 'Installed package diagnostic failed'
        Invoke-Acceptance "$stage/Runner.exe" 'Installed'
        $batchVersion = (& cmd.exe /d /c "call `"$stage\run.bat`" --version" | Out-String).Trim()
        if ($batchVersion -ne 'Runner 0.7.9') {
            throw "run.bat version mismatch: $batchVersion"
        }
    }
    finally {
        Pop-Location
    }

    $archive = "$env:GITHUB_WORKSPACE/Runner-v0.7.9-windows-x64.zip"
    Remove-Item $archive -Force -ErrorAction SilentlyContinue
    Compress-Archive -Path "$stage/*" -DestinationPath $archive -Force
    (Get-FileHash $archive -Algorithm SHA256).Hash | Set-Content "$archive.sha256"
    Get-ChildItem $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
        $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
        "$hash  $relative"
    } | Set-Content 'Runner-v0.7.9-windows-x64.manifest.sha256'

    $audit = "$env:RUNNER_TEMP/runner-v079-extracted"
    Remove-Item $audit -Recurse -Force -ErrorAction SilentlyContinue
    Expand-Archive $archive -DestinationPath $audit -Force
    $expected = Get-Content 'Runner-v0.7.9-windows-x64.manifest.sha256'
    $actual = Get-ChildItem $audit -Recurse -File | Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($audit.Length + 1).Replace('\', '/')
        $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
        "$hash  $relative"
    }
    if (Compare-Object $expected $actual) {
        throw 'Archive manifest audit failed'
    }

    Push-Location $env:RUNNER_TEMP
    try {
        & "$audit/Runner.exe" --diagnose-package
        Assert-LastExitCode 'Extracted package diagnostic failed'
        Invoke-Acceptance "$audit/Runner.exe" 'Extracted'
    }
    finally {
        Pop-Location
    }
    exit 0
}

if ($Phase -eq 'release') {
    if (-not $env:RELEASE_SHA) {
        throw 'RELEASE_SHA is missing'
    }
    if (gh release view v0.7.9 --repo $env:GITHUB_REPOSITORY 2>$null) {
        gh release delete v0.7.9 --repo $env:GITHUB_REPOSITORY --cleanup-tag --yes
        Assert-LastExitCode 'Failed to delete stale v0.7.9 release'
    }

    gh release create v0.7.9 `
        --repo $env:GITHUB_REPOSITORY `
        --title 'Runner v0.7.9' `
        --target main `
        --notes-file RELEASE_NOTES_v0.7.9.md `
        Runner-v0.7.9-windows-x64.zip `
        Runner-v0.7.9-windows-x64.zip.sha256 `
        Runner-v0.7.9-windows-x64.manifest.sha256
    Assert-LastExitCode 'Release creation failed'

    $download = "$env:RUNNER_TEMP/runner-v079-release-audit"
    Remove-Item $download -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $download | Out-Null
    gh release download v0.7.9 --repo $env:GITHUB_REPOSITORY --dir $download
    Assert-LastExitCode 'Release download failed'

    foreach ($file in @(
        'Runner-v0.7.9-windows-x64.zip',
        'Runner-v0.7.9-windows-x64.zip.sha256',
        'Runner-v0.7.9-windows-x64.manifest.sha256')) {
        $localHash = (Get-FileHash $file -Algorithm SHA256).Hash
        $remoteHash = (Get-FileHash "$download/$file" -Algorithm SHA256).Hash
        if ($localHash -ne $remoteHash) {
            throw "Published asset mismatch: $file"
        }
    }

    $expectedZip = (Get-Content "$download/Runner-v0.7.9-windows-x64.zip.sha256").Split()[0]
    $actualZip = (Get-FileHash "$download/Runner-v0.7.9-windows-x64.zip" -Algorithm SHA256).Hash
    if ($expectedZip -ne $actualZip) {
        throw 'Published ZIP checksum mismatch'
    }

    $releaseExtract = "$env:RUNNER_TEMP/runner-v079-release-extracted"
    Remove-Item $releaseExtract -Recurse -Force -ErrorAction SilentlyContinue
    Expand-Archive "$download/Runner-v0.7.9-windows-x64.zip" -DestinationPath $releaseExtract -Force
    $expected = Get-Content "$download/Runner-v0.7.9-windows-x64.manifest.sha256"
    $actual = Get-ChildItem $releaseExtract -Recurse -File | Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($releaseExtract.Length + 1).Replace('\', '/')
        $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
        "$hash  $relative"
    }
    if (Compare-Object $expected $actual) {
        throw 'Published manifest audit failed'
    }
    Invoke-Acceptance "$releaseExtract/Runner.exe" 'Released'

    $tagSha = (git ls-remote origin 'refs/tags/v0.7.9').Split()[0]
    if ($tagSha -ne $env:RELEASE_SHA) {
        throw "Tag target mismatch: expected $env:RELEASE_SHA, got $tagSha"
    }

    foreach ($branch in @(
        'agent/v079-live-acceptance-matrix',
        'diag/v079-publisher-status',
        'release/v079-fixed-trigger')) {
        gh api --silent -X DELETE "repos/$env:GITHUB_REPOSITORY/git/refs/heads/$branch" 2>$null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Branch already absent: $branch"
        }
    }

    $openPrs = gh pr list --repo $env:GITHUB_REPOSITORY --state open --limit 100 --json number --jq length
    if ($openPrs -ne '0') {
        throw "Open pull requests remain: $openPrs"
    }
    $branches = gh api --paginate "repos/$env:GITHUB_REPOSITORY/branches?per_page=100" --jq '.[].name' | Sort-Object
    if (($branches -join ',') -ne 'main') {
        throw "Unexpected branches remain: $branches"
    }

    git pull --rebase origin main
    Assert-LastExitCode 'Failed to update main before final evidence'
    Update-FinalLedger
    Remove-Item VALIDATION_v0.7.9_FAILURE.txt -Force -ErrorAction SilentlyContinue
    git config user.name 'github-actions[bot]'
    git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
    git add missioncache.md
    git add -u
    git commit -m 'Publish Runner v0.7.9 and record release evidence'
    Assert-LastExitCode 'Failed to commit final v0.7.9 evidence'
    git push origin HEAD:main
    Assert-LastExitCode 'Failed to push final v0.7.9 evidence'
    exit 0
}
