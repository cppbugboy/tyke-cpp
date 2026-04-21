cmake_minimum_required(VERSION 3.16)

if (MSVC)
    add_definitions(/utf-8)
endif()

set(CONTROLLER_SOURCES
    controllers/example_request_controller.cpp
    controllers/example_response_controller.cpp
)

add_executable(tyke_server_example server.cpp ${CONTROLLER_SOURCES})
target_link_libraries(tyke_server_example PRIVATE tyke)
target_include_directories(tyke_server_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/include
)

add_executable(tyke_client_example client.cpp ${CONTROLLER_SOURCES})
target_link_libraries(tyke_client_example PRIVATE tyke)
target_include_directories(tyke_client_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/include
)

# Auto-copy OpenSSL DLLs on Windows
if(WIN32)
    foreach(TARGET tyke_server_example tyke_client_example)
        # Try to find libcrypto and libssl DLLs
        find_file(LIBCRYPTO_DLL
            NAMES libcrypto-3-x64.dll libcrypto-1_1-x64.dll libcrypto.dll
            PATHS "C:/Windows/System32" "C:/Program Files/Git/mingw64/bin"
            NO_DEFAULT_PATH
        )
        find_file(LIBSSL_DLL
            NAMES libssl-3-x64.dll libssl-1_1-x64.dll libssl.dll
            PATHS "C:/Windows/System32" "C:/Program Files/Git/mingw64/bin"
            NO_DEFAULT_PATH
        )
        
        if(LIBCRYPTO_DLL)
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${LIBCRYPTO_DLL}"
                    "$<TARGET_FILE_DIR:${TARGET}>"
                COMMENT "Copying OpenSSL crypto DLL to output directory"
            )
        endif()
        
        if(LIBSSL_DLL)
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${LIBSSL_DLL}"
                    "$<TARGET_FILE_DIR:${TARGET}>"
                COMMENT "Copying OpenSSL SSL DLL to output directory"
            )
        endif()
    endforeach()
endif()
