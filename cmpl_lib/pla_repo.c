#include "pla_pkg.h"
#include "pla_repo.h"
#include "pla_repo_lsnr.h"

void pla_repo_init(pla_repo_t * repo, ul_es_ctx_t * es_ctx) {
	*repo = (pla_repo_t){ 0 };

	ul_hst_init(&repo->hst);

	repo->es_node = ul_es_create_node(es_ctx, repo);

	InitializeCriticalSection(&repo->lock);
}
void pla_repo_cleanup(pla_repo_t * repo) {
	DeleteCriticalSection(&repo->lock);

	ul_es_destroy_node(repo->es_node);

	pla_pkg_destroy(repo->root);

	ul_hst_cleanup(&repo->hst);

	memset(repo, 0, sizeof(*repo));
}

static void broadcast_upd_nl(pla_repo_t * repo) {
	ul_es_node_t * es_node = repo->es_node;

	for (ul_es_node_t ** it = es_node->nodes, **it_end = it + es_node->nodes_size; it != it_end; ++it) {
		ul_es_node_t * node = *it;

		pla_repo_lsnr_t * lsnr = node->user_data;

		pla_repo_lsnr_update_nl(lsnr, repo);
	}
}
void pla_repo_broadcast_upd(pla_repo_t * repo) {
	EnterCriticalSection(&repo->lock);
	AcquireSRWLockShared(&repo->es_node->ctx->lock);

	broadcast_upd_nl(repo);
	
	LeaveCriticalSection(&repo->lock);
	ReleaseSRWLockShared(&repo->es_node->ctx->lock);
}

static void attach_lsnr_nl(pla_repo_t * repo, pla_repo_lsnr_t * lsnr) {
	ul_es_link(repo->es_node, lsnr->es_node);

	pla_repo_lsnr_update_nl(lsnr, repo);
}
void pla_repo_attach_lsnr(pla_repo_t * repo, pla_repo_lsnr_t * lsnr) {
	EnterCriticalSection(&repo->lock);

	attach_lsnr_nl(repo, lsnr);

	LeaveCriticalSection(&repo->lock);
}
