#include "pch.h"
#include "pla_ast_t.h"
#include "pla_ast_t_s.h"
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

typedef struct vb vb_t;
struct vb {
	vb_t * prev;
	ira_lo_t * lo;
};

typedef struct pla_ast_t_ctx {
	pla_ast_t * ast;

	ira_pec_t * out;

	u_hst_t * hst;

	vb_t * vb;

	wchar_t * buf;
	size_t buf_cap;
} ctx_t;

ira_pec_t * pla_ast_t_get_pec(pla_ast_t_ctx_t * ctx) {
	return ctx->out;
}

static void push_vb(ctx_t * ctx, vb_t * vb) {
	vb->prev = ctx->vb;

	ctx->vb = vb;
}
static void pop_vb(ctx_t * ctx) {
	ctx->vb = ctx->vb->prev;
}

ira_lo_t * pla_ast_t_resolve_name(pla_ast_t_ctx_t * ctx, u_hs_t * name) {
	for (vb_t * vb = ctx->vb; vb != NULL; vb = vb->prev) {
		for (ira_lo_t * it = vb->lo->body; it != NULL; it = it->next) {
			if (it->name == name) {
				return it;
			}
		}
	}

	return NULL;
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

bool pla_ast_t_calculate_expr(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	pla_stmt_t ret = { .type = PlaStmtRet, .ret.expr = expr };
	pla_stmt_t blk = { .type = PlaStmtBlk, .block.body = &ret };

	ira_func_t * func = NULL;

	if (!pla_ast_t_s_translate(ctx, NULL, &blk, &func)) {
		ira_func_destroy(func);
		return false;
	}

	bool result = ira_pec_ip_interpret(ctx->out, func, out);

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

static bool translate_lo(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out);

static bool translate_lo_nspc_body(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t * lo) {
	ira_lo_t ** ins = &lo->body;

	for (pla_dclr_t * it = dclr->nspc.body; it != NULL; it = it->next) {
		if (!translate_lo(ctx, it, ins)) {
			return false;
		}

		while (*ins != NULL) {
			ins = &(*ins)->next;
		}
	}

	return true;
}
static bool translate_lo_nspc(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	*out = ira_lo_create(IraLoNspc, dclr->name);

	vb_t vb = { .lo = *out };

	push_vb(ctx, &vb);

	bool result = translate_lo_nspc_body(ctx, dclr, *out);

	pop_vb(ctx);

	return result;
}
static bool translate_lo_func(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	*out = ira_lo_create(IraLoFunc, dclr->name);

	ira_lo_t * lo = *out;

	ira_dt_t * func_dt = NULL;

	if (!pla_ast_t_calculate_expr_dt(ctx, dclr->func.dt_expr, &func_dt)
		|| func_dt->type != IraDtFunc) {
		return false;
	}

	if (!pla_ast_t_s_translate(ctx, func_dt, dclr->func.block, &lo->func)) {
		return false;
	}

	return true;
}
static bool translate_lo_impt(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	*out = ira_lo_create(IraLoImpt, dclr->name);

	ira_lo_t * lo = *out;

	if (!pla_ast_t_calculate_expr_dt(ctx, dclr->impt.dt_expr, &lo->impt.dt)
		|| !ira_dt_is_impt_dt_comp(lo->impt.dt)) {
		return false;
	}

	lo->impt.lib_name = dclr->impt.lib_name;
	lo->impt.sym_name = dclr->impt.sym_name;

	return true;
}

static bool translate_lo(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	switch (dclr->type) {
		case PlaDclrNone:
			break;
		case PlaDclrNspc:
			if (!translate_lo_nspc(ctx, dclr, out)) {
				return false;
			}
			break;
		case PlaDclrFunc:
			if (!translate_lo_func(ctx, dclr, out)) {
				return false;
			}
			break;
		case PlaDclrImpt:
			if (!translate_lo_impt(ctx, dclr, out)) {
				return false;
			}
			break;
		default:
			u_assert_switch(dclr->type);
	}

	return true;
}

static void generate_full_names_body(ctx_t * ctx, ira_lo_t * lo) {
	for (ira_lo_t * body = lo->body; body != NULL; body = body->next) {
		body->full_name = cat_hs_delim(ctx, lo->full_name, LO_FULL_NAME_DELIM, body->name);

		generate_full_names_body(ctx, body);
	}
}
static void generate_full_names(ctx_t * ctx) {
	for (ira_lo_t * body = ctx->out->root->body; body != NULL; body = body->next) {
		body->full_name = body->name;

		generate_full_names_body(ctx, body);
	}
}

static bool translate_core(ctx_t * ctx) {
	if (ctx->ast->root->type != PlaDclrNspc
		|| ctx->ast->root->name != NULL) {
		return false;
	}

	ctx->hst = ctx->ast->hst;

	ira_pec_init(ctx->out, ctx->hst);

	if (!translate_lo(ctx, ctx->ast->root, &ctx->out->root)) {
		return false;
	}

	generate_full_names(ctx);

	return true;
}
bool pla_ast_t_translate(pla_ast_t * ast, ira_pec_t * out) {
	ctx_t ctx = { .ast = ast, .out = out };

	bool result = translate_core(&ctx);

	free(ctx.buf);

	return result;
}
