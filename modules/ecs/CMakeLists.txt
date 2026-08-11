cmake_minimum_required(VERSION 3.25)

project(ecs VERSION 1.0.0 DESCRIPTION "Entity component system" LANGUAGES CXX)

add_module(ecs
    SOURCES
        src/registry.cpp
    HEADERS
        include/woki/ecs.hpp
        include/woki/ecs/entity.hpp
        include/woki/ecs/registry.hpp
        include/woki/ecs/guest.hpp
    DEPENDENCIES
        woki::core
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
