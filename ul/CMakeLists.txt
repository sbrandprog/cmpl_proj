
add_library(cmpl_ul SHARED)

target_sources(cmpl_ul
    PUBLIC
    pch.h
    ul.h
    ul_arr.h
    ul_assert.h
    ul_bs.h
    ul_ec.h
    ul_ec_buf.h
    ul_ec_fmtr.h
    ul_ec_prntr.h
    ul_fs.h
    ul_hs.h
    ul_hsb.h
    ul_hst.h
    ul_json.h
    ul_json_p.h
    ul_json_g.h
    ul_lib.h
    ul_misc.h
    ul_ros.h
    ul_spawn.h
    PRIVATE
    ul_arr.c
    ul_assert.c
    ul_bs.c
    ul_ec.c
    ul_ec_buf.c
    ul_ec_fmtr.c
    ul_ec_prntr.c
    ul_fs.c
    ul_hs.c
    ul_hsb.c
    ul_hst.c
    ul_json.c
    ul_json_p.c
    ul_json_g.c
    ul_misc.c
    ul_spawn.c
)

set_proj_trg_props(cmpl_ul PCH_NAME pch.h)

target_compile_definitions(cmpl_ul PRIVATE UL_BUILD_DLL)

add_subdirectory(tests)
