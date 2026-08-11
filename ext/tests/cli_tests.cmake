if(NOT DEFINED WOKIEXT OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "WOKIEXT and TEST_ROOT are required")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/working")

execute_process(COMMAND "${WOKIEXT}" RESULT_VARIABLE no_args_result OUTPUT_QUIET ERROR_QUIET)
if(NOT no_args_result EQUAL 2)
    message(FATAL_ERROR "wokiext without a command must return usage status 2")
endif()

execute_process(COMMAND "${WOKIEXT}" help RESULT_VARIABLE help_result OUTPUT_VARIABLE help_output)
if(NOT help_result EQUAL 0 OR NOT help_output MATCHES "Usage:")
    message(FATAL_ERROR "wokiext help failed")
endif()

foreach(language IN ITEMS c cpp)
    set(project_name "Test ${language} Extension")
    set(project_dir "${TEST_ROOT}/projects/test-${language}-extension")
    execute_process(
        COMMAND "${WOKIEXT}" create "${project_name}" --lang "${language}" --out "../projects"
        WORKING_DIRECTORY "${TEST_ROOT}/working"
        RESULT_VARIABLE create_result
        OUTPUT_VARIABLE create_output
        ERROR_VARIABLE create_error
    )
    if(NOT create_result EQUAL 0)
        message(FATAL_ERROR "wokiext create failed for ${language} (${create_result})\n${create_output}${create_error}")
    endif()

    file(READ "${project_dir}/CMakeLists.txt" project_contents)
    string(FIND "${project_contents}" "include(\"\${WOKI_CMAKE_DIR}/ExtensionProject.cmake\")\nproject(" toolchain_position)
    if(toolchain_position EQUAL -1)
        message(FATAL_ERROR "Generated ${language} project does not load ExtensionProject.cmake before project()")
    endif()

    if(language STREQUAL "cpp")
        set(build_config Debug)
        set(expected_config_flag "-O0")
    else()
        set(build_config Release)
        set(expected_config_flag "-O2")
    endif()
    execute_process(
        COMMAND "${WOKIEXT}" build "../projects/test-${language}-extension" --config "${build_config}"
        WORKING_DIRECTORY "${TEST_ROOT}/working"
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_output
        ERROR_VARIABLE build_error
    )
    set(package_dir "${project_dir}/build/package")
    if(NOT build_result EQUAL 0 OR NOT EXISTS "${package_dir}/extension.wasm" OR NOT EXISTS "${package_dir}/manifest.yaml")
        message(FATAL_ERROR "wokiext build failed for generated ${language} project (${build_result})\n${build_output}${build_error}")
    endif()
    if(NOT EXISTS "${package_dir}/.woki-state" OR EXISTS "${package_dir}.state")
        message(FATAL_ERROR "wokiext did not publish package content and state as one directory transaction")
    endif()
    execute_process(
        COMMAND "${WOKIEXT}" verify "${package_dir}"
        RESULT_VARIABLE verify_result
        OUTPUT_VARIABLE verify_output
        ERROR_VARIABLE verify_error
    )
    if(NOT verify_result EQUAL 0 OR NOT verify_output MATCHES "Verified extension:")
        message(FATAL_ERROR "wokiext verify failed for generated ${language} package (${verify_result})\n${verify_output}${verify_error}")
    endif()
    file(READ "${project_dir}/compile_commands.json" compile_commands)
    string(FIND "${compile_commands}" "${expected_config_flag}" config_flag_position)
    if(config_flag_position EQUAL -1)
        message(FATAL_ERROR "Generated ${language} project did not use ${build_config} guest flags")
    endif()

    if(language STREQUAL "cpp")
        set(archive "${TEST_ROOT}/test-cpp.wokiext")
        set(install_root "${TEST_ROOT}/install-root")
        execute_process(COMMAND "${WOKIEXT}" bundle "${project_dir}" --out "${archive}" RESULT_VARIABLE bundle_result ERROR_VARIABLE bundle_error)
        if(NOT bundle_result EQUAL 0)
            message(FATAL_ERROR "wokiext bundle failed (${bundle_result})\n${bundle_error}")
        endif()
        set(source_file "${project_dir}/src/plugin.cpp")
        set(source_backup "${TEST_ROOT}/plugin-backup.cpp")
        file(COPY_FILE "${source_file}" "${source_backup}")
        file(APPEND "${source_file}" "\n// content hash change\n")
        execute_process(COMMAND "${WOKIEXT}" bundle "${project_dir}" --out "${TEST_ROOT}/stale-source.wokiext" RESULT_VARIABLE stale_source_result OUTPUT_QUIET ERROR_QUIET)
        file(COPY_FILE "${source_backup}" "${source_file}")
        file(REMOVE "${source_backup}")
        if(stale_source_result EQUAL 0)
            message(FATAL_ERROR "wokiext bundle accepted source content changed after staging")
        endif()
        file(MAKE_DIRECTORY "${project_dir}/assets")
        file(WRITE "${project_dir}/assets/added-after-build.txt" "stale")
        execute_process(COMMAND "${WOKIEXT}" bundle "${project_dir}" --out "${TEST_ROOT}/stale-assets.wokiext" RESULT_VARIABLE stale_asset_result OUTPUT_QUIET ERROR_QUIET)
        file(REMOVE_RECURSE "${project_dir}/assets")
        if(stale_asset_result EQUAL 0)
            message(FATAL_ERROR "wokiext bundle accepted assets added after staging")
        endif()
        file(WRITE "${archive}" "preserve-existing")
        file(MAKE_DIRECTORY "${package_dir}/assets")
        file(CREATE_LINK "${project_dir}/manifest.yaml" "${package_dir}/assets/bad-link" SYMBOLIC RESULT link_result)
        if(NOT link_result)
            execute_process(COMMAND "${WOKIEXT}" bundle "${package_dir}" --out "${archive}" RESULT_VARIABLE failed_bundle OUTPUT_QUIET ERROR_QUIET)
            file(READ "${archive}" preserved_archive)
            file(REMOVE "${package_dir}/assets/bad-link")
            if(failed_bundle EQUAL 0 OR NOT preserved_archive STREQUAL "preserve-existing")
                message(FATAL_ERROR "failed bundle did not preserve the existing archive")
            endif()
        endif()
        execute_process(COMMAND "${WOKIEXT}" bundle "${project_dir}" --out "${archive}" RESULT_VARIABLE bundle_result ERROR_VARIABLE bundle_error)
        if(NOT bundle_result EQUAL 0)
            message(FATAL_ERROR "wokiext could not rebuild archive after failure: ${bundle_error}")
        endif()

        execute_process(COMMAND "${WOKIEXT}" bundle "${project_dir}" RESULT_VARIABLE default_bundle_result OUTPUT_VARIABLE default_bundle_output ERROR_VARIABLE default_bundle_error)
        set(default_archive "${TEST_ROOT}/projects/woki.test.cpp.extension-0.1.0.wokiext")
        if(NOT default_bundle_result EQUAL 0 OR NOT EXISTS "${default_archive}")
            message(FATAL_ERROR "default bundle output was not based on the project path\n${default_bundle_output}${default_bundle_error}")
        endif()
        execute_process(COMMAND "${WOKIEXT}" install "${archive}" --root "${install_root}" RESULT_VARIABLE install_result ERROR_VARIABLE install_error)
        if(NOT install_result EQUAL 0 OR NOT EXISTS "${install_root}/extensions/woki.test.cpp.extension/extension.wasm")
            message(FATAL_ERROR "wokiext install failed (${install_result})\n${install_error}")
        endif()
        execute_process(COMMAND "${WOKIEXT}" install "${archive}" --root "${install_root}" RESULT_VARIABLE duplicate_result OUTPUT_QUIET ERROR_QUIET)
        if(duplicate_result EQUAL 0)
            message(FATAL_ERROR "duplicate install unexpectedly succeeded without --force")
        endif()
        execute_process(COMMAND "${WOKIEXT}" install "${archive}" --root "${install_root}" --force RESULT_VARIABLE replace_result ERROR_VARIABLE replace_error)
        if(NOT replace_result EQUAL 0 OR NOT EXISTS "${install_root}/extensions/woki.test.cpp.extension/extension.wasm")
            message(FATAL_ERROR "wokiext replacement install failed (${replace_result})\n${replace_error}")
        endif()
        set(directory_install_root "${TEST_ROOT}/directory-install-root")
        execute_process(COMMAND "${WOKIEXT}" install "${package_dir}" --root "${directory_install_root}" RESULT_VARIABLE directory_install_result ERROR_VARIABLE directory_install_error)
        if(NOT directory_install_result EQUAL 0 OR NOT EXISTS "${directory_install_root}/extensions/woki.test.cpp.extension/extension.wasm")
            message(FATAL_ERROR "wokiext directory install failed (${directory_install_result})\n${directory_install_error}")
        endif()

        file(READ "${project_dir}/manifest.yaml" original_manifest)
        file(APPEND "${project_dir}/manifest.yaml" "# stale\n")
        execute_process(COMMAND "${WOKIEXT}" bundle "${project_dir}" --out "${TEST_ROOT}/stale.wokiext" RESULT_VARIABLE stale_result OUTPUT_QUIET ERROR_QUIET)
        file(WRITE "${project_dir}/manifest.yaml" "${original_manifest}")
        if(stale_result EQUAL 0)
            message(FATAL_ERROR "wokiext bundle accepted a stale project package")
        endif()

        execute_process(
            COMMAND "${WOKIEXT}" run "${project_dir}" --config "${build_config}"
            RESULT_VARIABLE run_result
            OUTPUT_VARIABLE run_output
            ERROR_VARIABLE run_error
        )
        if(NOT run_result EQUAL 0 OR NOT run_output MATCHES "Bundled extension:")
            message(FATAL_ERROR "wokiext run failed (${run_result})\n${run_output}${run_error}")
        endif()
    endif()

    execute_process(
        COMMAND "${WOKIEXT}" test "${project_dir}" --config "${build_config}"
        RESULT_VARIABLE test_result
        OUTPUT_VARIABLE test_output
        ERROR_VARIABLE test_error
    )
    if(NOT test_result EQUAL 0 OR NOT test_output MATCHES "Verified extension:")
        message(FATAL_ERROR "wokiext test failed for ${language} (${test_result})\n${test_output}${test_error}")
    endif()

    execute_process(COMMAND "${WOKIEXT}" clean "${project_dir}" RESULT_VARIABLE clean_result)
    if(NOT clean_result EQUAL 0 OR EXISTS "${project_dir}/build")
        message(FATAL_ERROR "wokiext clean failed for generated ${language} project")
    endif()
endforeach()

execute_process(
    COMMAND "${WOKIEXT}" schema
    RESULT_VARIABLE schema_result
    OUTPUT_VARIABLE schema_output
)
if(NOT schema_result EQUAL 0 OR NOT schema_output MATCHES "Woki extension manifest")
    message(FATAL_ERROR "Embedded schema is unavailable")
endif()

file(MAKE_DIRECTORY "${TEST_ROOT}/root/extensions")
file(WRITE "${TEST_ROOT}/root/sentinel" "keep")
execute_process(
    COMMAND "${WOKIEXT}" remove ../sentinel --root "${TEST_ROOT}/root"
    RESULT_VARIABLE remove_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(remove_result EQUAL 0 OR NOT EXISTS "${TEST_ROOT}/root/sentinel")
    message(FATAL_ERROR "Traversal-like extension id was not safely rejected")
endif()
