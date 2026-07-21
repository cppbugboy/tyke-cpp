cmake_minimum_required(VERSION 3.16)

if (MSVC)
    add_definitions(/utf-8)
endif ()

set(CONTROLLER_SOURCES
        controllers/example_request_controller.cpp
        controllers/example_response_controller.cpp
)

if (MSVC)
    set(MIMALLOC_LIB mimalloc-secure.dll.lib)
else ()
    set(MIMALLOC_LIB mimalloc-secure)
endif ()

message("THIRD_PARTY_LIB_DIR: " ${THIRD_PARTY_LIB_DIR})

add_executable(tyke_server_example server.cpp ${CONTROLLER_SOURCES})
target_include_directories(tyke_server_example PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${THIRD_PARTY_INCLUDE_DIR}
)
target_link_directories(tyke_server_example PRIVATE
        ${THIRD_PARTY_LIB_DIR}
)
target_link_libraries(tyke_server_example PRIVATE tyke ${MIMALLOC_LIB})


add_executable(tyke_client_example client.cpp ${CONTROLLER_SOURCES})
target_include_directories(tyke_client_example PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${THIRD_PARTY_INCLUDE_DIR}
)
target_link_directories(tyke_client_example PRIVATE
        ${THIRD_PARTY_LIB_DIR}
)
target_link_libraries(tyke_client_example PRIVATE tyke ${MIMALLOC_LIB})

if (MSVC)
    # 即使使用预编译库，在 Windows 上也建议开启强制覆盖
    target_link_options(tyke_server_example PRIVATE "/FORCE:MULTIPLE" "/INCLUDE:mi_version")
    target_link_options(tyke_client_example PRIVATE "/FORCE:MULTIPLE" "/INCLUDE:mi_version")

    # ---------- 需要复制的 DLL 文件列表 ----------
    set(WIN_DLL_FILES
            mimalloc-secure.dll
            mimalloc-redirect.dll
    )

    # ---------- 为每个可执行目标复制所有 DLL ----------
    foreach (exe IN ITEMS tyke_server_example tyke_client_example)
        foreach (dll IN LISTS WIN_DLL_FILES)
            add_custom_command(TARGET ${exe} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${THIRD_PARTY_LIB_DIR}/${dll}"
                    $<TARGET_FILE_DIR:${exe}>
                    COMMENT "Copying ${dll} to ${exe} output dir"
            )
        endforeach ()
    endforeach ()

    # 大数据量传输测试
    add_executable(large_transfer_test large_transfer_test.cpp)
    target_include_directories(large_transfer_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${THIRD_PARTY_INCLUDE_DIR}
    )
    target_link_directories(large_transfer_test PRIVATE
            ${THIRD_PARTY_LIB_DIR}
    )
    target_link_libraries(large_transfer_test PRIVATE tyke ${MIMALLOC_LIB})

    foreach (dll IN LISTS WIN_DLL_FILES)
        add_custom_command(TARGET large_transfer_test POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${THIRD_PARTY_LIB_DIR}/${dll}"
                $<TARGET_FILE_DIR:large_transfer_test>
                COMMENT "Copying ${dll} to large_transfer_test output dir"
        )
    endforeach ()
endif ()
