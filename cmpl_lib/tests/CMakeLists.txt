
set_dir_deps_trg(COPY_DLLS cmpl_lib ul_lib)

execute_process(
	COMMAND ${CMAKE_COMMAND} -E create_symlink
		${PROJECT_SOURCE_DIR}/pla_data
		${CMAKE_CURRENT_BINARY_DIR}/pla_data
	COMMAND ${CMAKE_COMMAND} -E create_symlink
		${CMAKE_CURRENT_SOURCE_DIR}/pla_file_tester_files
		${CMAKE_CURRENT_BINARY_DIR}/pla_file_tester_files
	COMMAND_ERROR_IS_FATAL ANY
)

add_proj_test_exe(cmpl_lib cmpl_zic_test LIBRARIES cmpl_lib)

add_proj_test_exe(cmpl_lib lnk_to_exe1 LIBRARIES cmpl_lib)
add_proj_test_exe(cmpl_lib lnk_to_exe2 LIBRARIES cmpl_lib)
add_proj_test_exe(cmpl_lib lnk_to_exe3 LIBRARIES cmpl_lib)

add_proj_test_exe(cmpl_lib lnk_pel_fail1 LIBRARIES cmpl_lib)
add_proj_test_exe(cmpl_lib lnk_pel_fail2 LIBRARIES cmpl_lib)

add_proj_test_exe(cmpl_lib mc_to_exe1 LIBRARIES cmpl_lib)
add_proj_test_exe(cmpl_lib mc_to_exe2 LIBRARIES cmpl_lib)

add_proj_test_exe(cmpl_lib mc_pea_fail1 LIBRARIES cmpl_lib)

add_proj_test_exe(cmpl_lib ira_to_exe1 LIBRARIES cmpl_lib)

add_proj_test_exe(cmpl_lib pla_file_tester NO_TEST LIBRARIES cmpl_lib)

add_test(NAME cmpl_lib_pla_test1 COMMAND cmpl_lib_pla_file_tester pla_file_tester_files/pla_to_exe1.pla)
add_test(NAME cmpl_lib_pla_test2 COMMAND cmpl_lib_pla_file_tester pla_file_tester_files/pla_va_test.pla)
add_test(NAME cmpl_lib_pla_test3 COMMAND cmpl_lib_pla_file_tester pla_file_tester_files/pla_enmn_test.pla)
