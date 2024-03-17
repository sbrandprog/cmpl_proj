#pragma once
#include "asm_it.h"
#include "lnk.h"

struct asm_pea {
	ul_hst_t * hst;
	
	asm_frag_t * frag;

	asm_it_t it;

	ul_hs_t * ep_name;
};

void asm_pea_init(asm_pea_t * pea, ul_hst_t * hst);
void asm_pea_cleanup(asm_pea_t * pea);

asm_frag_t * asm_pea_push_new_frag(asm_pea_t * pea, asm_frag_type_t frag_type);
