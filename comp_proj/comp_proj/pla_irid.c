#include "pch.h"
#include "pla_irid.h"

pla_irid_t * pla_irid_create(ul_hs_t * name) {
	pla_irid_t * irid = malloc(sizeof(*irid));

	ul_raise_check_mem_alloc(irid);

	*irid = (pla_irid_t){ .name = name };

	return irid;
}
void pla_irid_destroy(pla_irid_t * irid) {
	if (irid == NULL) {
		return;
	}

	pla_irid_destroy(irid->sub_name);

	free(irid);
}
