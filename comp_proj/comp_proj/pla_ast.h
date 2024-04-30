#pragma once
#include "pla_pds.h"
#include "ira.h"

struct pla_ast {
	ul_hst_t * hst;

	pla_pkg_t * root;

	ul_hs_t * pds[PlaPds_Count];
};

void pla_ast_init(pla_ast_t * ast, ul_hst_t * hst);
void pla_ast_cleanup(pla_ast_t * ast);
