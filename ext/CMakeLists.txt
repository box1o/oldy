cmake_minimum_required(VERSION 3.25)

project(wokiext
    VERSION 0.1.0
    DESCRIPTION "Woki extension development command line tool"
    LANGUAGES CXX
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    include/*.hpp
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
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
    "/${CMAKE_INSTALL_INCLUDEDIR}/woki"
)
configure_file(tool_paths.hpp.in generated/tool_paths.hpp @ONLY)

add_executable(wokiext
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(wokiext
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_compile_features(wokiext PRIVATE cxx_std_23)
apply_compiler_options(wokiext)

target_link_libraries(wokiext
    PRIVATE
        woki::extension
        cxxopts::cxxopts
)

if(NOT EMSCRIPTEN)
    target_link_libraries(wokiext PRIVATE woki::libarchive)
endif()

install(TARGETS wokiext
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ExtensionProject.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ExtensionWasm.cmake
    DESTINATION ${CMAKE_INSTALL_DATADIR}/woki/cmake
)

if(BUILD_TESTING)
    add_test(NAME wokiext_cli
        COMMAND ${CMAKE_COMMAND}
            -D WOKIEXT=$<TARGET_FILE:wokiext>
            -D TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cli-test
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_tests.cmake
    )
    if(WIN32 AND EXISTS "${CDEPS_ROOT}/wasmtime-c-api/lib/wasmtime.dll")
        set_tests_properties(wokiext_cli PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CDEPS_ROOT}/wasmtime-c-api/lib"
        )
    endif()
endif()
