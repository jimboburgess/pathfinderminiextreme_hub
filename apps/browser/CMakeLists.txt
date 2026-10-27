# Explicit native-only tool: no dependency or framebuffer enters firmware.
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/title_calls.inc"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_title.py"
        "${PROJECT_SOURCE_DIR}/src/graphics/display.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/title_calls.inc"
    DEPENDS extract_title.py "${PROJECT_SOURCE_DIR}/src/graphics/display.cpp"
    VERBATIM)
add_executable(pfme_display_capture capture.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/title_calls.inc"
    "${PROJECT_SOURCE_DIR}/src/graphics/sprites.cpp"
    third_party/adafruit_gfx/Adafruit_GFX.cpp)
target_include_directories(pfme_display_capture PRIVATE host third_party/adafruit_gfx
    "${CMAKE_CURRENT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(pfme_display_capture PRIVATE ARDUINO=100)
target_compile_features(pfme_display_capture PRIVATE cxx_std_11)
set(viewer "${CMAKE_CURRENT_BINARY_DIR}/viewer")
add_custom_target(pfme_browser_viewer ALL
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_viewer.py"
        $<TARGET_FILE:pfme_display_capture> "${viewer}" "${CMAKE_CURRENT_SOURCE_DIR}/web"
    DEPENDS pfme_display_capture package_viewer.py web/index.html web/viewer.js web/viewer.css
    VERBATIM)
if(BUILD_TESTING)
    add_test(NAME pfme_display_reference
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_viewer.py"
            $<TARGET_FILE:pfme_display_capture> "${CMAKE_CURRENT_BINARY_DIR}/test-viewer"
            "${CMAKE_CURRENT_SOURCE_DIR}/web")
    find_program(PFME_NODE_EXECUTABLE NAMES node)
    if(PFME_NODE_EXECUTABLE)
        add_test(NAME pfme_viewer_logic COMMAND "${PFME_NODE_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_viewer.cjs" "${viewer}")
    endif()
endif()
