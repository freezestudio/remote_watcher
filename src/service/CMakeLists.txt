#
# Service DLL
#

set(SERVICE_NAME rgmsvc)
add_library(${SERVICE_NAME} SHARED
    ./utils.cc
    ./entry.cc
)
target_include_directories(${SERVICE_NAME}
    PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/3rdpart/nats/src
    PRIVATE ./
)
target_sources(${SERVICE_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/assets/${SERVICE_NAME}.rc
)

#
# compress rgmsvc.dll and dependencies
#
set(SERVICE_FILE_NAME "${SERVICE_NAME}.dll")
# set(DEP_CORE_FILE_NAME "api-ms-win-core-file-l1-2-0.dll")
# more ...

if(CMAKE_BUILD_TYPE)
    set(SERVICE_FILE_PATH "${PROJECT_BINARY_DIR}/service/${SERVICE_FILE_NAME}")
else()
    set(SERVICE_FILE_PATH "${PROJECT_BINARY_DIR}/service/$<CONFIG>/${SERVICE_FILE_NAME}")
endif()
# set(DEP_CORE_FILE_PATH "D:/Windows Kits/10/bin/x64/${DEP_CORE_FILE_NAME}")
# more ...

add_custom_command(TARGET ${SERVICE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Make ${SERVICE_NAME} blob bundle ..."
    COMMAND ${CMAKE_COMMAND} -E copy ${SERVICE_FILE_PATH} ${PROJECT_SOURCE_DIR}/tools/${SERVICE_FILE_NAME}
    # COMMAND ${CMAKE_COMMAND} -E copy ${DEP_CORE_FILE_PATH} ${PROJECT_SOURCE_DIR}/tools/${DEP_CORE_FILE_NAME}
    # TODO: using cmake-tar
    # COMMAND ${CMAKE_COMMAND} -E chdir ${PROJECT_SOURCE_DIR}/tools minizip.exe -o -0 "${PROJECT_SOURCE_DIR}/assets/blob.zip" "${SERVICE_FILE_NAME}" "${DEP_CORE_FILE_NAME}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${PROJECT_SOURCE_DIR}/tools ${CMAKE_COMMAND} -E tar cz "${PROJECT_SOURCE_DIR}/assets/blob.tgz" -- "${SERVICE_FILE_NAME}" #"${DEP_CORE_FILE_NAME}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${PROJECT_SOURCE_DIR}/tools/${SERVICE_FILE_NAME}" #"${PROJECT_SOURCE_DIR}/tools/${DEP_CORE_FILE_NAME}"
    COMMAND ${CMAKE_COMMAND} -E echo "Make ${SERVICE_NAME} blob bundle done."
)
