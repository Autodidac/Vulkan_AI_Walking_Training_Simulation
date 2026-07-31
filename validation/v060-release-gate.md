# EpochRunner v0.6.0 release gate

Release candidate: `1a9cacba2ed0d396166a069567653cd8eedef501`

- vcpkg bootstrap: success
- Full configure: success
- Full Vulkan build: success
- Core, course, recovery, and concurrency tests: success
- Executable version: success
- SDL3/Vulkan diagnostic: success
- Install staging: success

## vcpkg bootstrap

```text
Downloading https://github.com/microsoft/vcpkg-tool/releases/download/2026-04-08/vcpkg.exe -> D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\vcpkg\vcpkg.exe... done.
Validating signature... done.

vcpkg package management program version 2026-04-08-e0612b42ce44e55a0e630f2ee9d3c533a63d8bc1

See LICENSE.txt for license information.

```

## Full configure

```text
-- Running vcpkg install
Detecting compiler hash for triplet x64-windows...
Compiler found: C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe
The following packages will be built and installed:
  * glslang:x64-windows@16.3.0#1
    sdl3[core,vulkan]:x64-windows@3.4.8#1
    shaderc:x64-windows@2026.2
  * spirv-headers:x64-windows@1.4.350.0
  * spirv-tools:x64-windows@1.4.350.0
  * vcpkg-cmake:x64-windows@2024-04-23
  * vcpkg-cmake-config:x64-windows@2024-05-23
  * vulkan-headers:x64-windows@1.4.350.0
    vulkan-loader:x64-windows@1.4.350.0
Additional packages (*) will be modified to complete this operation.
Restored 0 package(s) from D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\vcpkg-bincache in 121 us. Use --debug to see more details.
Installing 1/9 vcpkg-cmake-config:x64-windows@2024-05-23...
vcpkg-cmake-config:x64-windows@2024-05-23 package ABI: 87a1d35b793401fa55722beaf3be5c5715db5852fdc95710c67e0d13027afede
Building vcpkg-cmake-config:x64-windows@2024-05-23...
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vcpkg-cmake-config_x64-windows/share/vcpkg-cmake-config/vcpkg_cmake_config_fixup.cmake
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vcpkg-cmake-config_x64-windows/share/vcpkg-cmake-config/vcpkg-port-config.cmake
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vcpkg-cmake-config_x64-windows/share/vcpkg-cmake-config/copyright
-- Skipping post-build validation due to VCPKG_POLICY_EMPTY_PACKAGE
Starting submission of vcpkg-cmake-config:x64-windows@2024-05-23 to 1 binary cache(s) in the background
Elapsed time to handle vcpkg-cmake-config:x64-windows: 63.2 ms
Installing 2/9 vcpkg-cmake:x64-windows@2024-04-23...
vcpkg-cmake:x64-windows@2024-04-23 package ABI: 5e3801af10ae9816e3bfe731adabc0371c3d3fbf0c38164f872b2334eab7e0a4
Building vcpkg-cmake:x64-windows@2024-04-23...
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vcpkg-cmake_x64-windows/share/vcpkg-cmake/vcpkg_cmake_configure.cmake
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vcpkg-cmake_x64-windows/share/vcpkg-cmake/vcpkg_cmake_build.cmake
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vcpkg-cmake_x64-windows/share/vcpkg-cmake/vcpkg_cmake_install.cmake
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vcpkg-cmake_x64-windows/share/vcpkg-cmake/vcpkg-port-config.cmake
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vcpkg-cmake_x64-windows/share/vcpkg-cmake/copyright
-- Performing post-build validation
Starting submission of vcpkg-cmake:x64-windows@2024-04-23 to 1 binary cache(s) in the background
Elapsed time to handle vcpkg-cmake:x64-windows: 91.5 ms
Installing 3/9 sdl3[core,vulkan]:x64-windows@3.4.8#1...
sdl3[core,vulkan]:x64-windows@3.4.8#1 package ABI: f1e23574f436b0fcf1e1461a102e8ef1e611b93feadea600146943ddcb003aef
Building sdl3[core,vulkan]:x64-windows@3.4.8#1...
Downloading https://github.com/libsdl-org/SDL/archive/release-3.4.8.tar.gz -> libsdl-org-SDL-release-3.4.8.tar.gz
Successfully downloaded libsdl-org-SDL-release-3.4.8.tar.gz
-- Extracting source D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/libsdl-org-SDL-release-3.4.8.tar.gz
-- Applying patch fix-freebsd.patch
-- Using source at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/buildtrees/sdl3/src/ease-3.4.8-4fc18f166d.clean
-- Configuring x64-windows
CMake Warning at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg_installed/x64-windows/share/vcpkg-cmake/vcpkg_cmake_configure.cmake:344 (message):
  The following variables are not used in CMakeLists.txt:

      SDL_UNIX_CONSOLE_BUILD

  Please recheck them and remove the unnecessary options from the
  `vcpkg_cmake_configure` call.

  If these options should still be passed for whatever reason, please use the
  `MAYBE_UNUSED_VARIABLES` argument.
Call Stack (most recent call first):
  buildtrees/versioning_/versions/sdl3/9c4f5fd6e369cc9699cc5176eef458d64b07912a/portfile.cmake:58 (vcpkg_cmake_configure)
  scripts/ports.cmake:206 (include)


-- Building x64-windows-dbg
-- Building x64-windows-rel
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/sdl3_x64-windows/lib/pkgconfig/sdl3.pc
Downloading msys2-mingw-w64-x86_64-pkgconf-1~2.5.1-1-any.pkg.tar.zst, trying https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-pkgconf-1~2.5.1-1-any.pkg.tar.zst
Successfully downloaded msys2-mingw-w64-x86_64-pkgconf-1~2.5.1-1-any.pkg.tar.zst
Downloading msys2-msys2-runtime-3.6.5-1-x86_64.pkg.tar.zst, trying https://mirror.msys2.org/msys/x86_64/msys2-runtime-3.6.5-1-x86_64.pkg.tar.zst
Successfully downloaded msys2-msys2-runtime-3.6.5-1-x86_64.pkg.tar.zst
-- Using msys root at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/tools/msys2/3e71d1f8e22ab23f
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/sdl3_x64-windows/debug/lib/pkgconfig/sdl3.pc
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/sdl3_x64-windows/share/sdl3/usage
-- Performing post-build validation
Starting submission of sdl3[core,vulkan]:x64-windows@3.4.8#1 to 1 binary cache(s) in the background
Elapsed time to handle sdl3:x64-windows: 1.1 min
Completed submission of vcpkg-cmake-config:x64-windows@2024-05-23 to 1 binary cache(s) in 257 ms
Completed submission of vcpkg-cmake:x64-windows@2024-04-23 to 1 binary cache(s) in 69.6 ms
Installing 4/9 spirv-headers:x64-windows@1.4.350.0...
spirv-headers:x64-windows@1.4.350.0 package ABI: 5f8c569401704c665f6394a6683504733e499204b34981c74f455c2738e6f488
Building spirv-headers:x64-windows@1.4.350.0...
Downloading https://github.com/KhronosGroup/SPIRV-Headers/archive/vulkan-sdk-1.4.350.0.tar.gz -> KhronosGroup-SPIRV-Headers-vulkan-sdk-1.4.350.0.tar.gz
Successfully downloaded KhronosGroup-SPIRV-Headers-vulkan-sdk-1.4.350.0.tar.gz
-- Extracting source D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/KhronosGroup-SPIRV-Headers-vulkan-sdk-1.4.350.0.tar.gz
-- Using source at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/buildtrees/spirv-headers/src/-1.4.350.0-f21c9f9425.clean
-- Configuring x64-windows
-- Building x64-windows-dbg
-- Building x64-windows-rel
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/spirv-headers_x64-windows/share/pkgconfig/SPIRV-Headers.pc
-- Using cached msys2-mingw-w64-x86_64-pkgconf-1~2.5.1-1-any.pkg.tar.zst
-- Using cached msys2-msys2-runtime-3.6.5-1-x86_64.pkg.tar.zst
-- Using msys root at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/tools/msys2/3e71d1f8e22ab23f
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/spirv-headers_x64-windows/debug/share/pkgconfig/SPIRV-Headers.pc
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/spirv-headers_x64-windows/share/spirv-headers/copyright
-- Performing post-build validation
Starting submission of spirv-headers:x64-windows@1.4.350.0 to 1 binary cache(s) in the background
Elapsed time to handle spirv-headers:x64-windows: 3.7 s
Completed submission of sdl3[core,vulkan]:x64-windows@3.4.8#1 to 1 binary cache(s) in 1.8 s
Installing 5/9 spirv-tools:x64-windows@1.4.350.0...
spirv-tools:x64-windows@1.4.350.0 package ABI: 2172454270b4d61f69f352dddfceb269264ba35acf303e8e490d602b797334e6
Building spirv-tools:x64-windows@1.4.350.0...
Downloading https://github.com/KhronosGroup/SPIRV-Tools/archive/vulkan-sdk-1.4.350.0.tar.gz -> KhronosGroup-SPIRV-Tools-vulkan-sdk-1.4.350.0.tar.gz
Successfully downloaded KhronosGroup-SPIRV-Tools-vulkan-sdk-1.4.350.0.tar.gz
-- Extracting source D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/KhronosGroup-SPIRV-Tools-vulkan-sdk-1.4.350.0.tar.gz
-- Applying patch cmake-config-dir.diff
-- Applying patch spirv-tools-shared.diff
-- Applying patch fix-tool-deps.diff
-- Using source at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/buildtrees/spirv-tools/src/-1.4.350.0-630f5cfaf9.clean
Downloading https://www.python.org/ftp/python/3.14.2/python-3.14.2-embed-amd64.zip -> python-3.14.2-embed-amd64.zip
Successfully downloaded python-3.14.2-embed-amd64.zip
-- Configuring x64-windows
-- Building x64-windows-dbg
-- Building x64-windows-rel
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/spirv-tools_x64-windows/lib/pkgconfig/SPIRV-Tools-shared.pc
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/spirv-tools_x64-windows/lib/pkgconfig/SPIRV-Tools.pc
-- Using cached msys2-mingw-w64-x86_64-pkgconf-1~2.5.1-1-any.pkg.tar.zst
-- Using cached msys2-msys2-runtime-3.6.5-1-x86_64.pkg.tar.zst
-- Using msys root at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/tools/msys2/3e71d1f8e22ab23f
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/spirv-tools_x64-windows/debug/lib/pkgconfig/SPIRV-Tools-shared.pc
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/spirv-tools_x64-windows/debug/lib/pkgconfig/SPIRV-Tools.pc
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/spirv-tools_x64-windows/share/spirv-tools/usage
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/spirv-tools_x64-windows/share/spirv-tools/copyright
-- Performing post-build validation
Starting submission of spirv-tools:x64-windows@1.4.350.0 to 1 binary cache(s) in the background
Elapsed time to handle spirv-tools:x64-windows: 13 min
Completed submission of spirv-headers:x64-windows@1.4.350.0 to 1 binary cache(s) in 192 ms
Installing 6/9 glslang:x64-windows@16.3.0#1...
glslang:x64-windows@16.3.0#1 package ABI: 4122443b5b025b0b546b105e0c89891dfc204e12936ae18bf1898425c19593e4
Building glslang:x64-windows@16.3.0#1...
-- Note: glslang only supports static library linkage. Building static library.
Downloading https://github.com/KhronosGroup/glslang/archive/16.3.0.tar.gz -> KhronosGroup-glslang-16.3.0.tar.gz
Successfully downloaded KhronosGroup-glslang-16.3.0.tar.gz
-- Extracting source D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/KhronosGroup-glslang-16.3.0.tar.gz
-- Using source at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/buildtrees/glslang/src/16.3.0-ef135555c8.clean
-- Configuring x64-windows
-- Building x64-windows-dbg
-- Building x64-windows-rel
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/glslang_x64-windows/share/glslang/usage
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/glslang_x64-windows/share/glslang/copyright
-- Performing post-build validation
Starting submission of glslang:x64-windows@16.3.0#1 to 1 binary cache(s) in the background
Elapsed time to handle glslang:x64-windows: 1.7 min
Installing 7/9 shaderc:x64-windows@2026.2...
shaderc:x64-windows@2026.2 package ABI: 0412c7202525f488505a4096e70529d35ebf16b8e11a780e2bd7977a74af4d66
Building shaderc:x64-windows@2026.2...
-- Note: shaderc only supports static library linkage. Building static library.
Downloading https://github.com/google/shaderc/archive/v2026.2.tar.gz -> google-shaderc-v2026.2.tar.gz
Successfully downloaded google-shaderc-v2026.2.tar.gz
-- Extracting source D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/google-shaderc-v2026.2.tar.gz
-- Applying patch disable-update-version.patch
-- Applying patch fix-build-type.patch
-- Applying patch cmake-config-export.patch
-- Using source at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/buildtrees/shaderc/src/v2026.2-c0a4f684f3.clean
-- Configuring x64-windows
-- Building x64-windows-dbg
-- Building x64-windows-rel
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/shaderc_x64-windows/lib/pkgconfig/shaderc.pc
-- Using cached msys2-mingw-w64-x86_64-pkgconf-1~2.5.1-1-any.pkg.tar.zst
-- Using cached msys2-msys2-runtime-3.6.5-1-x86_64.pkg.tar.zst
-- Using msys root at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/tools/msys2/3e71d1f8e22ab23f
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/shaderc_x64-windows/debug/lib/pkgconfig/shaderc.pc
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/shaderc_x64-windows/share/shaderc/usage
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/shaderc_x64-windows/share/shaderc/copyright
-- Performing post-build validation
Starting submission of shaderc:x64-windows@2026.2 to 1 binary cache(s) in the background
Elapsed time to handle shaderc:x64-windows: 45 s
Installing 8/9 vulkan-headers:x64-windows@1.4.350.0...
vulkan-headers:x64-windows@1.4.350.0 package ABI: 15fa95989a7c4f4a4c032e06f70ce26f383b1c1eddf438c5688de9216534204c
Building vulkan-headers:x64-windows@1.4.350.0...
Downloading https://github.com/KhronosGroup/Vulkan-Headers/archive/vulkan-sdk-1.4.350.0.tar.gz -> KhronosGroup-Vulkan-Headers-vulkan-sdk-1.4.350.0.tar.gz
Successfully downloaded KhronosGroup-Vulkan-Headers-vulkan-sdk-1.4.350.0.tar.gz
-- Extracting source D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/KhronosGroup-Vulkan-Headers-vulkan-sdk-1.4.350.0.tar.gz
-- Using source at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/buildtrees/vulkan-headers/src/-1.4.350.0-d7a945c0fb.clean
-- Configuring x64-windows
-- Building x64-windows-rel
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vulkan-headers_x64-windows/share/vulkan-headers/copyright
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vulkan-headers_x64-windows/share/vulkan-headers/usage
-- Performing post-build validation
Starting submission of vulkan-headers:x64-windows@1.4.350.0 to 1 binary cache(s) in the background
Elapsed time to handle vulkan-headers:x64-windows: 3.3 s
Installing 9/9 vulkan-loader:x64-windows@1.4.350.0...
vulkan-loader:x64-windows@1.4.350.0 package ABI: 42eb542492cf88d393627b935efa850c23b37cb15059da0d89f9763f11ca969f
Building vulkan-loader:x64-windows@1.4.350.0...
Downloading https://github.com/KhronosGroup/Vulkan-Loader/archive/vulkan-sdk-1.4.350.0.tar.gz -> KhronosGroup-Vulkan-Loader-vulkan-sdk-1.4.350.0.tar.gz
Successfully downloaded KhronosGroup-Vulkan-Loader-vulkan-sdk-1.4.350.0.tar.gz
-- Extracting source D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/KhronosGroup-Vulkan-Loader-vulkan-sdk-1.4.350.0.tar.gz
-- Applying patch link-directfb.patch
-- Using source at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/buildtrees/vulkan-loader/src/-1.4.350.0-eb3e77a373.clean
-- Using cached msys2-mingw-w64-x86_64-pkgconf-1~2.5.1-1-any.pkg.tar.zst
-- Using cached msys2-msys2-runtime-3.6.5-1-x86_64.pkg.tar.zst
-- Using msys root at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/downloads/tools/msys2/3e71d1f8e22ab23f
-- Configuring x64-windows
CMake Warning at D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg_installed/x64-windows/share/vcpkg-cmake/vcpkg_cmake_configure.cmake:344 (message):
  The following variables are not used in CMakeLists.txt:

      BUILD_WSI_DIRECTFB_SUPPORT
      BUILD_WSI_WAYLAND_SUPPORT
      BUILD_WSI_XCB_SUPPORT
      BUILD_WSI_XLIB_SUPPORT
      Python3_EXECUTABLE

  Please recheck them and remove the unnecessary options from the
  `vcpkg_cmake_configure` call.

  If these options should still be passed for whatever reason, please use the
  `MAYBE_UNUSED_VARIABLES` argument.
Call Stack (most recent call first):
  buildtrees/versioning_/versions/vulkan-loader/8c5e48549412c251b157e18ff4d332176b71696d/portfile.cmake:26 (vcpkg_cmake_configure)
  scripts/ports.cmake:206 (include)


-- Building x64-windows-dbg
-- Building x64-windows-rel
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vulkan-loader_x64-windows/lib/pkgconfig/vulkan.pc
-- Fixing pkgconfig file: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vulkan-loader_x64-windows/debug/lib/pkgconfig/vulkan.pc
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg/packages/vulkan-loader_x64-windows/share/vulkan-loader/copyright
-- Performing post-build validation
Starting submission of vulkan-loader:x64-windows@1.4.350.0 to 1 binary cache(s) in the background
Elapsed time to handle vulkan-loader:x64-windows: 17 s
Installed contents are licensed to you by owners. Microsoft is not responsible for, nor does it grant any licenses to, third-party packages.
Some packages did not declare an SPDX license. Check the `copyright` file for each package for more information about their licensing.
Packages installed in this vcpkg installation declare the following licenses:
(Apache-2.0 OR MIT)
Apache-2.0
BSD-3-Clause
GPL-3.0-or-later
MIT
Zlib
sdl3 provides CMake targets:

  find_package(SDL3 CONFIG REQUIRED)
  target_link_libraries(main PRIVATE SDL3::SDL3)

shaderc provides CMake targets:

    find_package(unofficial-shaderc CONFIG REQUIRED)
    target_link_libraries(main PRIVATE unofficial::shaderc::shaderc)

The package vulkan-loader provides the vulkan loader.
Please be aware of https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderApplicationInterface.md#bundling-the-loader-with-an-application

Waiting for 5 remaining binary cache submissions...
Completed submission of spirv-tools:x64-windows@1.4.350.0 to 1 binary cache(s) in 2.9 min (1/5)
Completed submission of glslang:x64-windows@16.3.0#1 to 1 binary cache(s) in 8.7 s (2/5)
Completed submission of shaderc:x64-windows@2026.2 to 1 binary cache(s) in 821 ms (3/5)
Completed submission of vulkan-headers:x64-windows@1.4.350.0 to 1 binary cache(s) in 1.4 s (4/5)
Completed submission of vulkan-loader:x64-windows@1.4.350.0 to 1 binary cache(s) in 278 ms (5/5)
All requested installations completed successfully in: 17 min
-- Running vcpkg install - done
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
-- Found Vulkan: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/vcpkg_installed/x64-windows/debug/lib/vulkan-1.lib (found suitable version "1.4.350", minimum required is "1.3") found components: glslc missing components: glslangValidator
-- Configuring done (1048.3s)
-- Generating done (0.1s)
-- Build files have been written to: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/windows-release

```

## Full Vulkan build

```text
MSBuild version 18.7.8+1ac568fee for .NET Framework

  1>Checking Build System
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/windows-release/_deps/epochgui-src/CMakeLists.txt
  Scanning sources for module dependencies...
  image.cpp
  floating_window.cpp
  dock_layout.cpp
  dockable_window.cpp
  panel_host.cpp
  text_control.cpp
  popup_layout.cpp
  rounded_rect.cpp
  input.cpp
  epoch.gui.font.ixx
  epoch.gui.image.ixx
  epoch.gui.ixx
  epoch.gui.input.ixx
  epoch.gui.rounded_rect.ixx
  Compiling...
  epoch.gui.ixx
  floating_window.cpp
  panel_host.cpp
  dock_layout.cpp
  dockable_window.cpp
  popup_layout.cpp
  text_control.cpp
  epoch.gui.image.ixx
  epoch.gui.font.ixx
  epoch.gui.input.ixx
  epoch.gui.rounded_rect.ixx
  image.cpp
  rounded_rect.cpp
  input.cpp
  EpochGui.vcxproj -> D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\windows-release\_deps\epochgui-build\Release\EpochGui.lib
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt
  Scanning sources for module dependencies...
  shader_compiler.cpp
  Compiling...
  shader_compiler.cpp
  EpochRunnerShaderCompiler.vcxproj -> D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\windows-release\Release\EpochRunnerShaderCompiler.exe
  2>Compiling flat.vert with vcpkg shaderc
  1>Compiling flat.frag with vcpkg shaderc
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt
  Scanning sources for module dependencies...
  ppo_trainer.cpp
  ppo_network.cpp
  simulation.cpp
  ppo_parallel.cpp
  training_checkpoint.cpp
  autonomy_commands.cpp
  autonomy_curriculum.cpp
  autonomy_runtime.cpp
  autonomy_persistence.cpp
  Compiling...
  simulation.cpp
  ppo_trainer.cpp
  ppo_network.cpp
  ppo_parallel.cpp
  autonomy_runtime.cpp
  autonomy_commands.cpp
  autonomy_curriculum.cpp
  training_checkpoint.cpp
  autonomy_persistence.cpp
  EpochRunnerCore.vcxproj -> D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\windows-release\Release\EpochRunnerCore.lib
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt
  Scanning sources for module dependencies...
  renderer.cpp
  app.cpp
  canvas.cpp
  main.cpp
  Compiling...
  canvas.cpp
  main.cpp
  app.cpp
  renderer.cpp
  EpochRunner.vcxproj -> D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\windows-release\Release\EpochRunner.exe
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt
  Scanning sources for module dependencies...
  concurrency_benchmark.cpp
  Compiling...
  concurrency_benchmark.cpp
  EpochRunnerConcurrencyBenchmark.vcxproj -> D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\windows-release\Release\EpochRunnerConcurrencyBenchmark.exe
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt
  Scanning sources for module dependencies...
  core_tests.cpp
  Compiling...
  core_tests.cpp
  EpochRunnerCoreTests.vcxproj -> D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\windows-release\Release\EpochRunnerCoreTests.exe
  Building Custom Rule D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/CMakeLists.txt

```

## Core, course, recovery, and concurrency tests

```text
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
1: EpochRunner v0.6.0 procedural course, recovery, concurrency, gait, and rig-edit tests passed
1/2 Test #1: EpochRunner.Core ...................   Passed    0.56 sec
test 2
    Start 2: EpochRunner.ConcurrencyBenchmark

2: Test command: D:\a\Vulkan_AI_Walking_Training_Simulation\Vulkan_AI_Walking_Training_Simulation\build\windows-release\Release\EpochRunnerConcurrencyBenchmark.exe
2: Working Directory: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/build/windows-release
2: Test timeout computed to be: 30
2: mode=1 workers=1 updates=6 updates_per_second=1.49789 environment_steps_per_second=12270.7
2: mode=2 workers=2 updates=11 updates_per_second=2.74449 environment_steps_per_second=22482.9
2: mode=4 workers=2 updates=12 updates_per_second=2.99718 environment_steps_per_second=24552.9
2: EpochRunner v0.6 speed-mode throughput benchmark passed
2/2 Test #2: EpochRunner.ConcurrencyBenchmark ...   Passed   12.71 sec

100% tests passed out of 2

Total Test time (real) =  13.97 sec

```

## Executable version

```text
EpochRunner 0.6.0

```

## SDL3/Vulkan diagnostic

```text
EpochRunner 0.6.0 SDL3 Vulkan diagnostic passed: backend enabled, video_driver=windows; the CI runner has no Vulkan presentation surface (Installed Vulkan doesn't implement the VK_KHR_surface extension)

```

## Install staging

```text
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/package/EpochRunner-v0.6.0-windows-x64/EpochRunner.exe
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/package/EpochRunner-v0.6.0-windows-x64/shaders
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/package/EpochRunner-v0.6.0-windows-x64/shaders/flat.frag.spv
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/package/EpochRunner-v0.6.0-windows-x64/shaders/flat.vert.spv
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/package/EpochRunner-v0.6.0-windows-x64/assets
-- Installing: D:/a/Vulkan_AI_Walking_Training_Simulation/Vulkan_AI_Walking_Training_Simulation/package/EpochRunner-v0.6.0-windows-x64/assets/chicken.ppm

```
