
add_custom_target(wa_lib_tests_copy_runtime_data
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:ul_lib> $<TARGET_FILE:wa_lib> ${CMAKE_CURRENT_BINARY_DIR}
)

add_proj_test_exe(wa_lib wa_zic_test LIBRARIES wa_lib DEPENDENCIES wa_lib_tests_copy_runtime_data)
