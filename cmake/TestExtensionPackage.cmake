if(NOT DEFINED SOURCE_DIR OR NOT DEFINED PACKAGE_DIR OR NOT DEFINED WOKIEXT)
    message(FATAL_ERROR "SOURCE_DIR, PACKAGE_DIR, and WOKIEXT are required")
endif()

set(SOURCE_MANIFEST "${SOURCE_DIR}/manifest.yaml")
include("${CMAKE_CURRENT_LIST_DIR}/VerifyExtensionPackage.cmake")

set(PROJECT_DIR "${SOURCE_DIR}")
set(STATE_FILE "${PACKAGE_DIR}/.woki-state")
include("${CMAKE_CURRENT_LIST_DIR}/CheckExtensionState.cmake")

execute_process(
    COMMAND "${WOKIEXT}" verify "${PACKAGE_DIR}"
    RESULT_VARIABLE verify_result
    OUTPUT_VARIABLE verify_output
    ERROR_VARIABLE verify_error
)
if(NOT verify_result EQUAL 0)
    message(FATAL_ERROR "Kitty package verification failed (${verify_result})\n${verify_output}${verify_error}")
endif()
