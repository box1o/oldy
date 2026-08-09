# Include before project() in isolated extension guest CMakeLists.txt.
# Forces a wasm-capable clang/clang++ (not AppleClang or MSVC).

# Guest modules target wasm32, not the host OS (avoids -arch on Apple, MSVC defaults, etc.).
set(CMAKE_SYSTEM_NAME Generic CACHE STRING "Wasm guest system" FORCE)
set(CMAKE_SYSTEM_PROCESSOR wasm32 CACHE STRING "Wasm guest processor" FORCE)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "" CACHE STRING "Wasm guest has no macOS arch" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "" CACHE STRING "Wasm guest has no macOS deployment target" FORCE)
endif()

set(_llvm_bin_paths)
if(APPLE)
    list(APPEND _llvm_bin_paths
        /opt/homebrew/opt/llvm/bin
        /usr/local/opt/llvm/bin
    )
elseif(WIN32)
    list(APPEND _llvm_bin_paths
        "C:/Program Files/LLVM/bin"
    )
endif()

if(NOT WOKI_WASM_COMPILER)
    find_program(WOKI_WASM_COMPILER
        NAMES clang++
        PATHS ${_llvm_bin_paths}
        NO_DEFAULT_PATH
    )
endif()
if(NOT WOKI_WASM_COMPILER)
    find_program(WOKI_WASM_COMPILER clang++ REQUIRED)
endif()

if(NOT WOKI_WASM_C_COMPILER)
    find_program(WOKI_WASM_C_COMPILER
        NAMES clang
        PATHS ${_llvm_bin_paths}
        NO_DEFAULT_PATH
    )
endif()
if(NOT WOKI_WASM_C_COMPILER)
    find_program(WOKI_WASM_C_COMPILER clang REQUIRED)
endif()

set(CMAKE_CXX_COMPILER "${WOKI_WASM_COMPILER}" CACHE STRING "Wasm extension C++ compiler" FORCE)
set(CMAKE_C_COMPILER "${WOKI_WASM_C_COMPILER}" CACHE STRING "Wasm extension C compiler" FORCE)
