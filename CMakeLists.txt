cmake_minimum_required(VERSION 3.25)

project(AKEngine
    VERSION 10.8.0
    DESCRIPTION "AK Engine bootstrap"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif()

if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif()

option(AK_BUILD_EDITOR "Build AK Engine editor" ON)
option(AK_BUILD_PLAYER "Build AK Engine player/runtime executable" ON)
option(AK_BUILD_TOOLS "Build AK Engine command line tools" ON)
option(AK_ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

add_compile_definitions(
    AK_ENGINE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    AK_ENGINE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    AK_ENGINE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(AK_DEBUG=1)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
    if(AK_ENABLE_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(AK_ENABLE_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

add_subdirectory(engine)

if(AK_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(AK_BUILD_PLAYER)
    add_subdirectory(runtime)
endif()

if(AK_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
