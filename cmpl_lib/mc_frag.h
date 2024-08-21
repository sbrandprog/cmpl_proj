#pragma once
#include "lnk.h"
#include "mc.h"

enum mc_frag_type {
	McFragCode,
	McFragRoData,
	McFragRwData,
	McFrag_Count
};
struct mc_frag {
	mc_frag_type_t type;
	mc_frag_t * next;

	size_t insts_size;
	mc_inst_t * insts;
	size_t insts_cap;

	size_t align;
};

MC_API mc_frag_t * mc_frag_create(mc_frag_type_t type);
MC_API void mc_frag_destroy(mc_frag_t * frag);

MC_API void mc_frag_destroy_chain(mc_frag_t * frag);

MC_API void mc_frag_push_inst(mc_frag_t * frag, const mc_inst_t * inst);

MC_API bool mc_frag_build(mc_frag_t * frag, lnk_sect_t ** out);

MC_API extern const mc_defs_sd_type_t mc_frag_type_to_sd[McFrag_Count];
