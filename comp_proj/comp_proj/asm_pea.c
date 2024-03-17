#include "pch.h"
#include "asm_pea.h"
#include "asm_frag.h"

void asm_pea_init(asm_pea_t * pea, ul_hst_t * hst) {
	*pea = (asm_pea_t){ .hst = hst };

	InitializeCriticalSection(&pea->frag_lock);

	asm_it_init(&pea->it);
}
void asm_pea_cleanup(asm_pea_t * pea) {
	asm_it_cleanup(&pea->it);

	DeleteCriticalSection(&pea->frag_lock);

	asm_frag_destroy_chain(pea->frag);

	memset(pea, 0, sizeof(*pea));
}

static asm_frag_t * push_pea_new_frag_nl(asm_pea_t * pea, asm_frag_type_t frag_type) {
	asm_frag_t * frag = asm_frag_create(frag_type);

	frag->next = pea->frag;
	pea->frag = frag;

	return frag;
}
asm_frag_t * asm_pea_push_new_frag(asm_pea_t * pea, asm_frag_type_t frag_type) {
	EnterCriticalSection(&pea->frag_lock);

	asm_frag_t * res;

	__try {
		res = push_pea_new_frag_nl(pea, frag_type);
	}
	__finally {
		LeaveCriticalSection(&pea->frag_lock);
	}

	return res;
}
