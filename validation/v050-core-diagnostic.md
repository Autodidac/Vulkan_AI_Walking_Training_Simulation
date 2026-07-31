# EpochRunner v0.5 core diagnostic

Source: 707cee9a2b43a38631b4bda2438e5181f56ed4e5

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
-- Configuring done (13.3s)
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
  ppo_network.cpp
  training_checkpoint.cpp
  simulation.cpp
  autonomy_persistence.cpp
  autonomy_commands.cpp
  autonomy_runtime.cpp
  autonomy_curriculum.cpp
  Compiling...
  ppo_trainer.cpp
  training_checkpoint.cpp
  ppo_network.cpp
  simulation.cpp
  autonomy_curriculum.cpp
  autonomy_commands.cpp
  autonomy_persistence.cpp
  autonomy_runtime.cpp
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
1/1 Test #1: EpochRunner.Core .................   Passed    0.51 sec

100% tests passed out of 1

Total Test time (real) =   0.55 sec

`
