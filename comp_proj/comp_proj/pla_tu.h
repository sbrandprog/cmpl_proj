#pragma once
#include "pla.h"

struct pla_tu_ref {
	pla_cn_t * cn;
	bool is_rel;
};
struct pla_tu {
	ul_hs_t * name;

	pla_tu_t * next;
	
	pla_dclr_t * root;

	size_t refs_size;
	pla_tu_ref_t * refs;
	size_t refs_cap;
};

pla_tu_t * pla_tu_create(ul_hs_t * name);
void pla_tu_destroy(pla_tu_t * tu);
void pla_tu_destroy_chain(pla_tu_t * tu);

pla_tu_ref_t * pla_tu_push_ref(pla_tu_t * tu);
