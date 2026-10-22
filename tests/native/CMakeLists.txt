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
