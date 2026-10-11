# Builds the example plugin alone (outside the Shaderixel source tree):
#   cmake -S . -B build -A x64 && cmake --build build --config Release
# then copy build/Release/example_plugin.dll and plugin.json into
# %LOCALAPPDATA%/Shaderixel/Plugins/Exemple/ (or <Shaderixel>/plugins/Exemple/).
cmake_minimum_required(VERSION 3.16)
project(ShaderixelExamplePlugin CXX)
set(CMAKE_CXX_STANDARD 17)
add_library(example_plugin SHARED example_plugin.cpp)
target_include_directories(example_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
if(MSVC)
    target_compile_options(example_plugin PRIVATE /utf-8)
endif()
