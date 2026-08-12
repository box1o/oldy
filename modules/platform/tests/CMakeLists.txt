set(platform_test_sources event_tests.cpp)
if(NOT EMSCRIPTEN)
    list(APPEND platform_test_sources file_watcher_tests.cpp)
endif()

add_module_test(platform_tests
    SOURCES
        ${platform_test_sources}
    LIBRARIES
        woki::platform
        Catch2::Catch2WithMain
)
