#include "pch.h"
#include "gia_pkg.h"
#include "gia_repo.h"

void gia_repo_init(gia_repo_t * repo) {
	*repo = (gia_repo_t){ 0 };

	ul_hst_init(&repo->hst);

	InitializeCriticalSection(&repo->lock);
}
void gia_repo_cleanup(gia_repo_t * repo) {
	DeleteCriticalSection(&repo->lock);

	gia_pkg_destroy(repo->root);

	ul_hst_cleanup(&repo->hst);

	memset(repo, 0, sizeof(*repo));
}
