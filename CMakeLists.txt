cmake_minimum_required(VERSION 3.20.0)

# project name
project(remote_win32)

#
# compile nats
#
set(NATS_BUILD_NO_SPIN ON)
set(NATS_BUILD_TLS_FORCE_HOST_VERIFY OFF)
set(NATS_BUILD_WITH_TLS OFF)
set(NATS_BUILD_EXAMPLES OFF)
set(NATS_BUILD_STREAMING OFF)
set(NATS_BUILD_LIB_SHARED OFF)
add_subdirectory(3rdpart/nats ${PROJECT_BINARY_DIR}/nats)

#
# compile zlib
# disable: *.asm
# link *.obj: set_target_properties(xxx PROPERTIES LINK_FLAGS /link yyy.obj)
#
set(ASM686 OFF)
set(AMD64 OFF)
add_subdirectory(3rdpart/zlib ${PROJECT_BINARY_DIR}/zlib)

#
# minizip (miniunz)
#
add_subdirectory(tools)

#
# compile googletest (debug only)
#
set(BUILD_GMOCK OFF)
set(INSTALL_GTEST OFF)
# set(gtest_force_shared_crt ON)
add_subdirectory(3rdpart/googletest ${PROJECT_BINARY_DIR}/googletest)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake
)
include(common)

#
# rgmsvc.dll service
#
add_subdirectory(src/service ${PROJECT_BINARY_DIR}/service)

#
# the Manager of Service (rgmsvc)
#

file(GLOB MONITOR_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/*.cc)
add_executable(monitor ${MONITOR_SRC})
target_include_directories(monitor 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/wtl/include ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/zlib ./include
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
    COMMAND ${CMAKE_COMMAND} -E echo "will build project: monitor ..."
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
# Some GoogleTest (debug only)
#

# ? can not test condition
if($<CONFIG:Debug>)
    add_subdirectory(test)
endif()
