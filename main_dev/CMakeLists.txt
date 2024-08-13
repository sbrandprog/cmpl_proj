
add_custom_target(main_dev_copy_pla_data
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${PROJECT_SOURCE_DIR}/pla_data ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(main_dev)

add_dependencies(main_dev main_dev_copy_pla_data)

target_sources(main_dev
	PUBLIC
		gia.h
		gia_edit.c
		gia_edit.h
		gia_repo_view.c
		gia_repo_view.h
		main.c
		pch.h
)

set_proj_trg_props(main_dev COPY_DLLS PCH_NAME pch.h)

target_link_libraries(main_dev cmpl_lib wa_lib ul_lib)
