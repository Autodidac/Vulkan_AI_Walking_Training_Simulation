$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root
$cache = Join-Path $root 'vcpkg-bincache'
New-Item -ItemType Directory -Force $cache | Out-Null
$env:VCPKG_DEFAULT_BINARY_CACHE = $cache

cmake --preset windows-release --fresh
if ($LASTEXITCODE -ne 0) { throw 'Windows configure failed' }
cmake --build --preset windows-release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
ctest --preset windows-release --output-on-failure -V --timeout 1800
if ($LASTEXITCODE -ne 0) { throw 'Windows tests failed' }

$exe = (Resolve-Path 'build/windows-release/Release/Runner.exe').Path
Push-Location $env:RUNNER_TEMP
try {
    $version = (& $exe --version | Out-String).Trim()
    if ($version -ne 'Runner 0.7.13') { throw "Build-tree version mismatch: $version" }
    & $exe --diagnose-package
    if ($LASTEXITCODE -ne 0) { throw 'Build-tree package diagnostic failed' }
    & $exe --diagnose-acceptance
    if ($LASTEXITCODE -ne 0) { throw 'Build-tree acceptance diagnostic failed' }
}
finally { Pop-Location }

$stage = Join-Path $env:RUNNER_TEMP 'Runner-v0.7.13-windows-x64'
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
cmake --install build/windows-release --config Release --prefix $stage
if ($LASTEXITCODE -ne 0) { throw 'Install failed' }
Copy-Item README.md, LICENSE, CHANGELOG.md, missioncache.md -Destination $stage -Force
Get-ChildItem build/windows-release/Release -Filter *.dll -File -ErrorAction SilentlyContinue |
    Copy-Item -Destination $stage -Force

$required = @(
    'Runner.exe', 'run.bat', 'README.md', 'CHANGELOG.md', 'missioncache.md',
    'assets/chicken.ppm', 'shaders/flat.vert.spv', 'shaders/flat.frag.spv'
)
foreach ($relative in $required) {
    if (-not (Test-Path (Join-Path $stage $relative))) {
        throw "Missing package file: $relative"
    }
}

Push-Location $env:RUNNER_TEMP
try {
    & (Join-Path $stage 'Runner.exe') --diagnose-package
    if ($LASTEXITCODE -ne 0) { throw 'Installed package diagnostic failed' }
    & (Join-Path $stage 'Runner.exe') --diagnose-acceptance
    if ($LASTEXITCODE -ne 0) { throw 'Installed acceptance diagnostic failed' }
    $runOutput = & (Join-Path $stage 'run.bat') --version
    if (($runOutput | Out-String).Trim() -ne 'Runner 0.7.13') {
        throw 'Installed run.bat version mismatch'
    }
}
finally { Pop-Location }

$archive = Join-Path $root 'Runner-v0.7.13-windows-x64.zip'
$checksum = "$archive.sha256"
$manifest = Join-Path $root 'Runner-v0.7.13-windows-x64.manifest.sha256'
Remove-Item $archive, $checksum, $manifest -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force
(Get-FileHash $archive -Algorithm SHA256).Hash | Set-Content $checksum
Get-ChildItem $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
    "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash)  $relative"
} | Set-Content $manifest

$audit = Join-Path $env:RUNNER_TEMP 'runner-v0713-extracted'
Remove-Item $audit -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive $archive -DestinationPath $audit -Force
Push-Location $env:RUNNER_TEMP
try {
    & (Join-Path $audit 'Runner.exe') --diagnose-package
    if ($LASTEXITCODE -ne 0) { throw 'Extracted package diagnostic failed' }
    & (Join-Path $audit 'Runner.exe') --diagnose-acceptance
    if ($LASTEXITCODE -ne 0) { throw 'Extracted acceptance diagnostic failed' }
    $runOutput = & (Join-Path $audit 'run.bat') --version
    if (($runOutput | Out-String).Trim() -ne 'Runner 0.7.13') {
        throw 'Extracted run.bat version mismatch'
    }
}
finally { Pop-Location }

Write-Host "Runner v0.7.13 Windows package validation passed: $archive"
