add_module_test(ecs_tests
    SOURCES
        entity_tests.cpp
        registry_tests.cpp
    LIBRARIES
        woki::ecs
        Catch2::Catch2WithMain
)
