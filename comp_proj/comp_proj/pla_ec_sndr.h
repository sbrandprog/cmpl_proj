#pragma once
#include "pla_ec_buf.h"

struct pla_ec_sndr {
	pla_ec_buf_t buf;

	pla_ec_t ec;

	ul_es_node_t * es_node;
};

void pla_ec_sndr_init(pla_ec_sndr_t * sndr, ul_es_ctx_t * es_ctx);
void pla_ec_sndr_cleanup(pla_ec_sndr_t * sndr);

void pla_ec_sndr_attach_rcvr(pla_ec_sndr_t * sndr, pla_ec_rcvr_t * rcvr);
