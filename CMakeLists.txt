cmake_minimum_required(VERSION 3.20.0)

# project name
project(remote_win32 VERSION 0.9.0)

# if Release set THIS_DISABLE_DEBUG_TEST to OFF, else set ON.
option(THIS_DISABLE_DEBUG_TEST "if config is not `Debug`, then disable build tests." OFF)
message(STATUS "THIS_DISABLE_DEBUG_TEST? " ${THIS_DISABLE_DEBUG_TEST})

#
# compile nats
# set some options
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
# rgmsvc Service (.dll or .exe)
#
set(SERVICE_EXEC_TYPE ON)
add_subdirectory(src/service ${PROJECT_BINARY_DIR}/service)

#
# the Service Manager (monitor[.exe])
#

set(EXEC_NAME monitor)

# auto find *.cc in src/controller
file(GLOB EXEC_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/*.cc)
add_executable(${EXEC_NAME} WIN32 ${EXEC_SRC})
# if(SERVICE_BUILD_TYPE)
#     add_dependencies(${EXEC_NAME} ${SERVICE_EXE})
# else()
#     add_dependencies(${EXEC_NAME} ${SERVICE_DLL})
# endif()
add_dependencies(${EXEC_NAME} rgmsvc)
target_include_directories(${EXEC_NAME} 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/wtl/include ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/zlib
    PUBLIC ${PROJECT_BINARY_DIR}/zlib
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/controller  ./include
)
# 1. failure
#target_link_options(monitor
#    PRIVATE /SUBSYSTEM:WINDOWS
#    PUBLIC /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
#)
# 2. failure
# SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /level='requireAdministrator' /uiAccess='false'")
# 3. success
set_target_properties(${EXEC_NAME} PROPERTIES LINK_FLAGS /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'")
target_sources(${EXEC_NAME} PUBLIC
    ./assets/${EXEC_NAME}.rc
    ./assets/${EXEC_NAME}.ico
    # TODO: test if exists
    ./assets/blob.tgz
)
target_link_libraries(${EXEC_NAME} zlibstatic)
#
# custom command: check blob
#
add_custom_command(TARGET ${EXEC_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Before build ${EXEC_NAME}: check assets/blob ..."
)

#
# about CMAKE_BUILD_TYPE
#
message(STATUS "Has [CMAKE_BUILD_TYPE]? " ${CMAKE_BUILD_TYPE})
message(STATUS "Has [CMAKE_CONFIG_TYPE]? " ${CMAKE_CONFIG_TYPE})
# set(TEST_HAS_CONFIG_VAR $<CONFIG>)
# message(STATUS "Has <Config>? " "${TEST_HAS_CONFIG_VAR}")

#
# mock-custom-target: ...
#
add_custom_target(debug-only
    COMMAND ${CMAKE_COMMAND} -E echo "Is Debug Config? $<CONFIG:Debug>"
)

#
# Some GoogleTest (debug only)
#

# ? can not test condition $<CONFIG:Debug>
if((CMAKE_BUILD_TYPE EQUAL "Debug") OR (NOT THIS_DISABLE_DEBUG_TEST))
    add_subdirectory(test)
endif()
