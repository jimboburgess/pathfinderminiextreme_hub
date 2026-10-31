cmake_minimum_required(VERSION 3.18)

project(pathfinder_mini_extreme_native LANGUAGES CXX)

add_library(pfme_portable_leaf STATIC
    src/characters/combat_rules.cpp
    src/dungeon/dungeontopology.cpp
    src/dungeon/puzzleinstructions.cpp
    src/dungeon/riddles.cpp
    src/multiplayer/multiplayer_snapshot.cpp
    src/multiplayer/network_protocol.cpp
    src/session/game_session.cpp
    src/session/party_registry.cpp
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

add_library(pfme_headless_cli STATIC apps/headless/interactive.cpp)
target_link_libraries(pfme_headless_cli PUBLIC pfme_portable_leaf)
target_include_directories(pfme_headless_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/apps/headless)
set_target_properties(pfme_headless_cli PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(MSVC)
    target_compile_options(pfme_headless_cli PRIVATE /W4)
else()
    target_compile_options(pfme_headless_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(pfme_headless
    apps/headless/main.cpp
)

target_link_libraries(pfme_headless PRIVATE pfme_headless_cli)

set_target_properties(pfme_headless PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(pfme_headless PRIVATE /W4)
else()
    target_compile_options(pfme_headless PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS pfme_headless RUNTIME DESTINATION bin)

include(CTest)

if(BUILD_TESTING AND WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(
        PFME_WINDOWS_CXX_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    configure_file(
        cmake/CTestWindowsRuntime.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/CTestWindowsRuntime.cmake"
        @ONLY)
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
        "${CMAKE_CURRENT_BINARY_DIR}/CTestWindowsRuntime.cmake")
endif()

if(BUILD_TESTING)
    add_test(NAME pfme_headless_smoke COMMAND pfme_headless)
    add_subdirectory(tests/native)
endif()

option(PFME_BUILD_PLAYABLE "Compile the original handheld runtime for private native play" OFF)
if(PFME_BUILD_PLAYABLE)
    add_subdirectory(apps/runtime)
endif()
option(PFME_BUILD_SERVER "Build the private loopback WebSocket adapter" OFF)
if(PFME_BUILD_SERVER)
    add_subdirectory(apps/server)
endif()

option(PFME_BUILD_BROWSER_CAPTURE "Build the original handheld title-screen capture and offline viewer" OFF)
if(PFME_BUILD_BROWSER_CAPTURE)
    add_subdirectory(apps/browser)
endif()
