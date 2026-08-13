cmake_minimum_required(VERSION 3.25)

project(ui VERSION 2.0.0 DESCRIPTION "retained user interface with GFX canvas integration" LANGUAGES CXX)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_module(ui
    SOURCES ${SOURCES}
    HEADERS ${HEADERS}
    PUBLIC_DEPENDENCIES woki::config woki::core woki::gfx woki::math
)

install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/woki/docs/ui)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
