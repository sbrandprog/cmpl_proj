#include "pch.h"
#include "pla_repo_lsnr.h"

void pla_repo_lsnr_init(pla_repo_lsnr_t * lsnr, void * user_data, pla_repo_lsnr_update_proc_nl_t * update_proc, ul_es_ctx_t * es_ctx) {
	*lsnr = (pla_repo_lsnr_t){ .user_data = user_data, .update_proc = update_proc };

	lsnr->es_node = ul_es_create_node(es_ctx, lsnr);
}
void pla_repo_lsnr_cleanup(pla_repo_lsnr_t * lsnr) {
	ul_es_destroy_node(lsnr->es_node);

	memset(lsnr, 0, sizeof(*lsnr));
}
