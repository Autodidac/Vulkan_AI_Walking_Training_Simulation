cmake_minimum_required(VERSION 3.28)
project(SandHybrid VERSION 2.5.9 LANGUAGES NONE)

add_library(stale_epochsimengine INTERFACE)
add_library(EpochSimEngine::EpochSimEngine ALIAS stale_epochsimengine)
