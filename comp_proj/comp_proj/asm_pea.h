#pragma once
#include "asm_it.h"
#include "lnk.h"
#include "u_hs.h"

struct asm_pea {
	u_hst_t * hst;

	asm_frag_t * frag;

	asm_it_t it;

	u_hs_t * ep_name;
};

void asm_pea_init(asm_pea_t * pea, u_hst_t * hst);
void asm_pea_cleanup(asm_pea_t * pea);
