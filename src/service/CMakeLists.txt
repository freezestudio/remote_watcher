#
# rgmsvc service
#

option(SERVICE_EXEC_TYPE "use exe service? otherwise dll" ON)

if(SERVICE_EXEC_TYPE)
    #
    # SERVICE EXE
    #
    set(SERVICE_EXE rgmsvc)
    add_executable(${SERVICE_EXE}
        ./main.cc
        ./utils.cc
    )
    target_include_directories(${SERVICE_EXE}
        PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/3rdpart/nats/src
        PRIVATE ./
    )
    target_sources(${SERVICE_EXE} PUBLIC
        ${PROJECT_SOURCE_DIR}/assets/${SERVICE_EXE}.rc
    )

    set(SERVICE_EXE_NAME "${SERVICE_EXE}.exe")
    # set(DEP_CORE_FILE_NAME "api-ms-win-core-file-l1-2-0.dll")
    # more ...

    if(CMAKE_BUILD_TYPE)
        set(SERVICE_EXE_PATH "${PROJECT_BINARY_DIR}/service/${SERVICE_EXE_NAME}")
    else()
        set(SERVICE_EXE_PATH "${PROJECT_BINARY_DIR}/service/$<CONFIG>/${SERVICE_EXE_NAME}")
    endif()
    # set(DEP_CORE_FILE_PATH "D:/Windows Kits/10/bin/x64/${DEP_CORE_FILE_NAME}")
    # more ...

    add_custom_command(TARGET ${SERVICE_EXE} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Make ${SERVICE_EXE} blob bundle ..."
        COMMAND ${CMAKE_COMMAND} -E copy ${SERVICE_EXE_PATH} ${PROJECT_SOURCE_DIR}/tools/${SERVICE_EXE_NAME}
        COMMAND ${CMAKE_COMMAND} -E chdir ${PROJECT_SOURCE_DIR}/tools 
            ${CMAKE_COMMAND} -E tar cz "${PROJECT_SOURCE_DIR}/assets/blob.tgz" -- 
                "${SERVICE_EXE_NAME}"
                #"${DEP_CORE_FILE_NAME}"
                # more ...
        COMMAND ${CMAKE_COMMAND} -E rm -f 
            "${PROJECT_SOURCE_DIR}/tools/${SERVICE_DLL_NAME}"
            #"${PROJECT_SOURCE_DIR}/tools/${DEP_CORE_FILE_NAME}"
            # more ...
        COMMAND ${CMAKE_COMMAND} -E echo "Make ${SERVICE_DLL} blob bundle done."
    )
else()
    #
    # Service DLL
    #

    set(SERVICE_DLL rgmsvc)
    add_library(${SERVICE_DLL} SHARED
        ./utils.cc
        ./entry.cc
    )
    target_compile_definitions(${SERVICE_DLL} PUBLIC RGMSVC_EXPORTS)
    target_include_directories(${SERVICE_DLL}
        PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/3rdpart/nats/src
        PRIVATE ./
    )
    target_sources(${SERVICE_DLL} PUBLIC
        ${PROJECT_SOURCE_DIR}/assets/${SERVICE_DLL}.rc
    )

    #
    # compress rgmsvc.dll and dependencies
    #
    set(SERVICE_DLL_NAME "${SERVICE_DLL}.dll")
    # set(DEP_CORE_FILE_NAME "api-ms-win-core-file-l1-2-0.dll")
    # more ...

    if(CMAKE_BUILD_TYPE)
        set(SERVICE_DLL_PATH "${PROJECT_BINARY_DIR}/service/${SERVICE_DLL_NAME}")
    else()
        set(SERVICE_DLL_PATH "${PROJECT_BINARY_DIR}/service/$<CONFIG>/${SERVICE_DLL_NAME}")
    endif()
    # set(DEP_CORE_FILE_PATH "D:/Windows Kits/10/bin/x64/${DEP_CORE_FILE_NAME}")
    # more ...

    add_custom_command(TARGET ${SERVICE_DLL} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Make ${SERVICE_DLL} blob bundle ..."
        COMMAND ${CMAKE_COMMAND} -E copy ${SERVICE_DLL_PATH} ${PROJECT_SOURCE_DIR}/tools/${SERVICE_DLL_NAME}
        # COMMAND ${CMAKE_COMMAND} -E copy ${DEP_CORE_FILE_PATH} ${PROJECT_SOURCE_DIR}/tools/${DEP_CORE_FILE_NAME}
        # TODO: using cmake-tar
        # COMMAND ${CMAKE_COMMAND} -E chdir ${PROJECT_SOURCE_DIR}/tools minizip.exe -o -0 "${PROJECT_SOURCE_DIR}/assets/blob.zip" "${SERVICE_DLL_NAME}" "${DEP_CORE_FILE_NAME}"
        COMMAND ${CMAKE_COMMAND} -E chdir ${PROJECT_SOURCE_DIR}/tools ${CMAKE_COMMAND} -E tar cz "${PROJECT_SOURCE_DIR}/assets/blob.tgz" -- "${SERVICE_DLL_NAME}" #"${DEP_CORE_FILE_NAME}"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${PROJECT_SOURCE_DIR}/tools/${SERVICE_DLL_NAME}" #"${PROJECT_SOURCE_DIR}/tools/${DEP_CORE_FILE_NAME}"
        COMMAND ${CMAKE_COMMAND} -E echo "Make ${SERVICE_DLL} blob bundle done."
    )

endif()
