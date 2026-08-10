# Include before project() in isolated extension guest CMakeLists.txt.

# Guest modules target wasm32, not the host OS (avoids -arch on Apple, MSVC
# defaults, and executable checks that cannot run on the build host).
set(CMAKE_SYSTEM_NAME Generic CACHE STRING "Wasm guest system" FORCE)
set(CMAKE_SYSTEM_PROCESSOR wasm32 CACHE STRING "Wasm guest processor" FORCE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY CACHE STRING "Wasm compiler check type" FORCE)
set(CMAKE_LINK_DEPENDS_USE_LINKER FALSE CACHE BOOL "Wasm linker depfiles are unsupported" FORCE)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "" CACHE STRING "Wasm guest has no macOS arch" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "" CACHE STRING "Wasm guest has no macOS deployment target" FORCE)
endif()

set(_woki_llvm_bin_paths)
if(DEFINED ENV{WOKI_LLVM_PREFIX})
    list(APPEND _woki_llvm_bin_paths "$ENV{WOKI_LLVM_PREFIX}/bin")
endif()
if(CMAKE_HOST_APPLE)
    list(APPEND _woki_llvm_bin_paths
        /opt/homebrew/opt/llvm/bin
        /usr/local/opt/llvm/bin
    )
elseif(CMAKE_HOST_WIN32)
    list(APPEND _woki_llvm_bin_paths "C:/Program Files/LLVM/bin")
endif()

# Discover each driver independently. project(LANGUAGES C) must not require a
# C++ driver, and project(LANGUAGES CXX) must not require a C driver.
if(NOT WOKI_WASM_COMPILER)
    find_program(WOKI_WASM_COMPILER
        NAMES clang++
        HINTS ${_woki_llvm_bin_paths}
    )
endif()
if(NOT WOKI_WASM_C_COMPILER)
    if(DEFINED ENV{WOKI_WASM_CLANG} AND EXISTS "$ENV{WOKI_WASM_CLANG}")
        set(WOKI_WASM_C_COMPILER "$ENV{WOKI_WASM_CLANG}" CACHE FILEPATH "Wasm extension C compiler")
    endif()
endif()
if(NOT WOKI_WASM_C_COMPILER)
    find_program(WOKI_WASM_C_COMPILER
        NAMES clang
        HINTS ${_woki_llvm_bin_paths}
    )
endif()

if(WOKI_WASM_COMPILER)
    set(CMAKE_CXX_COMPILER "${WOKI_WASM_COMPILER}" CACHE FILEPATH "Wasm extension C++ compiler" FORCE)
    set(CMAKE_CXX_COMPILER_TARGET wasm32-unknown-unknown CACHE STRING "Wasm extension C++ target" FORCE)
endif()
if(WOKI_WASM_C_COMPILER)
    set(CMAKE_C_COMPILER "${WOKI_WASM_C_COMPILER}" CACHE FILEPATH "Wasm extension C compiler" FORCE)
    set(CMAKE_C_COMPILER_TARGET wasm32-unknown-unknown CACHE STRING "Wasm extension C target" FORCE)
endif()
