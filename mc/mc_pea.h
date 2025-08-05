#pragma once
#include "mc_pea_it.h"

struct mc_pea
{
    ul_hst_t * hst;
    ul_ec_fmtr_t * ec_fmtr;

    mc_frag_t * frag;

    mc_pea_it_t it;

    ul_hs_t * ep_name;
};

MC_API void mc_pea_init(mc_pea_t * pea, ul_hst_t * hst, ul_ec_fmtr_t * ec_fmtr);
MC_API void mc_pea_cleanup(mc_pea_t * pea);
