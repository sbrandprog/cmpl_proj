
function(set_proj_trg_props trg)
    cmake_parse_arguments(ARGS "COPY_DLLS" "PCH_NAME" "" ${ARGN})

    target_include_directories(${trg} PUBLIC ${PROJECT_SOURCE_DIR})

    target_precompile_headers(${trg} PRIVATE ${ARGS_PCH_NAME})

    set_target_properties(${trg} PROPERTIES
        COMPILE_WARNING_AS_ERROR ON
        C_STANDARD 11
        C_EXTENSIONS OFF
        CXX_STANDARD 20
        CXX_EXTENSIONS OFF
    )

    if(MSVC)
        target_compile_options(${trg} PRIVATE /W3 /utf-8)
        target_compile_definitions(${trg} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    if(WIN32)
        if(ARGS_COPY_DLLS)
            set(trg_dlls $<TARGET_RUNTIME_DLLS:${trg}>)

            add_custom_command(TARGET ${trg} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E $<IF:$<BOOL:${trg_dlls}>,copy_if_different,true> $<TARGET_RUNTIME_DLLS:${trg}> $<TARGET_FILE_DIR:${trg}>
                COMMAND_EXPAND_LISTS
            )
        endif()
    endif()
endfunction()

function(add_proj_test_exe test_prefix exe_name)
    set(main_file ${exe_name}.c)
    set(exe_name ${test_prefix}_${exe_name})

    cmake_parse_arguments(ARGS "NO_TEST" "" "LIBRARIES;DEPENDENCIES" ${ARGN})

    add_executable(${exe_name} ${main_file})

    set_proj_trg_props(${exe_name} COPY_DLLS)

    target_link_libraries(${exe_name} ${ARGS_LIBRARIES})

    if(ARGS_DEPENDENCIES)
        add_dependencies(${exe_name} ${ARGS_DEPENDENCIES})
    endif()

    if(NOT ARGS_NO_TEST)
        add_test(NAME ${exe_name}_test COMMAND ${exe_name})
    endif()
endfunction()
