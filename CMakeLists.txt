cmake_minimum_required(VERSION 3.16)

project(pathfinder_mini_extreme_native LANGUAGES CXX)

add_library(pfme_portable_leaf STATIC
    src/characters/combat_rules.cpp
    src/dungeon/dungeontopology.cpp
    src/dungeon/riddles.cpp
    src/multiplayer/multiplayer_snapshot.cpp
    src/multiplayer/network_protocol.cpp
    src/session/game_session.cpp
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

if(BUILD_TESTING)
    add_test(NAME pfme_headless_smoke COMMAND pfme_headless)
    add_subdirectory(tests/native)
endif()
