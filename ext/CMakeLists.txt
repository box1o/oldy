cmake_minimum_required(VERSION 3.25)

project(wokiext
    VERSION 0.1.0
    DESCRIPTION "Woki extension development command line tool"
    LANGUAGES CXX
)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../modules/extension/schema/manifest.schema.json"
    WOKI_MANIFEST_SCHEMA)
configure_file(manifest_schema.hpp.in generated/manifest_schema.hpp @ONLY)

file(RELATIVE_PATH WOKI_INSTALL_CMAKE_DIR_FROM_BINDIR
    "/${CMAKE_INSTALL_BINDIR}"
    "/${CMAKE_INSTALL_DATADIR}/woki/cmake"
)
file(RELATIVE_PATH WOKI_INSTALL_SDK_DIR_FROM_BINDIR
    "/${CMAKE_INSTALL_BINDIR}"
    "/${CMAKE_INSTALL_INCLUDEDIR}"
)
configure_file(tool_paths.hpp.in generated/tool_paths.hpp @ONLY)

add_library(wokiext_cli STATIC
    src/build.cpp
    src/bundle.cpp
    src/clean.cpp
    src/cli.cpp
    src/commands.cpp
    src/create.cpp
    src/diagnostics.cpp
    src/filesystem.cpp
    src/install.cpp
    src/manage.cpp
    src/process.cpp
    src/schema.cpp
    src/verify.cpp
)

target_include_directories(wokiext_cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_compile_features(wokiext_cli PRIVATE cxx_std_23)
apply_compiler_options(wokiext_cli)
target_link_libraries(wokiext_cli PRIVATE woki::extension woki::libarchive)
set_target_properties(wokiext_cli PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

add_executable(wokiext src/main.cpp)
target_compile_features(wokiext PRIVATE cxx_std_23)
target_include_directories(wokiext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
apply_compiler_options(wokiext)
target_link_libraries(wokiext PRIVATE wokiext_cli)

install(TARGETS wokiext
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ExtensionProject.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ExtensionWasm.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ExtensionManifest.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ExtensionInputHash.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CheckExtensionState.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/StageExtensionPackage.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/VerifyExtensionPackage.cmake
    DESTINATION ${CMAKE_INSTALL_DATADIR}/woki/cmake
)

if(BUILD_TESTING)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test-tmp)
    add_executable(wokiext_cli_unit_tests
        tests/cli_unit_tests.cpp
    )
    target_compile_features(wokiext_cli_unit_tests PRIVATE cxx_std_23)
    target_include_directories(wokiext_cli_unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    apply_compiler_options(wokiext_cli_unit_tests)
    target_link_libraries(wokiext_cli_unit_tests PRIVATE wokiext_cli Catch2::Catch2WithMain)
    add_test(NAME wokiext_cli_unit COMMAND wokiext_cli_unit_tests)
    set_tests_properties(wokiext_cli_unit PROPERTIES
        ENVIRONMENT "TMPDIR=${CMAKE_CURRENT_BINARY_DIR}/test-tmp;TEMP=${CMAKE_CURRENT_BINARY_DIR}/test-tmp;TMP=${CMAKE_CURRENT_BINARY_DIR}/test-tmp"
    )

    add_test(NAME wokiext_cli
        COMMAND ${CMAKE_COMMAND}
            -D WOKIEXT=$<TARGET_FILE:wokiext>
            -D TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cli-test
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_tests.cmake
    )
    add_test(NAME extension_external_projects
        COMMAND ${CMAKE_COMMAND}
            -D TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/external-project-test
            -D WOKI_CMAKE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../cmake
            -D WOKI_SDK_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../modules/extension/sdk
            -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/TestExternalExtensionProjects.cmake
    )
    set_tests_properties(wokiext_cli PROPERTIES
        ENVIRONMENT "TMPDIR=${CMAKE_CURRENT_BINARY_DIR}/test-tmp;TEMP=${CMAKE_CURRENT_BINARY_DIR}/test-tmp;TMP=${CMAKE_CURRENT_BINARY_DIR}/test-tmp"
    )
    if(WIN32 AND EXISTS "${CDEPS_ROOT}/wasmtime-c-api/lib/wasmtime.dll")
        set_tests_properties(wokiext_cli PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CDEPS_ROOT}/wasmtime-c-api/lib"
        )
    endif()
endif()
