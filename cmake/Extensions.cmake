# Register isolated extension guest builds without leaking their wasm toolchain
# into the host build.

function(woki_register_extensions extensions_dir)
    if(NOT EXISTS "${extensions_dir}")
        return()
    endif()

    if(NOT TARGET woki_extensions)
        add_custom_target(woki_extensions ALL)
        set_target_properties(woki_extensions PROPERTIES
            WOKI_EXTENSION_PACKAGES_DIR "${CMAKE_BINARY_DIR}/extension-packages/$<CONFIG>"
        )
    endif()

    file(GLOB extension_entries RELATIVE "${extensions_dir}" "${extensions_dir}/*")
    foreach(extension_entry IN LISTS extension_entries)
        set(extension_dir "${extensions_dir}/${extension_entry}")
        if(NOT IS_DIRECTORY "${extension_dir}" OR NOT EXISTS "${extension_dir}/CMakeLists.txt")
            continue()
        endif()

        set(extension_build_dir "${CMAKE_BINARY_DIR}/extensions-ninja/${extension_entry}/$<CONFIG>")
        set(extension_package_dir "${CMAKE_BINARY_DIR}/extension-packages/$<CONFIG>/${extension_entry}")
        set(extension_built_package_dir "${extension_build_dir}/package/$<CONFIG>")
        string(MAKE_C_IDENTIFIER "woki_extension_${extension_entry}" extension_target)
        include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ExtensionManifest.cmake")
        woki_extension_manifest_wasm("${extension_dir}/manifest.yaml" extension_wasm_relative)

        find_program(_woki_ninja NAMES ninja ninja-build REQUIRED)
        set(extension_generator_args -G "Ninja Multi-Config" "-DCMAKE_MAKE_PROGRAM=${_woki_ninja}")

        add_custom_target(${extension_target}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${extension_build_dir}"
            COMMAND ${CMAKE_COMMAND}
                ${extension_generator_args}
                -B "${extension_build_dir}"
                -S "${extension_dir}"
                -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
                -DWOKI_REPO_ROOT=${CMAKE_SOURCE_DIR}
            COMMAND ${CMAKE_COMMAND} --build "${extension_build_dir}" --config $<CONFIG>
            COMMAND ${CMAKE_COMMAND}
                -D SOURCE_DIR=${extension_built_package_dir}
                -D DESTINATION_DIR=${extension_package_dir}
                -D SOURCE_MANIFEST=${extension_dir}/manifest.yaml
                -D PROJECT_DIR=${extension_dir}
                -D SDK_DIR=${CMAKE_SOURCE_DIR}/modules/extension/sdk
                -D CONFIG=$<CONFIG>
                -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/StageExtensionPackage.cmake
            COMMENT "Building extension ${extension_entry}"
            VERBATIM
        )
        if(TARGET wokiext)
            add_custom_command(TARGET ${extension_target} POST_BUILD
                COMMAND $<TARGET_FILE:wokiext> verify "${extension_package_dir}"
                VERBATIM
            )
            add_dependencies(${extension_target} wokiext)
        endif()
        set_target_properties(${extension_target} PROPERTIES
            WOKI_EXTENSION_PACKAGE_DIR "${extension_package_dir}"
            WOKI_EXTENSION_WASM_OUTPUT "${extension_package_dir}/${extension_wasm_relative}"
        )
        add_dependencies(woki_extensions ${extension_target})

        if(BUILD_TESTING AND TARGET wokiext)
            add_test(NAME "extension_${extension_entry}_package"
                COMMAND ${CMAKE_COMMAND}
                    -D SOURCE_DIR=${extension_dir}
                    -D SDK_DIR=${CMAKE_SOURCE_DIR}/modules/extension/sdk
                    -D PACKAGE_DIR=${extension_package_dir}
                    -D CONFIG=$<CONFIG>
                    -D WOKIEXT=$<TARGET_FILE:wokiext>
                    -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/TestExtensionPackage.cmake
            )
        endif()
    endforeach()
endfunction()
