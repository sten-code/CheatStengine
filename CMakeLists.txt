cmake_minimum_required(VERSION 3.30)

project(CheatStengine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-DENGINE_IMGUI -DIMGUI_USE_WCHAR32)

add_subdirectory(Engine)
#add_subdirectory(SampleApp)
add_subdirectory(CheatStengine)
