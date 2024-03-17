#include "pch.h"
#include "asm_pea.h"
#include "asm_frag.h"

void asm_pea_init(asm_pea_t * pea, ul_hst_t * hst) {
	*pea = (asm_pea_t){ .hst = hst };

	asm_it_init(&pea->it);
}
void asm_pea_cleanup(asm_pea_t * pea) {
	asm_it_cleanup(&pea->it);

	asm_frag_destroy_chain(pea->frag);

	memset(pea, 0, sizeof(*pea));
}

asm_frag_t * asm_pea_push_new_frag(asm_pea_t * pea, asm_frag_type_t frag_type) {
	asm_frag_t * frag = asm_frag_create(frag_type);

	asm_frag_t * next;

	do {
		next = pea->frag;

		frag->next = next;
	} while (InterlockedCompareExchangePointer(&pea->frag, frag, next) != next);

	return frag;
}
