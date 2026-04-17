cmake_minimum_required(VERSION 3.16)

project(tyke_examples VERSION 1.0.0 LANGUAGES CXX)

set(SERVER_SOURCES
    server.cpp
    test_request_controller.cpp
    test_request_filter.cpp
    test_response_controller.cpp
    test_response_filter.cpp
)

set(CLIENT_SOURCES
    client.cpp
)

add_executable(server ${SERVER_SOURCES})
target_include_directories(server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/include
)
target_link_libraries(server PRIVATE tyke)

add_executable(client ${CLIENT_SOURCES})
target_include_directories(client PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/include
)
target_link_libraries(client PRIVATE tyke)

set_target_properties(server client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    target_compile_options(server PRIVATE /utf-8 /W4)
    target_compile_options(client PRIVATE /utf-8 /W4)
else()
    target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_INSTALL_EXAMPLES)
    include(GNUInstallDirs)
    install(TARGETS server client
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
    )
endif()
