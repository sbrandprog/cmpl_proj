#pragma once
#include "pla.h"

struct pla_irid {
	ul_hs_t * name;
	pla_irid_t * sub_name;
};

pla_irid_t * pla_irid_create(ul_hs_t * name);
void pla_irid_destroy(pla_irid_t * irid);
