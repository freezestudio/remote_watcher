cmake_minimum_required(VERSION 3.20)

project(remote_win32)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake
)
include(common)

include_directories(include)

add_executable(monitor src/controller/main.cc)
target_include_directories(monitor PUBLIC 3rdpart/wtl/include)

add_library(mnsvc SHARED src/service/entry.cc)
target_include_directories(mnsvc PUBLIC 3rdpart/nats/src)
