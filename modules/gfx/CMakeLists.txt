cmake_minimum_required(VERSION 3.25)

project(gfx VERSION 1.0.0 DESCRIPTION "Cooked shader assets and runtime graphics resources" LANGUAGES CXX)

option(WOKI_GFX_FETCH_TINT "Fetch the pinned Dawn source tree to build Tint shader tooling" OFF)
option(WOKI_GFX_REQUIRE_TINT "Fail configuration unless the pinned Tint adapter is enabled" OFF)
option(WOKI_GFX_ENABLE_DEVELOPMENT_SOURCES "Expose authored shader/pipeline source loading in non-shipping builds" OFF)
set(WOKI_GFX_TINT_REVISION "9fd4a731474f66c1ce5465f9b13919ad7487315d" CACHE STRING "Pinned Dawn revision used for Tint")
set(WOKI_GFX_PRECOOKED_ASSET_DIR "" CACHE PATH "Cooked assets produced by a native tools build for cross/web staging")
set(WOKI_GFX_PREGENERATED_ABI_DIR "" CACHE PATH "Generated ABI artifact root produced by a native tools build for cross/web compilation")

set(HEADERS include/woki/gfx.hpp include/woki/gfx/advanced.hpp include/woki/gfx/runtime.hpp)
set(SOURCES
    src/animation.cpp src/buffer_pool.cpp src/camera.cpp src/canvas.cpp src/compute.cpp
    src/draw_packet.cpp src/epoch.cpp src/feature.cpp src/gpu_scene.cpp
    src/graph/validation.cpp src/graph/dependencies.cpp src/graph/culling.cpp src/graph/schedule.cpp
    src/graph/lifetimes.cpp src/graph/transitions.cpp src/graph/dump.cpp src/graph/compile.cpp
    src/graph_executor.cpp src/indirect_draw.cpp src/layout.cpp src/library.cpp src/material.cpp
    src/material_library.cpp src/material_prepare.cpp src/material_product.cpp src/mesh.cpp
    src/mesh_library.cpp src/mesh_product.cpp src/physical_bindings.cpp src/pipeline.cpp
    src/pipeline_cache.cpp src/pipeline_library.cpp src/pipeline_product.cpp src/product.cpp
    src/readback.cpp src/readback_service.cpp src/reflection.cpp src/reload.cpp src/render_graph.cpp
    src/render_scene.cpp src/render_world.cpp src/resource.cpp src/runtime_services.cpp
    src/runtime/bootstrap.cpp src/runtime/frame.cpp src/runtime/scenes.cpp src/runtime/views.cpp
    src/runtime/presentation.cpp src/runtime/recovery.cpp src/runtime/facade_compute.cpp src/runtime/facade_readback.cpp
    src/features/common.cpp src/features/mesh.cpp src/features/depth.cpp src/features/lighting.cpp
    src/features/shadows.cpp src/features/environment.cpp src/features/temporal.cpp src/features/post.cpp
    src/features/presentation.cpp src/features/registration.cpp src/standard_programs.cpp src/temporal.cpp src/texture.cpp
    src/texture_cache.cpp src/texture_library.cpp src/texture_product.cpp src/upload.cpp
    src/view.cpp src/visibility.cpp)
set(GFX_NATIVE_TOOL_SOURCES
    src/compiler.cpp src/descriptor.cpp src/pack.cpp src/source.cpp src/variant.cpp
    src/texture_builder.cpp src/model_import.cpp src/model_import_assimp.cpp
    src/model_import_fastgltf.cpp src/mesh_builder.cpp src/mesh_processing.cpp)
if(NOT EMSCRIPTEN)
    list(APPEND SOURCES ${GFX_NATIVE_TOOL_SOURCES})
endif()

set(GFX_PUBLIC_DEPS woki::asset woki::config woki::core woki::math woki::platform)
set(GFX_PRIVATE_DEPS)
if(NOT EMSCRIPTEN)
    list(APPEND GFX_PRIVATE_DEPS
        "$<BUILD_INTERFACE:ktx_read>"
        "$<BUILD_INTERFACE:woki_stb>"
        "$<BUILD_INTERFACE:fastgltf::fastgltf>"
        "$<BUILD_INTERFACE:assimp::assimp>"
        "$<BUILD_INTERFACE:meshoptimizer>"
        "$<BUILD_INTERFACE:woki_mikktspace>")
endif()

if(NOT EMSCRIPTEN)
    if(WOKI_GFX_FETCH_TINT AND NOT TARGET tint_api)
        include(FetchContent)
        set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
        set(DAWN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(DAWN_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
        set(DAWN_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
        set(DAWN_BUILD_NODE_BINDINGS OFF CACHE BOOL "" FORCE)
        set(DAWN_BUILD_MONOLITHIC_LIBRARY OFF CACHE STRING "" FORCE)
        set(DAWN_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_SPIRV_VALIDATION OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_D3D11 OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_D3D12 OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_DESKTOP_GL OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_METAL OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_NULL OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_OPENGLES OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_VULKAN OFF CACHE BOOL "" FORCE)
        set(DAWN_USE_GLFW OFF CACHE BOOL "" FORCE)
        set(DAWN_USE_WAYLAND OFF CACHE BOOL "" FORCE)
        set(DAWN_USE_X11 OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_GLSL_WRITER OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_GLSL_VALIDATOR OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_HLSL_WRITER OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_IR_BINARY OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_MSL_WRITER OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_SPV_READER OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_SPV_WRITER OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_TINTD OFF CACHE BOOL "" FORCE)
        set(TINT_BUILD_WGSL_READER ON CACHE BOOL "" FORCE)
        set(TINT_BUILD_WGSL_WRITER OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(woki_tint
            URL "https://github.com/google/dawn/archive/${WOKI_GFX_TINT_REVISION}.tar.gz"
            DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        )
        FetchContent_GetProperties(woki_tint)
        if(NOT woki_tint_POPULATED)
            cmake_policy(PUSH)
            if(POLICY CMP0169)
                cmake_policy(SET CMP0169 OLD)
            endif()
            FetchContent_Populate(woki_tint)
            cmake_policy(POP)
        endif()
        if(NOT EXISTS "${woki_tint_SOURCE_DIR}/third_party/abseil-cpp/CMakeLists.txt")
            find_package(Python3 REQUIRED COMPONENTS Interpreter)
            execute_process(
                COMMAND ${Python3_EXECUTABLE} "${woki_tint_SOURCE_DIR}/tools/fetch_dawn_dependencies.py" --shallow
                WORKING_DIRECTORY "${woki_tint_SOURCE_DIR}"
                RESULT_VARIABLE _woki_tint_dependencies_result
                COMMAND_ERROR_IS_FATAL ANY
            )
        endif()
        add_compile_options($<$<COMPILE_LANG_AND_ID:CXX,Clang>:-Wno-c2y-extensions>)
        add_subdirectory("${woki_tint_SOURCE_DIR}" "${woki_tint_BINARY_DIR}" EXCLUDE_FROM_ALL)
        function(woki_gfx_set_tint_cxx_standard directory)
            get_property(_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
            foreach(_target IN LISTS _targets)
                set_property(TARGET "${_target}" PROPERTY CXX_STANDARD 23)
            endforeach()
            get_property(_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
            foreach(_subdirectory IN LISTS _subdirectories)
                woki_gfx_set_tint_cxx_standard("${_subdirectory}")
            endforeach()
        endfunction()
        woki_gfx_set_tint_cxx_standard("${woki_tint_SOURCE_DIR}")
    endif()
    if(TARGET tint_api)
        set(WOKI_GFX_HAS_TINT TRUE)
    endif()
endif()
if(WOKI_GFX_REQUIRE_TINT AND NOT WOKI_GFX_HAS_TINT)
    message(FATAL_ERROR "WOKI_GFX_REQUIRE_TINT requires tint_api from pinned Dawn ${WOKI_GFX_TINT_REVISION}")
endif()
if(NOT EMSCRIPTEN AND WOKI_GFX_HAS_TINT)
    list(APPEND SOURCES src/tint_compiler.cpp)
    set_source_files_properties(src/tint_compiler.cpp PROPERTIES
        COMPILE_OPTIONS "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-std=c++23>")
    # Build-only public ordering keeps the selected Tint implementation before
    # Dawn while avoiding an installed-package dependency on Tint.
    list(APPEND GFX_PUBLIC_DEPS "$<BUILD_INTERFACE:tint_api>")
endif()
# Keep Dawn after the pinned Tint libraries on native link lines. Dawn's static
# archive contains an internal Tint revision whose symbols are not ABI-compatible
# with the public Tint headers used by the offline shader compiler.
list(APPEND GFX_PUBLIC_DEPS woki::rhi)

cmake_path(NORMAL_PATH CMAKE_CURRENT_SOURCE_DIR OUTPUT_VARIABLE WOKI_GFX_MODULE_DIR)
cmake_path(GET WOKI_GFX_MODULE_DIR PARENT_PATH WOKI_GFX_MODULES_DIR)
cmake_path(GET WOKI_GFX_MODULES_DIR PARENT_PATH WOKI_GFX_REPOSITORY_DIR)
set(WOKI_GFX_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
# References are resolved in command order; foundational records precede users.
set(WOKI_GPU_STRUCT_SCHEMAS
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/gpu-light.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/frame.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/scene.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/cluster-grid.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/cluster-params.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/view.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/shadow.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/environment.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/object.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/gpu-instance.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/pbr-material.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/unlit-material.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/post.woki-gpu-struct"
    "${WOKI_GFX_REPOSITORY_DIR}/assets/gpu-structs/text.woki-gpu-struct")
if(NOT EMSCRIPTEN)
    add_executable(woki-gpu-struct tool/gpu_struct_main.cpp)
    target_compile_features(woki-gpu-struct PRIVATE cxx_std_23)
    target_link_libraries(woki-gpu-struct PRIVATE woki::config)
    apply_compiler_options(woki-gpu-struct)
    add_custom_command(
        OUTPUT "${WOKI_GFX_GENERATED_DIR}/include/woki/gfx/generated/render_abi.hpp" "${WOKI_GFX_GENERATED_DIR}/shaders/generated/render_abi.wgsl"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_GFX_GENERATED_DIR}/include/woki/gfx/generated" "${WOKI_GFX_GENERATED_DIR}/shaders/generated"
        COMMAND $<TARGET_FILE:woki-gpu-struct> generate "${WOKI_GFX_GENERATED_DIR}/include/woki/gfx/generated/render_abi.hpp" "${WOKI_GFX_GENERATED_DIR}/shaders/generated/render_abi.wgsl" ${WOKI_GPU_STRUCT_SCHEMAS}
        DEPENDS woki-gpu-struct ${WOKI_GPU_STRUCT_SCHEMAS}
        VERBATIM COMMENT "Generating deterministic CPU/WGSL render ABI")
    add_custom_target(woki_gfx_generated_abi DEPENDS "${WOKI_GFX_GENERATED_DIR}/include/woki/gfx/generated/render_abi.hpp" "${WOKI_GFX_GENERATED_DIR}/shaders/generated/render_abi.wgsl")
elseif(WOKI_GFX_PREGENERATED_ABI_DIR)
    add_custom_command(
        OUTPUT "${WOKI_GFX_GENERATED_DIR}/include/woki/gfx/generated/render_abi.hpp" "${WOKI_GFX_GENERATED_DIR}/shaders/generated/render_abi.wgsl"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_GFX_GENERATED_DIR}/include/woki/gfx/generated" "${WOKI_GFX_GENERATED_DIR}/shaders/generated"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WOKI_GFX_PREGENERATED_ABI_DIR}/include/woki/gfx/generated/render_abi.hpp" "${WOKI_GFX_GENERATED_DIR}/include/woki/gfx/generated/render_abi.hpp"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WOKI_GFX_PREGENERATED_ABI_DIR}/shaders/generated/render_abi.wgsl" "${WOKI_GFX_GENERATED_DIR}/shaders/generated/render_abi.wgsl"
        DEPENDS "${WOKI_GFX_PREGENERATED_ABI_DIR}/include/woki/gfx/generated/render_abi.hpp" "${WOKI_GFX_PREGENERATED_ABI_DIR}/shaders/generated/render_abi.wgsl"
        VERBATIM COMMENT "Staging native-generated CPU/WGSL render ABI")
    add_custom_target(woki_gfx_generated_abi DEPENDS "${WOKI_GFX_GENERATED_DIR}/include/woki/gfx/generated/render_abi.hpp" "${WOKI_GFX_GENERATED_DIR}/shaders/generated/render_abi.wgsl")
else()
    message(FATAL_ERROR "EMSCRIPTEN gfx builds require WOKI_GFX_PREGENERATED_ABI_DIR from a native tools build")
endif()

add_module(gfx NO_INSTALL_HEADERS
    SOURCES ${SOURCES}
    HEADERS ${HEADERS}
    PUBLIC_DEPENDENCIES ${GFX_PUBLIC_DEPS}
    PRIVATE_DEPENDENCIES ${GFX_PRIVATE_DEPS})
target_sources(woki_gfx PRIVATE
    src/binary_codec.hpp src/canonical_hash.hpp src/gfx_util.hpp
    src/internal/canvas_feature.hpp src/internal/graph.hpp src/internal/runtime_state.hpp
    src/internal/readback_manager.hpp src/internal/runtime_facade.hpp
    src/features/internal.hpp src/graph/internal.hpp)
target_include_directories(woki_gfx PUBLIC $<BUILD_INTERFACE:${WOKI_GFX_GENERATED_DIR}/include>)
if(TARGET woki_gfx_generated_abi)
    add_dependencies(woki_gfx woki_gfx_generated_abi)
endif()
if(WOKI_GFX_ENABLE_DEVELOPMENT_SOURCES)
    target_compile_definitions(woki_gfx PRIVATE WOKI_GFX_ENABLE_DEVELOPMENT_SOURCES=1)
endif()
if(NOT EMSCRIPTEN AND WOKI_GFX_HAS_TINT)
    target_compile_definitions(woki_gfx PRIVATE WOKI_GFX_HAS_TINT=1)
endif()
if(NOT EMSCRIPTEN)
    set_source_files_properties(src/model_import_assimp.cpp src/model_import_fastgltf.cpp
        PROPERTIES COMPILE_OPTIONS "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wno-missing-field-initializers>")
    add_subdirectory(tool)
    if(TARGET tint_api)
        add_custom_target(woki_gfx_tint_check DEPENDS woki_gfx)
        if(TARGET gfx_tint_tests)
            add_dependencies(woki_gfx_tint_check gfx_tint_tests)
        endif()
    endif()
endif()

set(WOKI_GFX_ASSET_SOURCE_DIR "${WOKI_GFX_REPOSITORY_DIR}/assets" CACHE INTERNAL "Woki authored asset source directory")
set(WOKI_GFX_ASSET_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets" CACHE INTERNAL "Woki staged asset directory")
add_custom_target(woki_gfx_assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_GFX_ASSET_BUILD_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${WOKI_GFX_ASSET_SOURCE_DIR}" "${WOKI_GFX_ASSET_BUILD_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_GFX_ASSET_BUILD_DIR}/cooked"
    COMMENT "Staging Woki graphics assets"
)
if(TARGET woki_gfx_generated_abi)
    add_dependencies(woki_gfx_assets woki_gfx_generated_abi)
    add_custom_command(TARGET woki_gfx_assets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_GFX_ASSET_BUILD_DIR}/shaders/generated"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WOKI_GFX_GENERATED_DIR}/shaders/generated/render_abi.wgsl" "${WOKI_GFX_ASSET_BUILD_DIR}/shaders/generated/render_abi.wgsl")
endif()
if(EMSCRIPTEN AND WOKI_GFX_PRECOOKED_ASSET_DIR)
    add_custom_command(TARGET woki_gfx_assets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${WOKI_GFX_PRECOOKED_ASSET_DIR}" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked"
        COMMENT "Staging native-cooked Woki products for web"
    )
endif()
add_library(woki_gfx_asset_preload INTERFACE)
add_library(woki::gfx_asset_preload ALIAS woki_gfx_asset_preload)
add_dependencies(woki_gfx_asset_preload woki_gfx_assets)
if(EMSCRIPTEN)
    target_link_options(woki_gfx_asset_preload INTERFACE
        "SHELL:--preload-file ${WOKI_GFX_ASSET_BUILD_DIR}/cooked@/assets/cooked"
    )
endif()
if(WOKI_GFX_ENABLE_DEVELOPMENT_SOURCES)
    install(DIRECTORY "${WOKI_GFX_ASSET_SOURCE_DIR}/"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/woki/assets"
        PATTERN "cooked" EXCLUDE
    )
endif()
install(DIRECTORY "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/woki/assets/cooked"
    OPTIONAL
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WokiGfxAssetPreload.cmake"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/woki/cmake"
)
install(DIRECTORY "${WOKI_GFX_GENERATED_DIR}/include/woki/gfx/generated/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/woki/gfx/generated")
install(FILES
    include/woki/gfx/camera.hpp
    include/woki/gfx/canvas.hpp
    include/woki/gfx/compute.hpp
    include/woki/gfx/diagnostics.hpp
    include/woki/gfx/handles.hpp
    include/woki/gfx/presentation.hpp
    include/woki/gfx/readback.hpp
    include/woki/gfx/runtime.hpp
    include/woki/gfx/scene.hpp
    include/woki/gfx/view.hpp
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/woki/gfx")
set(WOKI_GFX_ADVANCED_HEADERS
    include/woki/gfx/advanced/all.hpp
    include/woki/gfx/advanced/animation.hpp
    include/woki/gfx/advanced/buffer_pool.hpp
    include/woki/gfx/advanced/compiler.hpp
    include/woki/gfx/advanced/deferred_release.hpp
    include/woki/gfx/advanced/descriptor.hpp
    include/woki/gfx/advanced/draw_packet.hpp
    include/woki/gfx/advanced/environment.hpp
    include/woki/gfx/advanced/epoch.hpp
    include/woki/gfx/advanced/exposure.hpp
    include/woki/gfx/advanced/feature.hpp
    include/woki/gfx/advanced/graph_compile.hpp
    include/woki/gfx/advanced/graph_executor.hpp
    include/woki/gfx/advanced/gpu_scene.hpp
    include/woki/gfx/advanced/indirect_draw.hpp
    include/woki/gfx/advanced/layout.hpp
    include/woki/gfx/advanced/library.hpp
    include/woki/gfx/advanced/material.hpp
    include/woki/gfx/advanced/material_library.hpp
    include/woki/gfx/advanced/material_prepare.hpp
    include/woki/gfx/advanced/material_product.hpp
    include/woki/gfx/advanced/mesh.hpp
    include/woki/gfx/advanced/mesh_library.hpp
    include/woki/gfx/advanced/mesh_product.hpp
    include/woki/gfx/advanced/model_import.hpp
    include/woki/gfx/advanced/pack.hpp
    include/woki/gfx/advanced/physical_bindings.hpp
    include/woki/gfx/advanced/pipeline.hpp
    include/woki/gfx/advanced/pipeline_cache.hpp
    include/woki/gfx/advanced/pipeline_library.hpp
    include/woki/gfx/advanced/pipeline_product.hpp
    include/woki/gfx/advanced/product.hpp
    include/woki/gfx/advanced/reflection.hpp
    include/woki/gfx/advanced/reload.hpp
    include/woki/gfx/advanced/render_abi.hpp
    include/woki/gfx/advanced/render_graph.hpp
    include/woki/gfx/advanced/render_scene.hpp
    include/woki/gfx/advanced/render_world.hpp
    include/woki/gfx/advanced/resource.hpp
    include/woki/gfx/advanced/runtime.hpp
    include/woki/gfx/advanced/runtime_services.hpp
    include/woki/gfx/advanced/source.hpp
    include/woki/gfx/advanced/standard_features.hpp
    include/woki/gfx/advanced/temporal.hpp
    include/woki/gfx/advanced/texture.hpp
    include/woki/gfx/advanced/texture_cache.hpp
    include/woki/gfx/advanced/texture_library.hpp
    include/woki/gfx/advanced/texture_product.hpp
    include/woki/gfx/advanced/types.hpp
    include/woki/gfx/advanced/upload.hpp
    include/woki/gfx/advanced/variant.hpp
    include/woki/gfx/advanced/visibility.hpp)
install(FILES ${WOKI_GFX_ADVANCED_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/woki/gfx/advanced")
install(FILES include/woki/gfx/advanced.hpp DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/woki/gfx")
install(FILES include/woki/gfx.hpp DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/woki")
install(DIRECTORY docs/ DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/woki/docs/gfx")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/schema/woki-shader.schema.json" "${CMAKE_CURRENT_SOURCE_DIR}/schema/woki-gpu-struct.schema.json" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/woki/schemas")

if(NOT EMSCRIPTEN)
    if(WOKI_GFX_HAS_TINT)
    file(GLOB_RECURSE WOKI_STANDARD_SHADER_SOURCES CONFIGURE_DEPENDS
        "${WOKI_GFX_ASSET_SOURCE_DIR}/shaders/*.wgsl")
    set(WOKI_STANDARD_SHADER_NAMES
        bitmap-text canvas bloom-blur bloom-composite bloom-downsample bloom-threshold
        cluster-lights copy cube depth error exposure fxaa msdf-text pbr sdf-text sky taa tone-map
        unlit validate-compute gpu-visibility hiz-seed hiz-downsample
    )
    set(WOKI_STANDARD_SHADER_PRODUCTS)
    foreach(_shader IN LISTS WOKI_STANDARD_SHADER_NAMES)
        set(_product "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/shaders/${_shader}.woki-product")
        set(_interface "${WOKI_GFX_GENERATED_DIR}/include/woki/gfx/generated/${_shader}_interface.hpp")
        add_custom_command(
            OUTPUT "${_product}" "${_interface}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/shaders"
            COMMAND $<TARGET_FILE:woki-shader> generate-interface "shaders/descriptors/${_shader}.woki-shader" "${_interface}" "${WOKI_GFX_ASSET_BUILD_DIR}"
            COMMAND $<TARGET_FILE:woki-shader> compile "shaders/descriptors/${_shader}.woki-shader" "${_product}" "${WOKI_GFX_ASSET_BUILD_DIR}"
            DEPENDS woki-shader woki_gfx_assets woki_gfx_generated_abi
                "${WOKI_GFX_ASSET_SOURCE_DIR}/shaders/descriptors/${_shader}.woki-shader"
                ${WOKI_STANDARD_SHADER_SOURCES} ${WOKI_GPU_STRUCT_SCHEMAS}
            VERBATIM
            COMMENT "Cooking standard shader ${_shader}"
        )
        list(APPEND WOKI_STANDARD_SHADER_PRODUCTS "${_product}")
        list(APPEND WOKI_STANDARD_SHADER_INTERFACES "${_interface}")
    endforeach()
    add_custom_target(woki_gfx_cooked_shaders DEPENDS ${WOKI_STANDARD_SHADER_PRODUCTS} ${WOKI_STANDARD_SHADER_INTERFACES})

    set(WOKI_STANDARD_PIPELINE_NAMES compatibility-forward cube desktop-forward editor-forward mobile-forward)
    set(WOKI_STANDARD_PIPELINE_PRODUCTS)
    foreach(_pipeline IN LISTS WOKI_STANDARD_PIPELINE_NAMES)
        set(_product "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/pipelines/${_pipeline}.woki-product")
        add_custom_command(
            OUTPUT "${_product}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/pipelines"
            COMMAND $<TARGET_FILE:woki-pipeline> compile "render/${_pipeline}.woki-pipeline" "${_product}" "${WOKI_GFX_ASSET_SOURCE_DIR}"
            DEPENDS woki-pipeline woki_gfx_assets "${WOKI_GFX_ASSET_SOURCE_DIR}/render/${_pipeline}.woki-pipeline"
            VERBATIM
            COMMENT "Cooking standard render pipeline ${_pipeline}"
        )
        list(APPEND WOKI_STANDARD_PIPELINE_PRODUCTS "${_product}")
    endforeach()
    add_custom_target(woki_gfx_cooked_pipelines DEPENDS ${WOKI_STANDARD_PIPELINE_PRODUCTS})
    set(WOKI_STANDARD_MATERIAL_PRODUCTS
        "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/materials/df1258a0-824a-54ec-b058-a77e0b52c017.woki-product"
        "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/materials/398db147-4060-5923-bf8c-891905d4e26d.woki-product"
        "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/materials/19cf6eb8-b656-5af1-808f-e42ed23100e8.woki-product"
        "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/materials/51c90ad9-382b-545c-a531-8a1a2884b506.woki-product"
    )
    add_custom_command(
        OUTPUT ${WOKI_STANDARD_MATERIAL_PRODUCTS}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/materials"
        COMMAND $<TARGET_FILE:woki-material> cook-type "${WOKI_GFX_ASSET_SOURCE_DIR}/materials/types/standard-pbr.woki-material-type" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/shaders/pbr.woki-product" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/materials/df1258a0-824a-54ec-b058-a77e0b52c017.woki-product"
        COMMAND $<TARGET_FILE:woki-material> cook-type "${WOKI_GFX_ASSET_SOURCE_DIR}/materials/types/standard-unlit.woki-material-type" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/shaders/unlit.woki-product" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/materials/398db147-4060-5923-bf8c-891905d4e26d.woki-product"
        COMMAND $<TARGET_FILE:woki-material> cook-instance "${WOKI_GFX_ASSET_SOURCE_DIR}/materials/default-pbr.woki-material" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/materials/df1258a0-824a-54ec-b058-a77e0b52c017.woki-product" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/materials/19cf6eb8-b656-5af1-808f-e42ed23100e8.woki-product"
        COMMAND $<TARGET_FILE:woki-material> cook-instance "${WOKI_GFX_ASSET_SOURCE_DIR}/materials/red-unlit.woki-material" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/materials/398db147-4060-5923-bf8c-891905d4e26d.woki-product" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/materials/51c90ad9-382b-545c-a531-8a1a2884b506.woki-product"
        DEPENDS woki-material woki_gfx_cooked_shaders
            "${WOKI_GFX_ASSET_SOURCE_DIR}/materials/types/standard-pbr.woki-material-type"
            "${WOKI_GFX_ASSET_SOURCE_DIR}/materials/types/standard-unlit.woki-material-type"
            "${WOKI_GFX_ASSET_SOURCE_DIR}/materials/default-pbr.woki-material"
            "${WOKI_GFX_ASSET_SOURCE_DIR}/materials/red-unlit.woki-material"
        VERBATIM
        COMMENT "Cooking standard material definitions and instances"
    )
    add_custom_target(woki_gfx_cooked_materials DEPENDS ${WOKI_STANDARD_MATERIAL_PRODUCTS})
    set(WOKI_DEFAULT_TEXTURE_NAMES white black flat-normal magenta white-cube black-cube depth)
    set(WOKI_DEFAULT_TEXTURE_IDS
        b73e75ab-a346-51ce-8485-49e114d7d2e1
        aee2f8f1-7765-59ca-b658-3fe43d844b9f
        f72fb0bb-0bd7-5d44-a536-3568824ed392
        3766a4b6-ef5e-54aa-8753-d664ca038eed
        fd99a93d-40d1-55f9-8c93-a6d4077a87b9
        451a732a-2cc4-5949-bd8a-86e3d4266141
        a3c2678b-6cc9-50be-950d-2e5be6473194)
    set(WOKI_DEFAULT_TEXTURE_PRODUCTS)
    list(LENGTH WOKI_DEFAULT_TEXTURE_NAMES _texture_count)
    math(EXPR _texture_last "${_texture_count} - 1")
    foreach(_index RANGE ${_texture_last})
        list(GET WOKI_DEFAULT_TEXTURE_NAMES ${_index} _texture)
        list(GET WOKI_DEFAULT_TEXTURE_IDS ${_index} _texture_id)
        set(_product "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/textures/${_texture_id}.woki-product")
        add_custom_command(OUTPUT "${_product}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/textures"
            COMMAND $<TARGET_FILE:woki-texture> cook "${WOKI_GFX_ASSET_SOURCE_DIR}/textures/default/${_texture}.woki-texture" "${WOKI_GFX_ASSET_SOURCE_DIR}" "${_product}"
            DEPENDS woki-texture woki_gfx_assets
                "${WOKI_GFX_ASSET_SOURCE_DIR}/textures/default/${_texture}.woki-texture"
                "${WOKI_GFX_ASSET_SOURCE_DIR}/textures/default/white.ppm"
                "${WOKI_GFX_ASSET_SOURCE_DIR}/textures/default/black.ppm"
                "${WOKI_GFX_ASSET_SOURCE_DIR}/textures/default/flat-normal.ppm"
                "${WOKI_GFX_ASSET_SOURCE_DIR}/textures/default/magenta.ppm"
            VERBATIM COMMENT "Cooking default texture ${_texture}")
        list(APPEND WOKI_DEFAULT_TEXTURE_PRODUCTS "${_product}")
    endforeach()
    add_custom_target(woki_gfx_cooked_textures DEPENDS ${WOKI_DEFAULT_TEXTURE_PRODUCTS})
    set(WOKI_CLIMBING_COOKED_MANIFEST "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/mesh/manifest.wkam")
    set(WOKI_CLIMBING_PRODUCT_LIST "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/mesh/products.list")
    add_custom_command(
        OUTPUT "${WOKI_CLIMBING_COOKED_MANIFEST}" "${WOKI_CLIMBING_PRODUCT_LIST}"
        BYPRODUCTS "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/mesh/77ee2538-19d3-57ef-956f-890eac1ac253.woki-product"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WOKI_GFX_ASSET_BUILD_DIR}/cooked"
        COMMAND $<TARGET_FILE:woki-mesh> cook "${WOKI_GFX_ASSET_SOURCE_DIR}/anim/climbing.woki-mesh-import" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/shaders/pbr.woki-product"
        DEPENDS woki-mesh woki_gfx_assets woki_gfx_cooked_shaders
                "${WOKI_GFX_ASSET_SOURCE_DIR}/anim/climbing.woki-mesh-import"
                "${WOKI_GFX_ASSET_SOURCE_DIR}/anim/climbing.fbx"
        VERBATIM
        COMMENT "Cooking climbing mesh product"
    )
    add_custom_target(woki_gfx_cooked_meshes DEPENDS "${WOKI_CLIMBING_COOKED_MANIFEST}" "${WOKI_CLIMBING_PRODUCT_LIST}")
    add_custom_target(woki_gfx_cooked_assets
        DEPENDS woki_gfx_cooked_shaders woki_gfx_cooked_pipelines woki_gfx_cooked_materials woki_gfx_cooked_textures woki_gfx_cooked_meshes
        COMMAND $<TARGET_FILE:woki-asset> package
            "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/studio.wkpkg"
            "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/manifest.wkam"
            native "engine://cooked/studio.wkpkg" all --
            ${WOKI_STANDARD_SHADER_PRODUCTS}
            ${WOKI_STANDARD_PIPELINE_PRODUCTS}
            ${WOKI_STANDARD_MATERIAL_PRODUCTS}
            ${WOKI_DEFAULT_TEXTURE_PRODUCTS}
            "@${WOKI_CLIMBING_PRODUCT_LIST}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WOKI_GFX_ASSET_SOURCE_DIR}/cook/studio.json" "${WOKI_GFX_ASSET_BUILD_DIR}/cooked/programs.json"
        COMMENT "Packaging the Studio cooked product manifest"
    )
    add_dependencies(woki_gfx_cooked_assets woki-asset)
    add_dependencies(woki_gfx_asset_preload woki_gfx_cooked_assets)
    endif()
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
if(TARGET woki_gfx_tint_check AND TARGET gfx_tint_tests)
    add_dependencies(woki_gfx_tint_check gfx_tint_tests)
endif()
