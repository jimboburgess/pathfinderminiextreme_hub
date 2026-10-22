cmake_minimum_required(VERSION 3.16)

project(pathfinder_mini_extreme_native LANGUAGES CXX)

add_library(pfme_portable_leaf STATIC
    src/dungeon/riddles.cpp
    src/multiplayer/multiplayer_snapshot.cpp
    src/multiplayer/network_protocol.cpp
)

target_include_directories(pfme_portable_leaf
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(pfme_portable_leaf PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(pfme_portable_leaf PRIVATE /W4)
else()
    target_compile_options(pfme_portable_leaf PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests/native)
endif()
