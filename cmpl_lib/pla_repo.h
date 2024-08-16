#pragma once
#include "pla.h"

struct pla_repo {
	ul_hst_t * hst;

	CRITICAL_SECTION lock;

	pla_pkg_t * root;

	ul_es_node_t * es_node;
};

PLA_API void pla_repo_init(pla_repo_t * repo, ul_hst_t * hst, ul_es_ctx_t * es_ctx);
PLA_API void pla_repo_cleanup(pla_repo_t * repo);

PLA_API void pla_repo_broadcast_upd(pla_repo_t * repo);

PLA_API void pla_repo_attach_lsnr(pla_repo_t * repo, pla_repo_lsnr_t * lsnr);
