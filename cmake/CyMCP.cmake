set(SX_CYMCP_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../CyMCPSDK" CACHE PATH "Local CyMCPSDK source checkout")
set(SX_CYMCP_PIN "791d4d7d7680213b282ec8529d248a10f8494d77")

# Add only the modules used by the editor. Local models use existing model servers.
set(CYMCP_BUILD_HTTP ON)
set(CYMCP_BUILD_PLATFORM ON)
set(CYMCP_BUILD_MCP ON)
set(CYMCP_BUILD_AI ON)
set(CYMCP_BUILD_CHAT_UI OFF)
set(CYMCP_BUILD_LOCAL OFF)
set(CYMCP_BUILD_EXAMPLES OFF)
set(CYMCP_BUILD_TESTS OFF)
set(CYMCP_BUILD_BENCHMARKS OFF)
set(CYMCP_BUILD_CHAT_APP OFF)

if(EXISTS "${SX_CYMCP_SOURCE_DIR}/CMakeLists.txt")
    get_filename_component(SX_CYMCP_SOURCE_DIR "${SX_CYMCP_SOURCE_DIR}" REALPATH)
    add_subdirectory("${SX_CYMCP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/cymcp" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(shaderixel_cymcp GIT_REPOSITORY https://github.com/MrMybal/CyMCPSDK.git
        GIT_TAG "${SX_CYMCP_PIN}")
    FetchContent_MakeAvailable(shaderixel_cymcp)
    set(SX_CYMCP_SOURCE_DIR "${shaderixel_cymcp_SOURCE_DIR}")
endif()

execute_process(COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=${SX_CYMCP_SOURCE_DIR}"
    -C "${SX_CYMCP_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE SX_CYMCP_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=${SX_CYMCP_SOURCE_DIR}"
    -C "${SX_CYMCP_SOURCE_DIR}" status --porcelain --untracked-files=no
    OUTPUT_VARIABLE SX_CYMCP_CHANGES OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(SX_CYMCP_CHANGES)
    string(APPEND SX_CYMCP_REVISION "-dirty")
endif()
message(STATUS "Shaderixel CyMCPSDK source revision: ${SX_CYMCP_REVISION}")
# Shaderixel's locked CyUI lacks the SDK panel's message composer and image
# types. Reuse the UI-independent controller, which owns Conversation and I/O.
add_library(ShaderixelChatController STATIC "${SX_CYMCP_SOURCE_DIR}/src/chat_controller.cpp")
target_link_libraries(ShaderixelChatController PUBLIC CyMCP::AI)
if(MSVC)
    target_compile_options(ShaderixelChatController PRIVATE /utf-8)
endif()
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/generated/ShaderixelMcpVersion.h"
    CONTENT "#pragma once\n#define SX_CYMCP_REVISION \"@SX_CYMCP_REVISION@\"\n" @ONLY)
set(SX_CYMCP_NOTICES "${CMAKE_BINARY_DIR}/cymcp-licenses")
foreach(notice IN ITEMS LICENSE THIRD_PARTY_NOTICES.md third_party/nlohmann/LICENSE.MIT
        third_party/httplib/LICENSE third_party/picosha2/LICENSE)
    configure_file("${SX_CYMCP_SOURCE_DIR}/${notice}" "${SX_CYMCP_NOTICES}/${notice}" COPYONLY)
endforeach()
