function(woki_add_developer_link target)
    if(EMSCRIPTEN)
        return()
    endif()

    set(link_dir "${CMAKE_SOURCE_DIR}/.bin")
    file(MAKE_DIRECTORY "${link_dir}")
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${target}>"
                "${link_dir}/$<TARGET_FILE_NAME:${target}>"
            VERBATIM
        )
        add_custom_target(${target}_developer_link ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${target}>"
                "${link_dir}/$<TARGET_FILE_NAME:${target}>"
            DEPENDS ${target}
            VERBATIM
        )
    else()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f "${link_dir}/${target}"
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "$<TARGET_FILE:${target}>"
                "${link_dir}/${target}"
            VERBATIM
        )
        add_custom_target(${target}_developer_link ALL
            COMMAND ${CMAKE_COMMAND} -E rm -f "${link_dir}/${target}"
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "$<TARGET_FILE:${target}>"
                "${link_dir}/${target}"
            DEPENDS ${target}
            VERBATIM
        )
    endif()
endfunction()
