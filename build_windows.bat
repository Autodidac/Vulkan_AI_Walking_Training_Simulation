@echo off
setlocal EnableExtensions

if not defined VCPKG_ROOT (
  if exist "%USERPROFILE%\source\repos\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=%USERPROFILE%\source\repos\vcpkg"
  )
)

if not defined VCPKG_ROOT (
  echo VCPKG_ROOT is not set and vcpkg was not found at:
  echo   %USERPROFILE%\source\repos\vcpkg
  exit /b 1
)

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
  echo Invalid VCPKG_ROOT: %VCPKG_ROOT%
  exit /b 1
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
  echo Bootstrapping vcpkg...
  call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics || exit /b 1
)

set "RUNNER_PRESET=windows-release"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
  set "VS2026_PATH="
  set "VS2022_PATH="

  for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -version "[18.0,19.0)" -property installationPath`) do set "VS2026_PATH=%%I"
  if not defined VS2026_PATH (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -version "[17.0,18.0)" -property installationPath`) do set "VS2022_PATH=%%I"
    if defined VS2022_PATH set "RUNNER_PRESET=windows-vs2022-release"
  )
)

echo Configuring %RUNNER_PRESET% with vcpkg manifest mode...
cmake --preset %RUNNER_PRESET% --fresh || exit /b 1
cmake --build --preset %RUNNER_PRESET% || exit /b 1
ctest --preset %RUNNER_PRESET% || exit /b 1

echo Built: build\%RUNNER_PRESET%\Release\Runner.exe
