cmake_minimum_required(VERSION 3.20)

include_directories(
    ${PROJECT_SOURCE_DIR}/3rdpart/nats/src
    ${PROJECT_SOURCE_DIR}/3rdpart/wtl/include
    ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest
    ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest/include
    ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest/src
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/controller
    ${PROJECT_SOURCE_DIR}/src/service
)

# file(GLOB gtest_src CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest/src/*.cc)
add_executable(service_utils_test
    ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest/src/gtest-all.cc
    ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest/src/gtest_main.cc
    ./service-utils.test.cc
)
