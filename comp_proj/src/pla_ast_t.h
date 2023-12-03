#pragma once
#include "pla_ast.h"

enum pla_ast_t_tse_type {
	PlaAstTTseNone,
	PlaAstTTseDclr,
	PlaAstTTseStmt,
	PlaAstTTseExpr,
	PlaAstTTse_Count
};
struct pla_ast_t_tse {
	pla_ast_t_tse_type_t type;
	pla_ast_t_tse_t * prev;
	union {
		pla_dclr_t * dclr;
		pla_stmt_t * stmt;
		pla_expr_t * expr;
	};
};

enum pla_ast_t_vse_type {
	PlaAstTVseNone,
	PlaAstTVseNspc,
	PlaAstTVse_Count
};
struct pla_ast_t_vse {
	pla_ast_t_vse_type_t type;
	pla_ast_t_vse_t * prev;
	union {
		struct {
			ira_lo_t * lo;
		} nspc;
	};
};

void pla_ast_t_report(pla_ast_t_ctx_t * ctx, const wchar_t * format, ...);

ira_pec_t * pla_ast_t_get_pec(pla_ast_t_ctx_t * ctx);

void pla_ast_t_push_tse(pla_ast_t_ctx_t * ctx, pla_ast_t_tse_t * tse);
void pla_ast_t_pop_tse(pla_ast_t_ctx_t * ctx);
pla_ast_t_tse_t * pla_ast_t_get_tse(pla_ast_t_ctx_t * ctx);

void pla_ast_t_push_vse(pla_ast_t_ctx_t * ctx, pla_ast_t_vse_t * vse);
void pla_ast_t_pop_vse(pla_ast_t_ctx_t * ctx);
pla_ast_t_vse_t * pla_ast_t_get_vse(pla_ast_t_ctx_t * ctx);

u_hs_t * pla_ast_t_get_pds(pla_ast_t_ctx_t * ctx, pla_pds_t pds);
u_hs_t * pla_ast_t_get_unq_var_name(pla_ast_t_ctx_t * ctx, u_hs_t * var_name, size_t var_index);

void pla_ast_t_print_ts(pla_ast_t_ctx_t * ctx, FILE * file);

bool pla_ast_t_calculate_expr(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out);
bool pla_ast_t_calculate_expr_dt(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_dt_t ** out);

bool pla_ast_t_translate(pla_ast_t * ast, ira_pec_t * out);
