#include "ul_es.h"
#include "ul_ec_rn.h"
#include "ul_ec_sn.h"

static void process_actn_proc(ul_ec_sn_t * sn, const ul_ec_actn_t * actn) {
	ul_ec_process_actn(&sn->buf.ec, actn);

	ul_es_node_t * es_node = sn->es_node;

	for (ul_es_node_t ** it = es_node->nodes, **it_end = it + es_node->nodes_size; it != it_end; ++it) {
		ul_es_node_t * node = *it;

		ul_ec_rn_t * rn = node->user_data;

		ul_ec_process_actn(rn->ec, actn);
	}
}

void ul_ec_sn_init(ul_ec_sn_t * sn, ul_es_ctx_t * es_ctx) {
	*sn = (ul_ec_sn_t){ 0 };

	ul_ec_buf_init(&sn->buf);

	ul_ec_init(&sn->ec, sn, process_actn_proc);

	sn->es_node = ul_es_create_node(es_ctx, sn);
}
void ul_ec_sn_cleanup(ul_ec_sn_t * sn) {
	ul_es_destroy_node(sn->es_node);

	ul_ec_cleanup(&sn->ec);

	ul_ec_buf_cleanup(&sn->buf);

	memset(sn, 0, sizeof(*sn));
}

void ul_ec_attach_rn(ul_ec_sn_t * sn, ul_ec_rn_t * rn) {
	ul_ec_clear_all(rn->ec);

	ul_ec_buf_repost(&sn->buf, rn->ec);

	ul_es_link(sn->es_node, rn->es_node);
}
