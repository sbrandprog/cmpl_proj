#pragma once
#include "pla.h"

struct pla_cn {
	ul_hs_t * name;
	pla_cn_t * sub_name;
};

pla_cn_t * pla_cn_create();
void pla_cn_destroy(pla_cn_t * cn);
