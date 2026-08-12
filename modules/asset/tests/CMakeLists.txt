cmake_minimum_required(VERSION 3.25)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "*.cpp"
)

add_module_test(asset_tests
    SOURCES ${SOURCES}
    LIBRARIES
        woki::asset
        Catch2::Catch2WithMain
)
