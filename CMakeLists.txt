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
# Shaderixel patch: interface translation hook on the text drawing / measuring functions (tools/patches).
file(READ ${IMGUI_DIR}/imgui.cpp SX_IMGUI_CORE)
string(FIND "${SX_IMGUI_CORE}" "GSXTranslateFn" SX_IMGUI_I18N_PATCHED)
if(SX_IMGUI_I18N_PATCHED EQUAL -1)
    execute_process(COMMAND git apply --ignore-whitespace ${CMAKE_SOURCE_DIR}/tools/patches/imgui_i18n.patch
                    WORKING_DIRECTORY ${IMGUI_DIR} RESULT_VARIABLE SX_IMGUI_I18N_RESULT)
    if(NOT SX_IMGUI_I18N_RESULT EQUAL 0)
        message(FATAL_ERROR "Could not apply tools/patches/imgui_i18n.patch to ${IMGUI_DIR}")
    endif()
endif()
# Shaderixel patch: per-window text filter (Unreal-like) on the labeled widgets (tools/patches).
file(READ ${IMGUI_DIR}/imgui_widgets.cpp SX_IMGUI_WIDGETS)
string(FIND "${SX_IMGUI_WIDGETS}" "GSXFilterFn" SX_IMGUI_FILTER_PATCHED)
if(SX_IMGUI_FILTER_PATCHED EQUAL -1)
    execute_process(COMMAND git apply --ignore-whitespace ${CMAKE_SOURCE_DIR}/tools/patches/imgui_filter.patch
                    WORKING_DIRECTORY ${IMGUI_DIR} RESULT_VARIABLE SX_IMGUI_FILTER_RESULT)
    if(NOT SX_IMGUI_FILTER_RESULT EQUAL 0)
        message(FATAL_ERROR "Could not apply tools/patches/imgui_filter.patch to ${IMGUI_DIR}")
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
# Test engine hooks: every item drawn is reported (labels, boxes) to the search index (src/ui/Search.cpp).
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS IMGUI_ENABLE_TEST_ENGINE)

# ---------------------------------------------------------------- xatlas (automatic UV unwrapping, MIT)
# Build a generated copy with relative diagnostic filenames; keep upstream sources untouched.
set(XA_SOURCE ${EXT}/xatlas/source/xatlas/xatlas.cpp)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${XA_SOURCE})
file(READ ${XA_SOURCE} SX_XATLAS_SOURCE)
string(REPLACE "__FILE__" "\"xatlas.cpp\"" SX_XATLAS_SOURCE "${SX_XATLAS_SOURCE}")
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
file(WRITE ${CMAKE_BINARY_DIR}/generated/xatlas.cpp "${SX_XATLAS_SOURCE}")
add_library(xatlas STATIC ${CMAKE_BINARY_DIR}/generated/xatlas.cpp)
target_include_directories(xatlas PUBLIC ${EXT}/xatlas/source/xatlas)
if(MSVC)
    target_compile_options(xatlas PRIVATE /W0 /O2)
endif()

# ---------------------------------------------------------------- ufbx (FBX loading, MIT)
add_library(ufbx STATIC ${EXT}/ufbx/ufbx.c)
target_include_directories(ufbx PUBLIC ${EXT}/ufbx)
if(MSVC)
    target_compile_options(ufbx PRIVATE /W0 /O2)
endif()

# ---------------------------------------------------------------- LibRaw (camera RAW decoding, LGPL 2.1 / CDDL 1.0), optional
set(SX_HAVE_LIBRAW OFF)
if(EXISTS ${EXT}/LibRaw/libraw/libraw.h)
    file(GLOB_RECURSE LIBRAW_SOURCES ${EXT}/LibRaw/src/*.cpp)
    # Files #included by others (not separate translation units).
    list(FILTER LIBRAW_SOURCES EXCLUDE REGEX "src/.*_ph\\.cpp$")
    add_library(libraw STATIC ${LIBRAW_SOURCES})
    target_include_directories(libraw PUBLIC ${EXT}/LibRaw)
    target_compile_definitions(libraw PUBLIC LIBRAW_NODLL PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    target_link_libraries(libraw PUBLIC ws2_32)
    if(MSVC)
        target_compile_options(libraw PRIVATE /W0 /O2 /EHsc /MP)
    endif()
    set(SX_HAVE_LIBRAW ON)
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

# Keep dictionaries next to both executables, including dictionary-only updates.
add_custom_target(ShaderixelLanguages ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/library/lang ${CMAKE_BINARY_DIR}/lang
    COMMENT "Copying interface dictionaries")

function(sx_app target)
    add_executable(${target} WIN32 ${SX_SOURCES} ${ARGN} ${CMAKE_SOURCE_DIR}/assets/shaderixel.rc)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/sdk/include ${EXT}/stb)
    target_link_libraries(${target} PRIVATE imgui slang xatlas ufbx comdlg32 shell32 ole32 d3d11 windowscodecs advapi32 pdh)
    target_compile_definitions(${target} PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /wd4100 /wd4201 /wd4324 /permissive- /utf-8 /MP)
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SLANG_DIR}/bin/slang.dll ${SLANG_DIR}/bin/slang-compiler.dll
            ${SLANG_DIR}/bin/slang-glslang.dll ${SLANG_DIR}/bin/slang-rt.dll
            $<TARGET_FILE_DIR:${target}>)
    add_dependencies(${target} ShaderixelExamplePlugin ShaderixelPalettePlugin ShaderixelScriptPlugins ShaderixelLanguages)
    if(SX_HAVE_LIBRAW)
        target_link_libraries(${target} PRIVATE libraw)
        target_compile_definitions(${target} PRIVATE SX_LIBRAW=1)
    endif()
endfunction()

# Example plugin (sdk/examples): built into <exe dir>/plugins/Exemple with its manifest.
add_library(ShaderixelExamplePlugin SHARED ${CMAKE_SOURCE_DIR}/sdk/examples/example_plugin/example_plugin.cpp)
target_include_directories(ShaderixelExamplePlugin PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include)
set_target_properties(ShaderixelExamplePlugin PROPERTIES OUTPUT_NAME "example_plugin"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/Exemple
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/plugins/Exemple)
if(MSVC)
    target_compile_options(ShaderixelExamplePlugin PRIVATE /W4 /utf-8)
endif()
add_custom_command(TARGET ShaderixelExamplePlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/sdk/examples/example_plugin/plugin.json ${CMAKE_BINARY_DIR}/plugins/Exemple/plugin.json)
# Script plugin example: copied at every build (no compilation step to trigger it).
add_custom_target(ShaderixelScriptPlugins ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/sdk/examples/script_plugin ${CMAKE_BINARY_DIR}/plugins/ExempleScript)

# Palette plugin (shipped): dominant colors of the active layer.
add_library(ShaderixelPalettePlugin SHARED ${CMAKE_SOURCE_DIR}/sdk/examples/palette_plugin/palette_plugin.cpp)
target_include_directories(ShaderixelPalettePlugin PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include)
set_target_properties(ShaderixelPalettePlugin PROPERTIES OUTPUT_NAME "palette_plugin"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/Palette
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/plugins/Palette)
if(MSVC)
    target_compile_options(ShaderixelPalettePlugin PRIVATE /W4 /utf-8)
endif()
add_custom_command(TARGET ShaderixelPalettePlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/sdk/examples/palette_plugin/plugin.json ${CMAKE_BINARY_DIR}/plugins/Palette/plugin.json)

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
