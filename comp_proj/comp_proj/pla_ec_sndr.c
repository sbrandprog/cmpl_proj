#include "pch.h"
#include "pla_ec_rcvr.h"
#include "pla_ec_sndr.h"

static void process_actn_nl(pla_ec_sndr_t * sndr, pla_ec_actn_t * actn) {
	pla_ec_do_actn(&sndr->buf.ec, actn);

	ul_es_node_t * es_node = sndr->es_node;

	for (ul_es_node_t ** it = es_node->nodes, **it_end = it + es_node->nodes_size; it != it_end; ++it) {
		ul_es_node_t * node = *it;

		pla_ec_rcvr_t * rcvr = node->user_data;

		pla_ec_do_actn(rcvr->ec, actn);
	}
}
static void process_actn_proc(void * user_data, pla_ec_actn_t * actn) {
	pla_ec_sndr_t * sndr = user_data;

	AcquireSRWLockShared(&sndr->es_node->ctx->lock);

	process_actn_nl(sndr, actn);

	ReleaseSRWLockShared(&sndr->es_node->ctx->lock);
}

void pla_ec_sndr_init(pla_ec_sndr_t * sndr, ul_es_ctx_t * es_ctx) {
	*sndr = (pla_ec_sndr_t){ 0 };

	pla_ec_buf_init(&sndr->buf);

	pla_ec_init(&sndr->ec, sndr, process_actn_proc);

	sndr->es_node = ul_es_create_node(es_ctx, NULL);
}
void pla_ec_sndr_cleanup(pla_ec_sndr_t * sndr) {
	ul_es_destroy_node(sndr->es_node);

	pla_ec_cleanup(&sndr->ec);

	pla_ec_buf_cleanup(&sndr->buf);

	memset(sndr, 0, sizeof(*sndr));
}

void pla_ec_sndr_attach_rcvr(pla_ec_sndr_t * sndr, pla_ec_rcvr_t * rcvr) {
	ul_assert(&sndr->ec != rcvr->ec);
	
	ul_es_link(sndr->es_node, rcvr->es_node);

	pla_ec_clear_all(rcvr->ec);

	pla_ec_buf_repost(&sndr->buf, rcvr->ec);
}
