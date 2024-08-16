
add_custom_target(cmpl_lib_tests_copy_runtime_data
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:ul_lib> $<TARGET_FILE:cmpl_lib> ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${PROJECT_SOURCE_DIR}/pla_data ${CMAKE_CURRENT_BINARY_DIR}
)

add_proj_test_exe(cmpl_lib lnk_to_exe1 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_runtime_data)
add_proj_test_exe(cmpl_lib lnk_to_exe2 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_runtime_data)
add_proj_test_exe(cmpl_lib lnk_to_exe3 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_runtime_data)

add_proj_test_exe(cmpl_lib mc_to_exe1 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_runtime_data)
add_proj_test_exe(cmpl_lib mc_to_exe2 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_runtime_data)

add_proj_test_exe(cmpl_lib ira_to_exe1 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_runtime_data)


add_proj_test_exe(cmpl_lib pla_file_tester
	NO_TEST
	LIBRARIES cmpl_lib
	DEPENDENCIES cmpl_lib_tests_copy_runtime_data
	COPY_LOCAL pla_to_exe1.pla pla_va_test.pla
)

add_test(NAME cmpl_lib_pla_test1 COMMAND cmpl_lib_pla_file_tester pla_to_exe1.pla)
add_test(NAME cmpl_lib_pla_test2 COMMAND cmpl_lib_pla_file_tester pla_va_test.pla)
