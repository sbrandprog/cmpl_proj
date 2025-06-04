
set_dir_deps_trg(COPY_DLLS cmpl_lib ul_lib)

execute_process(
	COMMAND ${CMAKE_COMMAND} -E create_symlink
		${PROJECT_SOURCE_DIR}/pla_data
		${CMAKE_CURRENT_BINARY_DIR}/pla_data
	COMMAND_ERROR_IS_FATAL ANY
)

add_executable(main_dev)

target_sources(main_dev
    PUBLIC
    main.c
    pch.h
)

set_proj_trg_props(main_dev PCH_NAME pch.h)

target_link_libraries(main_dev cmpl_lib ul_lib)
