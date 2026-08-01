@echo off
setlocal EnableExtensions

rem Always operate relative to this script, not the caller's working directory.
cd /d "%~dp0"

rem The same launcher works both in an extracted release and in the source tree.
set "EPOCHRUNNER_EXE=%~dp0EpochRunner.exe"
if exist "%EPOCHRUNNER_EXE%" goto launch

set "EPOCHRUNNER_EXE=%~dp0build\windows-release\Release\EpochRunner.exe"
if exist "%EPOCHRUNNER_EXE%" goto launch

echo EpochRunner has not been built yet. Building the Windows Release target...
echo.

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake is not available on PATH.
    goto failed
)

rem Use Adam's normal vcpkg location when VCPKG_ROOT is not already set.
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

if not exist "%EPOCHRUNNER_EXE%" (
    echo ERROR: The build completed without producing:
    echo        %EPOCHRUNNER_EXE%
    goto failed
)

:launch
for %%I in ("%EPOCHRUNNER_EXE%") do set "EPOCHRUNNER_DIR=%%~dpI"
pushd "%EPOCHRUNNER_DIR%"
"%EPOCHRUNNER_EXE%" %*
set "EPOCHRUNNER_RESULT=%ERRORLEVEL%"
popd

if not "%EPOCHRUNNER_RESULT%"=="0" (
    echo.
    echo EpochRunner exited with code %EPOCHRUNNER_RESULT%.
    pause
)
exit /b %EPOCHRUNNER_RESULT%

:build_failed
echo.
echo ERROR: EpochRunner failed to configure or build.

:failed
echo.
pause
exit /b 1
