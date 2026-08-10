cmake_minimum_required(VERSION 3.25)

set(WOKI_PROJECT_LANGUAGES C CXX)
if(APPLE)
    list(APPEND WOKI_PROJECT_LANGUAGES OBJCXX)
endif()

project(woki VERSION 0.0.1 LANGUAGES ${WOKI_PROJECT_LANGUAGES})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(namespace "woki" CACHE STRING "Project namespace")

include(Options)
include(Dependencies)
include(Module)
include(CTest)

if(BUILD_TESTING)
    add_custom_target(woki_tests)
endif()

message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler:       ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ standard:   C++${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

add_subdirectory(3rdparty)

set(modules_dir "${CMAKE_SOURCE_DIR}/modules")
file(GLOB module_entries RELATIVE "${modules_dir}" "${modules_dir}/*")

foreach(module_entry IN LISTS module_entries)
    if(IS_DIRECTORY "${modules_dir}/${module_entry}")
        message(STATUS "Module: ${module_entry}")
        add_subdirectory("${modules_dir}/${module_entry}")
    endif()
endforeach()

if(NOT EMSCRIPTEN)
    add_subdirectory(ext)
endif()

include(Extensions)
woki_register_extensions("${CMAKE_SOURCE_DIR}/extensions")

add_subdirectory(studio)
