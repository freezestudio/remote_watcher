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
# deprecated. ignore now.
#
# add_subdirectory(tools)

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

set(EXEC_NAME monitor)

# auto find *.cc in src/controller
file(GLOB EXEC_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/*.cc)
add_executable(${EXEC_NAME} WIN32 ${EXEC_SRC})
target_include_directories(${EXEC_NAME} 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/wtl/include ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/zlib
    PUBLIC ${PROJECT_BINARY_DIR}/zlib
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/controller  ./include
)
#target_link_options(monitor
#    PRIVATE /SUBSYSTEM:WINDOWS
#    PUBLIC /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
#)
set_target_properties(${EXEC_NAME} PROPERTIES LINK_FLAGS /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'")
# SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /level='requireAdministrator' /uiAccess='false'")
target_sources(${EXEC_NAME} PUBLIC
    ./assets/controller.rc
    ./assets/controller.ico
    # ./assets/blob.tgz
    # TODO: test if exists
    ./assets/blob.tgz
)
target_link_libraries(${EXEC_NAME} zlibstatic)

#
# custom command: check blob
#
add_custom_command(TARGET ${EXEC_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "before build monitor: check blob ..."
)

#
# about CMAKE_BUILD_TYPE
#
message(STATUS "Has [CMAKE_BUILD_TYPE]? " ${CMAKE_BUILD_TYPE})
message(STATUS "Has [CMAKE_CONFIG_TYPE]? " ${CMAKE_CONFIG_TYPE})

#
# mock-target: ...
#
add_custom_target(debug-only
    COMMAND ${CMAKE_COMMAND} -E echo "Is Debug Config? $<CONFIG:Debug>"
)

#
# Some GoogleTest (debug only)
#

option(THIS_DISABLE_DEBUG_TEST "if config is not `Debug`, then disable build tests." ON)
# ? can not test condition $<CONFIG:Debug>
message(STATUS "THIS_DISABLE_DEBUG_TEST? " ${THIS_DISABLE_DEBUG_TEST})
if((CMAKE_BUILD_TYPE EQUAL "Debug") OR (NOT THIS_DISABLE_DEBUG_TEST))
    add_subdirectory(test)
endif()
