cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED WOKI_REPO_ROOT)
    get_filename_component(WOKI_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()

include("${WOKI_REPO_ROOT}/cmake/ExtensionProject.cmake")
project(woki_extension LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${WOKI_REPO_ROOT}/cmake/ExtensionWasm.cmake")
add_wokiext(kitty
    LANGUAGE CXX
    MANIFEST manifest.yaml
    WASM extension.wasm
    LIBRARIES math ecs
    PERMISSIONS log
    SOURCES src/plugin.cpp
)
