cmake_minimum_required(VERSION 3.25)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "*.cpp"
)

add_module_test(extension_tests
    SOURCES
        ${SOURCES}
    LIBRARIES
        woki::extension
        Catch2::Catch2WithMain
)

target_compile_definitions(extension_tests PRIVATE
    WOKI_EXTENSION_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    WOKI_KITTY_PACKAGE_DIR="${CMAKE_BINARY_DIR}/extension-packages/$<CONFIG>/kitty"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_woki_contract_llvm_hints)
if(DEFINED ENV{WOKI_LLVM_PREFIX})
    list(APPEND _woki_contract_llvm_hints "$ENV{WOKI_LLVM_PREFIX}/bin")
endif()
find_program(WOKI_CONTRACT_CLANG NAMES clang HINTS ${_woki_contract_llvm_hints} REQUIRED)
find_program(WOKI_CONTRACT_CLANGXX NAMES clang++ HINTS ${_woki_contract_llvm_hints} REQUIRED)
target_compile_definitions(extension_tests PRIVATE
    WOKI_TEST_WASM_CLANGXX="${WOKI_CONTRACT_CLANGXX}"
)
set(WOKI_CONTRACT_COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/check_contract.py"
    --cc "${WOKI_CONTRACT_CLANG}"
    --cxx "${WOKI_CONTRACT_CLANGXX}"
)
add_custom_target(extension_guest_contract_check
    COMMAND ${WOKI_CONTRACT_COMMAND}
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/check_contract.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/fixtures/raw-abi-v1.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/fixtures/raw-abi-v1-guest.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/fixtures/freestanding/string.h.fixture"
        "${CMAKE_CURRENT_SOURCE_DIR}/../web/woki_ext_test.js"
    VERBATIM
)
add_dependencies(woki_tests extension_guest_contract_check)
add_test(
    NAME extension_guest_contract
    COMMAND ${WOKI_CONTRACT_COMMAND}
)

find_program(WOKI_NODE NAMES node REQUIRED)
add_test(
    NAME extension_web_bridge
    COMMAND "${WOKI_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/../web/woki_ext_test.js"
)
