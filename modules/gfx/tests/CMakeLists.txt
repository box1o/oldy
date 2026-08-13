cmake_minimum_required(VERSION 3.25)

set(SOURCES
    camera_tests.cpp graph_tests.cpp layout_tests.cpp
    pipeline_tests.cpp product_tests.cpp
    reload_tests.cpp resource_runtime_tests.cpp render_world_tests.cpp
    standard_feature_tests.cpp)
if(NOT EMSCRIPTEN)
    list(APPEND SOURCES descriptor_tests.cpp pack_tests.cpp reflection_tests.cpp source_tests.cpp variant_tests.cpp)
endif()
add_module_test(gfx_tests SOURCES ${SOURCES} LIBRARIES woki::gfx woki::math Catch2::Catch2WithMain)
add_dependencies(gfx_tests woki_gfx_assets)
target_compile_definitions(gfx_tests PRIVATE WOKI_GFX_TEST_ASSET_ROOT="${WOKI_GFX_ASSET_BUILD_DIR}")
if(TARGET woki_gfx_cooked_assets)
    add_dependencies(gfx_tests woki_gfx_cooked_assets)
endif()
if(NOT EMSCRIPTEN)
    add_module_test(gfx_climbing_integration_tests SOURCES climbing_integration_tests.cpp LIBRARIES woki::gfx Catch2::Catch2WithMain)
    add_dependencies(gfx_climbing_integration_tests woki_gfx_assets)
    target_compile_definitions(gfx_climbing_integration_tests PRIVATE WOKI_GFX_TEST_ASSET_ROOT="${WOKI_GFX_ASSET_BUILD_DIR}")
    set_tests_properties(gfx_climbing_integration_tests PROPERTIES TIMEOUT 120 LABELS "gfx;integration;native")
endif()
if(WOKI_GFX_HAS_TINT)
    target_compile_definitions(gfx_tests PRIVATE WOKI_GFX_TEST_HAS_TINT=1)
    add_module_test(gfx_tint_tests SOURCES reflection_tests.cpp pack_tests.cpp LIBRARIES woki::gfx Catch2::Catch2WithMain)
    add_dependencies(gfx_tint_tests woki_gfx_assets)
    target_compile_definitions(gfx_tint_tests PRIVATE WOKI_GFX_TEST_ASSET_ROOT="${WOKI_GFX_ASSET_BUILD_DIR}" WOKI_GFX_TEST_HAS_TINT=1)
endif()
