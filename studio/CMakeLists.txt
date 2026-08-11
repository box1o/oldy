set(_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(_RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")
set(_CFG_DIR "${CMAKE_SOURCE_DIR}/config")
set(_ICONS_DIR "${_CFG_DIR}/icons")
set(_DEFAULT_CONFIG "${_CFG_DIR}/conf/studio.yaml")

file(GLOB_RECURSE _HEADERS
    CONFIGURE_DEPENDS
    ${_SRC_DIR}/*.h
    ${_SRC_DIR}/*.hpp
)

file(GLOB_RECURSE _SOURCES
    CONFIGURE_DEPENDS
    ${_SRC_DIR}/*.cpp
)

add_executable(studio
    ${_HEADERS}
    ${_SOURCES}
)

include(DeveloperLink)
woki_add_developer_link(studio)

target_compile_features(studio PRIVATE cxx_std_23)
apply_compiler_options(studio)

target_compile_definitions(studio
    PRIVATE
        _VERSION="${PROJECT_VERSION}"
)

target_include_directories(studio
    PRIVATE
        ${_SRC_DIR}
)

set(_DEPS
    woki::core
    woki::platform
    woki::rhi
    woki::extension
)

if(EMSCRIPTEN)
    message(STATUS "Building studio for web")

    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(_WEB_RES_MOUNT "/resources")
    set(_WEB_CFG_MOUNT "/config")

    target_compile_definitions(studio
        PRIVATE
            __WEB
            __WEBGPU_TEST
            WOKI_HAS_DAWN
            WEBGPU_BACKEND_EMSCRIPTEN
    )

    if(NOT DEFINED EMDAWNWEBGPU_PORT OR EMDAWNWEBGPU_PORT STREQUAL "")
        set(_WGPU_PORT "emdawnwebgpu")
        message(STATUS "Using Emscripten WebGPU port: ${_WGPU_PORT}")
    else()
        set(_WGPU_PORT "${EMDAWNWEBGPU_PORT}")
        message(STATUS "Using external emdawnwebgpu port: ${_WGPU_PORT}")
    endif()

    target_compile_options(studio PRIVATE
        "--use-port=${_WGPU_PORT}"
    )

    if(EXISTS "${_RES_DIR}")
        target_link_options(studio PRIVATE
            "SHELL:--preload-file ${_RES_DIR}@${_WEB_RES_MOUNT}"
        )
        target_compile_definitions(studio PRIVATE
            __RES_ROOT="${_WEB_RES_MOUNT}"
        )
    endif()

    if(EXISTS "${_CFG_DIR}")
        target_link_options(studio PRIVATE
            "SHELL:--preload-file ${_CFG_DIR}@${_WEB_CFG_MOUNT}"
        )
    endif()

    target_link_options(studio PRIVATE
        "--use-port=${_WGPU_PORT}"
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sASSERTIONS=1"
        "-sNO_DISABLE_EXCEPTION_CATCHING"
        "--js-library" "${CMAKE_SOURCE_DIR}/modules/extension/web/woki_ext.js"
        "--shell-file" "${CMAKE_SOURCE_DIR}/web/index.html"
    )

    set(_EXT_DIR "${CMAKE_BINARY_DIR}/extension-packages/$<CONFIG>")
    target_link_options(studio PRIVATE
        "SHELL:--preload-file ${_EXT_DIR}@/extensions"
    )
    target_compile_definitions(studio PRIVATE WOKI_SOURCE_EXTENSIONS_DIR="/extensions")

    set_target_properties(studio PROPERTIES
        OUTPUT_NAME "studio"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    add_custom_command(
        TARGET studio
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/web/favicon.svg"
            "${CMAKE_BINARY_DIR}/favicon.svg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_ICONS_DIR}/favicon.ico"
            "${CMAKE_BINARY_DIR}/favicon.ico"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_ICONS_DIR}/apple-touch-icon.png"
            "${CMAKE_BINARY_DIR}/apple-touch-icon.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_ICONS_DIR}/woki-32.png"
            "${CMAKE_BINARY_DIR}/woki-32.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_ICONS_DIR}/woki-64.png"
            "${CMAKE_BINARY_DIR}/woki-64.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/web/woki-sad.svg"
            "${CMAKE_BINARY_DIR}/woki-sad.svg"
    )
else()
    message(STATUS "Building studio for native")

    if(APPLE)
        set(_INSTALLED_STUDIO_DIR "/${CMAKE_INSTALL_BINDIR}/studio.app/Contents/MacOS")
    else()
        set(_INSTALLED_STUDIO_DIR "/${CMAKE_INSTALL_BINDIR}")
    endif()
    file(RELATIVE_PATH _INSTALLED_EXTENSIONS_FROM_BINDIR
        "${_INSTALLED_STUDIO_DIR}"
        "/${CMAKE_INSTALL_DATADIR}/woki/extensions"
    )

    target_compile_definitions(studio PRIVATE
        "WOKI_SOURCE_EXTENSIONS_DIR=(*woki::paths::ExecutablePath()).parent_path() / std::filesystem::path{\"${_INSTALLED_EXTENSIONS_FROM_BINDIR}/$<CONFIG>\"}"
        WOKI_SYSTEM_CONFIG_DIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}/woki"
    )

    find_package(Threads REQUIRED)

    list(APPEND _DEPS
        Threads::Threads
        msdfgen::msdfgen
    )

    if(NOT TARGET dawn::webgpu_dawn)
        find_package(Dawn CONFIG QUIET)
    endif()

    if(TARGET dawn::webgpu_dawn)
        message(STATUS "Dawn found - enabling WebGPU renderer")
        list(APPEND _DEPS dawn::webgpu_dawn)
        target_compile_definitions(studio PRIVATE WOKI_HAS_DAWN)
    elseif(TARGET webgpu_dawn)
        message(STATUS "Dawn found (legacy target) - enabling WebGPU renderer")
        list(APPEND _DEPS webgpu_dawn)
        target_compile_definitions(studio PRIVATE WOKI_HAS_DAWN)
    else()
        message(STATUS "Dawn not found - building studio without WebGPU renderer")
    endif()

    set_target_properties(studio PROPERTIES
        OUTPUT_NAME "studio"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    if(APPLE)
        set_target_properties(studio PROPERTIES MACOSX_BUNDLE ON)
    endif()
endif()

if(TARGET woki_extensions)
    add_dependencies(studio woki_extensions)
endif()

target_link_libraries(studio PRIVATE ${_DEPS})

if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_executable(studio_layer_stack_tests
        tests/layer_stack_tests.cpp
        src/core/layer_stack.cpp
    )
    target_compile_features(studio_layer_stack_tests PRIVATE cxx_std_23)
    target_include_directories(studio_layer_stack_tests PRIVATE ${_SRC_DIR})
    apply_compiler_options(studio_layer_stack_tests)
    target_link_libraries(studio_layer_stack_tests PRIVATE
        woki::core
        woki::platform
        Catch2::Catch2WithMain
    )
    add_test(NAME studio_layer_stack COMMAND studio_layer_stack_tests)
    if(TARGET woki_tests)
        add_dependencies(woki_tests studio_layer_stack_tests)
    endif()
endif()

install(TARGETS studio
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(NOT EMSCRIPTEN AND TARGET woki_extensions)
    add_custom_target(studio_extension_stage
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/woki/extensions/$<CONFIG>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/extension-packages/$<CONFIG>"
            "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/woki/extensions/$<CONFIG>"
        DEPENDS woki_extensions
        VERBATIM
    )
    add_dependencies(studio studio_extension_stage)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/extension-packages/$<CONFIG>/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/woki/extensions/$<CONFIG>"
    )
endif()

if(EXISTS "${_RES_DIR}" AND NOT EMSCRIPTEN)
    install(
        DIRECTORY ${_RES_DIR}/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/woki/resources
    )

    add_custom_command(
        TARGET studio
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_RES_DIR}"
            "${CMAKE_BINARY_DIR}/bin/resources"
    )
endif()

if(EXISTS "${_ICONS_DIR}/woki.svg" AND NOT EMSCRIPTEN)
    install(
        DIRECTORY "${_ICONS_DIR}/"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/woki/icons
        FILES_MATCHING
            REGEX ".*\\.(png|svg|ico)$"
    )
endif()

if(EXISTS "${_DEFAULT_CONFIG}" AND NOT EMSCRIPTEN)
    install(
        FILES "${_DEFAULT_CONFIG}"
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/woki
    )

    add_custom_target(studio_config ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_CFG_DIR}"
            "${CMAKE_BINARY_DIR}/bin/config"
        DEPENDS "${_DEFAULT_CONFIG}"
        COMMENT "Copying studio config"
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_USER_CONFIG_ROOT "$ENV{XDG_CONFIG_HOME}")
        if(_USER_CONFIG_ROOT STREQUAL "")
            set(_USER_CONFIG_ROOT "$ENV{HOME}/.config")
        endif()

        if(NOT _USER_CONFIG_ROOT STREQUAL "")
            add_custom_command(
                TARGET studio
                POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    -D SOURCE_FILE="${_DEFAULT_CONFIG}"
                    -D DEST_FILE="${_USER_CONFIG_ROOT}/woki/studio.yaml"
                    -P "${CMAKE_SOURCE_DIR}/cmake/CopyDefaultFile.cmake"
            )
        endif()
    endif()
endif()

if(BUILD_TESTING AND NOT EMSCRIPTEN AND TARGET wokiext AND TARGET woki_extensions)
    if(APPLE)
        set(_INSTALLED_STUDIO_RELATIVE "${CMAKE_INSTALL_BINDIR}/studio.app/Contents/MacOS/$<TARGET_FILE_NAME:studio>")
    else()
        set(_INSTALLED_STUDIO_RELATIVE "${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:studio>")
    endif()
    add_test(NAME installed_release_tree
        COMMAND ${CMAKE_COMMAND}
            -D BUILD_DIR=${CMAKE_BINARY_DIR}
            -D TEST_ROOT=${CMAKE_BINARY_DIR}/installed-release-tree-test
            -D CONFIG=$<CONFIG>
            -D BINDIR=${CMAKE_INSTALL_BINDIR}
            -D DATADIR=${CMAKE_INSTALL_DATADIR}
            -D INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}
            -D WOKIEXT_NAME=$<TARGET_FILE_NAME:wokiext>
            -D STUDIO_RELATIVE=${_INSTALLED_STUDIO_RELATIVE}
            -P ${CMAKE_SOURCE_DIR}/cmake/TestInstalledReleaseTree.cmake
    )
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/installed-release-tree-tmp")
    set_tests_properties(installed_release_tree PROPERTIES
        ENVIRONMENT "TMPDIR=${CMAKE_BINARY_DIR}/installed-release-tree-tmp;TEMP=${CMAKE_BINARY_DIR}/installed-release-tree-tmp;TMP=${CMAKE_BINARY_DIR}/installed-release-tree-tmp"
    )
endif()
