add_executable(woki-shader main.cpp)
target_compile_features(woki-shader PRIVATE cxx_std_23)
target_link_libraries(woki-shader PRIVATE woki::gfx)
apply_compiler_options(woki-shader)
