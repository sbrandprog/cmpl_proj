
add_library(wa_lib SHARED)

target_sources(wa_lib
	PUBLIC
		pch.h
		wa.h
		wa_accel.h
		wa_ctl.h
		wa_ctx.h
		wa_err.h
		wa_host.h
		wa_splt.h
		wa_style.h
		wa_wcr.h
		wa_wnd.h
	PRIVATE
		wa_accel.c
		wa_ctl.c
		wa_ctx.c
		wa_err.c
		wa_host.c
		wa_splt.c
		wa_style.c
		wa_wcr.c
		wa_wnd.c
)

set_proj_trg_props(wa_lib)

target_link_libraries(wa_lib ul_lib uxtheme.lib)

target_compile_definitions(wa_lib PRIVATE WA_BUILD_DLL)
