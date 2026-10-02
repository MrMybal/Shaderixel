cmake_minimum_required(VERSION 3.24)
project(Shaderixel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT WIN32)
    message(FATAL_ERROR "Shaderixel currently targets Windows/D3D12 only (Vulkan/Metal backends are planned).")
endif()

set(EXT ${CMAKE_SOURCE_DIR}/external)
set(IMGUI_DIR ${EXT}/imgui)
set(SLANG_DIR ${EXT}/slang)

# ---------------------------------------------------------------- Dear ImGui
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends ${IMGUI_DIR}/misc/cpp)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)

# ---------------------------------------------------------------- Slang (prebuilt)
add_library(slang SHARED IMPORTED)
set_target_properties(slang PROPERTIES
    IMPORTED_LOCATION ${SLANG_DIR}/bin/slang.dll
    IMPORTED_IMPLIB ${SLANG_DIR}/lib/slang.lib
    INTERFACE_INCLUDE_DIRECTORIES ${SLANG_DIR}/include)

# ---------------------------------------------------------------- App
file(GLOB_RECURSE SX_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*.h)
add_executable(Shaderixel WIN32 ${SX_SOURCES})
target_include_directories(Shaderixel PRIVATE ${CMAKE_SOURCE_DIR}/src ${EXT}/stb)
target_link_libraries(Shaderixel PRIVATE imgui slang d3d12 dxgi dxguid comdlg32 shell32 ole32)
target_compile_definitions(Shaderixel PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    SX_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders")
if(MSVC)
    target_compile_options(Shaderixel PRIVATE /W4 /wd4100 /wd4201 /wd4324 /permissive- /utf-8 /MP)
    target_compile_options(imgui PRIVATE /utf-8 /MP)
endif()

# Runtime DLLs: Slang + DXC (DXIL backend used by Slang) next to the exe.
set(WINSDK_BIN "")
file(GLOB WINSDK_BINS "C:/Program Files (x86)/Windows Kits/10/bin/10.*")
list(SORT WINSDK_BINS COMPARE NATURAL ORDER DESCENDING)
foreach(dir ${WINSDK_BINS})
    if(EXISTS "${dir}/x64/dxcompiler.dll")
        set(WINSDK_BIN "${dir}/x64")
        break()
    endif()
endforeach()

add_custom_command(TARGET Shaderixel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SLANG_DIR}/bin/slang.dll ${SLANG_DIR}/bin/slang-compiler.dll
        ${SLANG_DIR}/bin/slang-glslang.dll ${SLANG_DIR}/bin/slang-rt.dll
        $<TARGET_FILE_DIR:Shaderixel>)
if(WINSDK_BIN)
    add_custom_command(TARGET Shaderixel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WINSDK_BIN}/dxcompiler.dll" "${WINSDK_BIN}/dxil.dll"
            $<TARGET_FILE_DIR:Shaderixel>)
else()
    message(WARNING "dxcompiler.dll not found in Windows SDK; Slang DXIL output will fail at runtime.")
endif()
