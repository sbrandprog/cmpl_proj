#pragma once
#include "gia.h"

struct gia_repo {
	ul_hst_t hst;

	CRITICAL_SECTION lock;

	gia_pkg_t * root;
};

void gia_repo_init(gia_repo_t * repo);
void gia_repo_cleanup(gia_repo_t * repo);
