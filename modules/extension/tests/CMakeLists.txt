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
    WOKI_DINOLAB_PACKAGE_DIR="${CMAKE_BINARY_DIR}/extension-packages/$<CONFIG>/dino-lab"
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
add_test(
    NAME extension_guest_contract
    COMMAND ${WOKI_CONTRACT_COMMAND}
)
