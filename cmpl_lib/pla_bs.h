#pragma once
#include "lnk_pel.h"
#include "pla.h"

struct pla_bs_src {
	pla_repo_t * repo;
	ul_hs_t * first_tus_name;
	
	lnk_pel_sett_t lnk_sett;
};

PLA_API void pla_bs_src_init(pla_bs_src_t * src, pla_repo_t * repo, ul_hs_t * first_tus_name);
PLA_API void pla_bs_src_cleanup(pla_bs_src_t * src);

PLA_API bool pla_bs_build_nl(const pla_bs_src_t * src);
