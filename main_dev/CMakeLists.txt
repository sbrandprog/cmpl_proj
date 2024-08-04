
add_executable(main_dev)

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

set_proj_trg_props(main_dev)

target_link_libraries(main_dev cmpl_lib wa_lib ul_lib)


set(MAIN_DEV_DEBUG_ENV_PATH "debug_env")

install(TARGETS main_dev RUNTIME DESTINATION ${MAIN_DEV_DEBUG_ENV_PATH})
install(FILES $<TARGET_RUNTIME_DLLS:main_dev> DESTINATION ${MAIN_DEV_DEBUG_ENV_PATH})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/pla_data/ DESTINATION ${MAIN_DEV_DEBUG_ENV_PATH})
