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

echo Configuring Visual Studio 2022 x64 with vcpkg manifest mode...
cmake --preset windows-release --fresh || exit /b 1
cmake --build --preset windows-release || exit /b 1
ctest --preset windows-release || exit /b 1

echo Built: build\windows-release\Release\EpochRunner.exe
