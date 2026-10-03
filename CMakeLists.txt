cmake_minimum_required(VERSION 3.24)
project(Shaderixel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT WIN32)
    message(FATAL_ERROR "Shaderixel currently targets Windows (D3D12, and Vulkan as a second backend); Linux/macOS platform layers are planned.")
endif()

set(EXT ${CMAKE_SOURCE_DIR}/external)
set(IMGUI_DIR ${EXT}/imgui)
set(SLANG_DIR ${EXT}/slang)

# ---------------------------------------------------------------- Dear ImGui
# Shaderixel patch: relative slider/drag behavior with speed modifiers (tools/patches).
file(READ ${IMGUI_DIR}/imgui_widgets.cpp SX_IMGUI_WIDGETS)
string(FIND "${SX_IMGUI_WIDGETS}" "GSXRelativeDragFrame" SX_IMGUI_PATCHED)
if(SX_IMGUI_PATCHED EQUAL -1)
    execute_process(COMMAND git apply --ignore-whitespace ${CMAKE_SOURCE_DIR}/tools/patches/imgui_relative_drag.patch
                    WORKING_DIRECTORY ${IMGUI_DIR} RESULT_VARIABLE SX_IMGUI_PATCH_RESULT)
    if(NOT SX_IMGUI_PATCH_RESULT EQUAL 0)
        message(FATAL_ERROR "Could not apply tools/patches/imgui_relative_drag.patch to ${IMGUI_DIR}")
    endif()
endif()

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends ${IMGUI_DIR}/misc/cpp)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)

# ---------------------------------------------------------------- xatlas (automatic UV unwrapping, MIT)
add_library(xatlas STATIC ${EXT}/xatlas/source/xatlas/xatlas.cpp)
target_include_directories(xatlas PUBLIC ${EXT}/xatlas/source/xatlas)
if(MSVC)
    target_compile_options(xatlas PRIVATE /W0 /O2)
endif()

# ---------------------------------------------------------------- Slang (prebuilt)
add_library(slang SHARED IMPORTED)
set_target_properties(slang PROPERTIES
    IMPORTED_LOCATION ${SLANG_DIR}/bin/slang.dll
    IMPORTED_IMPLIB ${SLANG_DIR}/lib/slang.lib
    INTERFACE_INCLUDE_DIRECTORIES ${SLANG_DIR}/include)

# ---------------------------------------------------------------- App (one executable per GPU backend)
file(GLOB_RECURSE SX_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*.h)
list(FILTER SX_SOURCES EXCLUDE REGEX "src/gpu/Device(D3D12|Vulkan)\.cpp$")

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

function(sx_app target)
    add_executable(${target} WIN32 ${SX_SOURCES} ${ARGN} ${CMAKE_SOURCE_DIR}/assets/shaderixel.rc)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src ${EXT}/stb)
    target_link_libraries(${target} PRIVATE imgui slang xatlas comdlg32 shell32 ole32)
    target_compile_definitions(${target} PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
        SX_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /wd4100 /wd4201 /wd4324 /permissive- /utf-8 /MP)
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SLANG_DIR}/bin/slang.dll ${SLANG_DIR}/bin/slang-compiler.dll
            ${SLANG_DIR}/bin/slang-glslang.dll ${SLANG_DIR}/bin/slang-rt.dll
            $<TARGET_FILE_DIR:${target}>)
endfunction()

# D3D12
sx_app(Shaderixel ${CMAKE_SOURCE_DIR}/src/gpu/DeviceD3D12.cpp ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp)
target_link_libraries(Shaderixel PRIVATE d3d12 dxgi dxguid)
if(WINSDK_BIN)
    add_custom_command(TARGET Shaderixel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WINSDK_BIN}/dxcompiler.dll" "${WINSDK_BIN}/dxil.dll"
            $<TARGET_FILE_DIR:Shaderixel>)
else()
    message(WARNING "dxcompiler.dll not found in Windows SDK; Slang DXIL output will fail at runtime.")
endif()

# Vulkan (headers + volk loader; no SDK needed at runtime beyond the driver's vulkan-1.dll)
if(EXISTS ${EXT}/vulkan-headers/include/vulkan/vulkan.h AND EXISTS ${EXT}/volk/volk.c)
    sx_app(ShaderixelVk ${CMAKE_SOURCE_DIR}/src/gpu/DeviceVulkan.cpp ${EXT}/volk/volk.c ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp)
    target_include_directories(ShaderixelVk PRIVATE ${EXT}/vulkan-headers/include ${EXT}/volk)
    target_compile_definitions(ShaderixelVk PRIVATE SX_VULKAN=1 VK_USE_PLATFORM_WIN32_KHR VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_USE_VOLK)
else()
    message(STATUS "Vulkan headers/volk not found in external/: ShaderixelVk not built.")
endif()
if(MSVC)
    target_compile_options(imgui PRIVATE /utf-8 /MP)
    set_source_files_properties(${IMGUI_DIR}/backends/imgui_impl_dx12.cpp ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp ${EXT}/volk/volk.c
        PROPERTIES COMPILE_OPTIONS "/W0")
endif()
