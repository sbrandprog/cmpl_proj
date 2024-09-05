#pragma once
#include "ul_ec_buf.h"

struct ul_ec_sn {
	ul_ec_buf_t buf;

	ul_ec_t ec;

	ul_es_node_t * es_node;
};

UL_API void ul_ec_sn_init(ul_ec_sn_t * sn, ul_es_ctx_t * es_ctx);
UL_API void ul_ec_sn_cleanup(ul_ec_sn_t * sn);

UL_API void ul_ec_attach_rn(ul_ec_sn_t * sn, ul_ec_rn_t * rn);
