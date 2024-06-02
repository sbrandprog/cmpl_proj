#include "pch.h"
#include "gia_repo_lsnr.h"

void gia_repo_lsnr_init(gia_repo_lsnr_t * lsnr, void * user_data, gia_repo_lsnr_update_proc_nl_t * update_proc, ul_es_ctx_t * es_ctx) {
	*lsnr = (gia_repo_lsnr_t){ .user_data = user_data, .update_proc = update_proc };

	lsnr->es_node = ul_es_create_node(es_ctx, lsnr);
}
void gia_repo_lsnr_cleanup(gia_repo_lsnr_t * lsnr) {
	ul_es_destroy_node(lsnr->es_node);

	memset(lsnr, 0, sizeof(*lsnr));
}
