#pragma once
#include "lnk.h"
#include "mc.h"

struct mc_it_sym {
	mc_it_sym_t * next;
	ul_hs_t * name;
	ul_hs_t * link_name;
};
struct mc_it_lib {
	mc_it_lib_t * next;
	ul_hs_t * name;
	mc_it_sym_t * sym;
};
struct mc_it {
	mc_it_lib_t * lib;
};

MC_API void mc_it_init(mc_it_t * it);
MC_API void mc_it_cleanup(mc_it_t * it);

MC_API mc_it_lib_t * mc_it_get_lib(mc_it_t * it, ul_hs_t * name);
MC_API mc_it_sym_t * mc_it_add_sym(mc_it_t * it, ul_hs_t * lib_name, ul_hs_t * sym_name, ul_hs_t * sym_link_name);

MC_API bool mc_it_build(mc_it_t * it, ul_hst_t * hst, lnk_pel_t * out);
