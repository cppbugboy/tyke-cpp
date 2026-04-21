cmake_minimum_required(VERSION 3.16)

project(tyke VERSION 1.0.0 LANGUAGES CXX)

file(GLOB_RECURSE TYKE_INC include/*.h)
file(GLOB_RECURSE TYKE_SRC src/*.cpp)


option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_BOTH_LIBS "Build both static and shared libraries" OFF)

if(BUILD_BOTH_LIBS)
    add_library(tyke_static STATIC ${TYKE_INC} ${TYKE_SRC})
    add_library(tyke_shared SHARED ${TYKE_INC} ${TYKE_SRC})
    
    target_compile_definitions(tyke_shared PRIVATE TYKE_BUILDING_DLL)
    
    foreach(TARGET tyke_static tyke_shared)
        target_include_directories(${TARGET}
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include>
            PRIVATE
                ${CMAKE_SOURCE_DIR}/third_party/include
        )
        
        target_compile_definitions(${TARGET}
            PUBLIC FMT_HEADER_ONLY
        )
        
        target_link_directories(${TARGET}
            PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/lib/${CMAKE_SYSTEM_NAME}>
        )
        
        target_link_libraries(${TARGET}
            PUBLIC crypto
        )
        
        if(MSVC)
            target_compile_options(${TARGET} PRIVATE /utf-8 /W4)
            if(CMAKE_BUILD_TYPE STREQUAL "Release")
                target_compile_options(${TARGET} PRIVATE /O2)
            endif()
        else()
            target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wpedantic -fvisibility=hidden)
            if(CMAKE_BUILD_TYPE STREQUAL "Release")
                target_compile_options(${TARGET} PRIVATE -O2)
            endif()
        endif()
        
        set_target_properties(${TARGET} PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION 1
            POSITION_INDEPENDENT_CODE ON
        )
    endforeach()
    
    set_target_properties(tyke_static PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/static"
        OUTPUT_NAME tyke
    )
    
    set_target_properties(tyke_shared PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/shared"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/shared"
        OUTPUT_NAME tyke
    )
    
    if(WIN32)
        set_target_properties(tyke_shared PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS OFF
        )
    endif()
    
else()
    if(BUILD_SHARED_LIBS)
        add_library(tyke SHARED ${TYKE_INC} ${TYKE_SRC})
        target_compile_definitions(tyke PRIVATE TYKE_BUILDING_DLL)
    else()
        add_library(tyke STATIC ${TYKE_INC} ${TYKE_SRC})
    endif()
    
    target_include_directories(tyke
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_SOURCE_DIR}/third_party/include
    )
    
    target_compile_definitions(tyke
        PUBLIC FMT_HEADER_ONLY
    )
    
    target_link_directories(tyke
        PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/lib/${CMAKE_SYSTEM_NAME}>
    )
    
    target_link_libraries(tyke
        PUBLIC crypto ssl
    )
    
    if(MSVC)
        target_compile_options(tyke PRIVATE /utf-8 /W4)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_compile_options(tyke PRIVATE /O2)
        endif()
    else()
        target_compile_options(tyke PRIVATE -Wall -Wextra -Wpedantic -fvisibility=hidden)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_compile_options(tyke PRIVATE -O2)
        endif()
    endif()
    
    set_target_properties(tyke PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        POSITION_INDEPENDENT_CODE ON
    )
    
    if(BUILD_SHARED_LIBS AND WIN32)
        set_target_properties(tyke PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS OFF
            OUTPUT_NAME tyke
        )
    endif()
endif()

include(GNUInstallDirs)

if(BUILD_BOTH_LIBS)
    install(TARGETS tyke_static tyke_shared
        EXPORT tykeTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
else()
    install(TARGETS tyke
        EXPORT tykeTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(EXPORT tykeTargets
    FILE tykeTargets.cmake
    NAMESPACE tyke::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tyke
)

message(STATUS "=== Tyke Library Configuration ===")
message(STATUS "BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
message(STATUS "BUILD_BOTH_LIBS: ${BUILD_BOTH_LIBS}")
if(BUILD_BOTH_LIBS)
    message(STATUS "Library type: both static and shared")
elseif(BUILD_SHARED_LIBS)
    message(STATUS "Library type: shared")
else()
    message(STATUS "Library type: static")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "================================")
