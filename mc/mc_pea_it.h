#pragma once
#include "mc.h"

struct mc_pea_it_sym
{
    mc_pea_it_sym_t * next;
    ul_hs_t * name;
    ul_hs_t * link_name;
};
struct mc_pea_it_lib
{
    mc_pea_it_lib_t * next;
    ul_hs_t * name;
    mc_pea_it_sym_t * sym;
};
struct mc_pea_it
{
    ul_hst_t * hst;
    ul_ec_fmtr_t * ec_fmtr;

    mc_pea_it_lib_t * lib;
};

MC_API void mc_pea_it_init(mc_pea_it_t * it, ul_hst_t * hst, ul_ec_fmtr_t * ec_fmtr);
MC_API void mc_pea_it_cleanup(mc_pea_it_t * it);

MC_API mc_pea_it_lib_t * mc_pea_it_get_lib(mc_pea_it_t * it, ul_hs_t * name);
MC_API mc_pea_it_sym_t * mc_pea_it_add_sym(mc_pea_it_t * it, ul_hs_t * lib_name, ul_hs_t * sym_name, ul_hs_t * sym_link_name);

MC_API bool mc_pea_it_build(mc_pea_it_t * it, lnk_pel_t * out);
