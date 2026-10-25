add_executable(pfme_native_tests
    portable_tests.cpp
)

target_link_libraries(pfme_native_tests PRIVATE pfme_portable_leaf)

set_target_properties(pfme_native_tests PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(pfme_native_tests PRIVATE /W4)
else()
    target_compile_options(pfme_native_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME pfme_native_tests COMMAND pfme_native_tests)

add_executable(pfme_dungeon_tests
    dungeon_tests.cpp
)

target_link_libraries(pfme_dungeon_tests PRIVATE pfme_portable_leaf)

set_target_properties(pfme_dungeon_tests PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(pfme_dungeon_tests PRIVATE /W4)
else()
    target_compile_options(pfme_dungeon_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(
    NAME pfme_dungeon_regression
    COMMAND pfme_dungeon_tests --self-test
)

add_test(
    NAME pfme_dungeon_batch
    COMMAND pfme_dungeon_tests --seed 1 --count 1000 --json
)
