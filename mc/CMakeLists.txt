
add_library(cmpl_mc SHARED)

target_sources(cmpl_mc
    PUBLIC
    mc.h
    mc_defs.h
    mc_frag.h
    mc_inst.h
    mc_pea.h
    mc_pea_b.h
    mc_pea_it.h
    mc_reg.h
    mc_size.h
    pch.h
    PRIVATE
    mc_defs.c
    mc_frag.c
    mc_inst.c
    mc_pea.c
    mc_pea_b.c
    mc_pea_it.c
    mc_reg.c
    mc_size.c
)

set_proj_trg_props(cmpl_mc PCH_NAME pch.h)

target_link_libraries(cmpl_mc PUBLIC cmpl_lnk)

target_compile_definitions(cmpl_mc PRIVATE MC_BUILD_DLL)

add_subdirectory(tests)
