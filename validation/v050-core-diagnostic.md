# EpochRunner v0.5 core diagnostic

Source: 5cf9cf0a66dbb3def04896c60cf9ffe1f7148299

Configure: success
Build: success
Test: success

## Configure log
`	ext
-- The CXX compiler identification is MSVC 19.51.36252.0
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD - Failed
-- Looking for pthread_create in pthreads
-- Looking for pthread_create in pthreads - not found
-- Looking for pthread_create in pthread
-- Looking for pthread_create in pthread - not found
-- Found Threads: TRUE
-- Configuring done (12.6s)
-- Generating done (0.1s)
-- Build files have been written to: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/core-diagnostic

`

## Build log
`	ext
MSBuild version 18.8.2+ce25c0108 for .NET Framework

  1>Checking Build System
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt
  Scanning sources for module dependencies...
  ppo_trainer.cpp
  ppo_parallel.cpp
  ppo_network.cpp
  simulation.cpp
  autonomy_runtime.cpp
  autonomy_curriculum.cpp
  training_checkpoint.cpp
  autonomy_commands.cpp
  autonomy_persistence.cpp
  Compiling...
  simulation.cpp
  ppo_trainer.cpp
  ppo_parallel.cpp
  ppo_network.cpp
  training_checkpoint.cpp
  autonomy_runtime.cpp
  autonomy_curriculum.cpp
  autonomy_commands.cpp
  autonomy_persistence.cpp
  EpochRunnerCore.vcxproj -> D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\core-diagnostic\Release\EpochRunnerCore.lib
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt
  Scanning sources for module dependencies...
  concurrency_benchmark.cpp
  Compiling...
  concurrency_benchmark.cpp
  EpochRunnerConcurrencyBenchmark.vcxproj -> D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\core-diagnostic\Release\EpochRunnerConcurrencyBenchmark.exe
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt
  Scanning sources for module dependencies...
  core_tests.cpp
  Compiling...
  core_tests.cpp
  EpochRunnerCoreTests.vcxproj -> D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\core-diagnostic\Release\EpochRunnerCoreTests.exe
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt

`

## Test log
`	ext
UpdateCTestConfiguration  from :D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/core-diagnostic/DartConfiguration.tcl
Parse Config file:D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/core-diagnostic/DartConfiguration.tcl
Test project D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/core-diagnostic
Constructing a list of tests
Done constructing a list of tests
Updating test list for fixtures
Added 0 tests to meet fixture requirements
Checking test dependency graph...
Checking test dependency graph end
test 1
    Start 1: EpochRunner.Core

1: Test command: D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\core-diagnostic\Release\EpochRunnerCoreTests.exe
1: Working Directory: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/core-diagnostic
1: Test timeout computed to be: 1500
1: EpochRunner v0.5.0 concurrency, gait, and rig-edit tests passed
1/2 Test #1: EpochRunner.Core ...................   Passed    0.52 sec
test 2
    Start 2: EpochRunner.ConcurrencyBenchmark

2: Test command: D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\core-diagnostic\Release\EpochRunnerConcurrencyBenchmark.exe
2: Working Directory: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/core-diagnostic
2: Test timeout computed to be: 30
2: mode=1 workers=1 updates=6 updates_per_second=1.49689 environment_steps_per_second=12262.5
2: mode=2 workers=2 updates=12 updates_per_second=2.99513 environment_steps_per_second=24536.1
2: mode=4 workers=2 updates=12 updates_per_second=2.99891 environment_steps_per_second=24567.1
2: EpochRunner v0.5 speed-mode throughput benchmark passed
2/2 Test #2: EpochRunner.ConcurrencyBenchmark ...   Passed   12.89 sec

100% tests passed out of 2

Total Test time (real) =  13.44 sec

`
