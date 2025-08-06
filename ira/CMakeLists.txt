
add_library(cmpl_ira SHARED)

target_sources(cmpl_ira
    PUBLIC
    ira.h
    ira_dt.h
    ira_func.h
    ira_inst.h
    ira_int.h
    ira_lo.h
    ira_optr.h
    ira_pds.h
    ira_pec.h
    ira_pec_c.h
    ira_pec_ip.h
    ira_val.h
    pch.h
    PRIVATE
    ira_dt.c
    ira_func.c
    ira_inst.c
    ira_int.c
    ira_lo.c
    ira_optr.c
    ira_pds.c
    ira_pec.c
    ira_pec_c.c
    ira_pec_ip.c
    ira_val.c
)

set_proj_trg_props(cmpl_ira PCH_NAME pch.h)

target_link_libraries(cmpl_ira PUBLIC cmpl_mc)

target_compile_definitions(cmpl_ira PRIVATE IRA_BUILD_DLL)

add_subdirectory(tests)
