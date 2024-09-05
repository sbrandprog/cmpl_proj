#pragma once
#include "ul_ec.h"

struct ul_ec_rn {
	ul_ec_t * ec;

	ul_es_node_t * es_node;
};

UL_API void ul_ec_rn_init(ul_ec_rn_t * rn, ul_ec_t * ec, ul_es_ctx_t * es_ctx);
UL_API void ul_ec_rn_cleanup(ul_ec_rn_t * rn);
