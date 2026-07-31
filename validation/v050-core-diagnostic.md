# EpochRunner v0.5 core diagnostic

Source: d0da06a846dee8ddd771caeb7392ab7b8db10f0c

Configure: success
Build: success
Test: success

## Configure log
`	ext
-- The CXX compiler identification is MSVC 19.51.36248.0
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
-- Configuring done (14.3s)
-- Generating done (0.1s)
-- Build files have been written to: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/core-diagnostic

`

## Build log
`	ext
MSBuild version 18.7.8+1ac568fee for .NET Framework

  1>Checking Build System
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt
  Scanning sources for module dependencies...
  ppo_trainer.cpp
  ppo_parallel.cpp
  ppo_network.cpp
  simulation.cpp
  training_checkpoint.cpp
  autonomy_runtime.cpp
  autonomy_curriculum.cpp
  autonomy_commands.cpp
  autonomy_persistence.cpp
  Compiling...
  ppo_parallel.cpp
  simulation.cpp
  ppo_trainer.cpp
  ppo_network.cpp
  autonomy_commands.cpp
  training_checkpoint.cpp
  autonomy_runtime.cpp
  autonomy_curriculum.cpp
  autonomy_persistence.cpp
  EpochRunnerCore.vcxproj -> D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\core-diagnostic\Release\EpochRunnerCore.lib
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
Test project D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/core-diagnostic
    Start 1: EpochRunner.Core
1/1 Test #1: EpochRunner.Core .................   Passed    0.53 sec

100% tests passed out of 1

Total Test time (real) =   0.58 sec

`
