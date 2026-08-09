if(NOT DEFINED WOKIEXT OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "WOKIEXT and TEST_ROOT are required")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/working")

foreach(language IN ITEMS c cpp)
    set(project_name "Test ${language} Extension")
    set(project_dir "${TEST_ROOT}/projects/test-${language}-extension")
    execute_process(
        COMMAND "${WOKIEXT}" create "${project_name}" --lang "${language}" --out "../projects"
        WORKING_DIRECTORY "${TEST_ROOT}/working"
        RESULT_VARIABLE create_result
        OUTPUT_VARIABLE create_output
        ERROR_VARIABLE create_error
    )
    if(NOT create_result EQUAL 0)
        message(FATAL_ERROR "wokiext create failed for ${language} (${create_result})\n${create_output}${create_error}")
    endif()

    file(READ "${project_dir}/CMakeLists.txt" project_contents)
    string(FIND "${project_contents}" "include(\"\${WOKI_CMAKE_DIR}/ExtensionProject.cmake\")\nproject(" toolchain_position)
    if(toolchain_position EQUAL -1)
        message(FATAL_ERROR "Generated ${language} project does not load ExtensionProject.cmake before project()")
    endif()

    execute_process(
        COMMAND "${WOKIEXT}" build "../projects/test-${language}-extension"
        WORKING_DIRECTORY "${TEST_ROOT}/working"
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_output
        ERROR_VARIABLE build_error
    )
    if(NOT build_result EQUAL 0 OR NOT EXISTS "${project_dir}/extension.wasm")
        message(FATAL_ERROR "wokiext build failed for generated ${language} project (${build_result})\n${build_output}${build_error}")
    endif()
endforeach()

execute_process(
    COMMAND "${WOKIEXT}" schema
    RESULT_VARIABLE schema_result
    OUTPUT_VARIABLE schema_output
)
if(NOT schema_result EQUAL 0 OR NOT schema_output MATCHES "Woki extension manifest")
    message(FATAL_ERROR "Embedded schema is unavailable")
endif()

file(MAKE_DIRECTORY "${TEST_ROOT}/root/extensions")
file(WRITE "${TEST_ROOT}/root/sentinel" "keep")
execute_process(
    COMMAND "${WOKIEXT}" remove ../sentinel --root "${TEST_ROOT}/root"
    RESULT_VARIABLE remove_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(remove_result EQUAL 0 OR NOT EXISTS "${TEST_ROOT}/root/sentinel")
    message(FATAL_ERROR "Traversal-like extension id was not safely rejected")
endif()
