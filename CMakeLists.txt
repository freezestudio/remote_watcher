cmake_minimum_required(VERSION 3.20)

project(remote_win32)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake
)
include(common)

file(GLOB MONITOR_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/*.cc)
add_executable(monitor ${MONITOR_SRC})
target_include_directories(monitor 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/wtl/include ./include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/controller 
)
# target_compile_definitions(monitor PUBLIC _WINDOWS)
target_link_options(monitor
    PRIVATE /SUBSYSTEM:WINDOWS
    PUBLIC /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
)
target_sources(monitor PUBLIC ./assets/controller.rc ./assets/controller.ico ./assets/blob.zip)

add_subdirectory(src/service)

add_subdirectory(test)
