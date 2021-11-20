cmake_minimum_required(VERSION 3.20)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(
    ${PROJECT_SOURCE_DIR}/3rdpart/nats/src
    ${PROJECT_SOURCE_DIR}/3rdpart/wtl/include
    googletest
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/controller
    ${PROJECT_SOURCE_DIR}/src/service
)
add_executable(service_utils_test
    ${PROJECT_SOURCE_DIR}/src/service/utils.cc
    ./service-utils.test.cc
)
target_link_libraries(gtest_main)
# gtest_discover_tests(service_utils_test)
