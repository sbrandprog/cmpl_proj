#include "pch.h"
#include "pla_cn.h"
#include "pla_dclr.h"
#include "pla_tu.h"

pla_tu_t * pla_tu_create(ul_hs_t * name) {
	pla_tu_t * tu = malloc(sizeof(*tu));

	ul_assert(tu != NULL);

	*tu = (pla_tu_t){ .name = name };

	return tu;
}
void pla_tu_destroy(pla_tu_t * tu) {
	if (tu == NULL) {
		return;
	}

	for (pla_tu_ref_t * ref = tu->refs, *ref_end = ref + tu->refs_size; ref != ref_end; ++ref) {
		pla_cn_destroy(ref->cn);
	}

	free(tu->refs);

	pla_dclr_destroy(tu->root);

	free(tu);
}
void pla_tu_destroy_chain(pla_tu_t * tu) {
	while (tu != NULL) {
		pla_tu_t * next = tu->next;

		pla_tu_destroy(tu);

		tu = next;
	}
}

pla_tu_ref_t * pla_tu_push_ref(pla_tu_t * tu) {
	if (tu->refs_size + 1 > tu->refs_cap) {
		ul_arr_grow(&tu->refs_cap, (void **)&tu->refs, sizeof(*tu->refs), 1);
	}

	pla_tu_ref_t * ref = &tu->refs[tu->refs_size++];

	*ref = (pla_tu_ref_t){ 0 };

	return ref;
}
