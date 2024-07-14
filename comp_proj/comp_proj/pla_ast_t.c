#include "pch.h"
#include "ira_dt.h"
#include "ira_val.h"
#include "ira_inst.h"
#include "ira_func.h"
#include "ira_lo.h"
#include "ira_pec.h"
#include "ira_pec_ip.h"
#include "pla_cn.h"
#include "pla_expr.h"
#include "pla_stmt.h"
#include "pla_dclr.h"
#include "pla_tu.h"
#include "pla_pkg.h"
#include "pla_ast_t_s.h"

#define VSE_LO_FULL_NAME_DELIM L'.'

typedef pla_ast_t_tse_t tse_t;
typedef pla_ast_t_vse_t vse_t;

typedef struct pla_ast_t_tu {
	pla_tu_t * base;
	
	ul_hs_t * full_name;

	ul_hs_t ** refs;

	bool is_tlated;
} tu_t;

typedef struct pla_ast_t_ctx {
	pla_ast_t * ast;
	ira_pec_t * out;

	ul_hst_t * hst;
	ul_hsb_t hsb;

	size_t tus_size;
	tu_t * tus;
	size_t tus_cap;

	tse_t * tse;
	vse_t * vse;
} ctx_t;


static ul_hs_t * cat_hs_delim(ctx_t * ctx, ul_hs_t * str1, wchar_t delim, ul_hs_t * str2) {
	return ul_hsb_formatadd(&ctx->hsb, ctx->hst, L"%s%c%s", str1->str, delim, str2->str);
}


void pla_ast_t_report(pla_ast_t_ctx_t * ctx, const wchar_t * format, ...) {
	pla_ast_t_print_ts(ctx, stderr);

	va_list args;

	va_start(args, format);

	vfwprintf(stderr, format, args);

	va_end(args);

	fputwc(L'\n', stderr);
}
void pla_ast_t_report_pec_err(pla_ast_t_ctx_t * ctx) {
	pla_ast_t_report(ctx, L"pec function error");
}


ul_hsb_t * pla_ast_t_get_hsb(pla_ast_t_ctx_t * ctx) {
	return &ctx->hsb;
}
ul_hst_t * pla_ast_t_get_hst(pla_ast_t_ctx_t * ctx) {
	return ctx->hst;
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


static ul_hs_t * get_vse_raw_name(vse_t * vse) {
	switch (vse->type) {
		case PlaAstTVseNspc:
			return vse->nspc.name;
		default:
			ul_assert_unreachable();
	}

	return NULL;
}
void pla_ast_t_push_vse(pla_ast_t_ctx_t * ctx, pla_ast_t_vse_t * vse) {
	vse->prev = ctx->vse;

	ul_hs_t * vse_name = get_vse_raw_name(vse);

	ul_hs_t * prefix = vse->prev != NULL ? get_vse_raw_name(vse->prev) : NULL;

	if (prefix != NULL) {
		vse_name = cat_hs_delim(ctx, prefix, VSE_LO_FULL_NAME_DELIM, vse_name);
	}

	vse->name = vse_name;

	ctx->vse = vse;
}
void pla_ast_t_pop_vse(pla_ast_t_ctx_t * ctx) {
	vse_t * vse = ctx->vse;

	switch (vse->type) {
		case PlaAstTVseNspc:
			break;
		default:
			ul_assert_unreachable();
	}

	ctx->vse = vse->prev;
}
pla_ast_t_vse_t * pla_ast_t_get_vse(pla_ast_t_ctx_t * ctx) {
	return ctx->vse;
}


ul_hs_t * pla_ast_t_get_pds(pla_ast_t_ctx_t * ctx, pla_pds_t pds) {
	ul_assert(pds < PlaPds_Count);

	return ctx->ast->pds[pds];
}


void pla_ast_t_print_ts(pla_ast_t_ctx_t * ctx, FILE * file) {
	for (tse_t * tse = pla_ast_t_get_tse(ctx); tse != NULL; tse = tse->prev) {
		switch (tse->type) {
			case PlaAstTTseDclr:
			{
				pla_dclr_t * dclr = tse->dclr;

				const wchar_t * dclr_name;

				if (dclr->name != NULL) {
					dclr_name = dclr->name->str;
				}
				else {
					ul_assert(dclr->type == PlaDclrNspc);
					dclr_name = L"#root_nspc";
				}

				fwprintf(file, L"%12s %16s\n", pla_dclr_infos[dclr->type].type_str.str, dclr_name);
				break;
			}
			case PlaAstTTseStmt:
			{
				pla_stmt_t * stmt = tse->stmt;
				fwprintf(file, L"%12s \n", pla_stmt_infos[stmt->type].type_str.str);
				break;
			}
			case PlaAstTTseExpr:
			{
				pla_expr_t * expr = tse->expr;
				fwprintf(file, L"%12s \n", pla_expr_infos[expr->type].type_str.str);
				break;
			}
		}
	}
}


static bool calculate_expr_guard(pla_ast_t_ctx_t * ctx, pla_stmt_t * blk, ira_func_t ** func, ira_val_t ** out) {
	if (!pla_ast_t_s_translate(ctx, NULL, blk, func)) {
		return false;
	}

	if (!ira_pec_ip_interpret(ctx->out, *func, out)) {
		pla_ast_t_report(ctx, L"failed to calculate an expression\n");
		return false;
	}

	return true;
}
bool pla_ast_t_calculate_expr(pla_ast_t_ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	pla_stmt_t ret = { .type = PlaStmtRet, .ret.expr = expr };
	pla_stmt_t blk = { .type = PlaStmtBlk, .blk.body = &ret };

	ira_func_t * func = NULL;

	bool res = calculate_expr_guard(ctx, &blk, &func, out);

	ira_func_destroy(func);

	return res;
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


static bool translate_dclr(ctx_t * ctx, pla_dclr_t * dclr);

static ira_lo_t ** get_vse_lo_ins(ctx_t * ctx, ul_hs_t * name) {
	vse_t * vse = pla_ast_t_get_vse(ctx);

	if (vse == NULL) {
		ul_assert(name == NULL);

		return &ctx->out->root;
	}

	switch (vse->type) {
		case PlaAstTVseNspc:
		{
			ira_lo_nspc_node_t ** ins = &vse->nspc.lo->nspc.body;

			for (; *ins != NULL; ins = &(*ins)->next) {
				ira_lo_nspc_node_t * node = *ins;

				if (node->name == name) {
					break;
				}
			}

			if (*ins == NULL) {
				*ins = ira_lo_create_nspc_node(name);
			}

			return &(*ins)->lo;
		}
		default:
			ul_assert_unreachable();
	}
}

static ul_hs_t * make_vse_lo_hint_name(ctx_t * ctx, ul_hs_t * name) {
	ul_hs_t * hint_name = name;

	vse_t * vse = pla_ast_t_get_vse(ctx);

	ul_hs_t * vse_name = vse != NULL ? vse->name : NULL;

	if (vse_name != NULL) {
		hint_name = cat_hs_delim(ctx, vse_name, VSE_LO_FULL_NAME_DELIM, hint_name);
	}

	return hint_name;
}
static ira_lo_t * push_lo(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_type_t lo_type) {
	ul_hs_t * hint_name = make_vse_lo_hint_name(ctx, dclr->name);

	return ira_pec_push_unq_lo(ctx->out, lo_type, hint_name);
}

static bool init_lo_stct_var(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	*out = push_lo(ctx, dclr, IraLoVar);

	(*out)->var.qdt.dt = &ctx->out->dt_dt;
	(*out)->var.qdt.qual = ira_dt_qual_const;

	ira_dt_stct_tag_t * tag = ira_pec_get_dt_stct_tag(ctx->out);

	ira_dt_t * stct;

	if (!ira_pec_get_dt_stct(ctx->out, tag, ira_dt_qual_none, &stct)) {
		pla_ast_t_report_pec_err(ctx);
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->out, stct, &(*out)->var.val)) {
		pla_ast_t_report_pec_err(ctx);
		return false;
	}

	return true;
}

static bool translate_dclr_nspc_vse(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	for (pla_dclr_t * it = dclr->nspc.body; it != NULL; it = it->next) {
		if (!translate_dclr(ctx, it)) {
			return false;
		}
	}

	return true;
}
static bool translate_dclr_nspc(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	if (*out == NULL) {
		*out = push_lo(ctx, dclr, IraLoNspc);
	}
	else if ((*out)->type != IraLoNspc) {
		pla_ast_t_report(ctx, L"language object with [%s] name already exists", dclr->name->str);
		return false;
	}

	vse_t vse = { .type = PlaAstTVseNspc, .nspc = { .lo = *out, .name = dclr->name } };

	pla_ast_t_push_vse(ctx, &vse);

	bool res = translate_dclr_nspc_vse(ctx, dclr, out);

	pla_ast_t_pop_vse(ctx);

	return res;
}
static bool translate_dclr_func(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	if (*out != NULL) {
		pla_ast_t_report(ctx, L"language object with [%s] name already exists", dclr->name->str);
		return false;
	}

	*out = push_lo(ctx, dclr, IraLoFunc);

	ira_dt_t * func_dt = NULL;

	if (!pla_ast_t_calculate_expr_dt(ctx, dclr->func.dt_expr, &func_dt)) {
		return false;
	}

	if (func_dt->type != IraDtFunc) {
		pla_ast_t_report(ctx, L"function language object requires function data type");
		return false;
	}

	if (!pla_ast_t_s_translate(ctx, func_dt, dclr->func.block, &(*out)->func)) {
		return false;
	}

	return true;
}
static bool translate_dclr_impt(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	if (*out != NULL) {
		pla_ast_t_report(ctx, L"language object with [%s] name already exists", dclr->name->str);
		return false;
	}

	*out = push_lo(ctx, dclr, IraLoImpt);

	if (!pla_ast_t_calculate_expr_dt(ctx, dclr->impt.dt_expr, &(*out)->impt.dt)) {
		return false;
	}

	(*out)->impt.lib_name = dclr->impt.lib_name;
	(*out)->impt.sym_name = dclr->impt.sym_name;

	return true;
}
static bool translate_dclr_var_dt(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	if (*out != NULL) {
		pla_ast_t_report(ctx, L"language object with [%s] name already exists", dclr->name->str);
		return false;
	}

	*out = push_lo(ctx, dclr, IraLoVar);

	if (!pla_ast_t_calculate_expr_dt(ctx, dclr->var_dt.dt_expr, &(*out)->var.qdt.dt)) {
		return false;
	}

	(*out)->var.qdt.qual = dclr->var_dt.dt_qual;

	if (!ira_pec_is_dt_complete((*out)->var.qdt.dt)) {
		pla_ast_t_report(ctx, L"global variables must have only complete data types");
		return false;
	}

	if (!ira_pec_make_val_null(ctx->out, (*out)->var.qdt.dt, &(*out)->var.val)) {
		pla_ast_t_report_pec_err(ctx);
		return false;
	}

	return true;
}
static bool translate_dclr_var_val(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	if (*out != NULL) {
		pla_ast_t_report(ctx, L"language object with [%s] name already exists", dclr->name->str);
		return false;
	}

	*out = push_lo(ctx, dclr, IraLoVar);

	if (!pla_ast_t_calculate_expr(ctx, dclr->var_val.val_expr, &(*out)->var.val)) {
		return false;
	}

	(*out)->var.qdt.dt = (*out)->var.val->dt;
	(*out)->var.qdt.qual = dclr->var_val.dt_qual;

	return true;
}
static bool translate_dclr_stct(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	if (*out != NULL) {
		if ((*out)->type != IraLoVar) {
			pla_ast_t_report(ctx, L"language object with [%s] name already exists", dclr->name->str);
			return false;
		}

		if ((*out)->var.qdt.dt != &ctx->out->dt_dt
			|| (*out)->var.val == NULL
			|| (*out)->var.val->type != IraValImmDt
			|| (*out)->var.val->dt_val->type != IraDtStct) {
			pla_ast_t_report(ctx, L"invalid language object for struct [%s]", dclr->name->str);
			return false;
		}

		if ((*out)->var.val->dt_val->stct.tag->tpl != NULL) {
			pla_ast_t_report(ctx, L"struct [%s] already defined", dclr->name->str);
			return false;
		}
	}
	else {
		if (!init_lo_stct_var(ctx, dclr, out)) {
			return false;
		}
	}

	ira_dt_t * dt;

	if (!pla_ast_t_calculate_expr_dt(ctx, dclr->dt_stct.dt_stct_expr, &dt)) {
		return false;
	}

	if (dt->type != IraDtTpl) {
		pla_ast_t_report(ctx, L"invalid struct %[s] binding, expected tuple body expression", dclr->name->str);
		return false;
	}

	(*out)->var.val->dt_val->stct.tag->tpl = dt;

	return true;
}
static bool translate_dclr_stct_decl(ctx_t * ctx, pla_dclr_t * dclr, ira_lo_t ** out) {
	if (*out != NULL) {
		if ((*out)->type != IraLoVar) {
			pla_ast_t_report(ctx, L"language object with [%s] name already exists", dclr->name->str);
			return false;
		}
	}
	else {
		if (!init_lo_stct_var(ctx, dclr, out)) {
			return false;
		}
	}

	return true;
}
static bool translate_dclr_tse(ctx_t * ctx, pla_dclr_t * dclr) {
	ira_lo_t ** ins = get_vse_lo_ins(ctx, dclr->name);

	switch (dclr->type) {
		case PlaDclrNone:
			break;
		case PlaDclrNspc:
			if (!translate_dclr_nspc(ctx, dclr, ins)) {
				return false;
			}
			break;
		case PlaDclrFunc:
			if (!translate_dclr_func(ctx, dclr, ins)) {
				return false;
			}
			break;
		case PlaDclrImpt:
			if (!translate_dclr_impt(ctx, dclr, ins)) {
				return false;
			}
			break;
		case PlaDclrVarDt:
			if (!translate_dclr_var_dt(ctx, dclr, ins)) {
				return false;
			}
			break;
		case PlaDclrVarVal:
			if (!translate_dclr_var_val(ctx, dclr, ins)) {
				return false;
			}
			break;
		case PlaDclrDtStct:
			if (!translate_dclr_stct(ctx, dclr, ins)) {
				return false;
			}
			break;
		case PlaDclrDtStctDecl:
			if (!translate_dclr_stct_decl(ctx, dclr, ins)) {
				return false;
			}
			break;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool translate_dclr(ctx_t * ctx, pla_dclr_t * dclr) {
	tse_t tse = { .type = PlaAstTTseDclr, .dclr = dclr };

	pla_ast_t_push_tse(ctx, &tse);

	bool res = translate_dclr_tse(ctx, dclr);

	pla_ast_t_pop_tse(ctx);

	return res;
}

static ul_hs_t * convert_cn_to_hs(ctx_t * ctx, pla_cn_t * cn, wchar_t delim) {
	ul_hs_t * res = cn->name;

	cn = cn->sub_name;

	while (cn != NULL) {
		res = cat_hs_delim(ctx, res, delim, cn->name);

		cn = cn->sub_name;
	}

	return res;
}

static bool translate_tu(ctx_t * ctx, tu_t * tu);

static bool push_tu(ctx_t * ctx, pla_tu_t * base, ul_hs_t * parent_pkg_full_name) {
	if (ctx->tus_size + 1 > ctx->tus_cap) {
		ul_arr_grow(&ctx->tus_cap, &ctx->tus, sizeof(*ctx->tus), 1);
	}

	tu_t * tu = &ctx->tus[ctx->tus_size++];

	*tu = (tu_t){ .base = base, .full_name = base->name };

	if (parent_pkg_full_name != NULL) {
		tu->full_name = cat_hs_delim(ctx, parent_pkg_full_name, PLA_AST_NAME_DELIM, tu->full_name);
	}

	tu->refs = malloc(base->refs_size * sizeof(*tu->refs));

	ul_assert(tu->refs != NULL);

	ul_hs_t ** ref = tu->refs;

	for (pla_tu_ref_t * base_ref = base->refs, *base_ref_end = base_ref + base->refs_size; base_ref != base_ref_end; ++base_ref, ++ref) {
		if (base_ref->cn == NULL) {
			return false;
		}
		
		ul_hs_t * base_ref_hs = convert_cn_to_hs(ctx, base_ref->cn, PLA_AST_NAME_DELIM);

		if (base_ref->is_rel) {
			base_ref_hs = cat_hs_delim(ctx, base_ref_hs, PLA_AST_NAME_DELIM, parent_pkg_full_name);
		}

		*ref = base_ref_hs;
	}

	return true;
}
static bool form_tus_list(ctx_t * ctx, pla_pkg_t * pkg, ul_hs_t * parent_pkg_full_name) {
	ul_hs_t * pkg_full_name = pkg->name;

	if (parent_pkg_full_name != NULL) {
		ul_assert(pkg_full_name != NULL);

		pkg_full_name = cat_hs_delim(ctx, parent_pkg_full_name, PLA_AST_NAME_DELIM, pkg_full_name);
	}

	for (pla_tu_t * tu = pkg->tu; tu != NULL; tu = tu->next) {
		if (!push_tu(ctx, tu, pkg_full_name)) {
			return false;
		}
	}

	for (pla_pkg_t * sub_pkg = pkg->sub_pkg; sub_pkg != NULL; sub_pkg = sub_pkg->next) {
		if (!form_tus_list(ctx, sub_pkg, pkg_full_name)) {
			return false;
		}
	}

	return true;
}

static tu_t * find_tu_ref(ctx_t * ctx, ul_hs_t * ref) {
	for (tu_t * tu = ctx->tus, *tu_end = tu + ctx->tus_size; tu != tu_end; ++tu) {
		if (tu->full_name == ref) {
			return tu;
		}
	}

	return NULL;
}
static bool translate_tu_refs(ctx_t * ctx, tu_t * tu) {
	for (ul_hs_t ** ref = tu->refs, **ref_end = ref + tu->base->refs_size; ref != ref_end; ++ref) {
		tu_t * ref_tu = find_tu_ref(ctx, *ref);

		if (ref_tu == NULL) {
			return false;
		}

		if (ref_tu->is_tlated) {
			continue;
		}

		if (!translate_tu(ctx, ref_tu)) {
			return false;
		}
	}

	return true;
}
static bool translate_tu(ctx_t * ctx, tu_t * tu) {
	pla_tu_t * base = tu->base;

	if (base->root->type != PlaDclrNspc
		|| base->root->name != NULL) {
		return false;
	}

	if (!translate_tu_refs(ctx, tu)) {
		return false;
	}

	if (!translate_dclr(ctx, base->root)) {
		return false;
	}

	tu->is_tlated = true;

	return true;
}

static bool translate_tus(ctx_t * ctx) {
	if (!form_tus_list(ctx, ctx->ast->root, NULL)) {
		return false;
	}

	for (tu_t * tu = ctx->tus, *tu_end = tu + ctx->tus_size; tu != tu_end; ++tu) {
		if (tu->is_tlated) {
			continue;
		}

		if (!translate_tu(ctx, tu)) {
			return false;
		}
	}

	return true;
}

static bool translate_core(ctx_t * ctx) {
	ctx->hst = ctx->ast->hst;

	ul_hsb_init(&ctx->hsb);

	if (!ira_pec_init(ctx->out, ctx->hst)) {
		return false;
	}

	if (!translate_tus(ctx)) {
		return false;
	}

	ul_assert(ctx->tse == NULL && ctx->vse == NULL);

	return true;
}
bool pla_ast_t_translate(pla_ast_t * ast, ira_pec_t * out) {
	ctx_t ctx = { .ast = ast, .out = out };

	bool res = translate_core(&ctx);

	for (tu_t * tu = ctx.tus, *tu_end = tu + ctx.tus_size; tu != tu_end; ++tu) {
		free(tu->refs);
	}

	free(ctx.tus);

	ul_hsb_cleanup(&ctx.hsb);

	return res;
}
