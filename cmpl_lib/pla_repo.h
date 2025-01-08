#pragma once
#include "pla.h"

struct pla_repo {
	ul_hst_t * hst;

	pla_pkg_t * root;
};

PLA_API void pla_repo_init(pla_repo_t * repo, ul_hst_t * hst);
PLA_API void pla_repo_cleanup(pla_repo_t * repo);
