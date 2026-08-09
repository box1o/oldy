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
        set(WGPU_TARGET dawn::webgpu_dawn)
    elseif(TARGET webgpu_dawn)
        set(WGPU_TARGET webgpu_dawn)
    endif()

    if(WGPU_TARGET)
        list(APPEND RHI_DEPS ${WGPU_TARGET})

        include(CheckCXXSourceCompiles)
        set(CMAKE_REQUIRED_LIBRARIES ${WGPU_TARGET})
        check_cxx_source_compiles(
            "#include <webgpu/webgpu.h>
            int main() {
                auto function = &wgpuResourceTableInsertBinding;
                (void)function;
                return 0;
            }"
            WOKI_WGPU_HAS_RESOURCE_TABLE_BINDING_NAMES
        )
        check_cxx_source_compiles(
            "#include <webgpu/webgpu.h>
            int main() {
                auto feature = WGPUFeatureName_SharedBufferMemoryD3D12SharedMemoryFileMappingHandle;
                (void)feature;
                return 0;
            }"
            WOKI_WGPU_HAS_D3D12_FILE_MAPPING_FEATURE
        )
        unset(CMAKE_REQUIRED_LIBRARIES)
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

if(NOT EMSCRIPTEN)
    target_link_libraries(woki_rhi PRIVATE glfw)
endif()

if(WOKI_WGPU_HAS_RESOURCE_TABLE_BINDING_NAMES)
    target_compile_definitions(woki_rhi PRIVATE WOKI_WGPU_HAS_RESOURCE_TABLE_BINDING_NAMES=1)
endif()

if(WOKI_WGPU_HAS_D3D12_FILE_MAPPING_FEATURE)
    target_compile_definitions(woki_rhi PRIVATE WOKI_WGPU_HAS_D3D12_FILE_MAPPING_FEATURE=1)
endif()

if(EMSCRIPTEN)
    target_compile_options(woki_rhi PUBLIC
        "--use-port=${_WGPU_PORT}"
    )
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
