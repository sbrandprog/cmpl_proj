#include "ul_es.h"
#include "ul_ec_rn.h"

void ul_ec_rn_init(ul_ec_rn_t * rn, ul_ec_t * ec, ul_es_ctx_t * es_ctx) {
	*rn = (ul_ec_rn_t){ .ec = ec };

	rn->es_node = ul_es_create_node(es_ctx, rn);
}
void ul_ec_rn_cleanup(ul_ec_rn_t * rn) {
	ul_es_destroy_node(rn->es_node);

	memset(rn, 0, sizeof(*rn));
}
