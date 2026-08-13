cmake_minimum_required(VERSION 3.25)

project(asset
    VERSION 1.0.0
    DESCRIPTION "asset storage and product formats"
    LANGUAGES CXX
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    include/*.hpp
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
)

add_module(asset
    SOURCES ${SOURCES}
    HEADERS ${HEADERS}
    DEPENDENCIES woki::core
)

if(NOT EMSCRIPTEN)
    add_executable(woki-asset tool/main.cpp)
    target_compile_features(woki-asset PRIVATE cxx_std_23)
    target_link_libraries(woki-asset PRIVATE woki::asset)
    apply_compiler_options(woki-asset)
    install(TARGETS woki-asset RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
