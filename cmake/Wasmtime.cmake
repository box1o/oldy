include_guard(GLOBAL)

# Wasmtime C API integration for native extension hosting.
# Options (see cmake/Options.cmake): WOKI_WASMTIME_PROVIDER (prebuilt|source|auto),
# WOKI_WASMTIME_ROOT, WOKI_WASMTIME_USE_SHARED, WOKI_WASMTIME_VERSION.

include(FetchContent)

set(WOKI_WASMTIME_VERSION "45.0.1" CACHE STRING "Wasmtime release version (without leading v)")

function(_woki_wasmtime_detect_platform os_var arch_var ext_var supported_var)
    set(_os "")
    set(_arch "")
    set(_ext "")
    set(_supported FALSE)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_os linux)
        set(_ext tar.xz)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
            set(_arch x86_64)
            set(_supported TRUE)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
            set(_arch aarch64)
            set(_supported TRUE)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^riscv64")
            set(_arch riscv64gc)
            set(_supported TRUE)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^s390x")
            set(_arch s390x)
            set(_supported TRUE)
        endif()
    elseif(APPLE)
        set(_os macos)
        set(_ext tar.xz)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
            set(_arch aarch64)
            set(_supported TRUE)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
            set(_arch x86_64)
            set(_supported TRUE)
        endif()
    elseif(WIN32)
        set(_ext zip)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
            set(_arch x86_64)
            set(_supported TRUE)
            if(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                set(_os mingw)
            else()
                set(_os windows)
            endif()
        endif()
    endif()

    set(${os_var} "${_os}" PARENT_SCOPE)
    set(${arch_var} "${_arch}" PARENT_SCOPE)
    set(${ext_var} "${_ext}" PARENT_SCOPE)
    set(${supported_var} "${_supported}" PARENT_SCOPE)
endfunction()

function(_woki_wasmtime_register_alias)
    if(TARGET wasmtime AND NOT TARGET wasmtime::wasmtime)
        add_library(wasmtime::wasmtime ALIAS wasmtime)
        set_target_properties(wasmtime PROPERTIES SYSTEM ON)
    endif()
    if(TARGET wasmtime-cpp AND NOT TARGET wasmtime::cpp)
        add_library(wasmtime::cpp ALIAS wasmtime-cpp)
        set_target_properties(wasmtime-cpp PROPERTIES SYSTEM ON)
    endif()
endfunction()

function(_woki_wasmtime_register_cpp_from_prebuilt include_dir)
    if(NOT EXISTS "${include_dir}/wasmtime.hh")
        return()
    endif()
    if(TARGET wasmtime::cpp)
        return()
    endif()
    if(NOT TARGET wasmtime::wasmtime)
        return()
    endif()

    add_library(wasmtime_cpp INTERFACE)
    add_library(wasmtime::cpp ALIAS wasmtime_cpp)
    target_link_libraries(wasmtime_cpp INTERFACE wasmtime::wasmtime)
    target_include_directories(wasmtime_cpp INTERFACE "${include_dir}")
    target_compile_features(wasmtime_cpp INTERFACE cxx_std_17)

    if(MSVC)
        target_compile_definitions(wasmtime_cpp INTERFACE WASM_API_EXTERN= WASI_API_EXTERN=)
        target_link_libraries(wasmtime_cpp INTERFACE
            ws2_32 advapi32 userenv ntdll shell32 ole32 bcrypt)
    endif()
endfunction()

function(_woki_wasmtime_find_prebuilt_root root_var)
    set(_root "")

    if(WOKI_WASMTIME_ROOT
       AND EXISTS "${WOKI_WASMTIME_ROOT}/include/wasmtime.h"
       AND EXISTS "${WOKI_WASMTIME_ROOT}/include/wasm.h")
        set(_root "${WOKI_WASMTIME_ROOT}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/wasmtime.h"
           AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/wasm.h")
        set(_root "${CMAKE_CURRENT_SOURCE_DIR}")
    elseif(EXISTS "${CDEPS_ROOT}/wasmtime-c-api/include/wasmtime.h"
           AND EXISTS "${CDEPS_ROOT}/wasmtime-c-api/include/wasm.h")
        set(_root "${CDEPS_ROOT}/wasmtime-c-api")
    else()
        file(GLOB_RECURSE _wasmtime_headers
            "${CDEPS_ROOT}/wasmtime-c-api-*/include/wasmtime.h"
        )
        if(_wasmtime_headers)
            list(GET _wasmtime_headers 0 _wasmtime_h)
            get_filename_component(_include_dir "${_wasmtime_h}" DIRECTORY)
            get_filename_component(_root "${_include_dir}" DIRECTORY)
        endif()
    endif()

    set(${root_var} "${_root}" PARENT_SCOPE)
endfunction()

function(_woki_wasmtime_find_source_root root_var)
    set(_root "")

    if(WOKI_WASMTIME_ROOT AND EXISTS "${WOKI_WASMTIME_ROOT}/crates/c-api/CMakeLists.txt")
        set(_root "${WOKI_WASMTIME_ROOT}")
    elseif(EXISTS "${CDEPS_ROOT}/wasmtime-src/crates/c-api/CMakeLists.txt")
        set(_root "${CDEPS_ROOT}/wasmtime-src")
    endif()

    set(${root_var} "${_root}" PARENT_SCOPE)
endfunction()

function(_woki_wasmtime_download_prebuilt)
    _woki_wasmtime_detect_platform(_os _arch _ext _supported)

    if(NOT _supported)
        message(STATUS "Wasmtime: no prebuilt C API for ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
        return()
    endif()

    set(_tag "v${WOKI_WASMTIME_VERSION}")
    set(_archive "wasmtime-${_tag}-${_arch}-${_os}-c-api.${_ext}")
    set(_url "https://github.com/bytecodealliance/wasmtime/releases/download/${_tag}/${_archive}")
    set(_archive_path "${CDEPS_ROOT}/${_archive}")
    set(_extract_dir "${CDEPS_ROOT}/wasmtime-c-api")
    set(_extract_marker "${_extract_dir}/.extracted-${_tag}")

    file(MAKE_DIRECTORY "${CDEPS_ROOT}")

    if(NOT EXISTS "${_archive_path}")
        message(STATUS "Wasmtime: downloading ${_archive}")
        file(DOWNLOAD
            "${_url}"
            "${_archive_path}"
            SHOW_PROGRESS
            STATUS _download_status
        )
        list(GET _download_status 0 _download_code)
        if(NOT _download_code EQUAL 0)
            list(GET _download_status 1 _download_message)
            message(STATUS "Wasmtime: download failed (${_download_message})")
            file(REMOVE "${_archive_path}")
            return()
        endif()
    else()
        message(STATUS "Wasmtime: using cached archive ${_archive_path}")
    endif()

    if(NOT EXISTS "${_extract_marker}")
        message(STATUS "Wasmtime: extracting ${_archive}")
        file(MAKE_DIRECTORY "${_extract_dir}")
        if(_ext STREQUAL "tar.xz")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xJf "${_archive_path}"
                WORKING_DIRECTORY "${_extract_dir}"
                RESULT_VARIABLE _tar_result
            )
        else()
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xzf "${_archive_path}"
                WORKING_DIRECTORY "${_extract_dir}"
                RESULT_VARIABLE _tar_result
            )
        endif()
        if(NOT _tar_result EQUAL 0)
            message(FATAL_ERROR "Wasmtime: failed to extract ${_archive}")
        endif()
        file(WRITE "${_extract_marker}" "${_tag}")
    endif()

    file(GLOB_RECURSE _wasmtime_headers
        "${_extract_dir}/*/include/wasmtime.h"
        "${_extract_dir}/include/wasmtime.h"
    )
    if(NOT _wasmtime_headers)
        message(FATAL_ERROR "Wasmtime: extracted archive is missing include/wasmtime.h")
    endif()

    list(GET _wasmtime_headers 0 _wasmtime_h)
    get_filename_component(_include_dir "${_wasmtime_h}" DIRECTORY)
    get_filename_component(_prefix "${_include_dir}" DIRECTORY)

    if(NOT _prefix STREQUAL "${_extract_dir}")
        file(COPY "${_prefix}/include" DESTINATION "${_extract_dir}")
        file(COPY "${_prefix}/lib" DESTINATION "${_extract_dir}")
    endif()

    message(STATUS "Wasmtime: prebuilt C API installed under ${_extract_dir}")
endfunction()

function(_woki_wasmtime_fetch_source)
    set(_source_dir "${CDEPS_ROOT}/wasmtime-src")
    set(_tag "v${WOKI_WASMTIME_VERSION}")

    if(EXISTS "${_source_dir}/crates/c-api/CMakeLists.txt")
        return()
    endif()

    file(MAKE_DIRECTORY "${CDEPS_ROOT}")

    message(STATUS "Wasmtime: fetching source ${_tag}")
    FetchContent_Declare(wasmtime_src
        GIT_REPOSITORY https://github.com/bytecodealliance/wasmtime.git
        GIT_TAG ${_tag}
        GIT_SHALLOW TRUE
        SOURCE_DIR ${_source_dir}
    )
    FetchContent_GetProperties(wasmtime_src)
    if(NOT wasmtime_src_POPULATED)
        FetchContent_Populate(wasmtime_src)
    endif()

    if(NOT EXISTS "${_source_dir}/crates/c-api/CMakeLists.txt")
        message(FATAL_ERROR "Wasmtime source fetch did not provide crates/c-api")
    endif()
endfunction()

function(_woki_wasmtime_add_c_api_subdirectory source_root)
    if(TARGET wasmtime)
        _woki_wasmtime_register_alias()
        return()
    endif()

    find_program(WOKI_CARGO cargo)
    if(NOT WOKI_CARGO)
        message(STATUS "Wasmtime: cargo not found; cannot build C API from source")
        return()
    endif()

    set(_c_api_dir "${source_root}/crates/c-api")
    if(NOT EXISTS "${_c_api_dir}/CMakeLists.txt")
        message(STATUS "Wasmtime: missing ${_c_api_dir}/CMakeLists.txt")
        return()
    endif()

    set(_build_dir "${CMAKE_BINARY_DIR}/wasmtime-c-api")

    if(WOKI_WASMTIME_USE_SHARED)
        set(BUILD_SHARED_LIBS ON CACHE BOOL "Wasmtime C API shared library" FORCE)
    else()
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Wasmtime C API shared library" FORCE)
    endif()
    set(BUILD_TESTS OFF CACHE BOOL "Wasmtime C API tests" FORCE)
    set(WASMTIME_ALWAYS_BUILD ON CACHE BOOL "Wasmtime cargo build" FORCE)

    message(STATUS "Wasmtime: building C API via add_subdirectory (official CMake integration)")
    message(STATUS "  source: ${_c_api_dir}")
    message(STATUS "  first build may take several minutes (cargo build -p wasmtime-c-api)")

    add_subdirectory("${_c_api_dir}" "${_build_dir}" EXCLUDE_FROM_ALL)
    _woki_wasmtime_register_alias()
endfunction()

function(_woki_wasmtime_define_prebuilt_target root_dir)
    set(_include_dir "${root_dir}/include")
    set(_lib_dir "${root_dir}/lib")

    if(NOT EXISTS "${_include_dir}/wasmtime.h" OR NOT EXISTS "${_include_dir}/wasm.h")
        return()
    endif()

    if(WOKI_WASMTIME_USE_SHARED)
        find_library(WASMTIME_LIBRARY
            NAMES wasmtime libwasmtime
            PATHS "${_lib_dir}"
            NO_DEFAULT_PATH
        )
    elseif(WIN32)
        find_library(WASMTIME_LIBRARY
            NAMES wasmtime libwasmtime
            PATHS "${_lib_dir}"
            NO_DEFAULT_PATH
        )
    elseif(EXISTS "${_lib_dir}/libwasmtime.a")
        set(WASMTIME_LIBRARY "${_lib_dir}/libwasmtime.a")
    else()
        find_library(WASMTIME_LIBRARY
            NAMES libwasmtime.a wasmtime
            PATHS "${_lib_dir}"
            NO_DEFAULT_PATH
        )
    endif()

    if(NOT WASMTIME_LIBRARY)
        return()
    endif()

    if(TARGET wasmtime::wasmtime)
        return()
    endif()

    add_library(wasmtime::wasmtime UNKNOWN IMPORTED GLOBAL)
    set_target_properties(wasmtime::wasmtime PROPERTIES
        IMPORTED_LOCATION "${WASMTIME_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${_include_dir}"
    )

    if(WOKI_WASMTIME_USE_SHARED)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set_property(TARGET wasmtime::wasmtime APPEND PROPERTY
                INTERFACE_LINK_OPTIONS "LINKER:-rpath,$ORIGIN"
            )
        endif()
    else()
        if(WIN32)
            set_property(TARGET wasmtime::wasmtime APPEND PROPERTY
                INTERFACE_COMPILE_DEFINITIONS
                    WASM_API_EXTERN=
                    WASI_API_EXTERN=
            )
            set_property(TARGET wasmtime::wasmtime APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES
                    ws2_32 advapi32 userenv ntdll shell32 ole32 bcrypt
            )
        elseif(APPLE)
            set_property(TARGET wasmtime::wasmtime APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES "-framework CoreFoundation"
            )
        else()
            set_property(TARGET wasmtime::wasmtime APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES pthread dl m
            )
        endif()
    endif()

    message(STATUS "Wasmtime C API (prebuilt): ${root_dir}")
    message(STATUS "  include: ${_include_dir}")
    message(STATUS "  library: ${WASMTIME_LIBRARY}")
    if(WOKI_WASMTIME_USE_SHARED)
        message(STATUS "  linkage: shared")
    else()
        message(STATUS "  linkage: static")
    endif()

    _woki_wasmtime_register_cpp_from_prebuilt("${_include_dir}")
endfunction()

function(_woki_wasmtime_build_from_source)
    _woki_wasmtime_find_source_root(_source_root)
    if(NOT _source_root)
        _woki_wasmtime_fetch_source()
        _woki_wasmtime_find_source_root(_source_root)
    endif()
    if(_source_root)
        _woki_wasmtime_add_c_api_subdirectory("${_source_root}")
    endif()
endfunction()

function(woki_setup_wasmtime)
    if(TARGET wasmtime::wasmtime OR TARGET wasmtime)
        _woki_wasmtime_register_alias()
        return()
    endif()

    if(NOT WOKI_EXTENSION_WITH_WASMTIME)
        message(STATUS "Wasmtime: skipped (WOKI_EXTENSION_WITH_WASMTIME=OFF)")
        return()
    endif()

    string(TOLOWER "${WOKI_WASMTIME_PROVIDER}" _provider)
    if(_provider STREQUAL "build" OR _provider STREQUAL "rust" OR _provider STREQUAL "cargo")
        set(_provider "source")
    endif()

    if(NOT _provider MATCHES "^(prebuilt|source|auto)$")
        message(FATAL_ERROR
            "WOKI_WASMTIME_PROVIDER must be prebuilt, source, or auto (got '${WOKI_WASMTIME_PROVIDER}')")
    endif()

    message(STATUS "Wasmtime provider: ${_provider}")

    _woki_wasmtime_find_prebuilt_root(_root)
    if(_root)
        _woki_wasmtime_define_prebuilt_target("${_root}")
        if(TARGET wasmtime::wasmtime)
            return()
        endif()
    endif()

    _woki_wasmtime_find_source_root(_source_root)
    if(_source_root)
        _woki_wasmtime_add_c_api_subdirectory("${_source_root}")
        if(TARGET wasmtime::wasmtime)
            return()
        endif()
    endif()

    if(_provider STREQUAL "prebuilt" OR _provider STREQUAL "auto")
        _woki_wasmtime_download_prebuilt()

        _woki_wasmtime_find_prebuilt_root(_root)
        if(_root)
            _woki_wasmtime_define_prebuilt_target("${_root}")
            if(TARGET wasmtime::wasmtime)
                return()
            endif()
        endif()
    endif()

    if(_provider STREQUAL "source" OR _provider STREQUAL "auto")
        _woki_wasmtime_build_from_source()
        if(TARGET wasmtime::wasmtime)
            return()
        endif()
    endif()

    if(_provider STREQUAL "prebuilt")
        message(FATAL_ERROR
            "Wasmtime prebuilt C API could not be downloaded or found.\n"
            "  - Check network access and WOKI_WASMTIME_VERSION (${WOKI_WASMTIME_VERSION})\n"
            "  - Or set WOKI_WASMTIME_ROOT to a *-c-api install tree\n"
            "  - Or use WASMTIME_PROVIDER=source (requires Rust/cargo)\n"
            "  - Or use WASMTIME_PROVIDER=auto")
    endif()

    message(FATAL_ERROR
        "Wasmtime C API is required (WOKI_EXTENSION_WITH_WASMTIME=ON) but could not be obtained.\n"
        "  WASMTIME_PROVIDER=${WOKI_WASMTIME_PROVIDER}\n"
        "  prebuilt — download official *-c-api release (default)\n"
        "  source   — git fetch + cargo build (requires Rust)\n"
        "  auto     — prebuilt first, then source\n"
        "  Or set WOKI_WASMTIME_ROOT, or WOKI_EXTENSION_WITH_WASMTIME=OFF")
endfunction()
