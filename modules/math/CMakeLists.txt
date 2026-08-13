cmake_minimum_required(VERSION 3.25)

project(math VERSION 1.0.0 DESCRIPTION "header-only mathematics" LANGUAGES CXX)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS include/*.hpp)

add_module(math
    HEADER_ONLY
    HEADERS ${HEADERS}
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
