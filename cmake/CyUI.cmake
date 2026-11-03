# One locked CyUI checkout and the application's existing Dear ImGui target.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/dependencies.json")
file(READ "${CMAKE_CURRENT_LIST_DIR}/dependencies.json" SX_DEPENDENCIES)
string(JSON SX_CYUI_REVISION GET "${SX_DEPENDENCIES}" cyui revision)
set(SX_CYUI_DIR "${EXT}/cyui")
if(NOT EXISTS "${SX_CYUI_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "CyUI is missing. Run: python tools/fetch_cyui.py")
endif()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=${SX_CYUI_DIR}"
    -C "${SX_CYUI_DIR}" rev-parse HEAD OUTPUT_VARIABLE SX_CYUI_ACTUAL
    OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE SX_CYUI_GIT_RESULT)
if(NOT SX_CYUI_GIT_RESULT EQUAL 0 OR NOT SX_CYUI_ACTUAL STREQUAL SX_CYUI_REVISION)
    message(FATAL_ERROR "CyUI revision mismatch. Run: python tools/fetch_cyui.py")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=${SX_CYUI_DIR}"
    -C "${SX_CYUI_DIR}" status --porcelain OUTPUT_VARIABLE SX_CYUI_LOCAL_CHANGES
    OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE SX_CYUI_STATUS_RESULT)
if(NOT SX_CYUI_STATUS_RESULT EQUAL 0 OR NOT SX_CYUI_LOCAL_CHANGES STREQUAL "")
    message(FATAL_ERROR "CyUI has local changes. Preserve them before building the locked dependency.")
endif()
set(CYUI_IMGUI_TARGET imgui CACHE STRING "Shaderixel's Dear ImGui" FORCE)
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_WEB_CEF OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "Nodes;Images;Curves;Plots;CodeEditor" CACHE STRING "Shaderixel UI modules" FORCE)
add_subdirectory("${SX_CYUI_DIR}" "${CMAKE_BINARY_DIR}/cyui")
