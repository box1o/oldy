cmake_minimum_required(VERSION 3.25)

project(platform VERSION 1.0.0 DESCRIPTION "Platform and window abstraction" LANGUAGES CXX)

add_module(platform
    SOURCES
        src/window/window.cpp
    HEADERS
        include/woki/platform.hpp
        include/woki/window/window.hpp
        include/woki/events/base.hpp
        include/woki/events/type.hpp
        include/woki/events/format.hpp
        include/woki/events/events.hpp
        include/woki/events/category.hpp
        include/woki/events/dispatcher.hpp
        include/woki/events/input/events.hpp
        include/woki/events/window/events.hpp
        include/woki/events/renderer/events.hpp
        include/woki/events/application/events.hpp
    DEPENDENCIES
        woki::core
)

target_compile_definitions(woki_platform PRIVATE GLFW_INCLUDE_NONE)

if(EMSCRIPTEN)
    target_link_options(woki_platform PUBLIC "-sUSE_GLFW=3")
else()
    target_link_libraries(woki_platform PRIVATE glfw)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
