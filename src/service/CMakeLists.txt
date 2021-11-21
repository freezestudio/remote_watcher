
add_library(mnsvc SHARED
    ./utils.cc
    ./entry.cc
)
target_include_directories(mnsvc
    PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/3rdpart/nats/src
    PRIVATE ./
)
add_custom_command(TARGET mnsvc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "mnsvc project build done."
)