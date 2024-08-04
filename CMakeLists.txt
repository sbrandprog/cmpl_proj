cmake_minimum_required(VERSION 3.24)

project(comp_proj)

function(set_proj_trg_props trg)
	target_include_directories(${trg} PUBLIC ${PROJECT_SOURCE_DIR})

	target_precompile_headers(${trg} PRIVATE pch.h)

	set_target_properties(${trg} PROPERTIES
		COMPILE_WARNING_AS_ERROR ON
		C_STANDARD 11
		C_EXTENSIONS OFF
	)

	target_compile_definitions(${trg} PRIVATE UNICODE=1 _UNICODE=1)
	
	if (MSVC)
		target_compile_options(${trg} PRIVATE /W3 /WX)
	endif()
endfunction()

add_subdirectory(cmpl_lib)
add_subdirectory(main_dev)
add_subdirectory(ul_lib)
add_subdirectory(wa_lib)
