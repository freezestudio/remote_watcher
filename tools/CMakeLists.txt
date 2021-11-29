include_directories(
    ${PROJECT_SOURCE_DIR}/3rdpart/zlib
    ${PROJECT_BINARY_DIR}/zlib
    ${PROJECT_SOURCE_DIR}/3rdpart/zlib/contrib/minizip
)
link_libraries(zlibstatic)

add_executable(miniunz
    ${PROJECT_SOURCE_DIR}/3rdpart/zlib/contrib/minizip/miniunz.c
    ${PROJECT_SOURCE_DIR}/3rdpart/zlib/contrib/minizip/unzip.c
    ${PROJECT_SOURCE_DIR}/3rdpart/zlib/contrib/minizip/ioapi.c
    ${PROJECT_SOURCE_DIR}/3rdpart/zlib/contrib/minizip/iowin32.c
)
add_custom_command(TARGET miniunz POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/tools/$<CONFIG>/miniunz.exe ${PROJECT_SOURCE_DIR}/tools/miniunz.exe
)

add_executable(minizip
    ${PROJECT_SOURCE_DIR}/3rdpart/zlib/contrib/minizip/minizip.c
    ${PROJECT_SOURCE_DIR}/3rdpart/zlib/contrib/minizip/zip.c
    ${PROJECT_SOURCE_DIR}/3rdpart/zlib/contrib/minizip/ioapi.c
    ${PROJECT_SOURCE_DIR}/3rdpart/zlib/contrib/minizip/iowin32.c
)
add_custom_command(TARGET minizip POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/tools/$<CONFIG>/minizip.exe ${PROJECT_SOURCE_DIR}/tools/minizip.exe
)
