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

set(PFME_MONOCYPHER_DIR "" CACHE PATH "Offline directory containing pinned Monocypher sources")
if(NOT PFME_MONOCYPHER_DIR)
    set(PFME_MONOCYPHER_DIR "${CMAKE_BINARY_DIR}/_deps/monocypher")
    file(MAKE_DIRECTORY "${PFME_MONOCYPHER_DIR}")
endif()
foreach(entry
    "monocypher.c|57eb914fc88136119bd41655cccb8c250048bf54d470540625186f8ab16f64be"
    "monocypher.h|c494da712122da7ff679fdcf318a5317e84972b6c950fe9d896212947797facd"
    "LICENCE.md|5f8360e4c06ddcc584bdb4b210c6af824c4bb301e6a9a521869b6d90795ca4b3")
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 name)
    list(GET parts 1 checksum)
    if(NOT EXISTS "${PFME_MONOCYPHER_DIR}/${name}")
        if(name STREQUAL "LICENCE.md")
            set(source "${name}")
        else()
            set(source "src/${name}")
        endif()
        file(DOWNLOAD "https://raw.githubusercontent.com/LoupVaillant/Monocypher/4.0.3/${source}"
            "${PFME_MONOCYPHER_DIR}/${name}" EXPECTED_HASH "SHA256=${checksum}" TLS_VERIFY ON)
    endif()
    file(SHA256 "${PFME_MONOCYPHER_DIR}/${name}" actual)
    if(NOT actual STREQUAL checksum)
        message(FATAL_ERROR "Monocypher source checksum mismatch")
    endif()
endforeach()
enable_language(C)
find_package(Threads REQUIRED)
add_library(pfme_server_support STATIC credentials.cpp protocol.cpp service.cpp serve_identity.cpp
    boost_json.cpp "${PFME_MONOCYPHER_DIR}/monocypher.c")
target_include_directories(pfme_server_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(pfme_server_support SYSTEM PUBLIC
    "${pfme_boost_SOURCE_DIR}" "${PFME_MONOCYPHER_DIR}")
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
install(FILES "${PFME_MONOCYPHER_DIR}/LICENCE.md" DESTINATION share/licenses/pfme_server/monocypher)
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
