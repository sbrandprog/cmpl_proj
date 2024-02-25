#pragma once
#include "pla_ast.h"
#include "u_hsb.h"

enum pla_ast_t_optr_type {
	PlaAstTOptrNone,
	PlaAstTOptrUnrInstBool,
	PlaAstTOptrUnrInstInt,
	PlaAstTOptrBinInstInt,
	PlaAstTOptrBinInstIntBool,
	PlaAstTOptrBinInstPtrBool,
	PlaAstTOptr_Count
};
struct pla_ast_t_optr {
	pla_ast_t_optr_type_t type;
	pla_ast_t_optr_t * next;

	union {
		struct {
			ira_inst_type_t inst_type;
		} unr_inst_bool;
		struct {
			ira_inst_type_t inst_type;
		} unr_inst_int;
		struct {
			ira_inst_type_t inst_type;
		} bin_inst_int;
		struct {
			ira_inst_type_t inst_type;
			ira_int_cmp_t int_cmp;
		} bin_inst_int_bool;
		struct {
			ira_inst_type_t inst_type;
			ira_int_cmp_t int_cmp;
		} bin_inst_ptr_bool;
	};
};

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
void pla_ast_t_report_pec_err(pla_ast_t_ctx_t * ctx);

u_hsb_t * pla_ast_t_get_hsb(pla_ast_t_ctx_t * ctx);
u_hst_t * pla_ast_t_get_hst(pla_ast_t_ctx_t * ctx);
ira_pec_t * pla_ast_t_get_pec(pla_ast_t_ctx_t * ctx);

pla_ast_t_optr_t * pla_ast_t_get_optr_chain(pla_ast_t_ctx_t * ctx, pla_expr_type_t expr_type);
bool pla_ast_t_get_optr_dt(pla_ast_t_ctx_t * ctx, pla_ast_t_optr_t * optr, ira_dt_t * first, ira_dt_t * second, ira_dt_t ** out);

void pla_ast_t_push_tse(pla_ast_t_ctx_t * ctx, pla_ast_t_tse_t * tse);
void pla_ast_t_pop_tse(pla_ast_t_ctx_t * ctx);
pla_ast_t_tse_t * pla_ast_t_get_tse(pla_ast_t_ctx_t * ctx);

void pla_ast_t_push_vse(pla_ast_t_ctx_t * ctx, pla_ast_t_vse_t * vse);
void pla_ast_t_pop_vse(pla_ast_t_ctx_t * ctx);
pla_ast_t_vse_t * pla_ast_t_get_vse(pla_ast_t_ctx_t * ctx);

u_hs_t * pla_ast_t_get_pds(pla_ast_t_ctx_t * ctx, pla_pds_t pds);

void pla_ast_t_print_ts(pla_ast_t_ctx_t * ctx, FILE * file);

bool pla_ast_t_calculate_expr(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out);
bool pla_ast_t_calculate_expr_dt(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_dt_t ** out);

bool pla_ast_t_translate(pla_ast_t * ast, ira_pec_t * out);
