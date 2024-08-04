#pragma once
#include "pla_ec.h"

struct pla_cn {
	pla_ec_pos_t pos_start;
	pla_ec_pos_t pos_end;

	ul_hs_t * name;
	pla_cn_t * sub_name;
};

PLA_API pla_cn_t * pla_cn_create();
PLA_API void pla_cn_destroy(pla_cn_t * cn);
