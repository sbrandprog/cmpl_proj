
add_custom_target(cmpl_lib_tests_copy_dlls
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:ul_lib> $<TARGET_FILE:cmpl_lib> ${CMAKE_CURRENT_BINARY_DIR}
)

add_proj_test_exe(cmpl_lib lnk_to_exe1 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_dlls)
add_proj_test_exe(cmpl_lib lnk_to_exe2 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_dlls)
add_proj_test_exe(cmpl_lib lnk_to_exe3 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_dlls)

add_proj_test_exe(cmpl_lib mc_to_exe1 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_dlls)
add_proj_test_exe(cmpl_lib mc_to_exe2 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_dlls)

add_proj_test_exe(cmpl_lib ira_to_exe1 LIBRARIES cmpl_lib DEPENDENCIES cmpl_lib_tests_copy_dlls)
