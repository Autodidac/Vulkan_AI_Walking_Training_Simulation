cmake_minimum_required(VERSION 3.28)
project(Epoch2DWalkEngineInstalledConsumer LANGUAGES CXX)

find_package(Epoch2DWalkEngine 0.7 CONFIG REQUIRED)

add_executable(Epoch2DWalkEngineInstalledConsumer
    ../library_probe.cpp)
target_link_libraries(Epoch2DWalkEngineInstalledConsumer PRIVATE
    Epoch2DWalkEngine::Epoch2DWalkEngine)
target_compile_features(Epoch2DWalkEngineInstalledConsumer PRIVATE cxx_std_23)
set_target_properties(Epoch2DWalkEngineInstalledConsumer PROPERTIES CXX_EXTENSIONS OFF)
