set(WARNINGS_AS_ERRORS ON CACHE BOOL "Treat compiler warnings as errors")

if(NOT EMSCRIPTEN)
    option(WOKI_EXTENSION_WITH_WASMTIME
        "Enable Wasmtime wasm runtime in woki::extension (downloads or builds Wasmtime C API)"
        ON
    )

    set(WOKI_WASMTIME_ROOT "" CACHE PATH "Wasmtime C API install root (overrides auto-download)")
    set(WOKI_WASMTIME_PROVIDER "prebuilt" CACHE STRING
        "How to obtain Wasmtime C API: prebuilt, source, or auto")
    set_property(CACHE WOKI_WASMTIME_PROVIDER PROPERTY STRINGS prebuilt source auto)
    option(WOKI_WASMTIME_USE_SHARED "Link against shared libwasmtime instead of static" OFF)
endif()

function(apply_compiler_options target)
    if(MSVC)
        target_compile_definitions(${target} PRIVATE NOMINMAX)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /wd4201
        )
        if(WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wno-pedantic
            -Wno-unused-parameter
            -Wno-deprecated-declarations
        )
        if(EMSCRIPTEN)
            target_compile_options(${target} PRIVATE -fexceptions)
        endif()
        if(WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
