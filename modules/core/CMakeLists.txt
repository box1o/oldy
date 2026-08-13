cmake_minimum_required(VERSION 3.25)

project(core
    VERSION 1.0.0
    DESCRIPTION "core functionality"
    LANGUAGES CXX
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    include/*.hpp
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
)

set(CORE_PUBLIC_DEPS
    "$<BUILD_INTERFACE:cxxopts::cxxopts>"
    "$<INSTALL_INTERFACE:$<1:cxxopts::cxxopts>>")
set(CORE_PRIVATE_DEPS)

if (NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    list(APPEND CORE_PUBLIC_DEPS
        "$<BUILD_INTERFACE:fmt::fmt>"
        "$<INSTALL_INTERFACE:$<1:fmt::fmt>>"
        "$<BUILD_INTERFACE:spdlog::spdlog>"
        "$<INSTALL_INTERFACE:$<1:spdlog::spdlog>>")
    list(APPEND CORE_PRIVATE_DEPS
        "$<BUILD_INTERFACE:Threads::Threads>"
        "$<INSTALL_INTERFACE:$<1:Threads::Threads>>")
else()
    message(STATUS "Skipping fmt and spdlog for web build (using Emscripten's libc++)")
endif()


add_module(core
    SOURCES ${SOURCES}
    HEADERS ${HEADERS}
    PUBLIC_DEPENDENCIES ${CORE_PUBLIC_DEPS}
    PRIVATE_DEPENDENCIES ${CORE_PRIVATE_DEPS}
)

install(FILES docs/module.md docs/task.md DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/woki/docs/core)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
