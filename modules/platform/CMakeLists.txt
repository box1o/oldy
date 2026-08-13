cmake_minimum_required(VERSION 3.25)

project(platform VERSION 1.0.0 DESCRIPTION "Platform and window abstraction" LANGUAGES CXX)

set(file_watcher_source src/file_watcher_emscripten.cpp)
if(NOT EMSCRIPTEN)
    if(WIN32)
        set(file_watcher_source src/file_watcher_windows.cpp)
    elseif(APPLE)
        set(file_watcher_source src/file_watcher_macos.cpp)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(file_watcher_source src/file_watcher_linux.cpp)
    else()
        message(FATAL_ERROR "File watcher backend is unavailable for ${CMAKE_SYSTEM_NAME}")
    endif()
endif()

set(PLATFORM_PRIVATE_DEPS)
if(NOT EMSCRIPTEN)
    list(APPEND PLATFORM_PRIVATE_DEPS
        "$<BUILD_INTERFACE:glfw>"
        "$<INSTALL_INTERFACE:$<1:glfw>>")
endif()

add_module(platform
    SOURCES
        src/file_watcher.cpp
        ${file_watcher_source}
        src/window/window.cpp
        src/gesture_recognizer.cpp
    HEADERS
        include/woki/platform.hpp
        include/woki/platform/file_watcher.hpp
        include/woki/platform/event_sink.hpp
        include/woki/platform/backend.hpp
        include/woki/window/window.hpp
        include/woki/events/base.hpp
        include/woki/events/type.hpp
        include/woki/events/format.hpp
        include/woki/events/events.hpp
        include/woki/events/category.hpp
        include/woki/events/dispatcher.hpp
        include/woki/events/input/events.hpp
        include/woki/events/gesture/events.hpp
        include/woki/events/gamepad/events.hpp
        include/woki/input/capabilities.hpp
        include/woki/input/state.hpp
        include/woki/input/gesture_recognizer.hpp
        include/woki/events/window/events.hpp
        include/woki/events/renderer/events.hpp
        include/woki/events/application/events.hpp
    DEPENDENCIES
        woki::core
    PRIVATE_DEPENDENCIES ${PLATFORM_PRIVATE_DEPS}
)

target_compile_definitions(woki_platform PRIVATE GLFW_INCLUDE_NONE)

if(EMSCRIPTEN)
    target_link_options(woki_platform PUBLIC "-sUSE_GLFW=3")
endif()

if(APPLE)
    target_link_libraries(woki_platform PRIVATE "-framework CoreServices")
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
