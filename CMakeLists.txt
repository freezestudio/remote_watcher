cmake_minimum_required(VERSION 3.20)

project(remote_win32)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake
)
include(common)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB MONITOR_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/*.cc)
add_executable(monitor ${MONITOR_SRC})
target_include_directories(monitor 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/controller 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/wtl/include
)

add_library(mnsvc SHARED
    src/service/entry.cc
)
target_include_directories(mnsvc 
    PUBLIC 3rdpart/nats/src
    PRIVATE src/service
)

add_subdirectory(test)
