cmake_minimum_required(VERSION 3.25)

add_module_test(rhi_tests
    SOURCES
        conformance_tests.cpp
        null_rhi_tests.cpp
        validation_rhi_tests.cpp
    LIBRARIES
        woki::rhi
        Catch2::Catch2WithMain
)

add_module_test(rhi_wgpu_unit_tests
    SOURCES
        handle_tests.cpp
        wgpu_conversion_tests.cpp
    LIBRARIES
        woki::rhi
        Catch2::Catch2WithMain
)

target_include_directories(rhi_wgpu_unit_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../backend/wgpu
)

if(WOKI_WGPU_HAS_D3D12_FILE_MAPPING_FEATURE)
    target_compile_definitions(rhi_wgpu_unit_tests PRIVATE WOKI_WGPU_HAS_D3D12_FILE_MAPPING_FEATURE=1)
endif()
