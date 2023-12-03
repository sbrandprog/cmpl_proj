#include "pch.h"
#include "pla_ast_t.h"
#include "pla_ast_t_s.h"
#include "pla_expr.h"
#include "pla_stmt.h"
#include "pla_dclr.h"
#include "ira_dt.h"
#include "ira_val.h"
#include "ira_func.h"
#include "ira_lo.h"
#include "ira_pec.h"
#include "ira_pec_ip.h"
#include "u_misc.h"

#define UNQ_SUFFIX L"#"
#define LO_FULL_NAME_DELIM L'.'

typedef pla_ast_t_tse_t tse_t;
typedef pla_ast_t_vse_t vse_t;

typedef struct pla_ast_t_ctx {
	pla_ast_t * ast;

	ira_pec_t * out;

	u_hst_t * hst;

	tse_t * tse;
	vse_t * vse;

	wchar_t * buf;
	size_t buf_cap;
} ctx_t;

void pla_ast_t_report(pla_ast_t_ctx_t * ctx, const wchar_t * format, ...) {
	pla_ast_t_print_ts(ctx, stderr);

	va_list args;

	va_start(args, format);

	vfwprintf(stderr, format, args);

	va_end(args);

	fputwc(L'\n', stderr);
}

ira_pec_t * pla_ast_t_get_pec(pla_ast_t_ctx_t * ctx) {
	return ctx->out;
}

void pla_ast_t_push_tse(pla_ast_t_ctx_t * ctx, pla_ast_t_tse_t * tse) {
	tse->prev = ctx->tse;

	ctx->tse = tse;
}
void pla_ast_t_pop_tse(pla_ast_t_ctx_t * ctx) {
	ctx->tse = ctx->tse->prev;
}
pla_ast_t_tse_t * pla_ast_t_get_tse(pla_ast_t_ctx_t * ctx) {
	return ctx->tse;
}

void pla_ast_t_push_vse(pla_ast_t_ctx_t * ctx, pla_ast_t_vse_t * vse) {
	vse->prev = ctx->vse;

	ctx->vse = vse;
}
void pla_ast_t_pop_vse(pla_ast_t_ctx_t * ctx) {
	vse_t * vse = ctx->vse;

	switch (vse->type) {
		case PlaAstTVseNspc:
			break;
		default:
			u_assert_switch(vse->type);
	}

	ctx->vse = vse->prev;
}
pla_ast_t_vse_t * pla_ast_t_get_vse(pla_ast_t_ctx_t * ctx) {
	return ctx->vse;
}

static u_hs_t * cat_hs_delim(ctx_t * ctx, u_hs_t * str1, wchar_t delim, u_hs_t * str2) {
	size_t max_size = str1->size + 1 + str2->size;
	size_t max_size_null = max_size + 1;

	u_assert(max_size_null < INT32_MAX);

	if (max_size_null > ctx->buf_cap) {
		u_grow_arr(&ctx->buf_cap, &ctx->buf, sizeof(*ctx->buf), max_size_null - ctx->buf_cap);
	}

	int result = swprintf_s(ctx->buf, ctx->buf_cap, L"%s%c%s", str1->str, delim, str2->str);

	u_assert(result >= 0 && result < (int)max_size_null);

	return u_hst_hashadd(ctx->hst, (size_t)result, ctx->buf);
}

u_hs_t * pla_ast_t_get_pds(pla_ast_t_ctx_t * ctx, pla_pds_t pds) {
	u_assert(pds < PlaPds_Count);

	return ctx->ast->pds[pds];
}
u_hs_t * pla_ast_t_get_unq_var_name(pla_ast_t_ctx_t * ctx, u_hs_t * var_name, size_t unq_index) {
	size_t max_size = var_name->size + (_countof(UNQ_SUFFIX) - 1) + U_SIZE_T_NUM_SIZE;
	size_t max_size_null = max_size + 1;

	u_assert(max_size_null < INT32_MAX);

	if (max_size_null > ctx->buf_cap) {
		u_grow_arr(&ctx->buf_cap, &ctx->buf, sizeof(*ctx->buf), max_size_null - ctx->buf_cap);
	}

	int result = swprintf_s(ctx->buf, ctx->buf_cap, L"%s%s%zi", var_name->str, UNQ_SUFFIX, unq_index);

	u_assert(result >= 0 && result < (int)max_size);

	return u_hst_hashadd(ctx->hst, (size_t)result, ctx->buf);
}

void pla_ast_t_print_ts(pla_ast_t_ctx_t * ctx, FILE * file) {
	for (tse_t * tse = ctx->tse; tse != NULL; tse = tse->prev) {
		switch (tse->type) {
			case PlaAstTTseNone:
				fwprintf(file, L"#None\n");
				break;
			case PlaAstTTseDclr:
			{
				pla_dclr_t * dclr = tse->dclr;

				const wchar_t * dclr_name;

				if (dclr->name != NULL) {
					dclr_name = dclr->name->str;
				}
				else {
					u_assert(dclr->type == PlaDclrNspc);
					dclr_name = L"#root_nspc";
				}

				fwprintf(file, L"%10s %16s\n", pla_dclr_infos[dclr->type].type_str.str, dclr_name);
				break;
			}
			case PlaAstTTseStmt:
			{
				pla_stmt_t * stmt = tse->stmt;
				fwprintf(file, L"%10s \n", pla_stmt_infos[stmt->type].type_str.str);
				break;
			}
			case PlaAstTTseExpr:
			{
				pla_expr_t * expr = tse->expr;
				fwprintf(file, L"%10s \n", pla_expr_infos[expr->type].type_str.str);
				break;
			}
		}
	}
}

bool pla_ast_t_calculate_expr(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	pla_stmt_t ret = { .type = PlaStmtRet, .ret.expr = expr };
	pla_stmt_t blk = { .type = PlaStmtBlk, .block.body = &ret };

	ira_func_t * func = NULL;

	if (!pla_ast_t_s_translate(ctx, NULL, &blk, &func)) {
		ira_func_destroy(func);
		return false;
	}

	bool result = ira_pec_ip_interpret(ctx->out, func, out);

	if (!result) {
		pla_ast_t_report(ctx, L"failed to calculate an expression\n");
	}

	ira_func_destroy(func);

	return result;
}
bool pla_ast_t_calculate_expr_dt(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_dt_t ** out) {
	bool result = false;
	ira_val_t * val = NULL;

	if (pla_ast_t_calculate_expr(ctx, expr, &val)) {
		switch (val->type) {
			case IraValImmDt:
				*out = val->dt_val;
				result = true;
				break;
		}
	}

	ira_val_destroy(val);

	return result;
}

static bool translate_dclr(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out);

static bool translate_dclr_nspc_vse(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	ira_lo_t ** ins = &(*out)->nspc.body;

	for (pla_dclr_t * it = dclr->nspc.body; it != NULL; it = it->next) {
		if (!translate_dclr(ctx, it, ins)) {
			return false;
		}

		ins = &(*ins)->next;
		u_assert(*ins == NULL);
	}

	return true;
}
static bool translate_dclr_nspc(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	*out = ira_lo_create(IraLoNspc, dclr->name);

	vse_t vse = { .type = PlaAstTVseNspc, .nspc.lo = *out };

	pla_ast_t_push_vse(ctx, &vse);

	bool result = translate_dclr_nspc_vse(ctx, dclr, out);

	pla_ast_t_pop_vse(ctx);

	return result;
}
static bool translate_dclr_func(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	*out = ira_lo_create(IraLoFunc, dclr->name);

	ira_lo_t * lo = *out;

	ira_dt_t * func_dt = NULL;

	if (!pla_ast_t_calculate_expr_dt(ctx, dclr->func.dt_expr, &func_dt)) {
		return false;
	}

	if (func_dt->type != IraDtFunc) {
		pla_ast_t_report(ctx, L"function language object requires function data type");
		return false;
	}

	if (!pla_ast_t_s_translate(ctx, func_dt, dclr->func.block, &lo->func)) {
		return false;
	}

	return true;
}
static bool translate_dclr_impt(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	*out = ira_lo_create(IraLoImpt, dclr->name);

	ira_lo_t * lo = *out;

	if (!pla_ast_t_calculate_expr_dt(ctx, dclr->impt.dt_expr, &lo->impt.dt)) {
		return false;
	}

	if (!ira_dt_is_impt_comp(lo->impt.dt)) {
		pla_ast_t_report(ctx, L"import language object requires import compatible data type");
		return false;
	}

	lo->impt.lib_name = dclr->impt.lib_name;
	lo->impt.sym_name = dclr->impt.sym_name;

	return true;
}
static bool translate_dclr_tse(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	switch (dclr->type) {
		case PlaDclrNone:
			break;
		case PlaDclrNspc:
			if (!translate_dclr_nspc(ctx, dclr, out)) {
				return false;
			}
			break;
		case PlaDclrFunc:
			if (!translate_dclr_func(ctx, dclr, out)) {
				return false;
			}
			break;
		case PlaDclrImpt:
			if (!translate_dclr_impt(ctx, dclr, out)) {
				return false;
			}
			break;
		default:
			u_assert_switch(dclr->type);
	}

	return true;
}
static bool translate_dclr(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	tse_t tse = { .type = PlaAstTTseDclr, .dclr = dclr };

	pla_ast_t_push_tse(ctx, &tse);

	bool result = translate_dclr_tse(ctx, dclr, out);

	pla_ast_t_pop_tse(ctx);

	return result;
}

static void generate_full_names_depth(ctx_t * ctx, ira_lo_t * lo) {
	if (lo->type != IraLoNspc) {
		return;
	}

	for (ira_lo_t * body = lo->nspc.body; body != NULL; body = body->next) {
		body->full_name = cat_hs_delim(ctx, lo->full_name, LO_FULL_NAME_DELIM, body->name);

		generate_full_names_depth(ctx, body);
	}
}
static void generate_full_names(ctx_t * ctx) {
	for (ira_lo_t * body = ctx->out->root->nspc.body; body != NULL; body = body->next) {
		body->full_name = body->name;

		generate_full_names_depth(ctx, body);
	}
}

static bool translate_core(ctx_t * ctx) {
	if (ctx->ast->root->type != PlaDclrNspc
		|| ctx->ast->root->name != NULL) {
		return false;
	}

	ctx->hst = ctx->ast->hst;

	ira_pec_init(ctx->out, ctx->hst);

	if (!translate_dclr(ctx, ctx->ast->root, &ctx->out->root)) {
		return false;
	}

	generate_full_names(ctx);

	u_assert(ctx->tse == NULL && ctx->vse == NULL);

	return true;
}
bool pla_ast_t_translate(pla_ast_t * ast, ira_pec_t * out) {
	ctx_t ctx = { .ast = ast, .out = out };

	bool result = translate_core(&ctx);

	free(ctx.buf);

	return result;
}
