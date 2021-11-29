cmake_minimum_required(VERSION 3.20)

include_directories(
    ${PROJECT_SOURCE_DIR}/3rdpart/nats/src
    ${PROJECT_SOURCE_DIR}/3rdpart/wtl/include
    ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest
    ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest/include
    ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest/src
    ${PROJECT_BINARY_DIR}/zlib
    ${PROJECT_SOURCE_DIR}/3rdpart/zlib
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/controller
    ${PROJECT_SOURCE_DIR}/src/service
)

# file(GLOB gtest_src CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest/src/*.cc)
# add_executable(service_utils_test
#     ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest/src/gtest-all.cc
#     ${PROJECT_SOURCE_DIR}/3rdpart/googletest/googletest/src/gtest_main.cc
#     ./service-utils.test.cc
# )

#
# $<CONFIG>: config
# ${CMAKE_<CONFIG>_POSTFIX}: $<$<CONFIG:Debug>:d>
# $<CONFIG:cfg-list>: include=1, else 0
#
# link_libraries(
#     ${PROJECT_BINARY_DIR}/lib/$<CONFIG>/gtest$<$<CONFIG:Debug>:d>.lib
#     ${PROJECT_BINARY_DIR}/lib/$<CONFIG>/gtest_main$<$<CONFIG:Debug>:d>.lib
# )

link_libraries(gtest gtest_main)

#
# executor
#
add_executable(service_utils_test
    service-utils.test.cc
)

add_executable(service_zlib_test service-zlib.test.cc)
