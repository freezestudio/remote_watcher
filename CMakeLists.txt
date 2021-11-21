cmake_minimum_required(VERSION 3.20)

project(remote_win32)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake
)
include(common)

add_subdirectory(src/service)

#
# the Manager of Service (mnsvc)
#

file(GLOB MONITOR_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/*.cc)
add_executable(monitor ${MONITOR_SRC})
target_include_directories(monitor 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/wtl/include ./include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/controller 
)
target_link_options(monitor
    PRIVATE /SUBSYSTEM:WINDOWS
    PUBLIC /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
)
target_sources(monitor PUBLIC
    ./assets/controller.rc
    ./assets/controller.ico
    ./assets/blob.zip
)
add_custom_command(TARGET monitor PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "will build monitor project ..."
)

#
# about CMAKE_BUILD_TYPE
#
message(STATUS "Has [CMAKE_BUILD_TYPE]? ", ${CMAKE_BUILD_TYPE})
message(STATUS "Has [CMAKE_CONFIG_TYPE]? ", ${CMAKE_CONFIG_TYPE})
add_custom_target(debug-only
    COMMAND ${CMAKE_COMMAND} -E echo "Is Debug Config? $<CONFIG:Debug>"
)

#
# Some Test
#

add_subdirectory(test)
