
function(set_dir_deps_trg)
	cmake_parse_arguments(ARGS "" "" "COPY_DLLS" ${ARGN})

	string(MAKE_C_IDENTIFIER "${CMAKE_CURRENT_SOURCE_DIR}" trg_name)

	if (TARGET ${trg_name})
		message(FATAL_ERROR "Directory dependencies target for ${CMAKE_CURRENT_SOURCE_DIR} already defined.")
	endif()

	if (WIN32)
		foreach (dll ${ARGS_COPY_DLLS})
			list(APPEND dlls_list "$<TARGET_FILE:${dll}>")
		endforeach()

		if (dlls_list)
			add_custom_target(${trg_name}
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
				${dlls_list}
				${CMAKE_CURRENT_BINARY_DIR}
				COMMENT "Copying dependencies (${ARGS_COPY_DLLS}) in ${CMAKE_CURRENT_BINARY_DIR}")

			message(VERBOSE "Set up directory dependencies target for ${CMAKE_CURRENT_SOURCE_DIR}")

			set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY DLLS_DEPS_TRG ${trg_name})
		endif()
	endif()
endfunction()

function(set_proj_trg_props trg)
    cmake_parse_arguments(ARGS "" "PCH_NAME" "" ${ARGN})

    target_include_directories(${trg} PUBLIC ${PROJECT_SOURCE_DIR})

    target_precompile_headers(${trg} PRIVATE ${ARGS_PCH_NAME})

    set_target_properties(${trg} PROPERTIES
        COMPILE_WARNING_AS_ERROR ON
		EXPORT_COMPILE_COMMANDS ON
        C_STANDARD 11
        C_EXTENSIONS OFF
    )

    if(MSVC)
        target_compile_options(${trg} PRIVATE /W3 /utf-8)
        target_compile_definitions(${trg} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

	get_property(trg_dir TARGET ${trg} PROPERTY SOURCE_DIR)
	get_property(trg_dir_deps_trg DIRECTORY "${trg_dir}" PROPERTY DLLS_DEPS_TRG)

	if (TARGET ${trg_dir_deps_trg})
		add_dependencies(${trg} ${trg_dir_deps_trg})
	endif()
endfunction()

function(add_proj_test_exe test_prefix exe_name)
    set(main_file ${exe_name}.c)
    set(exe_name ${test_prefix}_${exe_name})

    cmake_parse_arguments(ARGS "NO_TEST" "" "LIBRARIES;DEPENDENCIES" ${ARGN})

    add_executable(${exe_name} ${main_file})

    set_proj_trg_props(${exe_name})

    target_link_libraries(${exe_name} ${ARGS_LIBRARIES})

    if(ARGS_DEPENDENCIES)
        add_dependencies(${exe_name} ${ARGS_DEPENDENCIES})
    endif()

    if(NOT ARGS_NO_TEST)
        add_test(NAME ${exe_name}_test COMMAND ${exe_name})
    endif()
endfunction()
