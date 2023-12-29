#include "pch.h"
#include "pla_irid.h"
#include "u_assert.h"

pla_irid_t * pla_irid_create(u_hs_t * name) {
	pla_irid_t * irid = malloc(sizeof(*irid));

	u_assert(irid != NULL);

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
