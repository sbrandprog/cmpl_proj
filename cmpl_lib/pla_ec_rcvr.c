#include "pla_ec_rcvr.h"

void pla_ec_rcvr_init(pla_ec_rcvr_t * rcvr, pla_ec_t * ec, ul_es_ctx_t * es_ctx) {
	*rcvr = (pla_ec_rcvr_t){ .ec = ec };

	rcvr->es_node = ul_es_create_node(es_ctx, rcvr);
}
void pla_ec_rcvr_cleanup(pla_ec_rcvr_t * rcvr) {
	ul_es_destroy_node(rcvr->es_node);

	memset(rcvr, 0, sizeof(*rcvr));
}
