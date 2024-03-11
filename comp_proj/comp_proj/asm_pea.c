#include "pch.h"
#include "asm_pea.h"
#include "asm_frag.h"

void asm_pea_init(asm_pea_t * pea, ul_hst_t * hst) {
	*pea = (asm_pea_t){ .hst = hst };
}
void asm_pea_cleanup(asm_pea_t * pea) {
	asm_frag_destroy_chain(pea->frag);

	asm_it_cleanup(&pea->it);

	memset(pea, 0, sizeof(*pea));
}
