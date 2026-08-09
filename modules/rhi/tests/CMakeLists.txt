cmake_minimum_required(VERSION 3.25)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "*.cpp"
)

add_module_test(rhi_tests
    SOURCES
        ${SOURCES}
    LIBRARIES
        woki::rhi
        Catch2::Catch2WithMain
)

target_include_directories(rhi_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../backend/wgpu
)

if(WOKI_WGPU_HAS_D3D12_FILE_MAPPING_FEATURE)
    target_compile_definitions(rhi_tests PRIVATE WOKI_WGPU_HAS_D3D12_FILE_MAPPING_FEATURE=1)
endif()
