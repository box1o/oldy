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
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(WOKI_CONTRACT_CLANG NAMES clang REQUIRED)
find_program(WOKI_CONTRACT_CLANGXX NAMES clang++ REQUIRED)
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
    VERBATIM
)
add_dependencies(woki_tests extension_guest_contract_check)
add_test(
    NAME extension_guest_contract
    COMMAND ${WOKI_CONTRACT_COMMAND}
)
