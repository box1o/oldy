if(TARGET woki::gfx_asset_preload)
    return()
endif()

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH _woki_gfx_woki_dir)
if(NOT DEFINED WOKI_GFX_INSTALL_DATAROOT)
    cmake_path(GET _woki_gfx_woki_dir PARENT_PATH WOKI_GFX_INSTALL_DATAROOT)
endif()

add_library(woki_gfx_asset_preload INTERFACE IMPORTED)
add_library(woki::gfx_asset_preload ALIAS woki_gfx_asset_preload)
if(EMSCRIPTEN)
    target_link_options(woki_gfx_asset_preload INTERFACE
        "SHELL:--preload-file ${WOKI_GFX_INSTALL_DATAROOT}/woki/assets/cooked@/assets/cooked"
    )
endif()

unset(_woki_gfx_woki_dir)
