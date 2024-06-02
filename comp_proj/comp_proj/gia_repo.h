#pragma once
#include "gia.h"

struct gia_repo {
	ul_hst_t hst;

	CRITICAL_SECTION lock;

	gia_pkg_t * root;

	ul_es_node_t * es_node;
};

void gia_repo_init(gia_repo_t * repo, ul_es_ctx_t * es_ctx);
void gia_repo_cleanup(gia_repo_t * repo);

void gia_repo_broadcast_upd(gia_repo_t * repo);

void gia_repo_attach_lsnr(gia_repo_t * repo, gia_repo_lsnr_t * lsnr);
