cmake_minimum_required(VERSION 3.25)
project(config VERSION 1.0.0 DESCRIPTION "bounded source-located configuration documents" LANGUAGES CXX)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_module(config
    SOURCES ${SOURCES}
    HEADERS ${HEADERS}
    PUBLIC_DEPENDENCIES woki::core
    PRIVATE_DEPENDENCIES
        "$<BUILD_INTERFACE:yaml-cpp::yaml-cpp>"
        "$<INSTALL_INTERFACE:$<1:yaml-cpp::yaml-cpp>>")
install(FILES docs/module.md DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/woki/docs/config)

if(BUILD_TESTING)
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_module_test(config_tests SOURCES ${TEST_SOURCES} LIBRARIES woki::config Catch2::Catch2WithMain)
    target_compile_definitions(config_tests PRIVATE WOKI_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endif()

if(NOT EMSCRIPTEN)
    add_executable(woki-config tool/main.cpp)
    target_compile_features(woki-config PRIVATE cxx_std_23)
    target_link_libraries(woki-config PRIVATE woki::config)
    apply_compiler_options(woki-config)
    install(TARGETS woki-config RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    set(WOKI_SCHEMA_DIR "${CMAKE_CURRENT_BINARY_DIR}/schema")
    set(WOKI_SCHEMA_FAMILIES studio.settings@1 extension.manifest@1 gfx.shader@2 gpu-struct@1 shader-pack@1 pipeline@1 feature@1 quality@1 material-type@1 material@1 texture@1 mesh-import@1 cook-manifest@1 mesh-manifest@1 ui.theme@1 ui.dock@1 ui.debug-tree@1)
    set(WOKI_SCHEMA_OUTPUTS)
    foreach(_schema IN LISTS WOKI_SCHEMA_FAMILIES)
        string(REPLACE "@" "-v" _name "${_schema}")
        set(_output "${WOKI_SCHEMA_DIR}/${_name}.schema.json")
        add_custom_command(OUTPUT "${_output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_SCHEMA_DIR}"
            COMMAND $<TARGET_FILE:woki-config> schema "${_schema}" "${_output}"
            DEPENDS woki-config VERBATIM)
        list(APPEND WOKI_SCHEMA_OUTPUTS "${_output}")
    endforeach()
    set(WOKI_SCHEMA_CATALOG "${WOKI_SCHEMA_DIR}/catalog.json")
    add_custom_command(OUTPUT "${WOKI_SCHEMA_CATALOG}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_SCHEMA_DIR}"
        COMMAND $<TARGET_FILE:woki-config> schema catalog "${WOKI_SCHEMA_CATALOG}"
        DEPENDS woki-config VERBATIM)
    add_custom_target(woki_config_schemas ALL DEPENDS ${WOKI_SCHEMA_OUTPUTS} "${WOKI_SCHEMA_CATALOG}")
    install(FILES ${WOKI_SCHEMA_OUTPUTS} "${WOKI_SCHEMA_CATALOG}" DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/woki/schemas)
endif()
