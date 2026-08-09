# Wasm guest extension build (outputs extension.wasm next to the project sources).
#
# Usage:
#   include(${WOKI_CMAKE_DIR}/ExtensionWasm.cmake)
#   add_wokiext(src/plugin.cpp)
#
# Requires clang/clang++ with --target=wasm32-unknown-unknown support.
# Set CMAKE_EXPORT_COMPILE_COMMANDS ON in the extension project for clangd.

set(WOKI_WASM_GUEST_EXPORTS
    ext_api_version
    ext_init
    ext_on_tick
    ext_on_event
    ext_on_command
    ext_on_unload
    ext_alloc
    ext_free
)

function(add_wokiext source_file)
    if(NOT WOKI_SDK_DIR)
        if(EXISTS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/extension/sdk/ext.h")
            set(WOKI_SDK_DIR "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/extension/sdk")
        elseif(EXISTS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../include/woki/ext.h")
            set(WOKI_SDK_DIR "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../include/woki")
        else()
            message(FATAL_ERROR "Cannot locate the Woki extension SDK; set WOKI_SDK_DIR")
        endif()
    endif()

    set(_source "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
    set(_wasm "${CMAKE_CURRENT_SOURCE_DIR}/extension.wasm")

    if(_source MATCHES "\\.c$")
        set(_std_flag -std=c17)
        if(NOT WOKI_WASM_C_COMPILER)
            find_program(WOKI_WASM_C_COMPILER clang REQUIRED)
        endif()
        set(_guest_compiler "${WOKI_WASM_C_COMPILER}")
    else()
        set(_std_flag -std=c++23)
        if(NOT WOKI_WASM_COMPILER)
            find_program(WOKI_WASM_COMPILER clang++ REQUIRED)
        endif()
        set(_guest_compiler "${WOKI_WASM_COMPILER}")
    endif()

    set(_guest_flags
        --target=wasm32-unknown-unknown
        -nostdlib
        -fno-builtin
        -I${WOKI_SDK_DIR}
        ${_std_flag}
    )

    set(_export_flags)
    foreach(_symbol IN LISTS WOKI_WASM_GUEST_EXPORTS)
        list(APPEND _export_flags "-Wl,--export=${_symbol}")
    endforeach()

    # OBJECT target mirrors guest flags for clangd (compile_commands.json).
    add_library(extension_guest OBJECT EXCLUDE_FROM_ALL "${_source}")
    target_compile_options(extension_guest PRIVATE ${_guest_flags})
    target_include_directories(extension_guest PRIVATE "${WOKI_SDK_DIR}")
    set_target_properties(extension_guest PROPERTIES EXPORT_COMPILE_COMMANDS ON)
    if(_source MATCHES "\\.c$")
        set_target_properties(extension_guest PROPERTIES LINKER_LANGUAGE C)
    else()
        set_target_properties(extension_guest PROPERTIES LINKER_LANGUAGE CXX)
    endif()

    add_custom_command(
        OUTPUT "${_wasm}"
        COMMAND ${_guest_compiler}
            ${_guest_flags}
            -O2
            -Wl,--no-entry
            -Wl,--allow-undefined
            -Wl,--export-memory
            ${_export_flags}
            -o "${_wasm}"
            "${_source}"
        DEPENDS "${_source}" extension_guest
        VERBATIM
    )

    add_custom_target(extension ALL DEPENDS "${_wasm}")
endfunction()
