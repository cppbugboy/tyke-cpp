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
