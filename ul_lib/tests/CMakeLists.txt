
add_custom_target(ul_lib_tests_copy_runtime_data
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:ul_lib> ${CMAKE_CURRENT_BINARY_DIR}
)

add_proj_test_exe(ul_lib ul_zic_test LIBRARIES ul_lib DEPENDENCIES ul_lib_tests_copy_runtime_data)
add_proj_test_exe(ul_lib ul_bs_test1 LIBRARIES ul_lib DEPENDENCIES ul_lib_tests_copy_runtime_data)
add_proj_test_exe(ul_lib ul_json_test1 LIBRARIES ul_lib DEPENDENCIES ul_lib_tests_copy_runtime_data)
