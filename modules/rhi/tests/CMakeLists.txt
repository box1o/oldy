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
