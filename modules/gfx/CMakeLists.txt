cmake_minimum_required(VERSION 3.25)

project(gfx VERSION 1.0.0 DESCRIPTION "Cooked shader assets and runtime graphics resources" LANGUAGES CXX)

option(WOKI_GFX_FETCH_TINT "Fetch the pinned Dawn source tree to build Tint shader tooling" OFF)
option(WOKI_GFX_REQUIRE_TINT "Fail configuration unless the pinned Tint adapter is enabled" OFF)
set(WOKI_GFX_TINT_REVISION "9fd4a731474f66c1ce5465f9b13919ad7487315d" CACHE STRING "Pinned Dawn revision used for Tint")

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/tint_compiler.cpp")

set(GFX_DEPS woki::asset woki::core woki::platform)

if(NOT EMSCRIPTEN)
    if(WOKI_GFX_FETCH_TINT AND NOT TARGET tint_api)
        include(FetchContent)
        set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
        set(DAWN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(DAWN_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
        set(DAWN_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
        set(DAWN_BUILD_NODE_BINDINGS OFF CACHE BOOL "" FORCE)
        set(DAWN_BUILD_MONOLITHIC_LIBRARY OFF CACHE STRING "" FORCE)
        set(DAWN_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_SPIRV_VALIDATION OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_D3D11 OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_D3D12 OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_DESKTOP_GL OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_METAL OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_NULL OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_OPENGLES OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_VULKAN OFF CACHE BOOL "" FORCE)
        set(DAWN_USE_GLFW OFF CACHE BOOL "" FORCE)
        set(DAWN_USE_WAYLAND OFF CACHE BOOL "" FORCE)
        set(DAWN_USE_X11 OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_GLSL_WRITER OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_GLSL_VALIDATOR OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_HLSL_WRITER OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_IR_BINARY OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_MSL_WRITER OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_SPV_READER OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_SPV_WRITER OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_TINTD OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_WGSL_READER ON CACHE BOOL "" FORCE)
        set(TINT_BUILD_WGSL_WRITER OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(woki_tint
            GIT_REPOSITORY https://github.com/google/dawn.git
            GIT_TAG ${WOKI_GFX_TINT_REVISION}
            GIT_SHALLOW FALSE
        )
        FetchContent_MakeAvailable(woki_tint)
    endif()
    if(TARGET tint_api)
        list(APPEND SOURCES src/tint_compiler.cpp)
        list(APPEND GFX_DEPS tint_api)
        set(WOKI_GFX_HAS_TINT TRUE)
    endif()
endif()
if(WOKI_GFX_REQUIRE_TINT AND NOT WOKI_GFX_HAS_TINT)
    message(FATAL_ERROR "WOKI_GFX_REQUIRE_TINT requires tint_api from pinned Dawn ${WOKI_GFX_TINT_REVISION}")
endif()
list(APPEND GFX_DEPS woki::rhi nlohmann_json::nlohmann_json)

add_module(gfx SOURCES ${SOURCES} HEADERS ${HEADERS} DEPENDENCIES ${GFX_DEPS})
if(WOKI_GFX_HAS_TINT)
    target_compile_definitions(woki_gfx PRIVATE WOKI_GFX_HAS_TINT=1)
endif()

cmake_path(NORMAL_PATH CMAKE_CURRENT_SOURCE_DIR OUTPUT_VARIABLE WOKI_GFX_MODULE_DIR)
cmake_path(GET WOKI_GFX_MODULE_DIR PARENT_PATH WOKI_GFX_MODULES_DIR)
cmake_path(GET WOKI_GFX_MODULES_DIR PARENT_PATH WOKI_GFX_REPOSITORY_DIR)
set(WOKI_GFX_ASSET_SOURCE_DIR "${WOKI_GFX_REPOSITORY_DIR}/assets" CACHE INTERNAL "Woki authored asset source directory")
set(WOKI_GFX_ASSET_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets" CACHE INTERNAL "Woki staged asset directory")
add_custom_target(woki_gfx_assets ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${WOKI_GFX_ASSET_BUILD_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${WOKI_GFX_ASSET_SOURCE_DIR}" "${WOKI_GFX_ASSET_BUILD_DIR}"
    COMMENT "Staging Woki shader assets"
)
add_library(woki_gfx_asset_preload INTERFACE)
add_library(woki::gfx_asset_preload ALIAS woki_gfx_asset_preload)
add_dependencies(woki_gfx_asset_preload woki_gfx_assets)
if(EMSCRIPTEN)
    target_link_options(woki_gfx_asset_preload INTERFACE
        "SHELL:--preload-file ${WOKI_GFX_ASSET_BUILD_DIR}/shaders@/assets/shaders"
    )
endif()
install(DIRECTORY "${WOKI_GFX_ASSET_SOURCE_DIR}/"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/woki/assets"
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(WOKI_GFX_HAS_TINT)
    add_custom_target(woki_gfx_tint_check DEPENDS woki_gfx)
    if(TARGET gfx_tint_tests)
        add_dependencies(woki_gfx_tint_check gfx_tint_tests)
    endif()
endif()

if(NOT EMSCRIPTEN)
    add_subdirectory(tool)
endif()
