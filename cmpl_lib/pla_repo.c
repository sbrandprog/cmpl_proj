#include "pla_repo.h"
#include "pla_pkg.h"

void pla_repo_init(pla_repo_t * repo, ul_hst_t * hst)
{
    *repo = (pla_repo_t){ .hst = hst };
}
void pla_repo_cleanup(pla_repo_t * repo)
{
    pla_pkg_destroy(repo->root);

    memset(repo, 0, sizeof(*repo));
}
