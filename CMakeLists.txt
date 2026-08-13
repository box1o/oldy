cmake_minimum_required(VERSION 3.25)

set(WOKI_PROJECT_LANGUAGES C CXX)
if(APPLE)
    list(APPEND WOKI_PROJECT_LANGUAGES OBJCXX)
endif()

project(woki VERSION 0.0.2 LANGUAGES ${WOKI_PROJECT_LANGUAGES})

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
set(module_entries core math config ecs platform asset extension rhi gfx ui)
foreach(module_entry IN LISTS module_entries)
    message(STATUS "Module: ${module_entry}")
    add_subdirectory("${modules_dir}/${module_entry}")
endforeach()

if(NOT EMSCRIPTEN)
    add_subdirectory(ext)
endif()

include(Extensions)
woki_register_extensions("${CMAKE_SOURCE_DIR}/extensions")
if(TARGET extension_tests AND TARGET woki_extensions)
    add_dependencies(extension_tests woki_extensions)
endif()

add_subdirectory(studio)

install(EXPORT ${namespace}Targets
    FILE ${namespace}Targets.cmake
    NAMESPACE ${namespace}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${namespace})

include(CMakePackageConfigHelpers)
set(WOKI_PACKAGE_NATIVE TRUE)
if(EMSCRIPTEN)
    set(WOKI_PACKAGE_NATIVE FALSE)
endif()
set(WOKI_CONFIG_FIND_WASMTIME "")
if(NOT EMSCRIPTEN AND WOKI_EXTENSION_WITH_WASMTIME)
    set(WOKI_CONFIG_FIND_WASMTIME "    find_dependency(wasmtime CONFIG)")
endif()
configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/wokiConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/wokiConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${namespace}")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/wokiConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/wokiConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/wokiConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${namespace}")
