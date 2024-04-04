#include "pch.h"
#include "pla_ast_t_s.h"
#include "pla_cn.h"
#include "pla_expr.h"
#include "pla_stmt.h"
#include "ira_val.h"
#include "ira_inst.h"
#include "ira_func.h"
#include "ira_lo.h"
#include "ira_pec.h"

#define UNQ_VAR_NAME_SUFFIX L"#"

typedef pla_ast_t_tse_t tse_t;
typedef pla_ast_t_vse_t vse_t;

typedef struct pla_ast_t_s_var var_t;
struct pla_ast_t_s_var {
	var_t * next;
	ul_hs_t * orig_name;
	ul_hs_t * inst_name;
	ira_dt_qdt_t qdt;
};
typedef struct pla_ast_t_s_vvb vvb_t;
struct pla_ast_t_s_vvb {
	vvb_t * prev;
	var_t * var;
};
typedef struct pla_ast_t_s_cfb cfb_t;
struct pla_ast_t_s_cfb {
	cfb_t * prev;
	ul_hs_t * name;
	ul_hs_t * exit_label;
	ul_hs_t * cnt_label;
};

typedef struct pla_ast_t_s_expr expr_t;
typedef enum pla_ast_t_s_expr_val_type {
	ExprValImmVar,
	ExprValImmDeref,
	ExprValVar,
	ExprValDeref,
	ExprVal_Count
} expr_val_type_t;
typedef union pla_ast_t_s_expr_val {
	var_t * var;
} expr_val_t;
typedef union pla_ast_t_s_expr_opd {
	bool boolean;
	ira_int_type_t int_type;
	ul_hs_t * hs;
	pla_cn_t * cn;
	expr_t * expr;
} expr_opd_t;
struct pla_ast_t_s_expr {
	pla_expr_t * base;
	union {
		expr_opd_t opds[PLA_EXPR_OPDS_SIZE];
		struct {
			expr_opd_t opd0, opd1, opd2;
		};
	};

	ira_dt_qdt_t val_qdt;

	expr_val_type_t val_type;
	expr_val_t val;

	union {
		struct {
			ira_val_t * val;
		} load_val;
		struct {
			size_t elems_size;
		} dt_stct;
		struct {
			size_t args_size;
		} dt_func;
		struct {
			enum expr_ident_type {
				ExprIdentVar,
				ExprIdentLo,
				ExprIdent_Count
			} type;
			union {
				ira_lo_t * lo;
				var_t * var;
			};
		} ident;
		struct {
			size_t args_size;
		} call;
		struct {
			ira_dt_t * res_dt;
		} mmbr_acc;
		pla_ast_t_optr_t * optr;
	};
};

typedef struct pla_ast_t_s_ctx {
	pla_ast_t_ctx_t * t_ctx;

	ira_dt_t * func_dt;

	pla_stmt_t * stmt;

	ira_func_t ** out;

	ul_hsb_t * hsb;
	ul_hst_t * hst;
	ira_pec_t * pec;

	ira_func_t * func;
	ira_dt_t * func_ret;

	vvb_t * vvb;
	size_t unq_var_index;
	cfb_t * cfb;
	size_t unq_label_index;
} ctx_t;

typedef bool val_proc_t(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out);


static const bool expr_val_is_left_val[ExprVal_Count] = {
	[ExprValImmVar] = false,
	[ExprValImmDeref] = false,
	[ExprValVar] = true,
	[ExprValDeref] = true
};

static const ul_ros_t label_base_logic_and = UL_ROS_MAKE(L"logic_and");
static const ul_ros_t label_base_logic_or = UL_ROS_MAKE(L"logic_or");
static const ul_ros_t label_base_cond = UL_ROS_MAKE(L"cond");
static const ul_ros_t label_base_pre_loop = UL_ROS_MAKE(L"pre_loop");
static const ul_ros_t label_base_post_loop = UL_ROS_MAKE(L"post_loop");

static const ul_ros_t label_suffix_end = UL_ROS_MAKE(L"end");
static const ul_ros_t label_suffix_tc_end = UL_ROS_MAKE(L"tc_end");
static const ul_ros_t label_suffix_fc_end = UL_ROS_MAKE(L"fc_end");
static const ul_ros_t label_suffix_body = UL_ROS_MAKE(L"body");
static const ul_ros_t label_suffix_cnt = UL_ROS_MAKE(L"cnt");
static const ul_ros_t label_suffix_exit = UL_ROS_MAKE(L"exit");

static ul_hs_t * get_unq_var_name(ctx_t * ctx, ul_hs_t * name) {
	return ul_hsb_formatadd(ctx->hsb, ctx->hst, L"%s%s%zi", name->str, UNQ_VAR_NAME_SUFFIX, ctx->unq_var_index++);
}
static ul_hs_t * get_unq_label_name(ctx_t * ctx, const ul_ros_t * base, size_t index, const ul_ros_t * suffix) {
	return ul_hsb_formatadd(ctx->hsb, ctx->hst, L"%s%zi%s", base->str, index, suffix->str);
}

static void push_vvb(ctx_t * ctx, vvb_t * vvb) {
	vvb->prev = ctx->vvb;

	ctx->vvb = vvb;
}
static void pop_vvb(ctx_t * ctx) {
	vvb_t * vvb = ctx->vvb;

	for (var_t * var = vvb->var; var != NULL; ) {
		var_t * next = var->next;

		free(var);

		var = next;
	}

	ctx->vvb = vvb->prev;
}
static void push_cfb(ctx_t * ctx, cfb_t * cfb) {
	cfb->prev = ctx->cfb;

	ctx->cfb = cfb;
}
static void pop_cfb(ctx_t * ctx) {
	cfb_t * cfb = ctx->cfb;

	ctx->cfb = cfb->prev;
}

static expr_t * create_expr(ctx_t * ctx, pla_expr_t * base) {
	expr_t * expr = malloc(sizeof(*expr));

	ul_raise_check_mem_alloc(expr);

	*expr = (expr_t){ .base = base };

	return expr;
}
static void destroy_expr(ctx_t * ctx, expr_t * expr) {
	if (expr == NULL) {
		return;
	}

	pla_expr_t * base = expr->base;

	const pla_expr_info_t * info = &pla_expr_infos[base->type];

	for (size_t opd = 0; opd < PLA_EXPR_OPDS_SIZE; ++opd) {
		switch (info->opds[opd]) {
			case PlaExprOpdNone:
			case PlaExprOpdBoolean:
			case PlaExprOpdIntType:
			case PlaExprOpdHs:
			case PlaExprOpdCn:
				break;
			case PlaExprOpdExpr:
				destroy_expr(ctx, expr->opds[opd].expr);
				break;
			case PlaExprOpdExprList1:
				for (expr_t * it = expr->opds[opd].expr; it != NULL; ) {
					expr_t * next = it->opds[1].expr;

					destroy_expr(ctx, it);

					it = next;
				}
				break;
			case PlaExprOpdExprListLink:
				break;
			default:
				ul_raise_unreachable();
		}
	}

	switch (base->type) {
		case PlaExprDtVoid:
		case PlaExprDtBool:
		case PlaExprDtInt:
		case PlaExprValVoid:
		case PlaExprValBool:
		case PlaExprChStr:
		case PlaExprNumStr:
		case PlaExprNullofDt:
			ira_val_destroy(expr->load_val.val);
			expr->load_val.val = NULL;
			break;
	}

	free(expr);
}

static var_t * create_var(ctx_t * ctx, ul_hs_t * name, ira_dt_t * dt, ira_dt_qual_t dt_qual, bool unq_name) {
	var_t * new_var = malloc(sizeof(*new_var));

	ul_raise_check_mem_alloc(new_var);

	*new_var = (var_t){ .orig_name = name, .inst_name = name, .qdt = { .dt = dt, .qual = dt_qual } };

	if (unq_name) {
		new_var->inst_name = get_unq_var_name(ctx, name);
	}

	return new_var;
}
static var_t * define_var(ctx_t * ctx, ul_hs_t * name, ira_dt_t * dt, ira_dt_qual_t dt_qual, bool unq_name) {
	var_t ** ins = &ctx->vvb->var;

	while (*ins != NULL) {
		var_t * var = *ins;

		if (name == var->orig_name) {
			return NULL;
		}

		ins = &(*ins)->next;
	}

	var_t * new_var = create_var(ctx, name, dt, dt_qual, unq_name);

	*ins = new_var;

	return new_var;
}
static var_t * get_imm_var(ctx_t * ctx, ira_dt_t * dt) {
	var_t ** ins = &ctx->vvb->var;

	while (*ins != NULL) {
		ins = &(*ins)->next;
	}

	var_t * new_var = create_var(ctx, pla_ast_t_get_pds(ctx->t_ctx, PlaPdsImmVarName), dt, ira_dt_qual_none, true);

	*ins = new_var;

	return new_var;
}

static void push_def_var_inst(ctx_t * ctx, var_t * var) {
	ira_inst_t def_var = { .type = IraInstDefVar, .opd0.hs = var->inst_name, .opd1.dt = var->qdt.dt, .opd2.dt_qual = var->qdt.qual };

	ira_func_push_inst(ctx->func, &def_var);
}
static void push_def_label_inst(ctx_t * ctx, ul_hs_t * name) {
	ira_inst_t def_label = { .type = IraInstDefLabel, .opd0.hs = name };

	ira_func_push_inst(ctx->func, &def_label);
}
static void push_inst_imm_var0(ctx_t * ctx, ira_inst_t * inst, ira_dt_t * dt, var_t ** out) {
	var_t * out_var = get_imm_var(ctx, dt);

	push_def_var_inst(ctx, out_var);

	inst->opd0.hs = out_var->inst_name;

	ira_func_push_inst(ctx->func, inst);

	*out = out_var;
}
static void push_inst_imm_var0_expr(ctx_t * ctx, expr_t * expr, ira_inst_t * inst) {
	expr->val_type = ExprValImmVar;
	push_inst_imm_var0(ctx, inst, expr->val_qdt.dt, &expr->val.var);
}

static bool translate_expr0(ctx_t * ctx, pla_expr_t * base, expr_t ** out);

static bool get_cs_ascii(ctx_t * ctx, ul_hs_t * str, ira_val_t ** out) {
	ira_val_t * val = ira_val_create(IraValImmArr, ctx->pec->dt_spcl.ascii_str);

	ira_int_type_t elem_type = val->dt->arr.body->int_type;

	ul_raise_assert(elem_type == IraIntU8);

	val->arr_val.size = str->size + 1;

	val->arr_val.data = malloc(val->arr_val.size * sizeof(*val->arr_val.data));

	ul_raise_check_mem_alloc(val->arr_val.data);

	memset(val->arr_val.data, 0, val->arr_val.size * sizeof(*val->arr_val.data));

	ira_val_t ** ins = val->arr_val.data;
	for (wchar_t * ch = str->str, *ch_end = ch + str->size; ch != ch_end; ++ch) {
		if ((*ch & ~0x7F) != 0) {
			pla_ast_t_report(ctx->t_ctx, L"non-ascii character [%c] in ascii string:\n%s", *ch, str->str);
			ira_val_destroy(val);
			return false;
		}

		if (!ira_pec_make_val_imm_int(ctx->pec, elem_type, (ira_int_t) {
			.ui8 = (uint8_t)*ch
		}, ins++)) {
			pla_ast_t_report_pec_err(ctx->t_ctx);
			return false;
		}
	}

	if (!ira_pec_make_val_imm_int(ctx->pec, elem_type, (ira_int_t) {
		.ui8 = 0
	}, ins++)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	size_t final_size = ins - val->arr_val.data;

	ul_raise_assert(final_size <= val->arr_val.size);

	val->arr_val.size = final_size;

	*out = val;

	return true;
}
static bool get_cs_wide(ctx_t * ctx, ul_hs_t * str, ira_val_t ** out) {
	ira_val_t * val = ira_val_create(IraValImmArr, ctx->pec->dt_spcl.wide_str);

	ira_int_type_t elem_type = val->dt->arr.body->int_type;

	ul_raise_assert(elem_type == IraIntU16);

	val->arr_val.size = str->size + 1;

	val->arr_val.data = malloc(val->arr_val.size * sizeof(*val->arr_val.data));

	ul_raise_check_mem_alloc(val->arr_val.data);

	memset(val->arr_val.data, 0, val->arr_val.size * sizeof(*val->arr_val.data));

	ira_val_t ** ins = val->arr_val.data;
	for (wchar_t * ch = str->str, *ch_end = ch + str->size; ch != ch_end; ++ch) {
		if (!ira_pec_make_val_imm_int(ctx->pec, elem_type, (ira_int_t) {
			.ui16 = (uint16_t)*ch
		}, ins++)) {
			pla_ast_t_report_pec_err(ctx->t_ctx);
			return false;
		}
	}

	if (!ira_pec_make_val_imm_int(ctx->pec, elem_type, (ira_int_t) {
		.ui16 = 0
	}, ins++)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	size_t final_size = ins - val->arr_val.data;

	ul_raise_assert(final_size <= val->arr_val.size);

	val->arr_val.size = final_size;

	*out = val;

	return true;
}

static bool is_int_ns(ctx_t * ctx, ul_hs_t * tag, ira_int_type_t * out) {
	if (tag == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsU8)) {
		*out = IraIntU8;
	}
	else if (tag == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsS8)) {
		*out = IraIntS8;
	}
	else if (tag == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsU16)) {
		*out = IraIntU16;
	}
	else if (tag == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsS16)) {
		*out = IraIntS16;
	}
	else if (tag == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsU32)) {
		*out = IraIntU32;
	}
	else if (tag == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsS32)) {
		*out = IraIntS32;
	}
	else if (tag == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsU64)) {
		*out = IraIntU64;
	}
	else if (tag == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsS64)) {
		*out = IraIntS64;
	}
	else {
		return false;
	}

	return true;
}
static bool get_ns_radix(wchar_t ch, uint64_t * out) {
	if (L'A' <= ch && ch <= L'Z') {
		ch += (L'a' - L'A');
	}

	switch (ch) {
		case L'b':
			*out = 2;
			return true;
		case L'o':
			*out = 8;
			return true;
		case L'd':
			*out = 10;
			return true;
		case L'x':
			*out = 16;
			return true;
	}

	return false;
}
static bool get_ns_digit(wchar_t ch, uint64_t * out) {
	if (L'0' <= ch && ch <= L'9') {
		*out = (uint64_t)(ch - L'0');
		return true;
	}
	else if (L'A' <= ch && ch <= L'Z') {
		*out = (uint64_t)(ch - L'A') + 10ull;
		return true;
	}
	else if (L'a' <= ch && ch <= L'z') {
		*out = (uint64_t)(ch - L'a') + 10ull;
		return true;
	}

	return false;
}
static bool get_ns_int(ctx_t * ctx, ul_hs_t * str, ira_int_type_t int_type, ira_val_t ** out) {
	const wchar_t * cur = str->str, * cur_end = cur + str->size;

	uint64_t var = 0, radix = 10;

	if (*cur == L'0' && cur + 1 != cur_end && get_ns_radix(*(cur + 1), &radix)) {
		if (cur + 2 == cur_end) {
			pla_ast_t_report(ctx->t_ctx, L"invalid integer string (radix form):\n%s", str->str);
			return false;
		}

		cur += 2;
	}

	const uint64_t pre_ovf_val = UINT64_MAX / radix, pre_ovf_digit = UINT64_MAX % radix;

	uint64_t digit;
	while (cur != cur_end) {
		if (*cur == L'_') {
			++cur;
			continue;
		}

		if (!get_ns_digit(*cur, &digit) || digit > radix) {
			pla_ast_t_report(ctx->t_ctx, L"digit [%llu] is greater than radix [%llu] in integer string:\n%s", digit, radix, str->str);
			return false;
		}

		if (var > pre_ovf_val || var == pre_ovf_val && digit > pre_ovf_digit) {
			pla_ast_t_report(ctx->t_ctx, L"integer string is exceeding precision limit of parsing unit:\n%s", str->str);
			return false;
		}

		var = var * radix + digit;

		++cur;
	}

	uint64_t lim = ira_int_infos[int_type].max;

	if (var > lim) {
		pla_ast_t_report(ctx->t_ctx, L"integer string is exceeding precision limit of data type [%i]:\n%s", int_type, str->str);
		return false;
	}

	if (!ira_pec_make_val_imm_int(ctx->pec, int_type, (ira_int_t) {
		.ui64 = var
	}, out)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	return true;
}

static bool get_dt_void_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	if (!ira_pec_make_val_imm_dt(ctx->pec, &ctx->pec->dt_void, out)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	return true;
}
static bool get_dt_bool_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	if (!ira_pec_make_val_imm_dt(ctx->pec, &ctx->pec->dt_bool, out)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	return true;
}
static bool get_dt_int_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	ira_int_type_t int_type = expr->opd0.int_type;

	if (int_type >= IraInt_Count) {
		pla_ast_t_report(ctx->t_ctx, L"invalid int_type for int dt:%i", int_type);
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->pec, &ctx->pec->dt_ints[expr->opd0.int_type], out)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	return true;
}
static bool get_void_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	if (!ira_pec_make_val_imm_void(ctx->pec, out)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	return true;
}
static bool get_bool_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	if (!ira_pec_make_val_imm_bool(ctx->pec, expr->opd0.boolean, out)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	return true;
}
static bool get_cs_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	ul_hs_t * data = expr->opd0.hs;
	ul_hs_t * tag = expr->opd1.hs;

	if (tag == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsAsciiStrTag)) {
		if (!get_cs_ascii(ctx, data, out)) {
			return false;
		}
	}
	else if (tag == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsWideStrTag)) {
		if (!get_cs_wide(ctx, data, out)) {
			return false;
		}
	}
	else {
		pla_ast_t_report(ctx->t_ctx, L"unknown type-tag [%s] for character string:\n%s", tag->str, data->str);
		return false;
	}

	return true;
}
static bool get_ns_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	ul_hs_t * data = expr->opd0.hs;
	ul_hs_t * tag = expr->opd1.hs;

	ira_int_type_t int_type;

	if (is_int_ns(ctx, tag, &int_type)) {
		if (!get_ns_int(ctx, data, int_type, out)) {
			return false;
		}
	}
	else {
		pla_ast_t_report(ctx->t_ctx, L"unknown type-tag [%s] for number string:\n%s", tag->str, data->str);
		return false;
	}

	return true;
}
static bool get_null_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	ira_dt_t * dt;

	if (!pla_ast_t_calculate_expr_dt(ctx->t_ctx, expr->opd0.expr, &dt)) {
		return false;
	}

	if (!ira_pec_make_val_null(ctx->pec, dt, out)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	return true;
}

static ira_lo_t * find_cn_lo(ctx_t * ctx, ira_lo_t * nspc, pla_cn_t * cn) {
	for (ira_lo_t * lo = nspc->nspc.body; lo != NULL; lo = lo->next) {
		if (lo->name == cn->name) {
			switch (lo->type) {
				case IraLoNone:
					break;
				case IraLoNspc:
					if (cn->sub_name != NULL) {
						return find_cn_lo(ctx, lo, cn->sub_name);
					}
					break;
				case IraLoFunc:
				case IraLoImpt:
				case IraLoVar:
				case IraLoDtStct:
				case IraLoRoVal:
					if (cn->sub_name == NULL) {
						return lo;
					}
					break;
				default:
					ul_raise_unreachable();
			}
		}
	}

	return NULL;
}
static bool process_ident_lo(ctx_t * ctx, expr_t * expr, ira_lo_t * lo) {
	switch (lo->type) {
		case IraLoNspc:
			return false;
		case IraLoFunc:
			if (!ira_pec_get_dt_ptr(ctx->pec, lo->func->dt, ira_dt_qual_none, &expr->val_qdt.dt)) {
				pla_ast_t_report_pec_err(ctx->t_ctx);
				return false;
			}
			break;
		case IraLoImpt:
			if (!ira_pec_get_dt_ptr(ctx->pec, lo->impt.dt, ira_dt_qual_none, &expr->val_qdt.dt)) {
				pla_ast_t_report_pec_err(ctx->t_ctx);
				return false;
			}
			break;
		case IraLoVar:
			expr->val_qdt = lo->var.qdt;
			break;
		case IraLoDtStct:
			expr->val_qdt.dt = &ctx->pec->dt_dt;
			break;
		case IraLoRoVal:
			expr->val_qdt.dt = lo->ro_val.val->dt;
			break;
		default:
			ul_raise_unreachable();
	}

	expr->ident.type = ExprIdentLo;
	expr->ident.lo = lo;

	return true;
}
static bool get_stct_mmbr_dt(ctx_t * ctx, ira_dt_t * dt, ul_hs_t * mmbr, ira_dt_t ** out) {
	ira_dt_sd_t * sd = dt->stct.lo->dt_stct.sd;

	if (sd == NULL) {
		return false;
	}

	ira_dt_ndt_t * elem = sd->elems, * elem_end = elem + sd->elems_size;

	for (; elem != elem_end; ++elem) {
		if (mmbr == elem->name) {
			if (!ira_pec_apply_qual(ctx->pec, elem->dt, dt->stct.qual, out)) {
				pla_ast_t_report_pec_err(ctx->t_ctx);
				return false;
			}
			
			return true;
		}
	}

	return false;
}
static bool get_mmbr_acc_dt(ctx_t * ctx, ira_dt_t * opd_dt, ul_hs_t * mmbr, ira_dt_t ** out) {
	switch (opd_dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
		case IraDtVec:
		case IraDtPtr:
			return false;
		case IraDtStct:
			if (!get_stct_mmbr_dt(ctx, opd_dt, mmbr, out)) {
				return false;
			}
			break;
		case IraDtArr:
			if (!get_stct_mmbr_dt(ctx, opd_dt->arr.assoc_stct, mmbr, out)) {
				return false;
			}
			break;
		case IraDtFunc:
			return false;
		default:
			ul_raise_unreachable();
	}

	return true;
}
static bool find_optr(ctx_t * ctx, expr_t * expr, ira_dt_t * first, ira_dt_t * second) {
	expr->optr = NULL;

	for (pla_ast_t_optr_t * optr = pla_ast_t_get_optr_chain(ctx->t_ctx, expr->base->type); optr != NULL; optr = optr->next) {
		if (pla_ast_t_get_optr_dt(ctx->t_ctx, optr, first, second, &expr->val_qdt.dt)) {
			expr->optr = optr;
			break;
		}
	}

	if (expr->optr == NULL) {
		if (second == NULL) {
			pla_ast_t_report(ctx->t_ctx, L"could not find an operator for [%s] expression with [%s] operand", pla_expr_infos[expr->base->type].type_str.str, ira_dt_infos[first->type].type_str.str);
		}
		else {
			pla_ast_t_report(ctx->t_ctx, L"could not find an operator for [%s] expression with [%s], [%s] operands", pla_expr_infos[expr->base->type].type_str.str, ira_dt_infos[first->type].type_str.str, ira_dt_infos[second->type].type_str.str);
		}

		return false;
	}

	ul_raise_assert(expr->val_qdt.dt != NULL);

	return true;
}

static bool translate_expr0_opds(ctx_t * ctx, expr_t * expr) {
	pla_expr_t * base = expr->base;

	const pla_expr_info_t * info = &pla_expr_infos[base->type];

	for (size_t opd = 0; opd < PLA_EXPR_OPDS_SIZE; ++opd) {
		switch (info->opds[opd]) {
			case PlaExprOpdNone:
				break;
			case PlaExprOpdBoolean:
				expr->opds[opd].boolean = base->opds[opd].boolean;
				break;
			case PlaExprOpdIntType:
				expr->opds[opd].int_type = base->opds[opd].int_type;
				break;
			case PlaExprOpdHs:
				expr->opds[opd].hs = base->opds[opd].hs;
				break;
			case PlaExprOpdCn:
				expr->opds[opd].cn = base->opds[opd].cn;
				break;
			case PlaExprOpdExpr:
				if (!translate_expr0(ctx, base->opds[opd].expr, &expr->opds[opd].expr)) {
					return false;
				}
				break;
			case PlaExprOpdExprList1:
			{
				expr_t ** it_ins = &expr->opds[opd].expr;

				for (pla_expr_t * it = base->opds[opd].expr; it != NULL; it = it->opds[1].expr, it_ins = &(*it_ins)->opds[1].expr) {
					if (!translate_expr0(ctx, it, it_ins)) {
						return false;
					}
				}
				break;
			}
			case PlaExprOpdExprListLink:
				break;
			default:
				ul_raise_unreachable();
		}
	}

	return true;
}
static bool translate_expr0_blank(ctx_t * ctx, expr_t * expr) {
	return true;
}
static bool translate_expr0_invalid(ctx_t * ctx, expr_t * expr) {
	pla_ast_t_report(ctx->t_ctx, L"this expression type is untranslatable");
	return false;
}
static bool translate_expr0_load_val(ctx_t * ctx, expr_t * expr) {
	pla_expr_t * base = expr->base;
	
	val_proc_t * proc;

	switch (base->type) {
		case PlaExprDtVoid:
			proc = get_dt_void_val_proc;
			break;
		case PlaExprDtBool:
			proc = get_dt_bool_val_proc;
			break;
		case PlaExprDtInt:
			proc = get_dt_int_val_proc;
			break;
		case PlaExprValVoid:
			proc = get_void_val_proc;
			break;
		case PlaExprValBool:
			proc = get_bool_val_proc;
			break;
		case PlaExprChStr:
			proc = get_cs_val_proc;
			break;
		case PlaExprNumStr:
			proc = get_ns_val_proc;
			break;
		case PlaExprNullofDt:
			proc = get_null_val_proc;
			break;
		default:
			ul_raise_unreachable();
	}

	if (!proc(ctx, base, &expr->load_val.val)) {
		return false;
	}

	expr->val_qdt.dt = expr->load_val.val->dt;

	return true;
}
static bool translate_expr0_dt_ptr(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	if (expr->opd0.expr->val_qdt.dt != dt_dt) {
		pla_ast_t_report(ctx->t_ctx, L"dt_ptr requires 'dt' data type for opd[0]");
		return false;
	}

	expr->val_qdt.dt = dt_dt;

	return true;
}
static bool translate_expr0_dt_stct(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	size_t elems_size = 0;

	for (expr_t * elem_expr = expr->opd1.expr; elem_expr != NULL; elem_expr = elem_expr->opd1.expr, ++elems_size) {
		if (elem_expr->opd0.expr->val_qdt.dt != dt_dt) {
			pla_ast_t_report(ctx->t_ctx, L"dt_stct requires 'dt' data type for [%zi]th argument", (size_t)(elems_size));
			return false;
		}
	}

	expr->dt_stct.elems_size = elems_size;

	expr->val_qdt.dt = dt_dt;

	return true;
}
static bool translate_expr0_dt_arr(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	if (expr->opd0.expr->val_qdt.dt != dt_dt) {
		pla_ast_t_report(ctx->t_ctx, L"dt_arr requires 'dt' data type for opd[0]");
		return false;
	}

	ira_dt_t * dim_opd_dt = expr->opd1.expr->val_qdt.dt;

	if (dim_opd_dt->type != IraDtVoid && dim_opd_dt->type != IraDtInt) {
		pla_ast_t_report(ctx->t_ctx, L"dt_arr requires 'void' or 'int' data type for opd[1]");
		return false;
	}

	expr->val_qdt.dt = dt_dt;

	return true;
}
static bool translate_expr0_dt_func(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	size_t args_size = 0;

	for (expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr, ++args_size) {
		if (arg_expr->opd0.expr->val_qdt.dt != dt_dt) {
			pla_ast_t_report(ctx->t_ctx, L"dt_func requires 'dt' data type for [%zi]th argument", (size_t)(args_size));
			return false;
		}
	}

	expr->dt_func.args_size = args_size;

	if (expr->opd0.expr->val_qdt.dt != dt_dt) {
		pla_ast_t_report(ctx->t_ctx, L"dt_func requires 'dt' data type for return value");
		return false;
	}

	expr->val_qdt.dt = dt_dt;

	return true;
}
static bool translate_expr0_dt_const(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;
	
	if (expr->opd0.expr->val_qdt.dt != dt_dt) {
		pla_ast_t_report(ctx->t_ctx, L"make_dt_const requires 'dt' data type for first operand");
		return false;
	}

	expr->val_qdt.dt = dt_dt;

	return true;
}
static bool translate_expr0_ident(ctx_t * ctx, expr_t * expr) {
	pla_cn_t * cn = expr->opd0.cn;

	if (cn->sub_name == NULL) {
		for (vvb_t * vvb = ctx->vvb; vvb != NULL; vvb = vvb->prev) {
			for (var_t * var = vvb->var; var != NULL; var = var->next) {
				if (var->orig_name == cn->name) {
					expr->val_qdt = var->qdt;

					expr->ident.type = ExprIdentVar;
					expr->ident.var = var;
					return true;
				}
			}
		}
	}

	for (vse_t * vse = pla_ast_t_get_vse(ctx->t_ctx); vse != NULL; vse = vse->prev) {
		switch (vse->type) {
			case PlaAstTVseNspc:
			{
				ira_lo_t * lo = find_cn_lo(ctx, vse->nspc, cn);

				if (lo != NULL && process_ident_lo(ctx, expr, lo)) {
					return true;
				}

				break;
			}
			default:
				ul_raise_unreachable();
		}
	}

	pla_ast_t_report(ctx->t_ctx, L"failed to find an object for identifier [%s]", cn->name->str);
	return false;
}
static bool translate_expr0_call_func_ptr(ctx_t * ctx, expr_t * expr, ira_dt_t * callee_dt) {
	ira_dt_t * func_dt = callee_dt->ptr.body;

	size_t args_size = 0;

	ira_dt_ndt_t * arg_dt = func_dt->func.args, * arg_dt_end = arg_dt + func_dt->func.args_size;
	expr_t * arg_expr = expr->opd1.expr;
	for (; arg_expr != NULL && arg_dt != arg_dt_end; arg_expr = arg_expr->opd1.expr, ++arg_dt, ++args_size) {
		if (!ira_dt_is_equivalent(arg_expr->opd0.expr->val_qdt.dt, arg_dt->dt)) {
			pla_ast_t_report(ctx->t_ctx, L"data type of [%zi]th call argument does not match data type of function argument", (size_t)(args_size));
			return false;
		}
	}

	if (args_size != func_dt->func.args_size) {
		pla_ast_t_report(ctx->t_ctx, L"amount of call arguments [%zi] does not match amount of function arguments [%zi]", args_size, func_dt->func.args_size);
		return false;
	}

	expr->call.args_size = args_size;

	expr->val_qdt.dt = func_dt->func.ret;

	return true;
}
static bool translate_expr0_call(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * callee_dt = expr->opd0.expr->val_qdt.dt;

	if (callee_dt->type == IraDtPtr && callee_dt->ptr.body->type == IraDtFunc) {
		if (!translate_expr0_call_func_ptr(ctx, expr, callee_dt)) {
			return false;
		}
	}
	else {
		pla_ast_t_report(ctx->t_ctx, L"unknown callee type");
		return false;
	}

	return true;
}
static bool translate_expr0_subscr(ctx_t * ctx, expr_t * expr) {
	expr_t * opd1 = expr->opd1.expr;
	
	if (opd1->val_qdt.dt->type != IraDtInt) {
		pla_ast_t_report(ctx->t_ctx, L"opd[1] must have an integer data type");
		return false;
	}

	expr_t * opd0 = expr->opd0.expr;
	ira_dt_t * opd0_dt = opd0->val_qdt.dt;

	ira_dt_t * res_dt;
	ira_dt_qual_t res_dt_qual;

	switch (opd0_dt->type) {
		case IraDtVec:
			res_dt = opd0_dt->vec.body;
			res_dt_qual = opd0_dt->vec.qual;
			break;
		case IraDtPtr:
			res_dt = opd0_dt->ptr.body;
			res_dt_qual = ira_dt_qual_none;
			break;
		case IraDtArr:
			res_dt = opd0_dt->arr.body;
			res_dt_qual = opd0_dt->arr.qual;
			break;
		default:
			pla_ast_t_report(ctx->t_ctx, L"opd[0] must have a (vector | pointer | array) data type");
			return false;
	}

	if (!ira_pec_apply_qual(ctx->pec, res_dt, res_dt_qual, &res_dt)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	expr->val_qdt.dt = res_dt;
	expr->val_qdt.qual = opd0->val_qdt.qual;

	return true;
}
static bool translate_expr0_mmbr_acc(ctx_t * ctx, expr_t * expr) {
	expr_t * opd = expr->opd0.expr;

	ira_dt_t * opd_dt = opd->val_qdt.dt;
	ul_hs_t * mmbr = expr->opd1.hs;

	if (!get_mmbr_acc_dt(ctx, opd_dt, mmbr, &expr->mmbr_acc.res_dt)) {
		pla_ast_t_report(ctx->t_ctx, L"operand of [%s] data type does not support member access or [%s] member", ira_dt_infos[opd_dt->type].type_str.str, mmbr->str);
		return false;
	}

	expr->val_qdt.dt = expr->mmbr_acc.res_dt;
	expr->val_qdt.qual = opd->val_qdt.qual;

	return true;
}
static bool translate_expr0_deref(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * ptr_dt = expr->opd0.expr->val_qdt.dt;

	if (ptr_dt->type != IraDtPtr) {
		pla_ast_t_report(ctx->t_ctx, L"deref: invalid operand [%s]", ira_dt_infos[ptr_dt->type].type_str.str);
		return false;
	}

	expr->val_qdt.dt = ptr_dt->ptr.body;
	expr->val_qdt.qual = ptr_dt->ptr.qual;

	return true;
}
static bool translate_expr0_addr_of(ctx_t * ctx, expr_t * expr) {
	expr_t * opd = expr->opd0.expr;

	if (!ira_pec_get_dt_ptr(ctx->pec, opd->val_qdt.dt, opd->val_qdt.qual, &expr->val_qdt.dt)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	return true;
}
static bool translate_expr0_cast(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * from = expr->opd0.expr->val_qdt.dt;
	ira_dt_t * to;

	if (expr->opd1.expr->val_qdt.dt != &ctx->pec->dt_dt
		|| !pla_ast_t_calculate_expr_dt(ctx->t_ctx, expr->opd1.expr->base, &to)) {
		pla_ast_t_report(ctx->t_ctx, L"'cast-to' operand requires a valid 'dt' data type");
		return false;
	}

	if (!ira_dt_is_castable(from, to)) {
		pla_ast_t_report(ctx->t_ctx, L"cannot cast operand of [%s] data type to a [%s] data type", ira_dt_infos[from->type].type_str.str, ira_dt_infos[to->type].type_str.str);
		return false;
	}

	expr->val_qdt.dt = to;

	return true;
}
static bool translate_expr0_unr(ctx_t * ctx, expr_t * expr) {
	if (!find_optr(ctx, expr, expr->opd0.expr->val_qdt.dt, NULL)) {
		return false;
	}

	return true;
}
static bool translate_expr0_bin(ctx_t * ctx, expr_t * expr) {
	if (!find_optr(ctx, expr, expr->opd0.expr->val_qdt.dt, expr->opd1.expr->val_qdt.dt)) {
		return false;
	}

	return true;
}
static bool translate_expr0_logic_optr(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_bool = &ctx->pec->dt_bool;

	if (expr->opd0.expr->val_qdt.dt != dt_bool) {
		pla_ast_t_report(ctx->t_ctx, L"logic_and: opd[0] should have a boolean data type");
		return false;
	}

	if (expr->opd1.expr->val_qdt.dt != dt_bool) {
		pla_ast_t_report(ctx->t_ctx, L"logic_and: opd[1] should have a boolean data type");
		return false;
	}

	expr->val_qdt.dt = dt_bool;

	return true;
}
static bool translate_expr0_asgn(ctx_t * ctx, expr_t * expr) {
	ira_dt_qdt_t * opd0_qdt = &expr->opd0.expr->val_qdt;
	ira_dt_t * opd1_dt = expr->opd1.expr->val_qdt.dt;

	if (opd0_qdt->qual.const_q == true) {
		pla_ast_t_report(ctx->t_ctx, L"assignment operator requires left-side operand to have a non-constant data type");
		return false;
	}

	if (!ira_dt_is_equivalent(opd0_qdt->dt, opd1_dt)) {
		pla_ast_t_report(ctx->t_ctx, L"assignment operator requires operands to be of equivalent data types");
		return false;
	}

	expr->val_qdt = *opd0_qdt;

	return true;
}
static bool translate_expr0_tse(ctx_t * ctx, pla_expr_t * base, expr_t ** out) {
	*out = create_expr(ctx, base);

	expr_t * expr = *out;

	if (!translate_expr0_opds(ctx, expr)) {
		return false;
	}

	typedef bool translate_expr0_proc_t(ctx_t * ctx, expr_t * expr);

	static translate_expr0_proc_t * const translate_expr0_procs[PlaExpr_Count] = {
		[PlaExprNone] = translate_expr0_invalid,
		[PlaExprDtVoid] = translate_expr0_load_val,
		[PlaExprDtBool] = translate_expr0_load_val,
		[PlaExprDtInt] = translate_expr0_load_val,
		[PlaExprDtPtr] = translate_expr0_dt_ptr,
		[PlaExprDtStct] = translate_expr0_dt_stct,
		[PlaExprDtStctElem] = translate_expr0_blank,
		[PlaExprDtArr] = translate_expr0_dt_arr,
		[PlaExprDtFunc] = translate_expr0_dt_func,
		[PlaExprDtFuncArg] = translate_expr0_blank,
		[PlaExprDtConst] = translate_expr0_dt_const,
		[PlaExprValVoid] = translate_expr0_load_val,
		[PlaExprValBool] = translate_expr0_load_val,
		[PlaExprChStr] = translate_expr0_load_val,
		[PlaExprNumStr] = translate_expr0_load_val,
		[PlaExprNullofDt] = translate_expr0_load_val,
		[PlaExprIdent] = translate_expr0_ident,
		[PlaExprCall] = translate_expr0_call,
		[PlaExprCallArg] = translate_expr0_blank,
		[PlaExprSubscr] = translate_expr0_subscr,
		[PlaExprMmbrAcc] = translate_expr0_mmbr_acc,
		[PlaExprDeref] = translate_expr0_deref,
		[PlaExprAddrOf] = translate_expr0_addr_of,
		[PlaExprCast] = translate_expr0_cast,
		[PlaExprLogicNeg] = translate_expr0_unr,
		[PlaExprBitNeg] = translate_expr0_unr,
		[PlaExprArithNeg] = translate_expr0_unr,
		[PlaExprMul] = translate_expr0_bin,
		[PlaExprDiv] = translate_expr0_bin,
		[PlaExprMod] = translate_expr0_bin,
		[PlaExprAdd] = translate_expr0_bin,
		[PlaExprSub] = translate_expr0_bin,
		[PlaExprLeShift] = translate_expr0_bin,
		[PlaExprRiShift] = translate_expr0_bin,
		[PlaExprLess] = translate_expr0_bin,
		[PlaExprLessEq] = translate_expr0_bin,
		[PlaExprGrtr] = translate_expr0_bin,
		[PlaExprGrtrEq] = translate_expr0_bin,
		[PlaExprEq] = translate_expr0_bin,
		[PlaExprNeq] = translate_expr0_bin,
		[PlaExprBitAnd] = translate_expr0_bin,
		[PlaExprBitXor] = translate_expr0_bin,
		[PlaExprBitOr] = translate_expr0_bin,
		[PlaExprLogicAnd] = translate_expr0_logic_optr,
		[PlaExprLogicOr] = translate_expr0_logic_optr,
		[PlaExprAsgn] = translate_expr0_asgn,
	};

	translate_expr0_proc_t * proc = translate_expr0_procs[base->type];

	ul_raise_assert(proc != NULL);

	if (!proc(ctx, expr)) {
		return false;
	}

	return true;
}
static bool translate_expr0(ctx_t * ctx, pla_expr_t * base, expr_t ** out) {
	tse_t tse = { .type = PlaAstTTseExpr, .expr = base };

	pla_ast_t_push_tse(ctx->t_ctx, &tse);

	bool res;
	
	__try {
		res = translate_expr0_tse(ctx, base, out);
	}
	__finally {
		pla_ast_t_pop_tse(ctx->t_ctx);
	}

	return res;
}

static bool translate_expr1(ctx_t * ctx, expr_t * expr);

static bool translate_expr1_imm_var(ctx_t * ctx, expr_t * expr, var_t ** out) {
	if (!translate_expr1(ctx, expr)) {
		return false;
	}

	switch (expr->val_type) {
		case ExprValImmVar:
		case ExprValVar:
			*out = expr->val.var;
			break;
		case ExprValImmDeref:
		case ExprValDeref:
		{
			ira_inst_t read_ptr = { .type = IraInstReadPtr, .opd1.hs = expr->val.var->inst_name };

			push_inst_imm_var0(ctx, &read_ptr, expr->val_qdt.dt, out);
			break;
		}
		default:
			ul_raise_unreachable();
	}

	return true;
}
static bool translate_expr1_var_ptr(ctx_t * ctx, expr_t * expr, var_t ** out) {
	if (!translate_expr1(ctx, expr)) {
		return false;
	}

	switch (expr->val_type) {
		case ExprValImmVar:
		case ExprValVar:
		{
			ira_dt_t * ptr_dt;

			if (!ira_pec_get_dt_ptr(ctx->pec, expr->val.var->qdt.dt, expr->val.var->qdt.qual, &ptr_dt)) {
				pla_ast_t_report_pec_err(ctx->t_ctx);
				return false;
			}

			ira_inst_t addr_of = { .type = IraInstAddrOf, .opd1.hs = expr->val.var->inst_name };

			push_inst_imm_var0(ctx, &addr_of, ptr_dt, out);

			break;
		}
		case ExprValImmDeref:
		case ExprValDeref:
			*out = expr->val.var;
			break;
		default:
			ul_raise_unreachable();
	}

	return true;
}

static void set_expr_ptr_val(ctx_t * ctx, expr_t * expr, expr_val_type_t val_type, var_t * ptr_var) {
	switch (val_type) {
		case ExprValImmVar:
		case ExprValImmDeref:
			expr->val_type = ExprValImmDeref;
			expr->val.var = ptr_var;
			break;
		case ExprValVar:
		case ExprValDeref:
			expr->val_type = ExprValDeref;
			expr->val.var = ptr_var;
			break;
		default:
			ul_raise_unreachable();
	}
}

static bool translate_expr1_load_val(ctx_t * ctx, expr_t * expr) {
	ira_inst_t load_val = { .type = IraInstLoadVal, .opd1.val = expr->load_val.val };

	push_inst_imm_var0_expr(ctx, expr, &load_val);

	expr->load_val.val = NULL;

	return true;
}
static bool translate_expr1_dt_ptr(ctx_t * ctx, expr_t * expr) {
	var_t * opd_var;

	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var)) {
		return false;
	}

	ira_inst_t make_dt_ptr = { .type = IraInstMakeDtPtr, .opd1.hs = opd_var->inst_name, .opd2.dt_qual = ira_dt_qual_none };

	push_inst_imm_var0_expr(ctx, expr, &make_dt_ptr);

	return true;
}
static bool translate_expr1_dt_stct(ctx_t * ctx, expr_t * expr) {
	size_t elems_size = expr->dt_stct.elems_size;

	ul_hs_t ** elems = malloc(elems_size * sizeof(*elems));

	ul_raise_check_mem_alloc(elems);
	
	ul_hs_t ** ids = malloc(elems_size * sizeof(*ids));

	ul_raise_check_mem_alloc(ids);

	ul_hs_t ** elem = elems;
	ul_hs_t ** id = ids;
	for (expr_t * elem_expr = expr->opd1.expr; elem_expr != NULL; elem_expr = elem_expr->opd1.expr, ++elem, ++id) {
		var_t * elem_var;

		if (!translate_expr1_imm_var(ctx, elem_expr->opd0.expr, &elem_var)) {
			free(elem);
			free(ids);
			return false;
		}

		*elem = elem_var->inst_name;
		*id = elem_expr->opd2.hs;
	}

	ira_inst_t make_dt_stct = { .type = IraInstMakeDtStct, .opd1.dt_qual = ira_dt_qual_none, .opd2.size = elems_size, .opd3.hss = elems, .opd4.hss = ids };

	push_inst_imm_var0_expr(ctx, expr, &make_dt_stct);

	return true;
}
static bool translate_expr1_dt_arr(ctx_t * ctx, expr_t * expr) {
	var_t * opd_var;

	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var)) {
		return false;
	}

	ira_dt_t * dim_opd_dt = expr->opd1.expr->val_qdt.dt;

	if (dim_opd_dt->type == IraDtVoid) {
		ira_inst_t make_dt_arr = { .type = IraInstMakeDtArr, .opd1.hs = opd_var->inst_name, .opd2.dt_qual = ira_dt_qual_none };

		push_inst_imm_var0_expr(ctx, expr, &make_dt_arr);
	}
	else {
		var_t * dim_var;
		
		if (!translate_expr1_imm_var(ctx, expr->opd1.expr, &dim_var)) {
			return false;
		}

		ira_inst_t make_dt_vec = { .type = IraInstMakeDtVec, .opd1.hs = opd_var->inst_name, .opd2.hs = dim_var->inst_name, .opd3.dt_qual = ira_dt_qual_none };

		push_inst_imm_var0_expr(ctx, expr, &make_dt_vec);
	}

	return true;
}
static bool translate_expr1_dt_func(ctx_t * ctx, expr_t * expr) {
	size_t args_size = expr->dt_func.args_size;

	ul_hs_t ** args = malloc(args_size * sizeof(*args));

	ul_raise_check_mem_alloc(args);

	ul_hs_t ** ids = malloc(args_size * sizeof(*ids));

	ul_raise_check_mem_alloc(ids);

	ul_hs_t ** arg = args;
	ul_hs_t ** id = ids;
	for (expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr, ++arg, ++id) {
		var_t * arg_var;

		if (!translate_expr1_imm_var(ctx, arg_expr->opd0.expr, &arg_var)) {
			free(args);
			free(ids);
			return false;
		}

		*arg = arg_var->inst_name;
		*id = arg_expr->opd2.hs;
	}

	var_t * ret_var;

	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &ret_var)) {
		free(args);
		free(ids);
		return false;
	}

	ira_inst_t make_dt_func = { .type = IraInstMakeDtFunc, .opd1.hs = ret_var->inst_name, .opd2.size = args_size, .opd3.hss = args, .opd4.hss = ids };

	push_inst_imm_var0_expr(ctx, expr, &make_dt_func);

	return true;
}
static bool translate_expr1_dt_const(ctx_t * ctx, expr_t * expr) {
	var_t * opd_var;

	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var)) {
		return false;
	}

	ira_inst_t make_dt_const = { .type = IraInstMakeDtConst, .opd1.hs = opd_var->inst_name };

	push_inst_imm_var0_expr(ctx, expr, &make_dt_const);

	return true;
}
static bool translate_expr1_ident(ctx_t * ctx, expr_t * expr) {
	switch (expr->ident.type) {
		case ExprIdentVar:
			expr->val_type = ExprValVar;
			expr->val.var = expr->ident.var;
			break;
		case ExprIdentLo:
			switch (expr->ident.lo->type) {
				case IraLoFunc:
				case IraLoImpt:
				{
					ira_val_t * lo_val;

					if (!ira_pec_make_val_lo_ptr(ctx->pec, expr->ident.lo, &lo_val)) {
						pla_ast_t_report_pec_err(ctx->t_ctx);
						return false;
					}

					ira_inst_t load_val = { .type = IraInstLoadVal, .opd1.val = lo_val };

					push_inst_imm_var0_expr(ctx, expr, &load_val);

					break;
				}
				case IraLoVar:
				{
					ira_val_t * lo_val;

					if (!ira_pec_make_val_lo_ptr(ctx->pec, expr->ident.lo, &lo_val)) {
						pla_ast_t_report_pec_err(ctx->t_ctx);
						return false;
					}

					ira_inst_t load_val = { .type = IraInstLoadVal, .opd1.val = lo_val };

					var_t * lo_ptr_var;

					push_inst_imm_var0(ctx, &load_val, load_val.opd1.val->dt, &lo_ptr_var);

					expr->val_type = ExprValDeref;
					expr->val.var = lo_ptr_var;

					break;
				}
				case IraLoDtStct:
				{
					ira_dt_t * dt;

					if (!ira_pec_get_dt_stct_lo(ctx->pec, expr->ident.lo, ira_dt_qual_none, &dt)) {
						pla_ast_t_report_pec_err(ctx->t_ctx);
						return false;
					}

					ira_val_t * val_dt;

					if (!ira_pec_make_val_imm_dt(ctx->pec, dt, &val_dt)) {
						return false;
					}

					ira_inst_t load_val = { .type = IraInstLoadVal, .opd1.val = val_dt };

					push_inst_imm_var0_expr(ctx, expr, &load_val);

					break;
				}
				case IraLoRoVal:
				{
					ira_inst_t load_val = { .type = IraInstLoadVal, .opd1.val = ira_val_copy(expr->ident.lo->ro_val.val) };

					push_inst_imm_var0_expr(ctx, expr, &load_val);

					break;
				}
				default:
					ul_raise_unreachable();
			}
			break;
		default:
			ul_raise_unreachable();
	}

	return true;
}
static bool translate_expr1_call_func_ptr(ctx_t * ctx, expr_t * expr, var_t * callee) {
	ira_dt_t * func_dt = callee->qdt.dt->ptr.body;

	size_t args_size = expr->call.args_size;

	ul_hs_t ** args = malloc(args_size * sizeof(*args));

	ul_raise_check_mem_alloc(args);

	ul_hs_t ** arg = args;
	ira_dt_ndt_t * arg_dt = func_dt->func.args;
	for (expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr, ++arg, ++arg_dt) {
		var_t * arg_var;

		if (!translate_expr1_imm_var(ctx, arg_expr->opd0.expr, &arg_var)) {
			free(args);
			return false;
		}

		*arg = arg_var->inst_name;
	}

	ira_inst_t call = { .type = IraInstCallFuncPtr, .opd1.hs = callee->inst_name, .opd2.size = args_size, .opd3.hss = args };

	push_inst_imm_var0_expr(ctx, expr, &call);

	return true;
}
static bool translate_expr1_call(ctx_t * ctx, expr_t * expr) {
	var_t * callee_var;

	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &callee_var)) {
		return false;
	}

	ira_dt_t * callee_dt = callee_var->qdt.dt;

	if (callee_dt->type == IraDtPtr && callee_dt->ptr.body->type == IraDtFunc) {
		if (!translate_expr1_call_func_ptr(ctx, expr, callee_var)) {
			return false;
		}
	}
	else {
		ul_raise_unreachable();
	}

	return true;
}
static bool translate_expr1_subscr(ctx_t * ctx, expr_t * expr) {
	expr_t * opd0 = expr->opd0.expr;
	ira_dt_t * opd0_dt = opd0->val_qdt.dt;
	
	var_t * ptr_var;

	switch (opd0_dt->type) {
		case IraDtVec:
		{
			var_t * opd0_ptr;

			if (!translate_expr1_var_ptr(ctx, opd0, &opd0_ptr)) {
				return false;
			}

			ira_dt_t * elem_dt;

			if (!ira_pec_apply_qual(ctx->pec, opd0_dt->vec.body, opd0_dt->vec.qual, &elem_dt)) {
				pla_ast_t_report_pec_err(ctx->t_ctx);
				return false;
			}

			ira_dt_t * ptr_var_dt;

			if (!ira_pec_get_dt_ptr(ctx->pec, elem_dt, opd0_ptr->qdt.dt->ptr.qual, &ptr_var_dt)) {
				pla_ast_t_report_pec_err(ctx->t_ctx);
				return false;
			}

			ira_inst_t cast = { .type = IraInstCast, .opd1.hs = opd0_ptr->inst_name, .opd2.dt = ptr_var_dt };

			push_inst_imm_var0(ctx, &cast, ptr_var_dt, &ptr_var);

			break;
		}
		case IraDtPtr:
			if (!translate_expr1_imm_var(ctx, opd0, &ptr_var)) {
				return false;
			}
			break;
		case IraDtArr:
		{
			var_t * opd0_ptr;

			if (!translate_expr1_var_ptr(ctx, opd0, &opd0_ptr)) {
				return false;
			}

			ul_hs_t * mmbr = ctx->pec->pds[IraPdsDataMmbr];
			ira_dt_t * mmbr_dt;

			if (!get_mmbr_acc_dt(ctx, opd0->val_qdt.dt, mmbr, &mmbr_dt)) {
				ul_raise_unreachable();
			}

			ira_dt_t * ptr_var_dt;

			if (!ira_pec_get_dt_ptr(ctx->pec, mmbr_dt, opd0_ptr->qdt.dt->ptr.qual, &ptr_var_dt)) {
				pla_ast_t_report_pec_err(ctx->t_ctx);
				return false;
			}

			var_t * mmbr_ptr;

			ira_inst_t mmbr_acc_ptr = { .type = IraInstMmbrAccPtr, .opd1.hs = opd0_ptr->inst_name, .opd2.hs = mmbr };

			push_inst_imm_var0(ctx, &mmbr_acc_ptr, ptr_var_dt, &mmbr_ptr);

			ira_inst_t read_ptr = { .type = IraInstReadPtr, .opd1.hs = mmbr_ptr->inst_name };

			push_inst_imm_var0(ctx, &read_ptr, mmbr_ptr->qdt.dt->ptr.body, &ptr_var);

			break;
		}
		default:
			ul_raise_unreachable();
	}

	var_t * opd1;

	if (!translate_expr1_imm_var(ctx, expr->opd1.expr, &opd1)) {
		return false;
	}

	var_t * res_var;

	ira_inst_t shift_ptr = { .type = IraInstShiftPtr, .opd1.hs = ptr_var->inst_name, .opd2.hs = opd1->inst_name };

	push_inst_imm_var0(ctx, &shift_ptr, ptr_var->qdt.dt, &res_var);

	expr->val_type = ExprValDeref;
	expr->val.var = res_var;

	return true;
}
static bool translate_expr1_mmbr_acc(ctx_t * ctx, expr_t * expr) {
	expr_t * opd = expr->opd0.expr;

	var_t * ptr_var;

	if (!translate_expr1_var_ptr(ctx, opd, &ptr_var)) {
		return false;
	}

	ira_dt_t * ptr_mmbr_dt;

	if (!ira_pec_get_dt_ptr(ctx->pec, expr->mmbr_acc.res_dt, ptr_var->qdt.dt->ptr.qual, &ptr_mmbr_dt)) {
		pla_ast_t_report_pec_err(ctx->t_ctx);
		return false;
	}

	var_t * ptr_mmbr_var;

	ira_inst_t mmbr_acc_ptr = { .type = IraInstMmbrAccPtr, .opd1.hs = ptr_var->inst_name, .opd2.hs = expr->opd1.hs };

	push_inst_imm_var0(ctx, &mmbr_acc_ptr, ptr_mmbr_dt, &ptr_mmbr_var);

	set_expr_ptr_val(ctx, expr, opd->val_type, ptr_mmbr_var);

	return true;
}
static bool translate_expr1_deref(ctx_t * ctx, expr_t * expr) {
	var_t * opd_var;

	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var)) {
		return false;
	}

	expr->val_type = ExprValDeref;
	expr->val.var = opd_var;

	return true;
}
static bool translate_expr1_addr_of(ctx_t * ctx, expr_t * expr) {
	expr_t * opd = expr->opd0.expr;

	if (!translate_expr1(ctx, opd)) {
		return false;
	}

	if (!expr_val_is_left_val[opd->val_type]) {
		pla_ast_t_report(ctx->t_ctx, L"address-of's operand is required to be an lvalue");
		return false;
	}

	switch (opd->val_type) {
		case ExprValVar:
		{
			var_t * opd_var = opd->val.var;

			ira_inst_t addr_of = { .type = IraInstAddrOf, .opd1.hs = opd_var->inst_name };

			push_inst_imm_var0_expr(ctx, expr, &addr_of);

			break;
		}
		case ExprValDeref:
		{
			expr->val_type = ExprValImmVar;
			expr->val.var = opd->val.var;
			break;
		}
		default:
			ul_raise_unreachable();
	}

	return true;
}
static bool translate_expr1_cast(ctx_t * ctx, expr_t * expr) {
	var_t * opd_var;

	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var)) {
		return false;
	}

	ira_inst_t cast = { .type = IraInstCast, .opd1.hs = opd_var->inst_name, .opd2.dt = expr->val_qdt.dt };

	push_inst_imm_var0_expr(ctx, expr, &cast);

	return true;
}
static bool translate_expr1_unr(ctx_t * ctx, expr_t * expr) {
	var_t * opd_var;

	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var)) {
		return false;
	}

	pla_ast_t_optr_t * optr = expr->optr;

	switch (optr->type) {
		case PlaAstTOptrUnrInstBool:
		{
			ira_inst_t unr_op = { .type = optr->unr_inst_int.inst_type, .opd1.hs = opd_var->inst_name };

			push_inst_imm_var0_expr(ctx, expr, &unr_op);

			break;
		}
		case PlaAstTOptrUnrInstInt:
		{
			ira_inst_t unr_op = { .type = optr->unr_inst_int.inst_type, .opd1.hs = opd_var->inst_name };

			push_inst_imm_var0_expr(ctx, expr, &unr_op);

			break;
		}
		default:
			ul_raise_unreachable();
	}

	return true;
}
static bool translate_expr1_bin(ctx_t * ctx, expr_t * expr) {
	var_t * opd0_var, * opd1_var;

	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd0_var)) {
		return false;
	}

	if (!translate_expr1_imm_var(ctx, expr->opd1.expr, &opd1_var)) {
		return false;
	}

	pla_ast_t_optr_t * optr = expr->optr;

	switch (optr->type) {
		case PlaAstTOptrBinInstInt:
		{
			ira_inst_t bin_op = { .type = optr->bin_inst_int.inst_type, .opd1.hs = opd0_var->inst_name, .opd2.hs = opd1_var->inst_name };

			push_inst_imm_var0_expr(ctx, expr, &bin_op);

			break;
		}
		case PlaAstTOptrBinInstIntBool:
		{
			ira_inst_t bin_op = { .type = optr->bin_inst_int_bool.inst_type, .opd1.hs = opd0_var->inst_name, .opd2.hs = opd1_var->inst_name, .opd3.int_cmp = optr->bin_inst_int_bool.int_cmp };

			push_inst_imm_var0_expr(ctx, expr, &bin_op);

			break;
		}
		case PlaAstTOptrBinInstPtrBool:
		{
			ira_inst_t bin_op = { .type = optr->bin_inst_ptr_bool.inst_type, .opd1.hs = opd0_var->inst_name, .opd2.hs = opd1_var->inst_name, .opd3.int_cmp = optr->bin_inst_ptr_bool.int_cmp };

			push_inst_imm_var0_expr(ctx, expr, &bin_op);

			break;
		}
		default:
			ul_raise_unreachable();
	}

	return true;
}
static bool translate_expr1_logic_optr(ctx_t * ctx, expr_t * expr) {
	var_t * opd0;
	
	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd0)) {
		return false;
	}

	const ul_ros_t * end_label_base, * end_label_suffix;
	ira_inst_type_t br_type;

	switch (expr->base->type) {
		case PlaExprLogicAnd:
			end_label_base = &label_base_logic_and;
			end_label_suffix = &label_suffix_end;
			br_type = IraInstBrf;
			break;
		case PlaExprLogicOr:
			end_label_base = &label_base_logic_or;
			end_label_suffix = &label_suffix_end;
			br_type = IraInstBrt;
			break;
		default:
			ul_raise_unreachable();
	}

	ul_hs_t * end_label = get_unq_label_name(ctx, end_label_base, ctx->unq_label_index++, end_label_suffix);

	ira_inst_t br = { .type = br_type, .opd0.hs = end_label, .opd1.hs = opd0->inst_name };

	ira_func_push_inst(ctx->func, &br);

	var_t * opd1;

	if (!translate_expr1_imm_var(ctx, expr->opd1.expr, &opd1)) {
		return false;
	}

	ira_inst_t copy = { .type = IraInstCopy, .opd0.hs = opd0->inst_name, .opd1.hs = opd1->inst_name };

	ira_func_push_inst(ctx->func, &copy);
	
	push_def_label_inst(ctx, end_label);

	expr->val_type = ExprValImmVar;
	expr->val.var = opd0;

	return true;
}
static bool translate_expr1_asgn(ctx_t * ctx, expr_t * expr) {
	expr_t * opd0 = expr->opd0.expr;
	var_t * opd1_var;

	if (!translate_expr1_imm_var(ctx, expr->opd1.expr, &opd1_var)) {
		return false;
	}

	if (!translate_expr1(ctx, opd0)) {
		return false;
	}

	if (!expr_val_is_left_val[opd0->val_type]) {
		pla_ast_t_report(ctx->t_ctx, L"assignment's left operand is required to be an lvalue");
		return false;
	}

	switch (opd0->val_type) {
		case ExprValVar:
		{
			var_t * opd0_var = opd0->val.var;

			ira_inst_t copy = { .type = IraInstCopy, .opd0.hs = opd0_var->inst_name, .opd1.hs = opd1_var->inst_name };

			ira_func_push_inst(ctx->func, &copy);

			break;
		}
		case ExprValDeref:
		{
			ira_inst_t write_ptr = { .type = IraInstWritePtr, .opd0.hs = opd0->val.var->inst_name, .opd1.hs = opd1_var->inst_name };

			ira_func_push_inst(ctx->func, &write_ptr);

			break;
		}
		default:
			ul_raise_unreachable();
	}

	expr->val_type = opd0->val_type;
	expr->val = opd0->val;

	return true;
}
static bool translate_expr1_tse(ctx_t * ctx, expr_t * expr) {
	typedef bool translate_expr1_proc_t(ctx_t * ctx, expr_t * expr);

	static translate_expr1_proc_t * const translate_expr1_procs[PlaExpr_Count] = {
		[PlaExprDtVoid] = translate_expr1_load_val,
		[PlaExprDtBool] = translate_expr1_load_val,
		[PlaExprDtInt] = translate_expr1_load_val,
		[PlaExprDtPtr] = translate_expr1_dt_ptr,
		[PlaExprDtStct] = translate_expr1_dt_stct,
		[PlaExprDtArr] = translate_expr1_dt_arr,
		[PlaExprDtFunc] = translate_expr1_dt_func,
		[PlaExprDtConst] = translate_expr1_dt_const,
		[PlaExprValVoid] = translate_expr1_load_val,
		[PlaExprValBool] = translate_expr1_load_val,
		[PlaExprChStr] = translate_expr1_load_val,
		[PlaExprNumStr] = translate_expr1_load_val,
		[PlaExprNullofDt] = translate_expr1_load_val,
		[PlaExprIdent] = translate_expr1_ident,
		[PlaExprCall] = translate_expr1_call,
		[PlaExprSubscr] = translate_expr1_subscr,
		[PlaExprMmbrAcc] = translate_expr1_mmbr_acc,
		[PlaExprDeref] = translate_expr1_deref,
		[PlaExprAddrOf] = translate_expr1_addr_of,
		[PlaExprCast] = translate_expr1_cast,
		[PlaExprLogicNeg] = translate_expr1_unr,
		[PlaExprBitNeg] = translate_expr1_unr,
		[PlaExprArithNeg] = translate_expr1_unr,
		[PlaExprMul] = translate_expr1_bin,
		[PlaExprDiv] = translate_expr1_bin,
		[PlaExprMod] = translate_expr1_bin,
		[PlaExprAdd] = translate_expr1_bin,
		[PlaExprSub] = translate_expr1_bin,
		[PlaExprLeShift] = translate_expr1_bin,
		[PlaExprRiShift] = translate_expr1_bin,
		[PlaExprLess] = translate_expr1_bin,
		[PlaExprLessEq] = translate_expr1_bin,
		[PlaExprGrtr] = translate_expr1_bin,
		[PlaExprGrtrEq] = translate_expr1_bin,
		[PlaExprEq] = translate_expr1_bin,
		[PlaExprNeq] = translate_expr1_bin,
		[PlaExprBitAnd] = translate_expr1_bin,
		[PlaExprBitXor] = translate_expr1_bin,
		[PlaExprBitOr] = translate_expr1_bin,
		[PlaExprLogicAnd] = translate_expr1_logic_optr,
		[PlaExprLogicOr] = translate_expr1_logic_optr,
		[PlaExprAsgn] = translate_expr1_asgn,
	};

	translate_expr1_proc_t * proc = translate_expr1_procs[expr->base->type];

	ul_raise_assert(proc != NULL);

	if (!proc(ctx, expr)) {
		return false;
	}

	return true;
}
static bool translate_expr1(ctx_t * ctx, expr_t * expr) {
	tse_t tse = { .type = PlaAstTTseExpr, .expr = expr->base };

	pla_ast_t_push_tse(ctx->t_ctx, &tse);

	bool res;
	
	__try {
		res = translate_expr1_tse(ctx, expr);
	}
	__finally {
		pla_ast_t_pop_tse(ctx->t_ctx);
	}

	return res;
}

static bool translate_expr_guard(ctx_t * ctx, pla_expr_t * pla_expr, expr_t ** expr, var_t ** out) {
	if (!translate_expr0(ctx, pla_expr, expr)) {
		return false;
	}

	if (out != NULL) {
		if (!translate_expr1_imm_var(ctx, *expr, out)) {
			return false;
		}
	}
	else {
		if (!translate_expr1(ctx, *expr)) {
			return false;
		}
	}

	return true;
}
static bool translate_expr(ctx_t * ctx, pla_expr_t * pla_expr, var_t ** out) {
	expr_t * expr = NULL;

	bool res;

	__try {
		res = translate_expr_guard(ctx, pla_expr, &expr, out);
	}
	__finally {
		destroy_expr(ctx, expr);
	}

	return res;
}

static bool translate_stmt(ctx_t * ctx, pla_stmt_t * stmt);

static bool translate_stmt_blk_vvb(ctx_t * ctx, pla_stmt_t * stmt) {
	for (pla_stmt_t * it = stmt->blk.body; it != NULL; it = it->next) {
		if (!translate_stmt(ctx, it)) {
			return false;
		}
	}

	return true;
}
static bool translate_stmt_blk(ctx_t * ctx, pla_stmt_t * stmt) {
	vvb_t vvb = { 0 };

	push_vvb(ctx, &vvb);

	bool res;
	
	__try {
		res = translate_stmt_blk_vvb(ctx, stmt);
	}
	__finally {
		pop_vvb(ctx);
	}

	return res;
}
static bool translate_stmt_expr(ctx_t * ctx, pla_stmt_t * stmt) {
	if (!translate_expr(ctx, stmt->expr.expr, NULL)) {
		return false;
	}

	return true;
}
static bool translate_stmt_var_dt(ctx_t * ctx, pla_stmt_t * stmt) {
	ira_dt_t * dt;

	if (!pla_ast_t_calculate_expr_dt(ctx->t_ctx, stmt->var_dt.dt_expr, &dt)) {
		return false;
	}

	if (!ira_dt_is_complete(dt)) {
		pla_ast_t_report(ctx->t_ctx, L"variables must have only complete data type");
		return false;
	}

	var_t * var = define_var(ctx, stmt->var_dt.name, dt, stmt->var_dt.dt_qual, true);

	if (var == NULL) {
		pla_ast_t_report(ctx->t_ctx, L"there is already variable of same name [%s] in this block", stmt->var_dt.name->str);
		return false;
	}

	push_def_var_inst(ctx, var);

	return true;
}
static bool translate_stmt_var_val(ctx_t * ctx, pla_stmt_t * stmt) {
	var_t * val_var;

	if (!translate_expr(ctx, stmt->var_val.val_expr, &val_var)) {
		return false;
	}

	var_t * var = define_var(ctx, stmt->var_val.name, val_var->qdt.dt, stmt->var_val.dt_qual, true);

	if (var == NULL) {
		pla_ast_t_report(ctx->t_ctx, L"there is already variable of same name [%s] in this block", stmt->var_val.name->str);
		return false;
	}

	ira_inst_t def_var_copy = { .type = IraInstDefVarCopy, .opd0.hs = var->inst_name, .opd1.hs = val_var->inst_name, .opd2.dt_qual = var->qdt.qual };

	ira_func_push_inst(ctx->func, &def_var_copy);

	return true;
}
static bool translate_stmt_cond(ctx_t * ctx, pla_stmt_t * stmt) {
	var_t * cond_var;

	if (!translate_expr(ctx, stmt->cond.cond_expr, &cond_var)) {
		return false;
	}

	if (cond_var->qdt.dt != &ctx->pec->dt_bool) {
		pla_ast_t_report(ctx->t_ctx, L"condition expression mush have a boolean data type");
		return false;
	}

	size_t label_index = ctx->unq_label_index++;

	ul_hs_t * exit_label;

	{
		ul_hs_t * tc_end = get_unq_label_name(ctx, &label_base_cond, label_index, &label_suffix_tc_end);

		ira_inst_t brf = { .type = IraInstBrf, .opd0.hs = tc_end, .opd1.hs = cond_var->inst_name };

		ira_func_push_inst(ctx->func, &brf);

		if (!translate_stmt(ctx, stmt->cond.true_br)) {
			return false;
		}

		exit_label = tc_end;
	}

	if (stmt->cond.false_br != false) {
		ul_hs_t * fc_end = get_unq_label_name(ctx, &label_base_cond, label_index, &label_suffix_fc_end);

		ira_inst_t bru = { .type = IraInstBru, .opd0.hs = fc_end };

		ira_func_push_inst(ctx->func, &bru);

		push_def_label_inst(ctx, exit_label);

		exit_label = fc_end;

		if (!translate_stmt(ctx, stmt->cond.false_br)) {
			return false;
		}
	}

	push_def_label_inst(ctx, exit_label);

	return true;
}
static bool translate_stmt_pre_loop_cfb(ctx_t * ctx, pla_stmt_t * stmt, cfb_t * cfb) {
	size_t label_index = ctx->unq_label_index++;

	cfb->exit_label = get_unq_label_name(ctx, &label_base_pre_loop, label_index, &label_suffix_exit);
	cfb->cnt_label = get_unq_label_name(ctx,  &label_base_pre_loop, label_index, &label_suffix_cnt);

	push_def_label_inst(ctx, cfb->cnt_label);

	var_t * cond_var;

	if (!translate_expr(ctx, stmt->pre_loop.cond_expr, &cond_var)) {
		return false;
	}

	{
		ira_inst_t brf = { .type = IraInstBrf, .opd0.hs = cfb->exit_label, .opd1.hs = cond_var->inst_name };

		ira_func_push_inst(ctx->func, &brf);
	}

	if (!translate_stmt(ctx, stmt->pre_loop.body)) {
		return false;
	}

	{
		ira_inst_t bru = { .type = IraInstBru, .opd0.hs = cfb->cnt_label };

		ira_func_push_inst(ctx->func, &bru);
	}

	push_def_label_inst(ctx, cfb->exit_label);

	return true;
}
static bool translate_stmt_pre_loop(ctx_t * ctx, pla_stmt_t * stmt) {
	cfb_t cfb = { .name = stmt->pre_loop.name };

	push_cfb(ctx, &cfb);

	bool res;
	
	__try {
		res = translate_stmt_pre_loop_cfb(ctx, stmt, &cfb);
	}
	__finally {
		pop_cfb(ctx);
	}

	return res;
}
static bool translate_stmt_post_loop_cfb(ctx_t * ctx, pla_stmt_t * stmt, cfb_t * cfb) {
	size_t label_index = ctx->unq_label_index++;

	cfb->exit_label = get_unq_label_name(ctx, &label_base_post_loop, label_index, &label_suffix_exit);
	cfb->cnt_label = get_unq_label_name(ctx,  &label_base_post_loop, label_index, &label_suffix_cnt);
	ul_hs_t * body_label = get_unq_label_name(ctx, &label_base_post_loop, label_index, &label_suffix_body);
	
	push_def_label_inst(ctx, body_label);

	if (!translate_stmt(ctx, stmt->post_loop.body)) {
		return false;
	}

	push_def_label_inst(ctx, cfb->cnt_label);

	var_t * cond_var;

	if (!translate_expr(ctx, stmt->pre_loop.cond_expr, &cond_var)) {
		return false;
	}

	{
		ira_inst_t brt = { .type = IraInstBrt, .opd0.hs = body_label, .opd1.hs = cond_var->inst_name };

		ira_func_push_inst(ctx->func, &brt);
	}

	push_def_label_inst(ctx, cfb->exit_label);

	return true;
}
static bool translate_stmt_post_loop(ctx_t * ctx, pla_stmt_t * stmt) {
	cfb_t cfb = { .name = stmt->post_loop.name };

	push_cfb(ctx, &cfb);

	bool res;
	
	__try {
		res = translate_stmt_post_loop_cfb(ctx, stmt, &cfb);
	}
	__finally {
		pop_cfb(ctx);
	}

	return res;
}
static bool translate_stmt_brk(ctx_t * ctx, pla_stmt_t * stmt) {
	ul_hs_t * cfb_name = stmt->brk.name;
	cfb_t * cfb = ctx->cfb;

	for (; cfb != NULL; cfb = cfb->prev) {
		if (cfb_name != NULL && cfb->name != cfb_name) {
			continue;
		}

		if (cfb->exit_label != NULL) {
			break;
		}
	}

	if (cfb == NULL) {
		pla_ast_t_report(ctx->t_ctx, L"failed to find target for break statement");
		return false;
	}

	{
		ira_inst_t bru = { .type = IraInstBru, .opd0.hs = cfb->exit_label };

		ira_func_push_inst(ctx->func, &bru);
	}

	return true;
}
static bool translate_stmt_cnt(ctx_t * ctx, pla_stmt_t * stmt) {
	ul_hs_t * cfb_name = stmt->brk.name;
	cfb_t * cfb = ctx->cfb;

	for (; cfb != NULL; cfb = cfb->prev) {
		if (cfb_name != NULL && cfb->name != cfb_name) {
			continue;
		}

		if (cfb->cnt_label != NULL) {
			break;
		}
	}

	if (cfb == NULL) {
		pla_ast_t_report(ctx->t_ctx, L"failed to find target for continue statement");
		return false;
	}

	{
		ira_inst_t bru = { .type = IraInstBru, .opd0.hs = cfb->cnt_label };

		ira_func_push_inst(ctx->func, &bru);
	}

	return true;
}
static bool translate_stmt_ret(ctx_t * ctx, pla_stmt_t * stmt) {
	var_t * ret_var;

	if (!translate_expr(ctx, stmt->expr.expr, &ret_var)) {
		return false;
	}

	if (ctx->func_ret != NULL) {
		if (!ira_dt_is_equivalent(ctx->func_ret, ret_var->qdt.dt)) {
			pla_ast_t_report(ctx->t_ctx, L"return data type does not match to function return data type");
			return false;
		}
	}
	else {
		ctx->func_ret = ret_var->qdt.dt;
	}

	ira_inst_t ret = { .type = IraInstRet, .opd0.hs = ret_var->inst_name };

	ira_func_push_inst(ctx->func, &ret);

	return true;
}
static bool translate_stmt_tse(ctx_t * ctx, pla_stmt_t * stmt) {
	switch (stmt->type) {
		case PlaStmtNone:
			break;
		case PlaStmtBlk:
			if (!translate_stmt_blk(ctx, stmt)) {
				return false;
			}
			break;
		case PlaStmtExpr:
			if (!translate_stmt_expr(ctx, stmt)) {
				return false;
			}
			break;
		case PlaStmtVarDt:
			if (!translate_stmt_var_dt(ctx, stmt)) {
				return false;
			}
			break;
		case PlaStmtVarVal:
			if (!translate_stmt_var_val(ctx, stmt)) {
				return false;
			}
			break;
		case PlaStmtCond:
			if (!translate_stmt_cond(ctx, stmt)) {
				return false;
			}
			break;
		case PlaStmtPreLoop:
			if (!translate_stmt_pre_loop(ctx, stmt)) {
				return false;
			}
			break;
		case PlaStmtPostLoop:
			if (!translate_stmt_post_loop(ctx, stmt)) {
				return false;
			}
			break;
		case PlaStmtBrk:
			if (!translate_stmt_brk(ctx, stmt)) {
				return false;
			}
			break;
		case PlaStmtCnt:
			if (!translate_stmt_cnt(ctx, stmt)) {
				return false;
			}
			break;
		case PlaStmtRet:
			if (!translate_stmt_ret(ctx, stmt)) {
				return false;
			}
			break;
		default:
			ul_raise_unreachable();
	}

	return true;
}
static bool translate_stmt(ctx_t * ctx, pla_stmt_t * stmt) {
	tse_t tse = { .type = PlaAstTTseStmt, .stmt = stmt };

	pla_ast_t_push_tse(ctx->t_ctx, &tse);

	bool res;
	
	__try {
		res = translate_stmt_tse(ctx, stmt);
	}
	__finally {
		pla_ast_t_pop_tse(ctx->t_ctx);
	}

	return res;
}

static bool translate_args_vvb(ctx_t * ctx) {
	if (ctx->func_dt != NULL) {
		ctx->func_ret = ctx->func_dt->func.ret;

		for (ira_dt_ndt_t * arg = ctx->func_dt->func.args, *arg_end = arg + ctx->func_dt->func.args_size; arg != arg_end; ++arg) {
			if (!ira_dt_is_complete(arg->dt)) {
				pla_ast_t_report(ctx->t_ctx, L"function arguments must have only complete data type");
				return false;
			}

			var_t * var = define_var(ctx, arg->name, arg->dt, ira_dt_qual_none, false);

			ul_raise_assert(var != NULL);
		}
	}

	if (!translate_stmt(ctx, ctx->stmt)) {
		return false;
	}

	if (ctx->func->dt == NULL) {
		if (ctx->func_ret == NULL) {
			return false;
		}

		if (!ira_pec_get_dt_func(ctx->pec, ctx->func_ret, 0, NULL, &ctx->func->dt)) {
			return false;
		}
	}

	return true;
}
static bool translate_args(ctx_t * ctx) {
	vvb_t vvb = { 0 };

	push_vvb(ctx, &vvb);

	bool res;
	
	__try {
		res = translate_args_vvb(ctx);
	}
	__finally {
		pop_vvb(ctx);
	}

	return res;
}

static bool translate_core(ctx_t * ctx) {
	if (ctx->func_dt != NULL && ctx->func_dt->type != IraDtFunc) {
		return false;
	}

	if (ctx->stmt->type != PlaStmtBlk) {
		return false;
	}

	ctx->hsb = pla_ast_t_get_hsb(ctx->t_ctx);
	ctx->hst = pla_ast_t_get_hst(ctx->t_ctx);
	ctx->pec = pla_ast_t_get_pec(ctx->t_ctx);

	*ctx->out = ira_func_create(ctx->func_dt);

	ctx->func = *ctx->out;

	if (!translate_args(ctx)) {
		return false;
	}

	return true;
}

bool pla_ast_t_s_translate(pla_ast_t_ctx_t * t_ctx, ira_dt_t * func_dt, pla_stmt_t * stmt, ira_func_t ** out) {
	ctx_t ctx = { .t_ctx = t_ctx, .func_dt = func_dt, .stmt = stmt, .out = out };

	bool res;
	
	__try {
		res = translate_core(&ctx);
	}
	__finally {

	}

	return res;
}
