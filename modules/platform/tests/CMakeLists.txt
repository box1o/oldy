add_module_test(platform_tests
    SOURCES
        event_tests.cpp
    LIBRARIES
        woki::platform
        Catch2::Catch2WithMain
)
