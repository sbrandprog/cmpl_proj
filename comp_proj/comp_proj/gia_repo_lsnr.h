#pragma once
#include "gia_repo.h"

typedef void gia_repo_lsnr_update_proc_nl_t(void * user_data, gia_repo_t * repo);

struct gia_repo_lsnr {
	void * user_data;

	gia_repo_lsnr_update_proc_nl_t * update_proc;

	ul_es_node_t * es_node;
};

void gia_repo_lsnr_init(gia_repo_lsnr_t * lsnr, void * user_data, gia_repo_lsnr_update_proc_nl_t * update_proc, ul_es_ctx_t * es_ctx);
void gia_repo_lsnr_cleanup(gia_repo_lsnr_t * lsnr);

inline void gia_repo_lsnr_update_nl(gia_repo_lsnr_t * lsnr, gia_repo_t * repo) {
	lsnr->update_proc(lsnr->user_data, repo);
}
