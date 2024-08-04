
add_library(ul_lib SHARED)

target_sources(ul_lib
	PUBLIC
		pch.h
		ul.h
		ul_arr.h
		ul_assert.h
		ul_es.h
		ul_hs.h
		ul_hsb.h
		ul_hst.h
		ul_lib.h
		ul_misc.h
		ul_ros.h
	PRIVATE
		ul_assert.c
		ul_es.c
		ul_hs.c
		ul_hsb.c
		ul_hst.c
)

set_proj_trg_props(ul_lib)

target_compile_definitions(ul_lib PRIVATE UL_BUILD_DLL)
