#include "pch.h"
#include "pla_ast_t_s.h"
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

typedef struct ev {
	var_t * var;
	bool is_lv;
} ev_t;

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

typedef struct expr expr_t;
typedef union expr_opd {
	bool val_bool;
	ira_int_type_t int_type;
	u_hs_t * hs;
	expr_t * expr;
} expr_opd_t;
typedef enum expr_ident_type {
	ExprIdentNone,
	ExprIdentLoVal,
	ExprIdentVar,
	ExprIdent_Count
} expr_ident_type_t;
struct expr {
	pla_expr_t * base;
	union {
		expr_opd_t opds[PLA_EXPR_OPDS_SIZE];
		struct {
			expr_opd_t opd0, opd1, opd2;
		};
	};
	ira_dt_t * res_dt;
	bool is_lv;

	union {
		struct {
			ira_val_t * val;
		} load_val;
		struct {
			size_t args_size;
		} make_dt_func;
		struct {
			expr_ident_type_t type;
			union {
				ira_val_t * lo_val;
				var_t * var;
			};
		} ident;
		struct {
			size_t args_size;
		} call;
		struct {
			pla_ast_t_optr_t * optr;
		} bin_op;
	};
};

typedef bool val_proc_t(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out);


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
			case PlaExprOpdValBool:
			case PlaExprOpdIntType:
			case PlaExprOpdHs:
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
		case PlaExprIdent:
			switch (expr->ident.type) {
				case ExprIdentLoVal:
					ira_val_destroy(expr->ident.lo_val);
					expr->ident.lo_val = NULL;
					break;
				case ExprIdentVar:
					break;
				default:
					u_assert_switch(expr->ident.type);
			}
			break;
	}

	free(expr);
}

static void push_def_var_inst(ctx_t * ctx, var_t * var) {
	ira_inst_t def_var = { .type = IraInstDefVar, .opd0.hs = var->inst_name, .opd1.dt = var->dt };

	ira_func_push_inst(ctx->func, &def_var);
}
static void push_def_label_inst(ctx_t * ctx, u_hs_t * name) {
	ira_inst_t def_label = { .type = IraInstDefLabel, .opd0.hs = name };

	ira_func_push_inst(ctx->func, &def_label);
}
static void push_inst_imm_var0(ctx_t * ctx, ira_inst_t * inst, ev_t * out, ira_dt_t * dt) {
	var_t * out_var = get_imm_var(ctx, dt);

	push_def_var_inst(ctx, out_var);

	inst->opd0.hs = out_var->inst_name;

	ira_func_push_inst(ctx->func, inst);

	*out = (ev_t){ .var = out_var };
}

static bool get_cs_ascii(ctx_t * ctx, u_hs_t * str, ira_val_t ** out) {
	ira_val_t * val = ira_val_create(IraValImmArr, ira_pec_get_dt_arr(ctx->pec, &ctx->pec->dt_ints[IraIntU8]));

	val->arr.size = str->size + 1;

	val->arr.data = malloc(val->arr.size * sizeof(*val->arr.data));

	u_assert(val->arr.data != NULL);

	memset(val->arr.data, 0, val->arr.size * sizeof(*val->arr.data));

	ira_val_t ** ins = val->arr.data;
	for (wchar_t * ch = str->str, *ch_end = ch + str->size; ch != ch_end;) {
		if ((*ch & ~0x7F) != 0) {
			pla_ast_t_report(ctx->t_ctx, L"non-ascii character [%c] in ascii string:\n%s", *ch, str->str);
			ira_val_destroy(val);
			return false;
		}

		uint8_t val = (uint8_t)*ch;

		if (*ch == L'\\') {
			++ch;

			if (ch == ch_end) {
				pla_ast_t_report(ctx->t_ctx, L"invalid escape sequence in ascii string:\n%s", str->str);
				return false;
			}

			switch (*ch) {
				case L'n':
					val = L'\n';
					break;
				default:
					pla_ast_t_report(ctx->t_ctx, L"invalid character [%c] of escape sequence in ascii string:\n%s", *ch, str->str);
					break;
			}
		}

		*ins++ = ira_pec_make_val_imm_int(ctx->pec, IraIntU8, (ira_int_t) {
			.ui8 = val
		});
		ch++;
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

		cur++;
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
static bool get_bool_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	*out = ira_pec_make_val_imm_bool(ctx->pec, expr->opd0.val_bool);

	return true;
}
static bool get_cs_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	u_hs_t * data = expr->opd0.hs;
	u_hs_t * tag = expr->opd1.hs;

	if (tag == ctx->pec->pds[IraPdsAsciiStrTag]) {
		if (!get_cs_ascii(ctx, data, out)) {
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

	switch (dt->type) {
		case IraDtPtr:
			*out = ira_val_create(IraValNullPtr, dt);
			break;
		default:
			u_assert_switch(dt->type);
	}

	return true;
}

static bool translate_expr0(ctx_t * ctx, pla_expr_t * base, expr_t ** out);

static bool translate_expr0_opds(ctx_t * ctx, expr_t * expr) {
	pla_expr_t * base = expr->base;

	const pla_expr_info_t * info = &pla_expr_infos[base->type];

	for (size_t opd = 0; opd < PLA_EXPR_OPDS_SIZE; ++opd) {
		switch (info->opds[opd]) {
			case PlaExprOpdNone:
				break;
			case PlaExprOpdValBool:
				expr->opds[opd].val_bool = base->opds[opd].val_bool;
				break;
			case PlaExprOpdIntType:
				expr->opds[opd].int_type = base->opds[opd].int_type;
				break;
			case PlaExprOpdHs:
				expr->opds[opd].hs = base->opds[opd].hs;
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

	expr->res_dt = expr->load_val.val->dt;

	return true;
}
static bool translate_expr0_make_dt_func(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	size_t args_size = 0;

	for (expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr, ++args_size) {
		if (arg_expr->opd0.expr->res_dt != dt_dt) {
			pla_ast_t_report(ctx->t_ctx, L"make_dt_func requires 'dt' data type for [%zi]th argument", (size_t)(args_size));
			return false;
		}
	}

	expr->make_dt_func.args_size = args_size;

	if (expr->opd0.expr->res_dt != dt_dt) {
		pla_ast_t_report(ctx->t_ctx, L"make_dt_func requires 'dt' data type for return value");
		return false;
	}

	expr->res_dt = dt_dt;

	return true;
}
static bool translate_expr0_ident(ctx_t * ctx, expr_t * expr) {
	u_hs_t * ident = expr->opd0.hs;

	for (vse_t * vse = pla_ast_t_get_vse(ctx->t_ctx); vse != NULL; vse = vse->prev) {
		switch (vse->type) {
			case PlaAstTVseNone:
				break;
			case PlaAstTVseNspc:
				for (ira_lo_t * lo = vse->nspc.lo->nspc.body; lo != NULL; lo = lo->next) {
					if (lo->name == ident) {
						switch (lo->type) {
							case IraLoNone:
								pla_ast_t_report(ctx->t_ctx, L"identifier [%s] can not reference 'None' language object", ident->str);
								return false;
							case IraLoFunc:
							case IraLoImpt:
							{
								expr->ident.type = ExprIdentLoVal;
								expr->ident.lo_val = ira_pec_make_val_lo_ptr(ctx->pec, lo);

								expr->res_dt = expr->ident.lo_val->dt;

								return true;
							}
							default:
								u_assert_switch(lo->type);
						}
					}
				}
				break;
			default:
				u_assert_switch(vse->type);
		}
	}

	for (blk_t * blk = ctx->blk; blk != NULL; blk = blk->prev) {
		for (var_t * var = blk->var; var != NULL; var = var->next) {
			if (var->name == ident) {
				expr->ident.type = ExprIdentVar;
				expr->ident.var = var;

				expr->res_dt = var->dt;
				expr->is_lv = true;
				return true;
			}
		}
	}

	pla_ast_t_report(ctx->t_ctx, L"failed to find an object for identifier [%s]", ident->str);
	return false;
}
static bool translate_expr0_call_func_ptr(ctx_t * ctx, expr_t * expr, ira_dt_t * callee_dt) {
	ira_dt_t * func_dt = callee_dt->ptr.body;

	size_t args_size = 0;

	ira_dt_n_t * arg_dt = func_dt->func.args, * arg_dt_end = arg_dt + func_dt->func.args_size;
	expr_t * arg_expr = expr->opd1.expr;
	for (; arg_expr != NULL && arg_dt != arg_dt_end; arg_expr = arg_expr->opd1.expr, ++arg_dt, ++args_size) {
		if (!ira_dt_is_equivalent(arg_expr->opd0.expr->res_dt, arg_dt->dt)) {
			pla_ast_t_report(ctx->t_ctx, L"data type of [%zi]th call argument does not match data type of function argument", (size_t)(args_size));
			return false;
		}
	}

	if (args_size != func_dt->func.args_size) {
		pla_ast_t_report(ctx->t_ctx, L"amount of call arguments [%zi] does not match amount of function arguments [%zi]", args_size, func_dt->func.args_size);
		return false;
	}

	expr->call.args_size = args_size;

	expr->res_dt = func_dt->func.ret;

	return true;
}
static bool translate_expr0_call(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * callee_dt = expr->opd0.expr->res_dt;

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
	ira_dt_t * opd_dt = expr->opd0.expr->res_dt;

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
				expr->res_dt = ctx->pec->dt_spcl.arr_size;
			}
			else if (mmbr == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsDataMmbr)) {
				expr->res_dt = ira_pec_get_dt_ptr(ctx->pec, opd_dt->arr.body);
			}
			else {
				pla_ast_t_report(ctx->t_ctx, L"operand of [%s] data type does not support [%s] member", ira_dt_infos[opd_dt->type].type_str.str, mmbr->str);
				return false;
			}
			break;
		default:
			u_assert_switch(opd_dt->type);
	}

	return true;
}
static bool translate_expr0_addr_of(ctx_t * ctx, expr_t * expr) {
	expr_t * opd = expr->opd0.expr;

	if (opd->is_lv) {
		if (!ira_dt_is_ptr_dt_comp(opd->res_dt)) {
			pla_ast_t_report(ctx->t_ctx, L"address_of does not support operand of [%s] data type", ira_dt_infos[opd->res_dt->type].type_str.str);
			return false;
		}

		expr->res_dt = ira_pec_get_dt_ptr(ctx->pec, opd->res_dt);
	}
	else {
		pla_ast_t_report(ctx->t_ctx, L"address_of requires left operand to be an lvalue");
		return false;
	}

	return true;
}
static bool translate_expr0_cast(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * to_dt;

	if (expr->opd1.expr->res_dt != &ctx->pec->dt_dt
		|| !pla_ast_t_calculate_expr_dt(ctx->t_ctx, expr->opd1.expr->base, &to_dt)) {
		pla_ast_t_report(ctx->t_ctx, L"'cast-to' operand requires a valid 'dt' data type");
		return false;
	}

	//Check dt for castability?

	expr->res_dt = to_dt;

	return true;
}
static bool translate_expr0_bin(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * opd0_dt = expr->opd0.expr->res_dt, * opd1_dt = expr->opd1.expr->res_dt;

	expr->bin_op.optr = NULL;

	pla_ast_t_optr_t * optr = pla_ast_t_get_optr_chain(ctx->t_ctx, expr->base->type);
	ira_dt_t * out_dt = NULL;

	while (optr != NULL) {
		if (pla_ast_t_get_optr_dt(ctx->t_ctx, optr, opd0_dt, opd1_dt, &out_dt)) {
			expr->bin_op.optr = optr;
			break;
		}
	}

	if (expr->bin_op.optr == NULL) {
		pla_ast_t_report(ctx->t_ctx, L"could not find an operator for [%s] expression with [%s], [%s] operands", pla_expr_infos[expr->base->type].type_str.str, ira_dt_infos[opd0_dt->type].type_str.str, ira_dt_infos[opd1_dt->type].type_str.str);
		return false;
	}

	u_assert(out_dt != NULL);

	expr->res_dt = out_dt;

	return true;
}
static bool translate_expr0_asgn(ctx_t * ctx, expr_t * expr) {
	ira_dt_t * opd0_dt = expr->opd0.expr->res_dt, * opd1_dt = expr->opd1.expr->res_dt;

	if (!ira_dt_is_equivalent(opd0_dt, opd1_dt)) {
		pla_ast_t_report(ctx->t_ctx, L"assignment operator requires operands to be of equivalent data types");
		return false;
	}

	if (!expr->opd0.expr->is_lv) {
		pla_ast_t_report(ctx->t_ctx, L"assignment requires left operand to be an lvalue");
		return false;
	}

	expr->res_dt = opd0_dt;

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
		case PlaExprDtPtr:
			if (expr->opd0.expr->res_dt != dt_dt) {
				pla_ast_t_report(ctx->t_ctx, L"make_dt_ptr requires 'dt' data type for first operand");
				return false;
			}

			expr->res_dt = dt_dt;
			break;
		case PlaExprDtArr:
			if (expr->opd0.expr->res_dt != dt_dt) {
				pla_ast_t_report(ctx->t_ctx, L"make_dt_arr requires 'dt' data type for first operand");
				return false;
			}

			expr->res_dt = dt_dt;
			break;
		case PlaExprDtFunc:
			if (!translate_expr0_make_dt_func(ctx, expr)) {
				return false;
			}
			break;
		case PlaExprDtFuncArg:
			break;
			//case PlaExprValVoid:
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
			//case PlaExprDeref:
		case PlaExprAddrOf:
			if (!translate_expr0_addr_of(ctx, expr)) {
				return false;
			}
			break;
			//case PlaExprPreInc:
			//case PlaExprPreDec:
			//case PlaExprLogicNeg:
		case PlaExprCast:
			if (!translate_expr0_cast(ctx, expr)) {
				return false;
			}
			break;
			//case PlaExprBitNeg:
			//case PlaExprArithNeg:
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

static bool translate_expr1(ctx_t * ctx, expr_t * expr, ev_t * out);

static bool translate_expr1_load_val(ctx_t * ctx, expr_t * expr, ev_t * out) {
	ira_inst_t load = { .type = IraInstLoadVal, .opd1.val = expr->load_val.val };

	push_inst_imm_var0(ctx, &load, out, load.opd1.val->dt);

	expr->load_val.val = NULL;

	return true;
}
static bool translate_expr1_make_dt_ptr(ctx_t * ctx, expr_t * expr, ev_t * out) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	ev_t ev_body;

	if (!translate_expr1(ctx, expr->opd0.expr, &ev_body)) {
		return false;
	}

	u_assert(ev_body.var->dt == dt_dt);

	ira_inst_t make_dt_ptr = { .type = IraInstMakeDtPtr, .opd1.hs = ev_body.var->inst_name };

	push_inst_imm_var0(ctx, &make_dt_ptr, out, dt_dt);

	return true;
}
static bool translate_expr1_make_dt_arr(ctx_t * ctx, expr_t * expr, ev_t * out) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	ev_t ev_body;

	if (!translate_expr1(ctx, expr->opd0.expr, &ev_body)) {
		return false;
	}

	u_assert(ev_body.var->dt == dt_dt);

	ira_inst_t make_dt_ptr = { .type = IraInstMakeDtArr, .opd1.hs = ev_body.var->inst_name };

	push_inst_imm_var0(ctx, &make_dt_ptr, out, dt_dt);

	return true;
}
static bool translate_expr1_make_dt_func(ctx_t * ctx, expr_t * expr, ev_t * out) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	size_t args_size = expr->make_dt_func.args_size;

	u_hs_t ** args = malloc(args_size * sizeof(*args));
	u_hs_t ** ids = malloc(args_size * sizeof(*ids));

	u_hs_t ** arg = args;
	u_hs_t ** id = ids;
	for (expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr, ++arg, ++id) {
		ev_t ev_arg;

		if (!translate_expr1(ctx, arg_expr->opd0.expr, &ev_arg)) {
			free(args);
			free(ids);
			return false;
		}

		u_assert(ev_arg.var->dt == dt_dt);

		*arg = ev_arg.var->inst_name;
		*id = arg_expr->opd2.hs;
	}

	ev_t ev_ret;

	if (!translate_expr1(ctx, expr->opd0.expr, &ev_ret)) {
		free(args);
		free(ids);
		return false;
	}

	u_assert(ev_ret.var->dt == dt_dt);

	ira_inst_t make_dt_func = { .type = IraInstMakeDtFunc, .opd1.hs = ev_ret.var->inst_name, .opd2.size = args_size, .opd3.hss = args, .opd4.hss = ids };

	push_inst_imm_var0(ctx, &make_dt_func, out, dt_dt);

	return true;
}
static bool translate_expr1_ident(ctx_t * ctx, expr_t * expr, ev_t * out) {
	switch (expr->ident.type) {
		//case ExprIdentNone: it is counted in default: case
		case ExprIdentLoVal:
			ira_inst_t load = { .type = IraInstLoadVal, .opd1.val = expr->ident.lo_val };

			push_inst_imm_var0(ctx, &load, out, load.opd1.val->dt);

			expr->ident.lo_val = NULL;

			return true;
		case ExprIdentVar:
			*out = (ev_t){ .var = expr->ident.var, .is_lv = true };

			return true;
		default:
			u_assert_switch(expr->ident.type);
	}
}
static bool translate_expr1_call_func_ptr(ctx_t * ctx, expr_t * expr, ev_t * out, ev_t * callee) {
	ira_dt_t * func_dt = callee->var->dt->ptr.body;

	size_t args_size = expr->call.args_size;

	u_assert(args_size == func_dt->func.args_size);

	u_hs_t ** args = malloc(args_size * sizeof(*args));

	u_hs_t ** arg = args;
	ira_dt_n_t * arg_dt = func_dt->func.args;
	for (expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr, ++arg, ++arg_dt) {
		ev_t ev_arg;

		if (!translate_expr1(ctx, arg_expr->opd0.expr, &ev_arg)) {
			free(args);
			return false;
		}

		u_assert(ira_dt_is_equivalent(ev_arg.var->dt, arg_dt->dt));

		*arg = ev_arg.var->inst_name;
	}

	ira_inst_t call = { .type = IraInstCallFuncPtr, .opd1.hs = callee->var->inst_name, .opd2.size = args_size, .opd3.hss = args };

	push_inst_imm_var0(ctx, &call, out, func_dt->func.ret);

	return true;
}
static bool translate_expr1_call(ctx_t * ctx, expr_t * expr, ev_t * out) {
	ev_t callee;

	if (!translate_expr1(ctx, expr->opd0.expr, &callee)) {
		return false;
	}

	ira_dt_t * callee_dt = callee.var->dt;

	if (callee_dt->type == IraDtPtr && callee_dt->ptr.body->type == IraDtFunc) {
		if (!translate_expr1_call_func_ptr(ctx, expr, out, &callee)) {
			return false;
		}
	}
	else {
		u_assert_switch(callee_dt);
	}

	return true;
}
static bool translate_expr1_mmbr_acc(ctx_t * ctx, expr_t * expr, ev_t * out) {
	ev_t opd_ev;

	if (!translate_expr1(ctx, expr->opd0.expr, &opd_ev)) {
		return false;
	}

	ira_dt_t * opd_dt = opd_ev.var->dt;

	ira_inst_t mmbr_acc = { .type = IraInstMmbrAcc, .opd1.hs = opd_ev.var->inst_name, .opd2.hs = expr->opd1.hs };

	push_inst_imm_var0(ctx, &mmbr_acc, out, expr->res_dt);

	return true;
}
static bool translate_expr1_addr_of(ctx_t * ctx, expr_t * expr, ev_t * out) {
	ev_t opd_ev;

	if (!translate_expr1(ctx, expr->opd0.expr, &opd_ev)) {
		return false;
	}

	u_assert(opd_ev.is_lv);

	u_assert(ira_dt_is_ptr_dt_comp(opd_ev.var->dt));

	ira_inst_t addr_of = { .type = IraInstAddrOf, .opd1.hs = opd_ev.var->inst_name };

	push_inst_imm_var0(ctx, &addr_of, out, expr->res_dt);

	return true;
}
static bool translate_expr1_cast(ctx_t * ctx, expr_t * expr, ev_t * out) {
	ev_t opd_ev;

	if (!translate_expr1(ctx, expr->opd0.expr, &opd_ev)) {
		return false;
	}

	ira_inst_t cast = { .type = IraInstCast, .opd1.hs = opd_ev.var->inst_name, .opd2.dt = expr->res_dt };

	push_inst_imm_var0(ctx, &cast, out, expr->res_dt);

	return true;
}
static bool translate_expr1_bin(ctx_t * ctx, expr_t * expr, ev_t * out) {
	ev_t ev0, ev1;

	if (!translate_expr1(ctx, expr->opd0.expr, &ev0)) {
		return false;
	}

	if (!translate_expr1(ctx, expr->opd1.expr, &ev1)) {
		return false;
	}

	pla_ast_t_optr_t * optr = expr->bin_op.optr;

	u_assert(optr != NULL);

	switch (optr->type) {
		case PlaAstTOptrBinInstInt:
		{
			ira_inst_t bin_op = { .type = optr->bin_inst_int.inst_type, .opd1.hs = ev0.var->inst_name, .opd2.hs = ev1.var->inst_name };

			push_inst_imm_var0(ctx, &bin_op, out, expr->res_dt);
			break;
		}
		case PlaAstTOptrBinInstIntBool:
		{
			ira_inst_t bin_op = { .type = optr->bin_inst_int_bool.inst_type, .opd1.hs = ev0.var->inst_name, .opd2.hs = ev1.var->inst_name, .opd3.int_cmp = optr->bin_inst_int_bool.int_cmp };

			push_inst_imm_var0(ctx, &bin_op, out, expr->res_dt);
			break;
		}
		default:
			u_assert_switch(optr->type);
	}

	return true;
}
static bool translate_expr1_asgn(ctx_t * ctx, expr_t * expr, ev_t * out) {
	ev_t ev0, ev1;

	if (!translate_expr1(ctx, expr->opd1.expr, &ev1)) {
		return false;
	}

	if (!translate_expr1(ctx, expr->opd0.expr, &ev0)) {
		return false;
	}

	u_assert(ev0.is_lv);

	u_assert(ira_dt_is_equivalent(ev0.var->dt, ev1.var->dt));

	ira_inst_t copy = { .type = IraInstCopy, .opd0.hs = ev0.var->inst_name, .opd1.hs = ev1.var->inst_name };

	ira_func_push_inst(ctx->func, &copy);

	*out = ev0;

	return true;
}
static bool translate_expr1_tse(ctx_t * ctx, expr_t * expr, ev_t * out) {
	switch (expr->base->type) {
		case PlaExprNone:
			return false;
		case PlaExprDtVoid:
		case PlaExprDtBool:
		case PlaExprDtInt:
			if (!translate_expr1_load_val(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprDtPtr:
			if (!translate_expr1_make_dt_ptr(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprDtArr:
			if (!translate_expr1_make_dt_arr(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprDtFunc:
			if (!translate_expr1_make_dt_func(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprDtFuncArg:
			return false;
			//case PlaExprValVoid:
		case PlaExprValBool:
		case PlaExprChStr:
		case PlaExprNumStr:
		case PlaExprNullofDt:
			if (!translate_expr1_load_val(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprIdent:
			if (!translate_expr1_ident(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprCall:
			if (!translate_expr1_call(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprCallArg:
			return false;
			//case PlaExprSubscr:
		case PlaExprMmbrAcc:
			if (!translate_expr1_mmbr_acc(ctx, expr, out)) {
				return false;
			}
			break;
			//case PlaExprPostInc:
			//case PlaExprPostDec:
			//case PlaExprDeref:
		case PlaExprAddrOf:
			if (!translate_expr1_addr_of(ctx, expr, out)) {
				return false;
			}
			break;
			//case PlaExprPreInc:
			//case PlaExprPreDec:
			//case PlaExprLogicNeg:
		case PlaExprCast:
			if (!translate_expr1_cast(ctx, expr, out)) {
				return false;
			}
			break;
			//case PlaExprBitNeg:
			//case PlaExprArithNeg:
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
			if (!translate_expr1_bin(ctx, expr, out)) {
				return false;
			}
			break;
			//case PlaExprLogicAnd:
			//case PlaExprLogicOr:
		case PlaExprAsgn:
			if (!translate_expr1_asgn(ctx, expr, out)) {
				return false;
			}
			break;
		default:
			u_assert_switch(expr->base->type);
	}

	return true;
}
static bool translate_expr1(ctx_t * ctx, expr_t * expr, ev_t * out) {
	tse_t tse = { .type = PlaAstTTseExpr, .expr = expr->base };

	pla_ast_t_push_tse(ctx->t_ctx, &tse);

	bool result = translate_expr1_tse(ctx, expr, out);

	pla_ast_t_pop_tse(ctx->t_ctx);

	return result;
}

static bool translate_expr(ctx_t * ctx, pla_expr_t * pla_expr, ev_t * out) {
	expr_t * expr;

	bool result = false;

	do {
		if (!translate_expr0(ctx, pla_expr, &expr)) {
			break;
		}

		if (!translate_expr1(ctx, expr, out)) {
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
	ev_t ev;

	if (!translate_expr(ctx, stmt->expr.expr, &ev)) {
		return false;
	}

	return true;
}
static bool translate_stmt_var(ctx_t * ctx, pla_stmt_t * stmt) {
	ira_dt_t * dt;

	if (!pla_ast_t_calculate_expr_dt(ctx->t_ctx, stmt->var.dt_expr, &dt)) {
		return false;
	}

	if (!ira_dt_is_var_comp(dt)) {
		pla_ast_t_report(ctx->t_ctx, L"variable [%s] can not have [%s] data type", stmt->var.name->str, ira_dt_infos[dt->type].type_str.str);
		return false;
	}

	var_t * var = define_var(ctx, stmt->var.name, dt, true);

	if (var == NULL) {
		pla_ast_t_report(ctx->t_ctx, L"there is already variable of same name [%s] in this block", stmt->var.name->str);
		return false;
	}

	push_def_var_inst(ctx, var);

	return true;
}
static bool translate_stmt_cond(ctx_t * ctx, pla_stmt_t * stmt) {
	ev_t cond_ev;
	
	if (!translate_expr(ctx, stmt->cond.cond_expr, &cond_ev)) {
		return false;
	}

	if (cond_ev.var->dt != &ctx->pec->dt_bool) {
		pla_ast_t_report(ctx->t_ctx, L"condition expression mush have a boolean data type");
		return false;
	}

	size_t label_index = ctx->unq_label_index++;

	u_hs_t * exit_label;

	{
		u_hs_t * tc_end = get_unq_label_name(ctx, &label_cond_base, label_index, &label_cond_tc_end);

		ira_inst_t brf = { .type = IraInstBrf, .opd0.hs = tc_end, .opd1.hs = cond_ev.var->inst_name };

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
	
	ev_t cond_ev;

	if (!translate_expr(ctx, stmt->cond.cond_expr, &cond_ev)) {
		return false;
	}

	{
		ira_inst_t brf = { .type = IraInstBrf, .opd0.hs = exit_label, .opd1.hs = cond_ev.var->inst_name };

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
	ev_t ev;

	if (!translate_expr(ctx, stmt->expr.expr, &ev)) {
		return false;
	}

	if (ctx->func_ret != NULL) {
		if (!ira_dt_is_equivalent(ctx->func_ret, ev.var->dt)) {
			pla_ast_t_report(ctx->t_ctx, L"return data type does not match to function return data type");
			return false;
		}
	}
	else {
		ctx->func_ret = ev.var->dt;
	}

	ira_inst_t ret = { .type = IraInstRet, .opd0.hs = ev.var->inst_name };

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
		case PlaStmtVar:
			if (!translate_stmt_var(ctx, stmt)) {
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
