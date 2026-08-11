if(NOT DEFINED BUILD_DIR OR NOT DEFINED TEST_ROOT OR NOT DEFINED WOKIEXT_NAME OR NOT DEFINED STUDIO_RELATIVE)
    message(FATAL_ERROR "BUILD_DIR, TEST_ROOT, WOKIEXT_NAME, and STUDIO_RELATIVE are required")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
set(prefix "${TEST_ROOT}/relocated prefix")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${prefix}" --config "${CONFIG}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Relocated install failed\n${install_output}${install_error}")
endif()

set(wokiext "${prefix}/${BINDIR}/${WOKIEXT_NAME}")
set(studio "${prefix}/${STUDIO_RELATIVE}")
foreach(required IN ITEMS
    "${wokiext}"
    "${studio}"
    "${prefix}/${DATADIR}/woki/cmake/ExtensionProject.cmake"
    "${prefix}/${DATADIR}/woki/cmake/ExtensionWasm.cmake"
    "${prefix}/${DATADIR}/woki/cmake/ExtensionGuestLibraries.cmake"
    "${prefix}/${DATADIR}/woki/cmake/guest-include/cstddef"
    "${prefix}/${DATADIR}/woki/cmake/ExtensionManifest.cmake"
    "${prefix}/${DATADIR}/woki/cmake/ExtensionInputHash.cmake"
    "${prefix}/${DATADIR}/woki/cmake/CheckExtensionState.cmake"
    "${prefix}/${DATADIR}/woki/cmake/StageExtensionPackage.cmake"
    "${prefix}/${DATADIR}/woki/cmake/VerifyExtensionPackage.cmake"
    "${prefix}/${DATADIR}/woki/templates/extensions/c/CMakeLists.txt.in"
    "${prefix}/${DATADIR}/woki/templates/extensions/c/src/extension.c.in"
    "${prefix}/${DATADIR}/woki/templates/extensions/cpp/CMakeLists.txt.in"
    "${prefix}/${DATADIR}/woki/templates/extensions/cpp/src/extension.cpp.in"
    "${prefix}/${INCLUDEDIR}/woki/ext/sdk/ext.h"
    "${prefix}/${INCLUDEDIR}/woki/extension.hpp"
    "${prefix}/${INCLUDEDIR}/woki/ext/ext.hpp"
    "${prefix}/${INCLUDEDIR}/woki/extension.hpp"
    "${prefix}/${INCLUDEDIR}/woki/math/guest.hpp"
    "${prefix}/${INCLUDEDIR}/woki/ecs/guest.hpp"
    "${prefix}/${DATADIR}/woki/extensions/${CONFIG}/kitty/manifest.yaml"
    "${prefix}/${DATADIR}/woki/extensions/${CONFIG}/kitty/extension.wasm"
)
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Installed release tree is missing ${required}")
    endif()
endforeach()

foreach(internal_header IN ITEMS host/api.hpp host/cabi.hpp registry.hpp runtime.hpp path_safety.hpp wasm/guest_module.hpp wasm/web_engine.hpp wasm/wasmtime_engine.hpp)
    if(EXISTS "${prefix}/${INCLUDEDIR}/woki/ext/${internal_header}")
        message(FATAL_ERROR "Installed release tree exposes internal host header ${internal_header}")
    endif()
endforeach()

file(STRINGS "${studio}" studio_strings REGEX "woki/extensions/${CONFIG}")
if(NOT studio_strings)
    message(FATAL_ERROR "Installed Studio does not contain its executable-relative bundled extension path")
endif()

set(project_parent "${TEST_ROOT}/installed cli project")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env --unset=WOKI_CMAKE_DIR --unset=WOKI_SDK_DIR
        "${wokiext}" create "Relocated Extension" --lang cpp --out "${project_parent}"
    RESULT_VARIABLE create_result
    OUTPUT_VARIABLE create_output
    ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
    message(FATAL_ERROR "Installed wokiext create failed\n${create_output}${create_error}")
endif()
file(READ "${project_parent}/relocated-extension/src/extension.cpp" relocated_source)
if(NOT relocated_source MATCHES "#include <woki/extension.hpp>" OR relocated_source MATCHES "Context")
    message(FATAL_ERROR "Installed wokiext did not render the installed C++ component template")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env --unset=WOKI_CMAKE_DIR --unset=WOKI_SDK_DIR
        "${wokiext}" test "${project_parent}/relocated-extension" --config Release
    RESULT_VARIABLE test_result
    OUTPUT_VARIABLE test_output
    ERROR_VARIABLE test_error
)
if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "Installed relocated wokiext test failed\n${test_output}${test_error}")
endif()
