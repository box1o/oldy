# Wasm guest extension build and package assembly.
#
# Usage:
#   include(${WOKI_CMAKE_DIR}/ExtensionWasm.cmake)
#   add_wokiext(my_extension
#       LANGUAGE CXX
#       MANIFEST manifest.yaml
#       SOURCES src/plugin.cpp src/commands.cpp
#       ASSETS assets/icon.png
#   )

set(WOKI_WASM_GUEST_REQUIRED_EXPORTS
    ext_api_version
    ext_init
    ext_on_tick
    ext_on_event
    ext_on_unload
)

set(WOKI_WASM_GUEST_OPTIONAL_EXPORTS
    ext_on_command
    ext_on_event_named
    ext_alloc
    ext_free
)

set(WOKI_WASM_HOST_IMPORTS
    host_log
    host_path_data
    host_path_cache
    host_file_read
    host_file_write
    host_file_append
    host_file_read_n
    host_file_write_n
    host_file_append_n
    host_config_get
    host_config_set
    host_event_subscribe
    host_event_emit
    host_event_subscribe_named
    host_event_emit_named
)

function(add_wokiext target)
    if(NOT target)
        message(FATAL_ERROR "add_wokiext requires a target name")
    endif()
    cmake_parse_arguments(WOKIEXT "" "LANGUAGE;MANIFEST" "SOURCES;ASSETS" ${ARGN})
    if(WOKIEXT_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "add_wokiext received unknown arguments: ${WOKIEXT_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT WOKIEXT_LANGUAGE)
        message(FATAL_ERROR "add_wokiext requires explicit LANGUAGE C or CXX")
    endif()
    string(TOUPPER "${WOKIEXT_LANGUAGE}" _language)
    if(NOT _language STREQUAL "C" AND NOT _language STREQUAL "CXX")
        message(FATAL_ERROR "add_wokiext LANGUAGE must be C or CXX")
    endif()
    if(NOT WOKIEXT_MANIFEST)
        message(FATAL_ERROR "add_wokiext requires MANIFEST")
    endif()
    if(NOT WOKIEXT_SOURCES)
        message(FATAL_ERROR "add_wokiext requires at least one source")
    endif()
    if(_language STREQUAL "C" AND NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Wasm C extensions require clang; set WOKI_WASM_C_COMPILER")
    elseif(_language STREQUAL "CXX" AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Wasm C++ extensions require clang++; set WOKI_WASM_COMPILER")
    endif()

    if(NOT WOKI_SDK_DIR)
        if(EXISTS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/extension/sdk/woki/ext/sdk/ext.h")
            set(WOKI_SDK_DIR "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/extension/sdk")
        elseif(EXISTS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../include/woki/ext/sdk/ext.h")
            set(WOKI_SDK_DIR "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../include")
        else()
            message(FATAL_ERROR "Cannot locate the Woki extension SDK; set WOKI_SDK_DIR")
        endif()
    endif()

    get_filename_component(_manifest "${WOKIEXT_MANIFEST}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${_manifest}")
        message(FATAL_ERROR "Extension manifest does not exist: ${_manifest}")
    endif()
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ExtensionManifest.cmake")
    woki_extension_manifest_wasm("${_manifest}" _wasm_relative)
    woki_extension_manifest_libraries("${_manifest}" _guest_libraries)
    woki_extension_manifest_permissions("${_manifest}" _permissions)

    set(_sources)
    foreach(_source_file IN LISTS WOKIEXT_SOURCES)
        get_filename_component(_source "${_source_file}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(NOT EXISTS "${_source}")
            message(FATAL_ERROR "Extension source does not exist: ${_source}")
        endif()
        list(APPEND _sources "${_source}")
    endforeach()

    if(WOKI_EXTENSION_PACKAGE_DIR)
        get_filename_component(_package_root "${WOKI_EXTENSION_PACKAGE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    else()
        set(_package_root "${CMAKE_CURRENT_BINARY_DIR}/package")
    endif()
    set(_package_dir "${_package_root}/$<CONFIG>")
    set(_wasm "${_package_dir}/${_wasm_relative}")
    get_filename_component(_wasm_output_dir "${_wasm}" DIRECTORY)
    get_filename_component(_wasm_output_name "${_wasm}" NAME_WE)
    get_filename_component(_wasm_output_extension "${_wasm}" EXT)
    set(_package_manifest "${_package_dir}/manifest.yaml")
    set(_allowed_imports "${CMAKE_CURRENT_BINARY_DIR}/${target}.allowed-imports")
    set(_allowed_host_imports)
    if("log" IN_LIST _permissions)
        list(APPEND _allowed_host_imports host_log)
    endif()
    if("paths" IN_LIST _permissions)
        list(APPEND _allowed_host_imports host_path_data host_path_cache)
    endif()
    if("storage" IN_LIST _permissions)
        list(APPEND _allowed_host_imports host_file_read host_file_write host_file_append host_file_read_n host_file_write_n host_file_append_n)
    endif()
    if("config" IN_LIST _permissions)
        list(APPEND _allowed_host_imports host_config_get host_config_set)
    endif()
    if("events" IN_LIST _permissions)
        list(APPEND _allowed_host_imports host_event_subscribe host_event_emit host_event_subscribe_named host_event_emit_named)
    endif()
    string(JOIN "\n" _allowed_import_contents ${_allowed_host_imports})
    file(GENERATE OUTPUT "${_allowed_imports}" CONTENT "${_allowed_import_contents}\n")

    set(_asset_sources)
    set(_asset_outputs)
    set(_asset_copy_commands)
    foreach(_asset IN LISTS WOKIEXT_ASSETS)
        get_filename_component(_asset_source "${_asset}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(RELATIVE_PATH _asset_relative "${CMAKE_CURRENT_SOURCE_DIR}" "${_asset_source}")
        if(IS_ABSOLUTE "${_asset_relative}" OR _asset_relative MATCHES "^\\.\\./" OR NOT _asset_relative MATCHES "^assets/")
            message(FATAL_ERROR "Extension assets must be relative paths below assets/: ${_asset}")
        endif()
        if(NOT EXISTS "${_asset_source}" OR IS_DIRECTORY "${_asset_source}" OR IS_SYMLINK "${_asset_source}")
            message(FATAL_ERROR "Extension asset is not a regular file: ${_asset_source}")
        endif()
        get_filename_component(_asset_output_dir "${_package_dir}/${_asset_relative}" DIRECTORY)
        list(APPEND _asset_sources "${_asset_source}")
        list(APPEND _asset_outputs "${_package_dir}/${_asset_relative}")
        list(APPEND _asset_copy_commands
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_asset_output_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_asset_source}" "${_package_dir}/${_asset_relative}"
        )
    endforeach()

    file(GLOB_RECURSE _sdk_headers CONFIGURE_DEPENDS "${WOKI_SDK_DIR}/woki/*.h" "${WOKI_SDK_DIR}/woki/*.hpp" "${WOKI_SDK_DIR}/woki/*.def")
    set(_config_flags
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Release>:-DNDEBUG>
        $<$<CONFIG:RelWithDebInfo>:-O2>
        $<$<CONFIG:RelWithDebInfo>:-g>
        $<$<CONFIG:RelWithDebInfo>:-DNDEBUG>
        $<$<CONFIG:MinSizeRel>:-Oz>
        $<$<CONFIG:MinSizeRel>:-DNDEBUG>
    )

    set(_export_flags)
    foreach(_symbol IN LISTS WOKI_WASM_GUEST_REQUIRED_EXPORTS)
        list(APPEND _export_flags "LINKER:--export=${_symbol}")
    endforeach()
    foreach(_symbol IN LISTS WOKI_WASM_GUEST_OPTIONAL_EXPORTS)
        list(APPEND _export_flags "LINKER:--export-if-defined=${_symbol}")
    endforeach()

    add_executable(${target} ${_sources})
    set_source_files_properties(${_sources} PROPERTIES LANGUAGE "${_language}")
    target_include_directories(${target} PRIVATE "${WOKI_SDK_DIR}")
    foreach(_permission IN ITEMS log paths storage config events)
        string(TOUPPER "${_permission}" _permission_upper)
        if(_permission IN_LIST _permissions)
            target_compile_definitions(${target} PRIVATE "WOKI_EXT_HAS_${_permission_upper}=1")
        else()
            target_compile_definitions(${target} PRIVATE "WOKI_EXT_HAS_${_permission_upper}=0")
        endif()
    endforeach()
    target_compile_options(${target} PRIVATE -nostdlib -fno-builtin ${_config_flags})
    if(_language STREQUAL "CXX")
        target_compile_features(${target} PRIVATE cxx_std_23)
        target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti)
    else()
        target_compile_features(${target} PRIVATE c_std_17)
    endif()
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ExtensionGuestLibraries.cmake")
    woki_add_guest_libraries("${target}" "${_language}" "${_guest_libraries}" _guest_library_targets)
    if(_guest_library_targets)
        target_link_libraries(${target} PRIVATE ${_guest_library_targets})
    endif()
    target_link_options(${target} PRIVATE
        -nostdlib
        LINKER:--no-entry
        LINKER:--unresolved-symbols=report-all
        "LINKER:--allow-undefined-file=${_allowed_imports}"
        LINKER:--export-memory
        LINKER:--max-memory=33554432
        ${_export_flags}
    )

    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${_wasm_output_name}"
        SUFFIX "${_wasm_output_extension}"
        RUNTIME_OUTPUT_DIRECTORY "${_wasm_output_dir}"
        EXPORT_COMPILE_COMMANDS ON
        LINKER_LANGUAGE "${_language}"
        LINK_DEPENDS "${_manifest};${_asset_sources};${_sdk_headers};${_allowed_imports}"
        WOKI_EXTENSION_WASM_OUTPUT "${_wasm}"
        WOKI_EXTENSION_PACKAGE_DIR "${_package_dir}"
    )

    add_custom_command(TARGET ${target} PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${_package_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_package_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_wasm_output_dir}"
        VERBATIM
    )
    add_custom_command(TARGET ${target} POST_BUILD
        BYPRODUCTS "${_package_manifest}" ${_asset_outputs}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_manifest}" "${_package_manifest}"
        ${_asset_copy_commands}
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endfunction()
