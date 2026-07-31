# EpochRunner v0.5.0 release validation

Build source: 8f354aea462810e2c7a85a84696735afbd1e3365
Compiler: Visual Studio 2026 / MSVC 19.51
Configuration: Windows x64 Release

## Version check
`	ext
EpochRunner 0.5.0

`

## Vulkan diagnostic
`	ext
EpochRunner 0.5.0 SDL3 Vulkan diagnostic passed: backend enabled, video_driver=windows; the CI runner has no Vulkan presentation surface (Installed Vulkan doesn't implement the VK_KHR_surface extension)

`

## Core and concurrency tests
`	ext
UpdateCTestConfiguration  from :D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/DartConfiguration.tcl
Test project D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/windows-release
Constructing a list of tests
Done constructing a list of tests
Updating test list for fixtures
Added 0 tests to meet fixture requirements
Checking test dependency graph...
Checking test dependency graph end
test 1
    Start 1: EpochRunner.Core

1: Test command: D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\windows-release\Release\EpochRunnerCoreTests.exe
1: Working Directory: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/windows-release
1: Test timeout computed to be: 10000000
1: EpochRunner v0.5.0 concurrency, gait, and rig-edit tests passed
1/2 Test #1: EpochRunner.Core ...................   Passed    0.53 sec
test 2
    Start 2: EpochRunner.ConcurrencyBenchmark

2: Test command: D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\windows-release\Release\EpochRunnerConcurrencyBenchmark.exe
2: Working Directory: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/windows-release
2: Test timeout computed to be: 30
2: mode=1 workers=1 updates=6 updates_per_second=1.49969 environment_steps_per_second=12285.5
2: mode=2 workers=2 updates=12 updates_per_second=2.98919 environment_steps_per_second=24487.4
2: mode=4 workers=2 updates=13 updates_per_second=3.24261 environment_steps_per_second=26563.5
2: EpochRunner v0.5 speed-mode throughput benchmark passed
2/2 Test #2: EpochRunner.ConcurrencyBenchmark ...   Passed   13.07 sec

100% tests passed out of 2

Total Test time (real) =  13.62 sec

`

Package SHA256: EBB7EC63A6B21979962311C37F4D1F479D762DD0EAB3BD908DC457EE7F8C3FFE
