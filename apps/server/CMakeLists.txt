# Everything here is native-only. PlatformIO never visits apps/ or build/_deps.
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(pfme_boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-b2-nodocs.tar.xz
    URL_HASH SHA256=ea7b982002cc9dfbe59b0b217b206f470dc75f3de0bb2973d844118934d82411
    SOURCE_SUBDIR pfme_no_cmake_subdirectory)
FetchContent_MakeAvailable(pfme_boost)

set(PFME_MONOCYPHER_DIR "" CACHE PATH
    "Offline Monocypher 4.0.3 tree containing src/ and LICENCE.md")
if(PFME_MONOCYPHER_DIR)
    set(pfme_monocypher_SOURCE_DIR "${PFME_MONOCYPHER_DIR}")
else()
    FetchContent_Declare(pfme_monocypher
        URL https://monocypher.org/download/monocypher-4.0.3.tar.gz
        URL_HASH SHA256=8cc9bc341a66249016db9bd70e9142d8d0aef9945973744b1ac05dbc55d8ee66
        SOURCE_SUBDIR pfme_no_cmake_subdirectory)
    FetchContent_MakeAvailable(pfme_monocypher)
endif()
set(PFME_MONOCYPHER_SOURCE "${pfme_monocypher_SOURCE_DIR}/src/monocypher.c")
set(PFME_MONOCYPHER_INCLUDE_DIR "${pfme_monocypher_SOURCE_DIR}/src")
set(PFME_MONOCYPHER_LICENSE "${pfme_monocypher_SOURCE_DIR}/LICENCE.md")
foreach(required
    "${PFME_MONOCYPHER_SOURCE}"
    "${PFME_MONOCYPHER_INCLUDE_DIR}/monocypher.h"
    "${PFME_MONOCYPHER_LICENSE}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Pinned Monocypher 4.0.3 tree is incomplete: ${required}")
    endif()
endforeach()
enable_language(C)
find_package(Threads REQUIRED)
add_library(pfme_server_support STATIC credentials.cpp protocol.cpp service.cpp serve_identity.cpp
    boost_json.cpp "${PFME_MONOCYPHER_SOURCE}")
target_include_directories(pfme_server_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(pfme_server_support SYSTEM PUBLIC
    "${pfme_boost_SOURCE_DIR}" "${PFME_MONOCYPHER_INCLUDE_DIR}")
target_link_libraries(pfme_server_support PUBLIC pfme_portable_leaf Threads::Threads)
if(WIN32)
    target_link_libraries(pfme_server_support PUBLIC ws2_32 mswsock bcrypt)
    target_compile_definitions(pfme_server_support PUBLIC _WIN32_WINNT=0x0601)
endif()
target_compile_features(pfme_server_support PUBLIC cxx_std_17)
if(PFME_BUILD_PLAYABLE)
    target_sources(pfme_server_support PRIVATE screen_stream.cpp)
    target_compile_definitions(pfme_server_support PUBLIC PFME_HAS_PLAYABLE=1)
    target_link_libraries(pfme_server_support PUBLIC pfme_handheld_runtime)
endif()
add_executable(pfme_server main.cpp transport.cpp)
target_link_libraries(pfme_server PRIVATE pfme_server_support)
if(PFME_BUILD_PLAYABLE)
    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/web_assets.inc"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/embed_web.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/web" "${CMAKE_CURRENT_BINARY_DIR}/web_assets.inc"
        DEPENDS embed_web.py web/play.html web/play.js web/play.css web/audio.js VERBATIM)
    target_sources(pfme_server PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/web_assets.inc")
    target_include_directories(pfme_server PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endif()
if(MSVC)
    target_compile_options(pfme_server PRIVATE /W4)
else()
    target_compile_options(pfme_server PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pfme_server_support PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>)
endif()
install(TARGETS pfme_server RUNTIME DESTINATION bin)
if(PFME_BUILD_PLAYABLE)
    install(FILES "${PROJECT_SOURCE_DIR}/apps/browser/third_party/adafruit_gfx/license.txt"
        DESTINATION share/licenses/pfme_server/adafruit_gfx)
endif()
install(FILES "${pfme_boost_SOURCE_DIR}/LICENSE_1_0.txt" DESTINATION share/licenses/pfme_server/boost)
install(FILES "${PFME_MONOCYPHER_LICENSE}" DESTINATION share/licenses/pfme_server/monocypher)
if(BUILD_TESTING)
    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    add_test(NAME pfme_server_lifetime COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/server/run_lifetime.py" "$<TARGET_FILE:pfme_server>")
    if(PFME_BUILD_PLAYABLE)
        add_executable(pfme_playable_service_tests ../../tests/server/playable_service_tests.cpp)
        target_link_libraries(pfme_playable_service_tests PRIVATE pfme_server_support)
        add_test(NAME pfme_playable_service COMMAND pfme_playable_service_tests)
        add_test(NAME pfme_playable_wire COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/server/run_playable.py" $<TARGET_FILE:pfme_server>)
        set_tests_properties(pfme_playable_wire PROPERTIES TIMEOUT 60)
        find_program(PFME_CLIENT_NODE NAMES node)
        if(PFME_CLIENT_NODE)
            add_test(NAME pfme_browser_client COMMAND "${PFME_CLIENT_NODE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/server/browser_client_tests.cjs"
                "${CMAKE_CURRENT_SOURCE_DIR}/web/play.js")
            add_test(NAME pfme_browser_pixel_interop COMMAND "${PFME_CLIENT_NODE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/server/browser_client_tests.cjs"
                "${CMAKE_CURRENT_SOURCE_DIR}/web/play.js"
                "${CMAKE_CURRENT_BINARY_DIR}/resync-packets.json"
                "${CMAKE_CURRENT_BINARY_DIR}/resync-pixels.rgb565")
            set_tests_properties(pfme_browser_pixel_interop PROPERTIES DEPENDS pfme_playable_service)
        endif()
    endif()
    add_executable(pfme_ws_security_tests ../../tests/server/security_tests.cpp)
    target_link_libraries(pfme_ws_security_tests PRIVATE pfme_server_support)
    add_test(NAME pfme_websocket_security
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/server/run_security.py"
            $<TARGET_FILE:pfme_server> $<TARGET_FILE:pfme_ws_security_tests>)
    set_tests_properties(pfme_websocket_security PROPERTIES TIMEOUT 90)
endif()

if(BUILD_TESTING AND PFME_BUILD_PLAYABLE)
    foreach(party_size RANGE 1 4)
        add_test(NAME pfme_party_wire_${party_size} COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/server/run_party.py" $<TARGET_FILE:pfme_server> ${party_size})
        set_tests_properties(pfme_party_wire_${party_size} PROPERTIES TIMEOUT 360 RUN_SERIAL TRUE)
    endforeach()
endif()

if(BUILD_TESTING AND PFME_BUILD_PLAYABLE AND PFME_CLIENT_NODE)
    add_test(NAME pfme_browser_audio COMMAND "${PFME_CLIENT_NODE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/server/audio_client_tests.cjs" "${CMAKE_CURRENT_SOURCE_DIR}/web/audio.js")
endif()

if(BUILD_TESTING)
    add_executable(pfme_serve_identity_tests ../../tests/server/serve_identity_tests.cpp)
    target_link_libraries(pfme_serve_identity_tests PRIVATE pfme_server_support)
    add_test(NAME pfme_serve_identity COMMAND pfme_serve_identity_tests)
    add_test(NAME pfme_cloudflare_options COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/server/cloudflare_native_options.py" "$<TARGET_FILE:pfme_server>")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import cryptography" RESULT_VARIABLE PFME_HAVE_CRYPTOGRAPHY OUTPUT_QUIET ERROR_QUIET)
    if(PFME_HAVE_CRYPTOGRAPHY EQUAL 0)
        add_test(NAME pfme_cloudflare_jwt COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/server/cloudflare_jwt_tests.py")
    else()
        message(STATUS "Cloudflare JWT tests unavailable: selected Python lacks cryptography; production gateway requires it")
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Explicit test-only target; never installed or part of the default build.
        add_executable(pfme_server_expiry_fixture EXCLUDE_FROM_ALL main.cpp transport.cpp serve_identity.cpp)
        target_link_libraries(pfme_server_expiry_fixture PRIVATE pfme_server_support)
        target_compile_definitions(pfme_server_expiry_fixture PRIVATE PFME_TEST_SERVE_LIFETIME=1)
        if(PFME_BUILD_PLAYABLE)
            target_sources(pfme_server_expiry_fixture PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/web_assets.inc")
            target_include_directories(pfme_server_expiry_fixture PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
    endif()
endif()

if(BUILD_TESTING AND PFME_BUILD_PLAYABLE AND PFME_CLIENT_NODE)
    add_test(NAME pfme_browser_private_client COMMAND "${PFME_CLIENT_NODE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/server/browser_client_tests.cjs"
        "${CMAKE_CURRENT_SOURCE_DIR}/web/play.js" --serve)
endif()
