#pragma once
#include "pla_ast.h"

enum pla_ast_t_tse_type {
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
	PlaAstTVseNspc,
	PlaAstTVse_Count
};
struct pla_ast_t_vse {
	pla_ast_t_vse_type_t type;
	
	ul_hs_t * name;
	pla_ast_t_vse_t * prev;
	
	union {
		struct {
			ira_lo_t * lo;
			ul_hs_t * name;
		} nspc;
	};
};

void pla_ast_t_report(pla_ast_t_ctx_t * ctx, const wchar_t * format, ...);
void pla_ast_t_report_pec_err(pla_ast_t_ctx_t * ctx);

ul_hsb_t * pla_ast_t_get_hsb(pla_ast_t_ctx_t * ctx);
ul_hst_t * pla_ast_t_get_hst(pla_ast_t_ctx_t * ctx);
ira_pec_t * pla_ast_t_get_pec(pla_ast_t_ctx_t * ctx);

void pla_ast_t_push_tse(pla_ast_t_ctx_t * ctx, pla_ast_t_tse_t * tse);
void pla_ast_t_pop_tse(pla_ast_t_ctx_t * ctx);
pla_ast_t_tse_t * pla_ast_t_get_tse(pla_ast_t_ctx_t * ctx);

void pla_ast_t_push_vse(pla_ast_t_ctx_t * ctx, pla_ast_t_vse_t * vse);
void pla_ast_t_pop_vse(pla_ast_t_ctx_t * ctx);
pla_ast_t_vse_t * pla_ast_t_get_vse(pla_ast_t_ctx_t * ctx);

ul_hs_t * pla_ast_t_get_pds(pla_ast_t_ctx_t * ctx, pla_pds_t pds);

void pla_ast_t_print_ts(pla_ast_t_ctx_t * ctx, FILE * file);

bool pla_ast_t_calculate_expr(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out);
bool pla_ast_t_calculate_expr_dt(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_dt_t ** out);

bool pla_ast_t_translate(pla_ast_t * ast, ira_pec_t * out);
