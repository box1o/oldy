function(woki_add_developer_link target)
    if(EMSCRIPTEN)
        return()
    endif()

    if(target STREQUAL "studio")
        set(link_name "woki-studio")
    else()
        set(link_name "${target}")
    endif()
    set(link_path "${CMAKE_SOURCE_DIR}/${link_name}$<TARGET_FILE_SUFFIX:${target}>")
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${target}>"
                "${link_path}"
            VERBATIM
        )
        add_custom_target(${target}_developer_link ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${target}>"
                "${link_path}"
            DEPENDS ${target}
            VERBATIM
        )
    else()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f "${link_path}"
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "$<TARGET_FILE:${target}>"
                "${link_path}"
            VERBATIM
        )
        add_custom_target(${target}_developer_link ALL
            COMMAND ${CMAKE_COMMAND} -E rm -f "${link_path}"
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "$<TARGET_FILE:${target}>"
                "${link_path}"
            DEPENDS ${target}
            VERBATIM
        )
    endif()
endfunction()
