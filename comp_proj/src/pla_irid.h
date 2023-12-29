#pragma once
#include "pla.h"
#include "u_hs.h"

struct pla_irid {
	u_hs_t * name;
	pla_irid_t * sub_name;
};

pla_irid_t * pla_irid_create(u_hs_t * name);
void pla_irid_destroy(pla_irid_t * irid);
