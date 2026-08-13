if(NOT DEFINED BUILD_DIR OR NOT DEFINED TEST_ROOT OR NOT DEFINED WOKIEXT_NAME OR NOT DEFINED STUDIO_RELATIVE OR NOT DEFINED DEVELOPMENT_SOURCES OR NOT DEFINED LIBDIR)
    message(FATAL_ERROR "BUILD_DIR, TEST_ROOT, WOKIEXT_NAME, STUDIO_RELATIVE, DEVELOPMENT_SOURCES, and LIBDIR are required")
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
    "${prefix}/${DATADIR}/woki/cmake/WokiGfxAssetPreload.cmake"
    "${prefix}/${DATADIR}/woki/cmake/StageExtensionPackage.cmake"
    "${prefix}/${DATADIR}/woki/cmake/VerifyExtensionPackage.cmake"
    "${prefix}/${DATADIR}/woki/templates/extensions/c/CMakeLists.txt.in"
    "${prefix}/${DATADIR}/woki/templates/extensions/c/src/extension.c.in"
    "${prefix}/${DATADIR}/woki/templates/extensions/cpp/CMakeLists.txt.in"
    "${prefix}/${DATADIR}/woki/templates/extensions/cpp/src/extension.cpp.in"
    "${prefix}/${INCLUDEDIR}/woki/ext/sdk/ext.h"
    "${prefix}/${INCLUDEDIR}/woki/extension.hpp"
    "${prefix}/${INCLUDEDIR}/woki/ext/ext.hpp"
    "${prefix}/${INCLUDEDIR}/woki/math.hpp"
    "${prefix}/${INCLUDEDIR}/woki/math/guest.hpp"
    "${prefix}/${INCLUDEDIR}/woki/math/vec/vec3.hpp"
    "${prefix}/${INCLUDEDIR}/woki/math/mat/mat4.hpp"
    "${prefix}/${INCLUDEDIR}/woki/math/quat/quat.hpp"
    "${prefix}/${INCLUDEDIR}/woki/task.hpp"
    "${prefix}/${INCLUDEDIR}/woki/task/executor.hpp"
    "${prefix}/${INCLUDEDIR}/woki/ui/render.hpp"
    "${prefix}/${INCLUDEDIR}/woki/ui/render/adapter.hpp"
    "${prefix}/${INCLUDEDIR}/woki/gfx/advanced/compiler.hpp"
    "${prefix}/${INCLUDEDIR}/woki/gfx/advanced/model_import.hpp"
    "${prefix}/${INCLUDEDIR}/woki/ecs/guest.hpp"
    "${prefix}/${DATADIR}/woki/extensions/${CONFIG}/kitty/manifest.yaml"
    "${prefix}/${DATADIR}/woki/extensions/${CONFIG}/kitty/extension.wasm"
    "${prefix}/${DATADIR}/woki/extensions/${CONFIG}/dino-lab/manifest.yaml"
    "${prefix}/${DATADIR}/woki/extensions/${CONFIG}/dino-lab/extension.wasm"
    "${prefix}/${DATADIR}/woki/extensions/${CONFIG}/dino-lab/assets/field-guide.txt"
)
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Installed release tree is missing ${required}")
    endif()
endforeach()

file(READ "${prefix}/${LIBDIR}/cmake/woki/wokiTargets.cmake" installed_targets)
if(installed_targets MATCHES "woki::(task|ui_gfx|gfx_tools)")
    message(FATAL_ERROR "Installed target export contains an obsolete folded module target")
endif()

if(DEVELOPMENT_SOURCES)
    foreach(required_source IN ITEMS
        "${prefix}/${DATADIR}/woki/assets/shaders/descriptors/cube.woki-shader"
        "${prefix}/${DATADIR}/woki/assets/render/cube.woki-pipeline"
        "${prefix}/${DATADIR}/woki/assets/render/quality/high.woki-quality"
        "${prefix}/${DATADIR}/woki/assets/materials/types/standard-unlit.woki-material-type"
        "${prefix}/${DATADIR}/woki/assets/materials/types/standard-pbr.woki-material-type"
        "${prefix}/${DATADIR}/woki/assets/materials/red-unlit.woki-material")
        if(NOT EXISTS "${required_source}")
            message(FATAL_ERROR "Development-source install is missing ${required_source}")
        endif()
    endforeach()
endif()

file(READ "${prefix}/${DATADIR}/woki/cmake/WokiGfxAssetPreload.cmake" preload_helper)
if(preload_helper MATCHES "_woki_gfx_prefix}/share")
    message(FATAL_ERROR "Installed graphics preload helper hard-codes share instead of the configured dataroot")
endif()

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
