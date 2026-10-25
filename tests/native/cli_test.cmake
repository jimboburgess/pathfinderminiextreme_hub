execute_process(COMMAND "${PROGRAM}" --interactive
    INPUT_FILE "${CMAKE_CURRENT_LIST_DIR}/cli_input.txt"
    OUTPUT_VARIABLE output ERROR_VARIABLE error RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR output MATCHES "error=")
    message(FATAL_ERROR "Interactive executable failed: ${result}\n${output}\n${error}")
endif()
foreach(expected "trusted local administrator" "player=1 name=Wizard room=0"
        "players=3 connected=3" "type=reconnected" "bye")
    string(FIND "${output}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing ${expected}: ${output}")
    endif()
endforeach()
