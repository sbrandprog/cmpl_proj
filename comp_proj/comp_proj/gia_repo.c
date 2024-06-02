#include "pch.h"
#include "gia_pkg.h"
#include "gia_repo.h"
#include "gia_repo_lsnr.h"

void gia_repo_init(gia_repo_t * repo, ul_es_ctx_t * es_ctx) {
	*repo = (gia_repo_t){ 0 };

	ul_hst_init(&repo->hst);

	repo->es_node = ul_es_create_node(es_ctx, repo);

	InitializeCriticalSection(&repo->lock);
}
void gia_repo_cleanup(gia_repo_t * repo) {
	DeleteCriticalSection(&repo->lock);

	ul_es_destroy_node(repo->es_node);

	gia_pkg_destroy(repo->root);

	ul_hst_cleanup(&repo->hst);

	memset(repo, 0, sizeof(*repo));
}

static void broadcast_upd_nl(gia_repo_t * repo) {
	ul_es_node_t * es_node = repo->es_node;

	for (ul_es_node_t ** it = es_node->nodes, **it_end = it + es_node->nodes_size; it != it_end; ++it) {
		ul_es_node_t * node = *it;

		gia_repo_lsnr_t * lsnr = node->user_data;

		gia_repo_lsnr_update_nl(lsnr, repo);
	}
}
void gia_repo_broadcast_upd(gia_repo_t * repo) {
	EnterCriticalSection(&repo->lock);
	AcquireSRWLockShared(&repo->es_node->ctx->lock);

	__try {
		broadcast_upd_nl(repo);
	}
	__finally {
		LeaveCriticalSection(&repo->lock);
		ReleaseSRWLockShared(&repo->es_node->ctx->lock);
	}
}

static void attach_lsnr_nl(gia_repo_t * repo, gia_repo_lsnr_t * lsnr) {
	ul_es_link(repo->es_node, lsnr->es_node);

	gia_repo_lsnr_update_nl(lsnr, repo);
}
void gia_repo_attach_lsnr(gia_repo_t * repo, gia_repo_lsnr_t * lsnr) {
	EnterCriticalSection(&repo->lock);

	__try {
		attach_lsnr_nl(repo, lsnr);
	}
	__finally {
		LeaveCriticalSection(&repo->lock);
	}
}
