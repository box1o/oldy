add_executable(woki-shader main.cpp)
target_compile_features(woki-shader PRIVATE cxx_std_23)
target_link_libraries(woki-shader PRIVATE woki::gfx)
apply_compiler_options(woki-shader)

add_executable(woki-pipeline pipeline_main.cpp)
target_compile_features(woki-pipeline PRIVATE cxx_std_23)
target_link_libraries(woki-pipeline PRIVATE woki::gfx)
apply_compiler_options(woki-pipeline)

add_executable(woki-graph graph_main.cpp)
target_compile_features(woki-graph PRIVATE cxx_std_23)
target_link_libraries(woki-graph PRIVATE woki::gfx)
apply_compiler_options(woki-graph)

add_executable(woki-material material_main.cpp)
target_compile_features(woki-material PRIVATE cxx_std_23)
target_link_libraries(woki-material PRIVATE woki::gfx)
apply_compiler_options(woki-material)

add_executable(woki-mesh mesh_main.cpp)
target_compile_features(woki-mesh PRIVATE cxx_std_23)
target_link_libraries(woki-mesh PRIVATE woki::gfx)
apply_compiler_options(woki-mesh)

add_executable(woki-texture texture_main.cpp)
target_compile_features(woki-texture PRIVATE cxx_std_23)
target_link_libraries(woki-texture PRIVATE woki::gfx)
apply_compiler_options(woki-texture)
