cmake_minimum_required(VERSION 3.25)

set(_RHI_LANGUAGES CXX)
if(APPLE)
    list(APPEND _RHI_LANGUAGES OBJCXX)
endif()

project(rhi
    VERSION 1.0.0
    DESCRIPTION "Render hardware interface"
    LANGUAGES ${_RHI_LANGUAGES}
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    include/*.hpp
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    backend/wgpu/*.cpp
)

if(APPLE)
    file(GLOB_RECURSE APPLE_SOURCES CONFIGURE_DEPENDS
        backend/wgpu/*.mm
    )
    list(APPEND SOURCES ${APPLE_SOURCES})
endif()

set(RHI_DEPS
    woki::core
    woki::platform
)

if(EMSCRIPTEN)
    if(NOT DEFINED EMDAWNWEBGPU_PORT OR EMDAWNWEBGPU_PORT STREQUAL "")
        set(_WGPU_PORT emdawnwebgpu)
    else()
        set(_WGPU_PORT "${EMDAWNWEBGPU_PORT}")
    endif()
else()
    find_package(Threads REQUIRED)
    find_package(Dawn CONFIG QUIET)

    if(TARGET dawn::webgpu_dawn)
        list(APPEND RHI_DEPS dawn::webgpu_dawn)
    elseif(TARGET webgpu_dawn)
        list(APPEND RHI_DEPS webgpu_dawn)
    endif()
endif()

add_module(rhi
    SOURCES ${SOURCES}
    HEADERS ${HEADERS}
    DEPENDENCIES ${RHI_DEPS}
)

target_include_directories(woki_rhi
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

if(EMSCRIPTEN)
    target_compile_options(woki_rhi PUBLIC
        "--use-port=${_WGPU_PORT}"
    )
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
