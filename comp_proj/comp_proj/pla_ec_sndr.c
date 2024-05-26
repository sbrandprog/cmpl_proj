#include "pch.h"
#include "pla_ec_rcvr.h"
#include "pla_ec_sndr.h"

static void post_err_nl(pla_ec_sndr_t * sndr, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, ul_hs_t * msg) {
	pla_ec_post(&sndr->buf.ec, group, pos_start, pos_end, msg);
	
	ul_es_node_t * es_node = sndr->es_node;

	for (ul_es_node_t ** it = es_node->nodes, **it_end = it + es_node->nodes_size; it != it_end; ++it) {
		ul_es_node_t * node = *it;

		pla_ec_rcvr_t * rcvr = node->user_data;

		pla_ec_post(rcvr->ec, group, pos_start, pos_end, msg);
	}
}
static void post_err_proc(void * user_data, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, ul_hs_t * msg) {
	pla_ec_sndr_t * sndr = user_data;
	
	AcquireSRWLockShared(&sndr->es_node->ctx->lock);

	__try {
		post_err_nl(sndr, group, pos_start, pos_end, msg);
	}
	__finally {
		ReleaseSRWLockShared(&sndr->es_node->ctx->lock);
	}
}

static void shift_errs_nl(pla_ec_sndr_t * sndr, size_t group, size_t start_line, size_t shift_size, bool shift_rev) {
	pla_ec_shift(&sndr->buf.ec, group, start_line, shift_size, shift_rev);

	ul_es_node_t * es_node = sndr->es_node;

	for (ul_es_node_t ** it = es_node->nodes, **it_end = it + es_node->nodes_size; it != it_end; ++it) {
		ul_es_node_t * node = *it;

		pla_ec_rcvr_t * rcvr = node->user_data;

		pla_ec_shift(rcvr->ec, group, start_line, shift_size, shift_rev);
	}
}
static void shift_errs_proc(void * user_data, size_t group, size_t start_line, size_t shift_size, bool shift_rev) {
	pla_ec_sndr_t * sndr = user_data;

	AcquireSRWLockShared(&sndr->es_node->ctx->lock);

	__try {
		shift_errs_nl(sndr, group, start_line, shift_size, shift_rev);
	}
	__finally {
		ReleaseSRWLockShared(&sndr->es_node->ctx->lock);
	}
}

static void clear_errs_nl(pla_ec_sndr_t * sndr, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end) {
	pla_ec_clear(&sndr->buf.ec, group, pos_start, pos_end);

	ul_es_node_t * es_node = sndr->es_node;

	for (ul_es_node_t ** it = es_node->nodes, **it_end = it + es_node->nodes_size; it != it_end; ++it) {
		ul_es_node_t * node = *it;

		pla_ec_rcvr_t * rcvr = node->user_data;

		pla_ec_clear(rcvr->ec, group, pos_start, pos_end);
	}
}
static void clear_errs_proc(void * user_data, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end) {
	pla_ec_sndr_t * sndr = user_data;

	AcquireSRWLockShared(&sndr->es_node->ctx->lock);

	__try {
		clear_errs_nl(sndr, group, pos_start, pos_end);
	}
	__finally {
		ReleaseSRWLockShared(&sndr->es_node->ctx->lock);
	}
}

void pla_ec_sndr_init(pla_ec_sndr_t * sndr, ul_es_ctx_t * es_ctx) {
	*sndr = (pla_ec_sndr_t){ 0 };

	pla_ec_buf_init(&sndr->buf);

	pla_ec_init(&sndr->ec, sndr, post_err_proc, shift_errs_proc, clear_errs_proc);

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
