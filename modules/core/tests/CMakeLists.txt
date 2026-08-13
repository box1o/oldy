cmake_minimum_required(VERSION 3.25)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "*.cpp"
)
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/task/smoke.cpp")

add_module_test(core_tests
    SOURCES
        ${SOURCES}
    LIBRARIES
        woki::core
        Catch2::Catch2WithMain
)

add_module_test(core_task_smoke
    SOURCES task/smoke.cpp
    LIBRARIES woki::core)
