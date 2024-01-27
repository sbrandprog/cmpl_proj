#include "pch.h"
#include "pla_ast_t_s.h"
#include "pla_irid.h"
#include "pla_expr.h"
#include "pla_stmt.h"
#include "ira_dt.h"
#include "ira_val.h"
#include "ira_inst.h"
#include "ira_func.h"
#include "ira_lo.h"
#include "ira_pec.h"
#include "u_assert.h"

#define UNQ_VAR_NAME_SUFFIX L"#"

typedef pla_ast_t_tse_t tse_t;
typedef pla_ast_t_vse_t vse_t;

typedef struct var var_t;
struct var {
	var_t * next;
	u_hs_t * name;
	u_hs_t * inst_name;
	ira_dt_t * dt;
};
typedef struct blk blk_t;
struct blk {
	blk_t * prev;
	var_t * var;
};

typedef struct expr expr_t;
typedef enum expr_val_type {
	ExprValImmVar,
	ExprValVar,
	ExprValDeref,
	ExprVal_Count
} expr_val_type_t;
typedef union expr_val {
	var_t * var;
} expr_val_t;
typedef union expr_opd {
	bool boolean;
	ira_int_type_t int_type;
	u_hs_t * hs;
	pla_irid_t * irid;
	expr_t * expr;
} expr_opd_t;
struct expr {
	pla_expr_t * base;
	union {
		expr_opd_t opds[PLA_EXPR_OPDS_SIZE];
		struct {
			expr_opd_t opd0, opd1, opd2;
		};
	};

	ira_dt_t * val_dt;

	expr_val_type_t val_type;
	expr_val_t val;

	union {
		struct {
			ira_val_t * val;
		} load_val;
		struct {
			size_t elems_size;
		} dt_tpl;
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
		pla_ast_t_optr_t * optr;
	};
};

typedef struct pla_ast_t_s_ctx {
	pla_ast_t_ctx_t * t_ctx;

	ira_dt_t * func_dt;

	pla_stmt_t * stmt;

	ira_func_t ** out;

	u_hsb_t * hsb;
	u_hst_t * hst;
	ira_pec_t * pec;

	ira_func_t * func;
	ira_dt_t * func_ret;

	blk_t * blk;
	size_t unq_var_index;
	size_t unq_label_index;
} ctx_t;

typedef bool val_proc_t(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out);


static const bool expr_val_is_left_val[ExprVal_Count] = {
	[ExprValImmVar] = false,
	[ExprValVar] = true,
	[ExprValDeref] = true
};

static const u_ros_t label_cond_base = U_MAKE_ROS(L"cond");
static const u_ros_t label_cond_tc_end = U_MAKE_ROS(L"tc_end");
static const u_ros_t label_cond_fc_end = U_MAKE_ROS(L"fc_end");

static const u_ros_t label_pre_loop_base = U_MAKE_ROS(L"pre_loop");
static const u_ros_t label_pre_loop_cond = U_MAKE_ROS(L"cond");
static const u_ros_t label_pre_loop_exit = U_MAKE_ROS(L"exit");


static u_hs_t * get_unq_var_name(ctx_t * ctx, u_hs_t * name) {
	return u_hsb_formatadd(ctx->hsb, ctx->hst, L"%s%s%zi", name->str, UNQ_VAR_NAME_SUFFIX, ctx->unq_var_index++);
}
static u_hs_t * get_unq_label_name(ctx_t * ctx, const u_ros_t * base, size_t index, const u_ros_t * suffix) {
	return u_hsb_formatadd(ctx->hsb, ctx->hst, L"%s%zi%s", base->str, index, suffix->str);
}

static void push_blk(ctx_t * ctx, blk_t * blk) {
	blk->prev = ctx->blk;

	ctx->blk = blk;
}
static void pop_blk(ctx_t * ctx) {
	blk_t * blk = ctx->blk;

	for (var_t * var = blk->var; var != NULL; ) {
		var_t * next = var->next;

		free(var);

		var = next;
	}

	ctx->blk = blk->prev;
}

static expr_t * create_expr(ctx_t * ctx, pla_expr_t * base) {
	expr_t * expr = malloc(sizeof(*expr));

	u_assert(expr != NULL);

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
			case PlaExprOpdIrid:
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
				u_assert_switch(info->opds[opd]);
		}
	}

	switch (base->type) {
		case PlaExprDtVoid:
		case PlaExprDtInt:
		case PlaExprChStr:
		case PlaExprNumStr:
		case PlaExprNullofDt:
			ira_val_destroy(expr->load_val.val);
			expr->load_val.val = NULL;
			break;
	}

	free(expr);
}

static var_t * create_var(ctx_t * ctx, u_hs_t * name, ira_dt_t * dt, bool unq_name) {
	var_t * new_var = malloc(sizeof(*new_var));

	u_assert(new_var != NULL);

	*new_var = (var_t){ .name = name, .inst_name = name, .dt = dt };

	if (unq_name) {
		new_var->inst_name = get_unq_var_name(ctx, name);
	}

	return new_var;
}
static var_t * define_var(ctx_t * ctx, u_hs_t * name, ira_dt_t * dt, bool unq_name) {
	var_t ** ins = &ctx->blk->var;

	while (*ins != NULL) {
		var_t * var = *ins;

		if (name == var->name) {
			return NULL;
		}

		ins = &(*ins)->next;
	}

	var_t * new_var = create_var(ctx, name, dt, unq_name);

	*ins = new_var;

	return new_var;
}
static var_t * get_imm_var(ctx_t * ctx, ira_dt_t * dt) {
	var_t ** ins = &ctx->blk->var;

	while (*ins != NULL) {
		ins = &(*ins)->next;
	}

	var_t * new_var = create_var(ctx, pla_ast_t_get_pds(ctx->t_ctx, PlaPdsImmVarName), dt, true);

	*ins = new_var;

	return new_var;
}

static void push_def_var_inst(ctx_t * ctx, var_t * var) {
	ira_inst_t def_var = { .type = IraInstDefVar, .opd0.hs = var->inst_name, .opd1.dt = var->dt };

	ira_func_push_inst(ctx->func, &def_var);
}
static void push_def_label_inst(ctx_t * ctx, u_hs_t * name) {
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
	push_inst_imm_var0(ctx, inst, expr->val_dt, &expr->val.var);
}

static bool translate_expr0(ctx_t * ctx, pla_expr_t * base, expr_t ** out);

static bool get_cs_ascii(ctx_t * ctx, u_hs_t * str, ira_val_t ** out) {
	ira_val_t * val = ira_val_create(IraValImmArr, ira_pec_get_dt_arr(ctx->pec, &ctx->pec->dt_ints[IraIntU8]));

	val->arr.size = str->size + 1;

	val->arr.data = malloc(val->arr.size * sizeof(*val->arr.data));

	u_assert(val->arr.data != NULL);

	memset(val->arr.data, 0, val->arr.size * sizeof(*val->arr.data));

	ira_val_t ** ins = val->arr.data;
	for (wchar_t * ch = str->str, *ch_end = ch + str->size; ch != ch_end; ++ch) {
		if ((*ch & ~0x7F) != 0) {
			pla_ast_t_report(ctx->t_ctx, L"non-ascii character [%c] in ascii string:\n%s", *ch, str->str);
			ira_val_destroy(val);
			return false;
		}

		*ins++ = ira_pec_make_val_imm_int(ctx->pec, IraIntU8, (ira_int_t) {
			.ui8 = (uint8_t)*ch
		});
	}

	*ins++ = ira_pec_make_val_imm_int(ctx->pec, IraIntU8, (ira_int_t) {
		.ui8 = 0
	});

	size_t final_size = ins - val->arr.data;

	u_assert(final_size <= val->arr.size);

	val->arr.size = final_size;

	*out = val;

	return true;
}
static bool get_cs_wide(ctx_t * ctx, u_hs_t * str, ira_val_t ** out) {
	ira_val_t * val = ira_val_create(IraValImmArr, ira_pec_get_dt_arr(ctx->pec, &ctx->pec->dt_ints[IraIntU16]));

	val->arr.size = str->size + 1;

	val->arr.data = malloc(val->arr.size * sizeof(*val->arr.data));

	u_assert(val->arr.data != NULL);

	memset(val->arr.data, 0, val->arr.size * sizeof(*val->arr.data));

	ira_val_t ** ins = val->arr.data;
	for (wchar_t * ch = str->str, *ch_end = ch + str->size; ch != ch_end; ++ch) {
		*ins++ = ira_pec_make_val_imm_int(ctx->pec, IraIntU16, (ira_int_t) {
			.ui16 = (uint16_t)*ch
		});
	}

	*ins++ = ira_pec_make_val_imm_int(ctx->pec, IraIntU8, (ira_int_t) {
		.ui16 = 0
	});

	size_t final_size = ins - val->arr.data;

	u_assert(final_size <= val->arr.size);

	val->arr.size = final_size;

	*out = val;

	return true;
}

static bool is_int_ns(ctx_t * ctx, u_hs_t * tag, ira_int_type_t * out) {
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
		*out = IraIntU16;
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
static bool get_ns_int(ctx_t * ctx, u_hs_t * str, ira_int_type_t int_type, ira_val_t ** out) {
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

	uint64_t lim = ira_int_get_max_pos(int_type);

	if (var > lim) {
		pla_ast_t_report(ctx->t_ctx, L"integer string is exceeding precision limit of data type [%i]:\n%s", int_type, str->str);
		return false;
	}

	*out = ira_pec_make_val_imm_int(ctx->pec, int_type, (ira_int_t) {
		.ui64 = var
	});

	return true;
}

static bool get_dt_void_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	*out = ira_pec_make_val_imm_dt(ctx->pec, &ctx->pec->dt_void);

	return true;
}
static bool get_dt_bool_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	*out = ira_pec_make_val_imm_dt(ctx->pec, &ctx->pec->dt_bool);

	return true;
}
static bool get_dt_int_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	ira_int_type_t int_type = expr->opd0.int_type;

	if (int_type >= IraInt_Count) {
		pla_ast_t_report(ctx->t_ctx, L"invalid int_type for int dt:%i", int_type);
		return false;
	}

	*out = ira_pec_make_val_imm_dt(ctx->pec, &ctx->pec->dt_ints[expr->opd0.int_type]);

	return true;
}
static bool get_void_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	*out = ira_pec_make_val_imm_void(ctx->pec);

	return true;
}
static bool get_bool_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	*out = ira_pec_make_val_imm_bool(ctx->pec, expr->opd0.boolean);

	return true;
}
static bool get_cs_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	u_hs_t * data = expr->opd0.hs;
	u_hs_t * tag = expr->opd1.hs;

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
	u_hs_t * data = expr->opd0.hs;
	u_hs_t * tag = expr->opd1.hs;

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

	*out = ira_pec_make_val_null(ctx->pec, dt);

	return true;
}

static ira_lo_t * find_irid_lo(ctx_t * ctx, ira_lo_t * nspc, pla_irid_t * irid) {
	for (ira_lo_t * lo = nspc->nspc.body; lo != NULL; lo = lo->next) {
		if (lo->name == irid->name) {
			switch (lo->type) {
				case IraLoNone:
					break;
				case IraLoNspc:
					if (irid->sub_name != NULL) {
						return find_irid_lo(ctx, lo, irid->sub_name);
					}
					break;
				case IraLoFunc:
				case IraLoImpt:
				case IraLoVar:
					if (irid->sub_name == NULL) {
						return lo;
					}
					break;
				default:
					u_assert_switch(lo->type);
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
			expr->val_dt = ira_pec_get_dt_ptr(ctx->pec, lo->func->dt);
			break;
		case IraLoImpt:
			expr->val_dt = ira_pec_get_dt_ptr(ctx->pec, lo->impt.dt);
			break;
		case IraLoVar:
			expr->val_dt = lo->var.dt;
			break;
		default:
			u_assert_switch(lo->type);
	}

	expr->ident.type = ExprIdentLo;
	expr->ident.lo = lo;

	return true;
}
static bool find_optr(ctx_t * ctx, expr_t * expr, ira_dt_t * first, ira_dt_t * second) {
	expr->optr = NULL;

	for (pla_ast_t_optr_t * optr = pla_ast_t_get_optr_chain(ctx->t_ctx, expr->base->type); optr != NULL; optr = optr->next) {
		if (pla_ast_t_get_optr_dt(ctx->t_ctx, optr, first, second, &expr->val_dt)) {
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

	u_assert(expr->val_dt != NULL);

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
			case PlaExprOpdIrid:
				expr->opds[opd].irid = base->opds[opd].irid;
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
				u_assert_switch(info->opds[opd]);
		}
	}

	return true;
}
static bool translate_expr0_load_val(ctx_t * ctx, expr_t * expr, val_proc_t * proc) {
	if (!proc(ctx, expr->base, &expr->load_val.val)) {
		return false;
	}

	expr->val_dt = expr->load_val.val->dt;

	return true;
}
static bool translate_expr0_dt_tpl(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	size_t elems_size = 0;

	for (expr_t * elem_expr = expr->opd1.expr; elem_expr != NULL; elem_expr = elem_expr->opd1.expr, ++elems_size) {
		if (elem_expr->opd0.expr->val_dt != dt_dt) {
			pla_ast_t_report(ctx->t_ctx, L"dt_tpl requires 'dt' data type for [%zi]th argument", (size_t)(elems_size));
			return false;
		}
	}

	expr->dt_tpl.elems_size = elems_size;

	expr->val_dt = dt_dt;

	return true;
}
static bool translate_expr0_dt_func(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	size_t args_size = 0;

	for (expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr, ++args_size) {
		if (arg_expr->opd0.expr->val_dt != dt_dt) {
			pla_ast_t_report(ctx->t_ctx, L"dt_func requires 'dt' data type for [%zi]th argument", (size_t)(args_size));
			return false;
		}
	}

	expr->dt_func.args_size = args_size;

	if (expr->opd0.expr->val_dt != dt_dt) {
		pla_ast_t_report(ctx->t_ctx, L"dt_func requires 'dt' data type for return value");
		return false;
	}

	expr->val_dt = dt_dt;

	return true;
}
static bool translate_expr0_ident(ctx_t * ctx, expr_t * expr) {
	pla_irid_t * irid = expr->opd0.irid;

	if (irid->sub_name == NULL) {
		for (blk_t * blk = ctx->blk; blk != NULL; blk = blk->prev) {
			for (var_t * var = blk->var; var != NULL; var = var->next) {
				if (var->name == irid->name) {
					expr->val_dt = var->dt;

					expr->ident.type = ExprIdentVar;
					expr->ident.var = var;
					return true;
				}
			}
		}
	}

	for (vse_t * vse = pla_ast_t_get_vse(ctx->t_ctx); vse != NULL; vse = vse->prev) {
		switch (vse->type) {
			case PlaAstTVseNone:
				break;
			case PlaAstTVseNspc:
			{
				ira_lo_t * lo = find_irid_lo(ctx, vse->nspc.lo, irid);

				if (lo != NULL && process_ident_lo(ctx, expr, lo)) {
					return true;
				}

				break;
			}
			default:
				u_assert_switch(vse->type);
		}
	}

	pla_ast_t_report(ctx->t_ctx, L"failed to find an object for identifier [%s]", irid->name->str);
	return false;
}
static bool translate_expr0_call_func_ptr(ctx_t * ctx, expr_t * expr, ira_dt_t * callee_dt) {
	ira_dt_t * func_dt = callee_dt->ptr.body;

	size_t args_size = 0;

	ira_dt_n_t * arg_dt = func_dt->func.args, * arg_dt_end = arg_dt + func_dt->func.args_size;
	expr_t * arg_expr = expr->opd1.expr;
	for (; arg_expr != NULL && arg_dt != arg_dt_end; arg_expr = arg_expr->opd1.expr, ++arg_dt, ++args_size) {
		if (!ira_dt_is_equivalent(arg_expr->opd0.expr->val_dt, arg_dt->dt)) {
			pla_ast_t_report(ctx->t_ctx, L"data type of [%zi]th call argument does not match data type of function argument", (size_t)(args_size));
			return false;
		}
	}

	if (args_size != func_dt->func.args_size) {
		pla_ast_t_report(ctx->t_ctx, L"amount of call arguments [%zi] does not match amount of function arguments [%zi]", args_size, func_dt->func.args_size);
		return false;
	}

	expr->call.args_size = args_size;

	expr->val_dt = func_dt->func.ret;

	return true;
}
static bool translate_expr0_call(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * callee_dt = expr->opd0.expr->val_dt;

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
static bool translate_expr0_mmbr_acc(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * opd_dt = expr->opd0.expr->val_dt;

	u_hs_t * mmbr = expr->opd1.hs;

	switch (opd_dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtInt:
		case IraDtPtr:
		case IraDtFunc:
			pla_ast_t_report(ctx->t_ctx, L"operand of [%s] data type does not support member access", ira_dt_infos[opd_dt->type].type_str.str);
			return false;
		case IraDtArr:
			if (mmbr == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsSizeMmbr)) {
				expr->val_dt = ctx->pec->dt_spcl.arr_size;
			}
			else if (mmbr == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsDataMmbr)) {
				expr->val_dt = ira_pec_get_dt_ptr(ctx->pec, opd_dt->arr.body);
			}
			else {
				pla_ast_t_report(ctx->t_ctx, L"operand of [%s] data type does not support [%s] member", ira_dt_infos[opd_dt->type].type_str.str, mmbr->str);
				return false;
			}
			break;
		case IraDtTpl:
		{
			ira_dt_n_t * elem = opd_dt->tpl.elems, * elem_end = elem + opd_dt->tpl.elems_size;

			for (; elem != elem_end; ++elem) {
				if (mmbr == elem->name) {
					expr->val_dt = elem->dt;
					break;
				}
			}

			if (elem == elem_end) {
				pla_ast_t_report(ctx->t_ctx, L"operand of [%s] data type does not support [%s] member", ira_dt_infos[opd_dt->type].type_str.str, mmbr->str);
				return false;
			}
			break;
		}
		default:
			u_assert_switch(opd_dt->type);
	}

	return true;
}
static bool translate_expr0_deref(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * res_dt = expr->opd0.expr->val_dt;

	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	if (res_dt == dt_dt) {
		expr->val_dt = dt_dt;
	}
	else if (res_dt->type == IraDtPtr) {
		expr->val_dt = res_dt->ptr.body;
		pla_ast_t_report(ctx->t_ctx, L"no implementation");
		return false;
	}
	else {
		pla_ast_t_report(ctx->t_ctx, L"deref: invalid operand [%s]", ira_dt_infos[res_dt->type].type_str.str);
		return false;
	}

	return true;
}
static bool translate_expr0_addr_of(ctx_t * ctx, expr_t * expr) {
	expr_t * opd = expr->opd0.expr;

	expr->val_dt = ira_pec_get_dt_ptr(ctx->pec, opd->val_dt);

	return true;
}
static bool translate_expr0_cast(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * to_dt;

	if (expr->opd1.expr->val_dt != &ctx->pec->dt_dt
		|| !pla_ast_t_calculate_expr_dt(ctx->t_ctx, expr->opd1.expr->base, &to_dt)) {
		pla_ast_t_report(ctx->t_ctx, L"'cast-to' operand requires a valid 'dt' data type");
		return false;
	}

	//Check dt for castability?

	expr->val_dt = to_dt;

	return true;
}
static bool translate_expr0_unr(ctx_t * ctx, expr_t * expr) {
	if (!find_optr(ctx, expr, expr->opd0.expr->val_dt, NULL)) {
		return false;
	}

	return true;
}
static bool translate_expr0_bin(ctx_t * ctx, expr_t * expr) {
	if (!find_optr(ctx, expr, expr->opd0.expr->val_dt, expr->opd1.expr->val_dt)) {
		return false;
	}

	return true;
}
static bool translate_expr0_asgn(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * opd0_dt = expr->opd0.expr->val_dt, * opd1_dt = expr->opd1.expr->val_dt;

	if (!ira_dt_is_equivalent(opd0_dt, opd1_dt)) {
		pla_ast_t_report(ctx->t_ctx, L"assignment operator requires operands to be of equivalent data types");
		return false;
	}

	expr->val_dt = opd0_dt;

	return true;
}
static bool translate_expr0_tse(ctx_t * ctx, pla_expr_t * base, expr_t ** out) {
	*out = create_expr(ctx, base);

	expr_t * expr = *out;

	if (!translate_expr0_opds(ctx, expr)) {
		return false;
	}

	ira_pec_t * pec = ctx->pec;
	ira_dt_t * dt_dt = &pec->dt_dt;

	switch (base->type) {
		case PlaExprNone:
			return false;
		case PlaExprDtVoid:
			if (!translate_expr0_load_val(ctx, expr, get_dt_void_val_proc)) {
				return false;
			}
			break;
		case PlaExprDtBool:
			if (!translate_expr0_load_val(ctx, expr, get_dt_bool_val_proc)) {
				return false;
			}
			break;
		case PlaExprDtInt:
			if (!translate_expr0_load_val(ctx, expr, get_dt_int_val_proc)) {
				return false;
			}
			break;
		case PlaExprDtArr:
			if (expr->opd0.expr->val_dt != dt_dt) {
				pla_ast_t_report(ctx->t_ctx, L"make_dt_arr requires 'dt' data type for first operand");
				return false;
			}

			expr->val_dt = dt_dt;
			expr->val_type = ExprValVar;
			break;
		case PlaExprDtTpl:
			if (!translate_expr0_dt_tpl(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprDtTplElem:
			break;
		case PlaExprDtFunc:
			if (!translate_expr0_dt_func(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprDtFuncArg:
			break;
		case PlaExprValVoid:
			if (!translate_expr0_load_val(ctx, expr, get_void_val_proc)) {
				return false;
			}
			break;
		case PlaExprValBool:
			if (!translate_expr0_load_val(ctx, expr, get_bool_val_proc)) {
				return false;
			}
			break;
		case PlaExprChStr:
			if (!translate_expr0_load_val(ctx, expr, get_cs_val_proc)) {
				return false;
			}
			break;
		case PlaExprNumStr:
			if (!translate_expr0_load_val(ctx, expr, get_ns_val_proc)) {
				return false;
			}
			break;
		case PlaExprNullofDt:
			if (!translate_expr0_load_val(ctx, expr, get_null_val_proc)) {
				return false;
			}
			break;
		case PlaExprIdent:
			if (!translate_expr0_ident(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprCall:
			if (!translate_expr0_call(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprCallArg:
			break;
			//case PlaExprSubscr:
		case PlaExprMmbrAcc:
			if (!translate_expr0_mmbr_acc(ctx, expr)) {
				return false;
			}
			break;
			//case PlaExprPostInc:
			//case PlaExprPostDec:
		case PlaExprDeref:
			if (!translate_expr0_deref(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprAddrOf:
			if (!translate_expr0_addr_of(ctx, expr)) {
				return false;
			}
			break;
			//case PlaExprPreInc:
			//case PlaExprPreDec:
		case PlaExprCast:
			if (!translate_expr0_cast(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprLogicNeg:
		case PlaExprBitNeg:
		case PlaExprArithNeg:
			if (!translate_expr0_unr(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprMul:
		case PlaExprDiv:
		case PlaExprMod:
		case PlaExprAdd:
		case PlaExprSub:
		case PlaExprLeShift:
		case PlaExprRiShift:
		case PlaExprLess:
		case PlaExprLessEq:
		case PlaExprGrtr:
		case PlaExprGrtrEq:
		case PlaExprEq:
		case PlaExprNeq:
		case PlaExprBitAnd:
		case PlaExprBitXor:
		case PlaExprBitOr:
			if (!translate_expr0_bin(ctx, expr)) {
				return false;
			}
			break;
			//case PlaExprLogicAnd:
			//case PlaExprLogicOr:
		case PlaExprAsgn:
			if (!translate_expr0_asgn(ctx, expr)) {
				return false;
			}
			break;
		default:
			u_assert_switch(base->type);
	}

	return true;
}
static bool translate_expr0(ctx_t * ctx, pla_expr_t * base, expr_t ** out) {
	tse_t tse = { .type = PlaAstTTseExpr, .expr = base };

	pla_ast_t_push_tse(ctx->t_ctx, &tse);

	bool result = translate_expr0_tse(ctx, base, out);

	pla_ast_t_pop_tse(ctx->t_ctx);

	return result;
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
		case ExprValDeref:
		{
			ira_inst_t read_ptr = { .type = IraInstReadPtr, .opd1.hs = expr->val.var->inst_name };

			push_inst_imm_var0(ctx, &read_ptr, expr->val_dt, out);
			break;
		}
		default:
			u_assert_switch(expr->val_type);
	}

	return true;
}

static bool translate_expr1_load_val(ctx_t * ctx, expr_t * expr) {
	ira_inst_t load_val = { .type = IraInstLoadVal, .opd1.val = expr->load_val.val };

	push_inst_imm_var0_expr(ctx, expr, &load_val);

	expr->load_val.val = NULL;

	return true;
}
static bool translate_expr1_dt_arr(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	var_t * opd_var;

	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var)) {
		return false;
	}

	ira_inst_t make_dt_ptr = { .type = IraInstMakeDtArr, .opd1.hs = opd_var->inst_name };

	push_inst_imm_var0_expr(ctx, expr, &make_dt_ptr);

	return true;
}
static bool translate_expr1_dt_tpl(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	size_t elems_size = expr->dt_tpl.elems_size;

	u_hs_t ** elems = malloc(elems_size * sizeof(*elems));
	u_hs_t ** ids = malloc(elems_size * sizeof(*ids));

	u_assert(elems != NULL && ids != NULL);

	u_hs_t ** elem = elems;
	u_hs_t ** id = ids;
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

	ira_inst_t make_dt_func = { .type = IraInstMakeDtTpl, .opd2.size = elems_size, .opd3.hss = elems, .opd4.hss = ids };

	push_inst_imm_var0_expr(ctx, expr, &make_dt_func);

	return true;
}
static bool translate_expr1_dt_func(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	size_t args_size = expr->dt_func.args_size;

	u_hs_t ** args = malloc(args_size * sizeof(*args));
	u_hs_t ** ids = malloc(args_size * sizeof(*ids));

	u_assert(args != NULL && ids != NULL);

	u_hs_t ** arg = args;
	u_hs_t ** id = ids;
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
					ira_inst_t load_val = { .type = IraInstLoadVal, .opd1.val = ira_pec_make_val_lo_ptr(ctx->pec, expr->ident.lo) };

					push_inst_imm_var0_expr(ctx, expr, &load_val);
					break;
				}
				case IraLoVar:
				{
					var_t * lo_ptr_var;

					ira_inst_t load_val = { .type = IraInstLoadVal, .opd1.val = ira_pec_make_val_lo_ptr(ctx->pec, expr->ident.lo) };

					push_inst_imm_var0(ctx, &load_val, load_val.opd1.val->dt, &lo_ptr_var);

					expr->val_type = ExprValDeref;
					expr->val.var = lo_ptr_var;

					break;
				}
				default:
					u_assert_switch(expr->ident.lo->type);
			}
			break;
		default:
			u_assert_switch(expr->val_type);
	}

	return true;
}
static bool translate_expr1_call_func_ptr(ctx_t * ctx, expr_t * expr, var_t * callee) {
	ira_dt_t * func_dt = callee->dt->ptr.body;

	size_t args_size = expr->call.args_size;

	u_hs_t ** args = malloc(args_size * sizeof(*args));

	u_hs_t ** arg = args;
	ira_dt_n_t * arg_dt = func_dt->func.args;
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

	ira_dt_t * callee_dt = callee_var->dt;

	if (callee_dt->type == IraDtPtr && callee_dt->ptr.body->type == IraDtFunc) {
		if (!translate_expr1_call_func_ptr(ctx, expr, callee_var)) {
			return false;
		}
	}
	else {
		u_assert_switch(callee_dt);
	}

	return true;
}
static bool translate_expr1_mmbr_acc(ctx_t * ctx, expr_t * expr) {
	expr_t * opd = expr->opd0.expr;

	if (!translate_expr1(ctx, opd)) {
		return false;
	}

	var_t * ptr_var;

	switch (opd->val_type) {
		case ExprValImmVar:
		case ExprValVar:
		{
			ira_inst_t addr_of = { .type = IraInstAddrOf, .opd1.hs = opd->val.var->inst_name };

			push_inst_imm_var0(ctx, &addr_of, ira_pec_get_dt_ptr(ctx->pec, opd->val.var->dt), &ptr_var);

			break;
		}
		case ExprValDeref:
		{
			ptr_var = opd->val.var;
			break;
		}
		default:
			u_assert_switch(opd->val_type);
	}

	var_t * ptr_mmbr_var;

	ira_inst_t mmbr_acc_ptr = { .type = IraInstMmbrAccPtr, .opd1.hs = ptr_var->inst_name, .opd2.hs = expr->opd1.hs };

	push_inst_imm_var0(ctx, &mmbr_acc_ptr, ira_pec_get_dt_ptr(ctx->pec, expr->val_dt), &ptr_mmbr_var);

	switch (opd->val_type) {
		case ExprValImmVar:
		{
			ira_inst_t read_ptr = { .type = IraInstReadPtr, .opd1.hs = ptr_mmbr_var->inst_name };

			push_inst_imm_var0_expr(ctx, expr, &read_ptr);

			break;
		}
		case ExprValVar:
		case ExprValDeref:
			expr->val_type = ExprValDeref;
			expr->val.var = ptr_mmbr_var;
			break;
		default:
			u_assert_switch(opd->val_type);
	}
	return true;
}
static bool translate_expr1_deref(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * res_dt = expr->opd0.expr->val_dt;

	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	if (res_dt == dt_dt) {
		var_t * body_var;

		if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &body_var)) {
			return false;
		}

		ira_inst_t make_dt_ptr = { .type = IraInstMakeDtPtr, .opd1.hs = body_var->inst_name };

		push_inst_imm_var0_expr(ctx, expr, &make_dt_ptr);
	}
	else if (res_dt->type == IraDtPtr) {
		var_t * opd_var;

		if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var)) {
			return false;
		}

		expr->val_type = ExprValDeref;
		expr->val.var = opd_var;
	}
	else {
		u_assert(false);
	}

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
			u_assert_switch(opd->val_type);
	}

	return true;
}
static bool translate_expr1_cast(ctx_t * ctx, expr_t * expr) {
	var_t * opd_var;

	if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var)) {
		return false;
	}

	ira_inst_t cast = { .type = IraInstCast, .opd1.hs = opd_var->inst_name, .opd2.dt = expr->val_dt };

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
		case PlaAstTOptrUnrInstInt:
		{
			ira_inst_t unr_op = { .type = optr->unr_inst_int.inst_type, .opd1.hs = opd_var->inst_name };

			push_inst_imm_var0_expr(ctx, expr, &unr_op);

			break;
		}
		default:
			u_assert_switch(optr->type);
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
			u_assert_switch(optr->type);
	}

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
			u_assert_switch(opd0->val_type);
	}

	expr->val_type = opd0->val_type;
	expr->val = opd0->val;

	return true;
}
static bool translate_expr1_tse(ctx_t * ctx, expr_t * expr) {
	switch (expr->base->type) {
		case PlaExprNone:
			return false;
		case PlaExprDtVoid:
		case PlaExprDtBool:
		case PlaExprDtInt:
			if (!translate_expr1_load_val(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprDtArr:
			if (!translate_expr1_dt_arr(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprDtTpl:
			if (!translate_expr1_dt_tpl(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprDtTplElem:
			break;
		case PlaExprDtFunc:
			if (!translate_expr1_dt_func(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprDtFuncArg:
			return false;
		case PlaExprValVoid:
		case PlaExprValBool:
		case PlaExprChStr:
		case PlaExprNumStr:
		case PlaExprNullofDt:
			if (!translate_expr1_load_val(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprIdent:
			if (!translate_expr1_ident(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprCall:
			if (!translate_expr1_call(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprCallArg:
			return false;
			//case PlaExprSubscr:
		case PlaExprMmbrAcc:
			if (!translate_expr1_mmbr_acc(ctx, expr)) {
				return false;
			}
			break;
			//case PlaExprPostInc:
			//case PlaExprPostDec:
		case PlaExprDeref:
			if (!translate_expr1_deref(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprAddrOf:
			if (!translate_expr1_addr_of(ctx, expr)) {
				return false;
			}
			break;
			//case PlaExprPreInc:
			//case PlaExprPreDec:
		case PlaExprCast:
			if (!translate_expr1_cast(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprLogicNeg:
		case PlaExprBitNeg:
		case PlaExprArithNeg:
			if (!translate_expr1_unr(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprMul:
		case PlaExprDiv:
		case PlaExprMod:
		case PlaExprAdd:
		case PlaExprSub:
		case PlaExprLeShift:
		case PlaExprRiShift:
		case PlaExprLess:
		case PlaExprLessEq:
		case PlaExprGrtr:
		case PlaExprGrtrEq:
		case PlaExprEq:
		case PlaExprNeq:
		case PlaExprBitAnd:
		case PlaExprBitXor:
		case PlaExprBitOr:
			if (!translate_expr1_bin(ctx, expr)) {
				return false;
			}
			break;
			//case PlaExprLogicAnd:
			//case PlaExprLogicOr:
		case PlaExprAsgn:
			if (!translate_expr1_asgn(ctx, expr)) {
				return false;
			}
			break;
		default:
			u_assert_switch(expr->base->type);
	}

	return true;
}
static bool translate_expr1(ctx_t * ctx, expr_t * expr) {
	tse_t tse = { .type = PlaAstTTseExpr, .expr = expr->base };

	pla_ast_t_push_tse(ctx->t_ctx, &tse);

	bool result = translate_expr1_tse(ctx, expr);

	pla_ast_t_pop_tse(ctx->t_ctx);

	return result;
}

static bool translate_expr(ctx_t * ctx, pla_expr_t * pla_expr, var_t ** out) {
	expr_t * expr;

	bool result = false;

	do {
		if (!translate_expr0(ctx, pla_expr, &expr)) {
			break;
		}

		if (!translate_expr1_imm_var(ctx, expr, out)) {
			break;
		}

		result = true;
	} while (false);

	destroy_expr(ctx, expr);

	return result;
}

static bool translate_stmt(ctx_t * ctx, pla_stmt_t * stmt);

static bool translate_stmt_blk_blk(ctx_t * ctx, pla_stmt_t * stmt) {
	for (pla_stmt_t * it = stmt->block.body; it != NULL; it = it->next) {
		if (!translate_stmt(ctx, it)) {
			return false;
		}
	}

	return true;
}
static bool translate_stmt_blk(ctx_t * ctx, pla_stmt_t * stmt) {
	blk_t blk = { 0 };

	push_blk(ctx, &blk);

	bool result = translate_stmt_blk_blk(ctx, stmt);

	pop_blk(ctx);

	return result;
}
static bool translate_stmt_expr(ctx_t * ctx, pla_stmt_t * stmt) {
	var_t * res;

	if (!translate_expr(ctx, stmt->expr.expr, &res)) {
		return false;
	}

	return true;
}
static bool translate_stmt_var_dt(ctx_t * ctx, pla_stmt_t * stmt) {
	ira_dt_t * dt;

	if (!pla_ast_t_calculate_expr_dt(ctx->t_ctx, stmt->var_dt.dt_expr, &dt)) {
		return false;
	}

	var_t * var = define_var(ctx, stmt->var_dt.name, dt, true);

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

	var_t * var = define_var(ctx, stmt->var_val.name, val_var->dt, true);

	if (var == NULL) {
		pla_ast_t_report(ctx->t_ctx, L"there is already variable of same name [%s] in this block", stmt->var_val.name->str);
		return false;
	}

	push_def_var_inst(ctx, var);

	ira_inst_t copy = { .type = IraInstCopy, .opd0.hs = var->inst_name, .opd1.hs = val_var->inst_name };

	ira_func_push_inst(ctx->func, &copy);

	return true;
}
static bool translate_stmt_cond(ctx_t * ctx, pla_stmt_t * stmt) {
	var_t * cond_var;

	if (!translate_expr(ctx, stmt->cond.cond_expr, &cond_var)) {
		return false;
	}

	if (cond_var->dt != &ctx->pec->dt_bool) {
		pla_ast_t_report(ctx->t_ctx, L"condition expression mush have a boolean data type");
		return false;
	}

	size_t label_index = ctx->unq_label_index++;

	u_hs_t * exit_label;

	{
		u_hs_t * tc_end = get_unq_label_name(ctx, &label_cond_base, label_index, &label_cond_tc_end);

		ira_inst_t brf = { .type = IraInstBrf, .opd0.hs = tc_end, .opd1.hs = cond_var->inst_name };

		ira_func_push_inst(ctx->func, &brf);

		if (!translate_stmt(ctx, stmt->cond.true_br)) {
			return false;
		}

		exit_label = tc_end;
	}

	if (stmt->cond.false_br != false) {
		u_hs_t * fc_end = get_unq_label_name(ctx, &label_cond_base, label_index, &label_cond_fc_end);

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
static bool translate_stmt_pre_loop(ctx_t * ctx, pla_stmt_t * stmt) {
	size_t label_index = ctx->unq_label_index++;

	u_hs_t * cond_label = get_unq_label_name(ctx, &label_pre_loop_base, label_index, &label_pre_loop_cond);
	u_hs_t * exit_label = get_unq_label_name(ctx, &label_pre_loop_base, label_index, &label_pre_loop_exit);

	push_def_label_inst(ctx, cond_label);

	var_t * cond_var;

	if (!translate_expr(ctx, stmt->cond.cond_expr, &cond_var)) {
		return false;
	}

	{
		ira_inst_t brf = { .type = IraInstBrf, .opd0.hs = exit_label, .opd1.hs = cond_var->inst_name };

		ira_func_push_inst(ctx->func, &brf);
	}

	if (!translate_stmt(ctx, stmt->pre_loop.body)) {
		return false;
	}

	{
		ira_inst_t bru = { .type = IraInstBru, .opd0.hs = cond_label };

		ira_func_push_inst(ctx->func, &bru);
	}

	push_def_label_inst(ctx, exit_label);

	return true;
}
static bool translate_stmt_ret(ctx_t * ctx, pla_stmt_t * stmt) {
	var_t * ret_var;

	if (!translate_expr(ctx, stmt->expr.expr, &ret_var)) {
		return false;
	}

	if (ctx->func_ret != NULL) {
		if (!ira_dt_is_equivalent(ctx->func_ret, ret_var->dt)) {
			pla_ast_t_report(ctx->t_ctx, L"return data type does not match to function return data type");
			return false;
		}
	}
	else {
		ctx->func_ret = ret_var->dt;
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
		case PlaStmtRet:
			if (!translate_stmt_ret(ctx, stmt)) {
				return false;
			}
			break;
		default:
			u_assert_switch(stmt->type);
	}

	return true;
}
static bool translate_stmt(ctx_t * ctx, pla_stmt_t * stmt) {
	tse_t tse = { .type = PlaAstTTseStmt, .stmt = stmt };

	pla_ast_t_push_tse(ctx->t_ctx, &tse);

	bool result = translate_stmt_tse(ctx, stmt);

	pla_ast_t_pop_tse(ctx->t_ctx);

	return result;
}

static bool translate_args_blk(ctx_t * ctx) {
	if (ctx->func_dt != NULL) {
		ctx->func_ret = ctx->func_dt->func.ret;

		for (ira_dt_n_t * arg = ctx->func_dt->func.args, *arg_end = arg + ctx->func_dt->func.args_size; arg != arg_end; ++arg) {
			var_t * var = define_var(ctx, arg->name, arg->dt, false);

			u_assert(var != NULL);
		}
	}

	if (!translate_stmt(ctx, ctx->stmt)) {
		return false;
	}

	if (ctx->func->dt == NULL) {
		if (ctx->func_ret == NULL) {
			return false;
		}

		ctx->func->dt = ira_pec_get_dt_func(ctx->pec, ctx->func_ret, 0, NULL);
	}

	return true;
}
static bool translate_args(ctx_t * ctx) {
	blk_t blk = { 0 };

	push_blk(ctx, &blk);

	bool result = translate_args_blk(ctx);

	pop_blk(ctx);

	return result;
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

	bool result = translate_core(&ctx);

	return result;
}
