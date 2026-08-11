if(NOT DEFINED TEST_ROOT OR NOT DEFINED WOKI_CMAKE_DIR OR NOT DEFINED WOKI_SDK_DIR)
    message(FATAL_ERROR "TEST_ROOT, WOKI_CMAKE_DIR, and WOKI_SDK_DIR are required")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
set(relocated_prefix "${TEST_ROOT}/relocated")
set(relocated_cmake "${relocated_prefix}/share/woki/cmake")
set(relocated_sdk "${relocated_prefix}/include")
file(MAKE_DIRECTORY "${relocated_cmake}" "${relocated_sdk}")
file(COPY
    "${WOKI_CMAKE_DIR}/ExtensionProject.cmake"
    "${WOKI_CMAKE_DIR}/ExtensionWasm.cmake"
    "${WOKI_CMAKE_DIR}/ExtensionManifest.cmake"
    DESTINATION "${relocated_cmake}"
)
file(MAKE_DIRECTORY "${relocated_sdk}/woki")
file(COPY "${WOKI_SDK_DIR}/woki/ext" DESTINATION "${relocated_sdk}/woki")
file(GLOB sdk_root_files "${WOKI_SDK_DIR}/*.h" "${WOKI_SDK_DIR}/*.def")
file(COPY ${sdk_root_files} DESTINATION "${relocated_sdk}")

execute_process(COMMAND "${CMAKE_COMMAND}" -E capabilities OUTPUT_VARIABLE cmake_capabilities)
find_program(ninja_executable ninja)
if(ninja_executable AND cmake_capabilities MATCHES "Ninja Multi-Config")
    set(generator_args -G "Ninja Multi-Config")
    set(is_multi_config TRUE)
else()
    set(generator_args)
    set(is_multi_config FALSE)
endif()

foreach(language IN ITEMS C CXX)
    string(TOLOWER "${language}" language_lower)
    set(source_dir "${TEST_ROOT}/${language_lower}-project")
    set(build_dir "${TEST_ROOT}/${language_lower}-build")
    file(MAKE_DIRECTORY "${source_dir}/src" "${source_dir}/assets")
    if(language STREQUAL "CXX")
        set(wasm_relative "bin/custom-guest.wasm")
    else()
        set(wasm_relative "extension.wasm")
    endif()
    file(WRITE "${source_dir}/manifest.yaml"
        "id: woki.test.external.${language_lower}\nname: External ${language}\nversion: 1.0.0\napiVersion: 1\nruntime:\n  wasm: ${wasm_relative}\npermissions:\n  - log\n")
    file(WRITE "${source_dir}/assets/data.txt" "${language} asset\n")

    if(language STREQUAL "CXX")
        set(extension .cpp)
        set(extern_open "extern \"C\" {\n")
        set(extern_close "}\n")
        set(unused_compiler "-DWOKI_WASM_C_COMPILER=${TEST_ROOT}/missing-clang")
    else()
        set(extension .c)
        set(extern_open "")
        set(extern_close "")
        set(unused_compiler "-DWOKI_WASM_COMPILER=${TEST_ROOT}/missing-clang++")
    endif()
    if(language STREQUAL "CXX")
        file(WRITE "${source_dir}/src/plugin.cpp" "#include <woki/ext/plugin.hpp>\nclass Guest { public: woki::ext::Status OnLoad(woki::ext::Context& context) { return context.GetLog().Info(\"external C++\"); } };\nWOKI_PLUGIN(Guest)\n")
        file(WRITE "${source_dir}/src/events.cpp" "#include <woki/ext/plugin.hpp>\nstatic_assert(woki::ext::StringView(\"woki.test.external.event\").Size() == 24u);\n")
    else()
        file(WRITE "${source_dir}/src/plugin.c"
            "#include <woki/ext/sdk/ext.h>\nWOKI_EXPORT(\"ext_api_version\") uint32_t ext_api_version(void) { return WOKI_EXT_API_VERSION; }\nWOKI_EXPORT(\"ext_init\") int32_t ext_init(void) { return 0; }\nWOKI_EXPORT(\"ext_on_tick\") void ext_on_tick(double value) { (void)value; }\n")
        file(WRITE "${source_dir}/src/events.c"
            "#include <woki/ext/sdk/ext.h>\nWOKI_EXPORT(\"ext_on_event\") void ext_on_event(uint32_t type, const uint8_t* data, uint32_t size) { (void)type; (void)data; (void)size; }\nWOKI_EXPORT(\"ext_on_unload\") void ext_on_unload(void) {}\n")
    endif()
    file(WRITE "${source_dir}/CMakeLists.txt"
        "cmake_minimum_required(VERSION 3.25)\ninclude(\"${relocated_cmake}/ExtensionProject.cmake\")\nproject(external_${language_lower} LANGUAGES ${language})\nset(CMAKE_EXPORT_COMPILE_COMMANDS ON)\ninclude(\"${relocated_cmake}/ExtensionWasm.cmake\")\nadd_wokiext(guest LANGUAGE ${language} MANIFEST manifest.yaml SOURCES src/plugin${extension} src/events${extension} ASSETS assets/data.txt)\nget_target_property(wasm guest WOKI_EXTENSION_WASM_OUTPUT)\nget_target_property(package guest WOKI_EXTENSION_PACKAGE_DIR)\nfile(GENERATE OUTPUT \"${build_dir}/target-properties-$<CONFIG>.txt\" CONTENT \"$<TARGET_FILE:guest>\\n\${wasm}\\n\${package}\\n\")\n")

    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${generator_args} -S "${source_dir}" -B "${build_dir}"
            -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "${unused_compiler}"
        RESULT_VARIABLE configure_result
        OUTPUT_VARIABLE configure_output
        ERROR_VARIABLE configure_error
    )
    if(NOT configure_result EQUAL 0)
        message(FATAL_ERROR "External ${language} configure failed\n${configure_output}${configure_error}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${build_dir}" --config Release --verbose
        RESULT_VARIABLE release_result
        OUTPUT_VARIABLE release_output
        ERROR_VARIABLE release_error
    )
    if(NOT release_result EQUAL 0)
        message(FATAL_ERROR "External ${language} Release build failed\n${release_output}${release_error}")
    endif()
    foreach(source IN ITEMS plugin events)
        string(REGEX MATCHALL "${source_dir}/src/${source}\\${extension}" source_mentions "${release_output}${release_error}")
        list(LENGTH source_mentions source_mention_count)
        if(NOT source_mention_count EQUAL 1)
            message(FATAL_ERROR "External ${language} ${source}${extension} was mentioned in ${source_mention_count} Release compile commands, expected 1\n${release_output}${release_error}")
        endif()
    endforeach()

    set(package_dir "${build_dir}/package/Release")
    if(NOT EXISTS "${package_dir}/${wasm_relative}" OR
       NOT EXISTS "${package_dir}/manifest.yaml" OR
       NOT EXISTS "${package_dir}/assets/data.txt")
        message(FATAL_ERROR "External ${language} package is incomplete")
    endif()
    file(READ "${build_dir}/target-properties-Release.txt" target_properties)
    string(REPLACE "\\" "/" target_properties "${target_properties}")
    string(REPLACE "\\" "/" expected_wasm "${package_dir}/${wasm_relative}")
    string(FIND "${target_properties}" "${expected_wasm}" wasm_property_position)
    if(wasm_property_position EQUAL -1)
        message(FATAL_ERROR "External ${language} target output properties do not identify the packaged wasm\n${target_properties}")
    endif()

    file(READ "${build_dir}/compile_commands.json" compile_commands)
    if(NOT compile_commands MATCHES "-O2" OR NOT compile_commands MATCHES "${extension}")
        message(FATAL_ERROR "External ${language} compilation database is missing Release guest commands")
    endif()

    if(is_multi_config)
        file(SHA256 "${package_dir}/${wasm_relative}" release_hash)
        execute_process(
            COMMAND "${CMAKE_COMMAND}" --build "${build_dir}" --config Debug --verbose
            RESULT_VARIABLE debug_result
            OUTPUT_VARIABLE debug_output
            ERROR_VARIABLE debug_error
        )
        if(NOT debug_result EQUAL 0 OR NOT debug_output MATCHES "-O0")
            message(FATAL_ERROR "External ${language} multi-config Debug build failed or omitted Debug guest flags\n${debug_output}${debug_error}")
        endif()
        set(debug_package_dir "${build_dir}/package/Debug")
        if(NOT EXISTS "${debug_package_dir}/${wasm_relative}" OR NOT EXISTS "${package_dir}/${wasm_relative}")
            message(FATAL_ERROR "External ${language} multi-config outputs were not isolated by configuration")
        endif()
        file(SHA256 "${debug_package_dir}/${wasm_relative}" debug_hash)
        if(release_hash STREQUAL debug_hash)
            message(FATAL_ERROR "External ${language} Debug and Release packages unexpectedly have the same hash")
        endif()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" --build "${build_dir}" --config Release
            RESULT_VARIABLE release_round_trip_result
        )
        file(SHA256 "${package_dir}/${wasm_relative}" release_round_trip_hash)
        if(NOT release_round_trip_result EQUAL 0 OR NOT release_hash STREQUAL release_round_trip_hash)
            message(FATAL_ERROR "External ${language} Release package hash changed after a Debug round trip")
        endif()
    endif()
endforeach()

set(hash_package "${TEST_ROOT}/hash-package")
file(WRITE "${hash_package}.state" "legacy-sidecar")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -D SOURCE_DIR=${package_dir}
        -D DESTINATION_DIR=${hash_package}
        -D SOURCE_MANIFEST=${source_dir}/manifest.yaml
        -D PROJECT_DIR=${source_dir}
        -D SDK_DIR=${relocated_sdk}
        -D CONFIG=Release
        -P "${WOKI_CMAKE_DIR}/StageExtensionPackage.cmake"
    RESULT_VARIABLE hash_stage_result
)
if(NOT hash_stage_result EQUAL 0)
    message(FATAL_ERROR "Could not stage package for SDK content-hash test")
endif()
if(EXISTS "${hash_package}.state")
    message(FATAL_ERROR "Legacy package state sidecar was not removed during migration")
endif()
set(sdk_hash_probe "${relocated_sdk}/woki/ext/sdk/ext.h")
file(READ "${sdk_hash_probe}" sdk_hash_probe_contents)
file(APPEND "${sdk_hash_probe}" "\n/* changed SDK content */\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -D PROJECT_DIR=${source_dir}
        -D SDK_DIR=${relocated_sdk}
        -D STATE_FILE=${hash_package}/.woki-state
        -P "${WOKI_CMAKE_DIR}/CheckExtensionState.cmake"
    RESULT_VARIABLE stale_sdk_result
    OUTPUT_QUIET
    ERROR_QUIET
)
file(WRITE "${sdk_hash_probe}" "${sdk_hash_probe_contents}")
if(stale_sdk_result EQUAL 0)
    message(FATAL_ERROR "SDK content changes did not invalidate the staged package")
endif()

set(failed_source "${TEST_ROOT}/failed-stage-source")
set(preserved_destination "${TEST_ROOT}/preserved-package")
file(MAKE_DIRECTORY "${failed_source}" "${preserved_destination}")
file(COPY "${package_dir}/" DESTINATION "${failed_source}")
file(WRITE "${failed_source}/${wasm_relative}" "not wasm")
file(WRITE "${preserved_destination}/sentinel" "preserved")
file(WRITE "${preserved_destination}/.woki-state" "preserved-state")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -D SOURCE_DIR=${failed_source}
        -D DESTINATION_DIR=${preserved_destination}
        -D SOURCE_MANIFEST=${source_dir}/manifest.yaml
        -D PROJECT_DIR=${source_dir}
        -D SDK_DIR=${relocated_sdk}
        -D CONFIG=Release
        -P "${WOKI_CMAKE_DIR}/StageExtensionPackage.cmake"
    RESULT_VARIABLE failed_stage_result
    OUTPUT_QUIET
    ERROR_QUIET
)
file(READ "${preserved_destination}/sentinel" preserved_package_contents)
file(READ "${preserved_destination}/.woki-state" preserved_state_contents)
if(failed_stage_result EQUAL 0 OR NOT preserved_package_contents STREQUAL "preserved" OR NOT preserved_state_contents STREQUAL "preserved-state")
    message(FATAL_ERROR "failed staging did not preserve the existing package and state")
endif()
