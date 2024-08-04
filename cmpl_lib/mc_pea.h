#pragma once
#include "mc_it.h"
#include "lnk.h"

struct mc_pea {
	ul_hst_t * hst;
	
	mc_frag_t * frag;

	mc_it_t it;

	ul_hs_t * ep_name;
};

MC_API void mc_pea_init(mc_pea_t * pea, ul_hst_t * hst);
MC_API void mc_pea_cleanup(mc_pea_t * pea);
