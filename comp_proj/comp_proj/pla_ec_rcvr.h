#pragma once
#include "pla_ec.h"

struct pla_ec_rcvr {
	pla_ec_t * ec;
	ul_es_node_t * es_node;
};

void pla_ec_rcvr_init(pla_ec_rcvr_t * rcvr, pla_ec_t * ec, ul_es_ctx_t * es_ctx);
void pla_ec_rcvr_cleanup(pla_ec_rcvr_t * rcvr);
