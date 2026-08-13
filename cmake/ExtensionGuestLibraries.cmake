# Curated Woki libraries that are safe to compile as freestanding Wasm guests.

function(woki_add_guest_libraries extension_target language libraries output)
    if(NOT libraries)
        set(${output} "" PARENT_SCOPE)
        return()
    endif()
    if(NOT language STREQUAL "CXX")
        message(FATAL_ERROR "Woki guest libraries require add_wokiext(... LANGUAGE CXX ...)")
    endif()

    if(WOKI_GUEST_INCLUDE_DIR)
        get_filename_component(_include_dir "${WOKI_GUEST_INCLUDE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    elseif(EXISTS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/math/include/woki/math/guest.hpp")
        get_filename_component(_include_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/math/include" ABSOLUTE)
    elseif(EXISTS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../include/woki/math/guest.hpp")
        get_filename_component(_include_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../include" ABSOLUTE)
    else()
        message(FATAL_ERROR "Cannot locate Woki guest library headers; set WOKI_GUEST_INCLUDE_DIR")
    endif()

    set(_include_dirs "${_include_dir}")
    if(EXISTS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/ecs/include/woki/ecs/guest.hpp")
        get_filename_component(_ecs_include_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../modules/ecs/include" ABSOLUTE)
        list(APPEND _include_dirs "${_ecs_include_dir}")
    endif()
    if("math" IN_LIST libraries)
        include(CheckIncludeFileCXX)
        check_include_file_cxx(array WOKI_WASM_HAS_CXX_STDLIB)
        if(NOT WOKI_WASM_HAS_CXX_STDLIB)
            list(PREPEND _include_dirs "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/guest-include")
        endif()
    endif()

    set(_targets)
    foreach(_library IN LISTS libraries)
        if(_library STREQUAL "math")
            set(_header "woki/math/guest.hpp")
        elseif(_library STREQUAL "ecs")
            set(_header "woki/ecs/guest.hpp")
        else()
            message(FATAL_ERROR "Unsupported Woki guest library '${_library}'; supported libraries are math and ecs")
        endif()

        set(_target "${extension_target}_woki_${_library}")
        string(MAKE_C_IDENTIFIER "${_target}" _anchor)
        set(_source "${CMAKE_CURRENT_BINARY_DIR}/${_target}.cpp")
        file(WRITE "${_source}" "#include <${_header}>\nextern \"C\" void ${_anchor}_anchor() {}\n")
        add_library(${_target} STATIC "${_source}")
        target_include_directories(${_target} PUBLIC ${_include_dirs})
        target_compile_features(${_target} PUBLIC cxx_std_23)
        target_compile_options(${_target} PRIVATE
            -nostdlib -fno-builtin -fno-exceptions -fno-rtti
            $<$<CONFIG:Debug>:-O0> $<$<CONFIG:Debug>:-g>
            $<$<CONFIG:Release>:-O2> $<$<CONFIG:Release>:-DNDEBUG>
            $<$<CONFIG:RelWithDebInfo>:-O2> $<$<CONFIG:RelWithDebInfo>:-g> $<$<CONFIG:RelWithDebInfo>:-DNDEBUG>
            $<$<CONFIG:MinSizeRel>:-Oz> $<$<CONFIG:MinSizeRel>:-DNDEBUG>
        )
        set_target_properties(${_target} PROPERTIES LINKER_LANGUAGE CXX)
        list(APPEND _targets ${_target})
    endforeach()

    set(${output} "${_targets}" PARENT_SCOPE)
endfunction()
