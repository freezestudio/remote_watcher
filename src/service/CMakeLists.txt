
add_library(rgmsvc SHARED
    ./utils.cc
    ./entry.cc
)
target_include_directories(rgmsvc
    PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/3rdpart/nats/src
    PRIVATE ./
)

#
# compress rgmsvc.dll and dependencies
#
add_custom_command(TARGET rgmsvc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "rgmsvc project build done."
)