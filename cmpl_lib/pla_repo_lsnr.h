#pragma once
#include "pla_repo.h"

typedef void pla_repo_lsnr_update_proc_nl_t(void * user_data, pla_repo_t * repo);

struct pla_repo_lsnr {
	void * user_data;

	pla_repo_lsnr_update_proc_nl_t * update_proc;

	ul_es_node_t * es_node;
};

PLA_API void pla_repo_lsnr_init(pla_repo_lsnr_t * lsnr, void * user_data, pla_repo_lsnr_update_proc_nl_t * update_proc, ul_es_ctx_t * es_ctx);
PLA_API void pla_repo_lsnr_cleanup(pla_repo_lsnr_t * lsnr);

inline void pla_repo_lsnr_update_nl(pla_repo_lsnr_t * lsnr, pla_repo_t * repo) {
	lsnr->update_proc(lsnr->user_data, repo);
}
