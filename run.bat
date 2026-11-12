@echo off
setlocal EnableExtensions

rem Always operate relative to this script, not the caller's working directory.
cd /d "%~dp0"

rem In a source checkout, never let a stale root-level executable override the
rem current build.  In an extracted release, CMakeLists.txt is absent and the
rem packaged executable beside this launcher is authoritative.
if exist "%~dp0CMakeLists.txt" goto source_tree

set "RUNNER_EXE=%~dp0Runner.exe"
if exist "%RUNNER_EXE%" goto launch

echo ERROR: The packaged Runner.exe is missing beside run.bat.
goto failed

:source_tree
set "RUNNER_EXE=%~dp0build\windows-release\Release\Runner.exe"
if exist "%RUNNER_EXE%" goto launch

echo Epoch2DWalkEngine Runner example has not been built yet. Building the Windows Release target...
echo.

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake is not available on PATH.
    goto failed
)

if not defined VCPKG_ROOT if exist "%USERPROFILE%\source\repos\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=%USERPROFILE%\source\repos\vcpkg"
)
if not defined VCPKG_ROOT if exist "%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
)

cmake --preset windows-release
if errorlevel 1 goto build_failed
cmake --build --preset windows-release --parallel
if errorlevel 1 goto build_failed
if not exist "%RUNNER_EXE%" (
    echo ERROR: The build completed without producing:
    echo        %RUNNER_EXE%
    goto failed
)

:launch
for %%I in ("%RUNNER_EXE%") do set "RUNNER_DIR=%%~dpI"
pushd "%RUNNER_DIR%"
"%RUNNER_EXE%" %*
set "RUNNER_RESULT=%ERRORLEVEL%"
popd
if not "%RUNNER_RESULT%"=="0" (
    echo.
    echo Epoch2DWalkEngine Runner example exited with code %RUNNER_RESULT%.
    rem Diagnostics and CI launch this file with arguments. Never turn a
    rem non-interactive failure into an unbounded pause; retain the friendly
    rem prompt only for a user who opened the launcher without arguments.
    if "%~1"=="" pause
)
exit /b %RUNNER_RESULT%

:build_failed
echo.
echo ERROR: Epoch2DWalkEngine Runner example failed to configure or build.

:failed
echo.
if "%~1"=="" pause
exit /b 1
