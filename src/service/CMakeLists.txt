
add_library(mnsvc SHARED
    ./utils.cc
    ./entry.cc
)
target_include_directories(mnsvc
    PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/3rdpart/nats/src
    PRIVATE ./
)
