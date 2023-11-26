#pragma once
#include "pla_ast.h"

struct pla_ast_t_vb {
	pla_ast_t_vb_t * prev;
	ira_lo_t * lo;
};

ira_pec_t * pla_ast_t_get_pec(pla_ast_t_ctx_t * ctx);

ira_lo_t * pla_ast_t_resolve_name(pla_ast_t_ctx_t * ctx, u_hs_t * name);

u_hs_t * pla_ast_t_get_pds(pla_ast_t_ctx_t * ctx, pla_pds_t pds);
u_hs_t * pla_ast_t_get_unq_var_name(pla_ast_t_ctx_t * ctx, u_hs_t * var_name, size_t var_index);

bool pla_ast_t_calculate_expr(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out);
bool pla_ast_t_calculate_expr_dt(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_dt_t ** out);

bool pla_ast_t_translate(pla_ast_t * ast, ira_pec_t * out);
