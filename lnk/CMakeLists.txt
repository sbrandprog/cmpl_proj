
add_library(cmpl_lnk SHARED)

target_sources(cmpl_lnk
    PUBLIC
    lnk.h
	lnk_lib.h
    lnk_pe.h
    lnk_pel.h
    lnk_pel_l.h
    lnk_pel_m.h
    lnk_sect.h
    pch.h
    PRIVATE
    lnk_pel.c
    lnk_pel_l.c
    lnk_pel_m.c
    lnk_sect.c
)

set_proj_trg_props(cmpl_lnk PCH_NAME pch.h)

target_link_libraries(cmpl_lnk PUBLIC cmpl_ul)

target_compile_definitions(cmpl_lnk PRIVATE LNK_BUILD_DLL)

add_subdirectory(tests)
