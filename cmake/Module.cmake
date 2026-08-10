function(add_module name)
    set(options NO_INSTALL_HEADERS)
    set(multi_value_args SOURCES HEADERS DEPENDENCIES)
    cmake_parse_arguments(ARG "${options}" "" "${multi_value_args}" ${ARGN})

    if(NOT ARG_HEADERS)
        message(FATAL_ERROR "Module '${name}' must define HEADERS")
    endif()

    set(target "${namespace}_${name}")

    add_library(${target}
        ${ARG_SOURCES}
        ${ARG_HEADERS}
    )

    add_library(${namespace}::${name} ALIAS ${target})

    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    if(ARG_DEPENDENCIES)
        target_link_libraries(${target} PUBLIC ${ARG_DEPENDENCIES})
    endif()

    target_compile_features(${target} PUBLIC cxx_std_23)

    apply_compiler_options(${target})

    install(TARGETS ${target}
        EXPORT ${namespace}Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    if(NOT ARG_NO_INSTALL_HEADERS)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )
    endif()
endfunction()

function(add_module_test name)
    set(multi_value_args SOURCES LIBRARIES)
    cmake_parse_arguments(ARG "" "" "${multi_value_args}" ${ARGN})

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "Test '${name}' must define SOURCES")
    endif()

    add_executable(${name}
        ${ARG_SOURCES}
    )

    target_compile_features(${name} PRIVATE cxx_std_23)
    apply_compiler_options(${name})

    if(MSVC)
        target_compile_definitions(${name} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    if(ARG_LIBRARIES)
        target_link_libraries(${name}
            PRIVATE
                ${ARG_LIBRARIES}
        )
    endif()

    if(EXISTS "${CDEPS_ROOT}/catch2-src/extras/Catch.cmake")
        include("${CDEPS_ROOT}/catch2-src/extras/Catch.cmake")
    else()
        include(Catch)
    endif()
    if(WIN32 AND EXISTS "${CDEPS_ROOT}/wasmtime-c-api/lib/wasmtime.dll")
        catch_discover_tests(${name}
            EXTRA_ARGS --durations yes
            DL_PATHS "${CDEPS_ROOT}/wasmtime-c-api/lib"
        )
    else()
        catch_discover_tests(${name} EXTRA_ARGS --durations yes)
    endif()

    if(TARGET woki_tests)
        add_dependencies(woki_tests ${name})
    endif()
endfunction()
