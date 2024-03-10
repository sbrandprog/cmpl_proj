#pragma once
#include "pla_pds.h"
#include "ira.h"
#include "u_hst.h"

struct pla_ast {
	u_hst_t * hst;

	pla_dclr_t * root;

	u_hs_t * pds[PlaPds_Count];
};

void pla_ast_init(pla_ast_t * ast, u_hst_t * hst);
void pla_ast_cleanup(pla_ast_t * ast);
