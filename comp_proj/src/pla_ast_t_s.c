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

typedef struct var var_t;
struct var {
	var_t * next;
	u_hs_t * real_name;
	u_hs_t * inst_name;
	ira_dt_t * dt;
};
typedef struct blk blk_t;
struct blk {
	blk_t * prev;
	var_t * var;
	size_t level;
};

typedef enum ev_lv_type {
	EvLvNone,
	EvLvVar,
	EvLv_Count
} ev_lv_type_t;
typedef struct ev {
	var_t * var;
	ev_lv_type_t lv_type;
} ev_t;

typedef struct pla_ast_t_s_ctx {
	pla_ast_t_ctx_t * t_ctx;

	ira_dt_t * func_dt;

	pla_stmt_t * stmt;

	ira_func_t ** out;

	ira_pec_t * pec;

	ira_func_t * func;
	ira_dt_t * func_ret;

	blk_t * blk;
	size_t unq_var_index;
} ctx_t;

typedef bool val_proc_t(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out);

static void push_blk(ctx_t * ctx, blk_t * blk) {
	blk->prev = ctx->blk;

	if (blk->prev != NULL) {
		blk->level = blk->prev->level + 1;
	}
	else {
		blk->level = 0;
	}

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

	*new_var = (var_t){ .real_name = name, .inst_name = name, .dt = dt };

	if (unq_name) {
		new_var->inst_name = pla_ast_t_get_unq_var_name(ctx->t_ctx, name, ctx->unq_var_index++);
	}

	return new_var;
}
static var_t * define_var(ctx_t * ctx, u_hs_t * name, ira_dt_t * dt, bool unq_name) {
	var_t ** ins = &ctx->blk->var;

	while (*ins != NULL) {
		var_t * var = *ins;

		if (name == var->real_name) {
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
static void push_inst_imm_var0(ctx_t * ctx, ira_inst_t * inst, ev_t * out, ira_dt_t * dt) {
	var_t * out_var = get_imm_var(ctx, dt);

	push_def_var_inst(ctx, out_var);

	inst->opd0.hs = out_var->inst_name;

	ira_func_push_inst(ctx->func, inst);

	*out = (ev_t){ .var = out_var };
}

static var_t * find_var(ctx_t * ctx, u_hs_t * name) {
	for (blk_t * blk = ctx->blk; blk != NULL; blk = blk->prev) {
		for (var_t * var = blk->var; var != NULL; var = var->next) {
			if (name == var->real_name) {
				return var;
			}
		}
	}

	return NULL;
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
			ira_val_destroy(val);
			return false;
		}

		uint8_t val = (uint8_t)*ch;

		if (*ch == L'\\') {
			++ch;

			if (ch == ch_end) {
				return false;
			}

			switch (*ch) {
				case L'n':
					val = L'\n';
					break;
				default:
					u_assert_switch(*ch);
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
			return false;
		}

		if (var > pre_ovf_val || var == pre_ovf_val && digit > pre_ovf_digit) {
			return false;
		}

		var = var * radix + digit;

		cur++;
	}

	uint64_t lim = ira_int_get_max_pos(int_type);

	if (var > lim) {
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
static bool get_dt_int_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	if (expr->opd0.int_type >= IraInt_Count) {
		return false;
	}

	*out = ira_pec_make_val_imm_dt(ctx->pec, &ctx->pec->dt_ints[expr->opd0.int_type]);

	return true;
}
static bool get_cs_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	u_hs_t * tag = expr->opd1.hs;

	if (tag == ctx->pec->pds[IraPdsAsciiStrTag]) {
		if (!get_cs_ascii(ctx, expr->opd0.hs, out)) {
			return false;
		}
	}
	else {
		return false;
	}

	return true;
}
static bool get_ns_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out) {
	u_hs_t * tag = expr->opd1.hs;

	ira_int_type_t int_type;

	if (is_int_ns(ctx, tag, &int_type)) {
		if (!get_ns_int(ctx, expr->opd0.hs, int_type, out)) {
			return false;
		}
	}
	else {
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

static bool translate_expr(ctx_t * ctx, pla_expr_t * expr, ev_t * out);

static bool translate_expr_load_val(ctx_t * ctx, pla_expr_t * expr, ev_t * out, val_proc_t * proc) {
	ira_inst_t load = { .type = IraInstLoadVal };

	if (!proc(ctx, expr, &load.opd1.val)) {
		return false;
	}

	push_inst_imm_var0(ctx, &load, out, load.opd1.val->dt);

	return true;
}
static bool translate_expr_make_dt_ptr(ctx_t * ctx, pla_expr_t * expr, ev_t * out) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	ev_t ev_body;

	if (!translate_expr(ctx, expr->opd0.expr, &ev_body)
		|| ev_body.var->dt != dt_dt) {
		return false;
	}

	ira_inst_t make_dt_ptr = { .type = IraInstMakeDtPtr, .opd1.hs = ev_body.var->inst_name };

	push_inst_imm_var0(ctx, &make_dt_ptr, out, dt_dt);

	return true;
}
static bool translate_expr_make_dt_arr(ctx_t * ctx, pla_expr_t * expr, ev_t * out) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	ev_t ev_body;

	if (!translate_expr(ctx, expr->opd0.expr, &ev_body)
		|| ev_body.var->dt != dt_dt) {
		return false;
	}

	ira_inst_t make_dt_ptr = { .type = IraInstMakeDtArr, .opd1.hs = ev_body.var->inst_name };

	push_inst_imm_var0(ctx, &make_dt_ptr, out, dt_dt);

	return true;
}
static bool translate_expr_make_dt_func(ctx_t * ctx, pla_expr_t * expr, ev_t * out) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	size_t args_size = 0;

	for (pla_expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr) {
		++args_size;
	}

	u_hs_t ** args = malloc(args_size * sizeof(*args));
	u_hs_t ** ids = malloc(args_size * sizeof(*ids));

	u_hs_t ** arg = args;
	u_hs_t ** id = ids;
	for (pla_expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr, ++arg, ++id) {
		ev_t ev_arg;

		if (!translate_expr(ctx, arg_expr->opd0.expr, &ev_arg)
			|| ev_arg.var->dt != dt_dt) {
			free(args);
			free(ids);
			return false;
		}

		*arg = ev_arg.var->inst_name;
		*id = arg_expr->opd2.hs;
	}

	ev_t ev_ret;

	if (!translate_expr(ctx, expr->opd0.expr, &ev_ret)
		|| ev_ret.var->dt != dt_dt) {
		free(args);
		free(ids);
		return false;
	}

	ira_inst_t make_dt_func = { .type = IraInstMakeDtFunc, .opd1.hs = ev_ret.var->inst_name, .opd2.size = args_size, .opd3.hss = args, .opd4.hss = ids };

	push_inst_imm_var0(ctx, &make_dt_func, out, dt_dt);

	return true;
}
static bool translate_expr_ident(ctx_t * ctx, pla_expr_t * expr, ev_t * out) {
	var_t * var = find_var(ctx, expr->opd0.hs);

	if (var != NULL) {
		*out = (ev_t){ .var = var, .lv_type = EvLvVar };
		return true;
	}

	ira_lo_t * lo = pla_ast_t_resolve_name(ctx->t_ctx, expr->opd0.hs);

	if (lo != NULL) {
		switch (lo->type) {
			case IraLoNone:
			case IraLoNspc:
				return false;
			case IraLoFunc:
			case IraLoImpt:
			{
				ira_inst_t load = { .type = IraInstLoadVal, .opd1.val = ira_pec_make_val_lo_ptr(ctx->pec, lo) };

				push_inst_imm_var0(ctx, &load, out, load.opd1.val->dt);
				break;
			}
			default:
				u_assert_switch(lo->type);
		}

		return true;
	}

	return false;
}
static bool translate_expr_call_func_ptr(ctx_t * ctx, pla_expr_t * expr, ev_t * out, ev_t * callee) {
	ira_dt_t * func_dt = callee->var->dt->ptr.body;

	size_t args_size = 0;

	for (pla_expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr) {
		++args_size;
	}

	if (args_size != func_dt->func.args_size) {
		return false;
	}

	u_hs_t ** args = malloc(args_size * sizeof(*args));

	u_hs_t ** arg = args;
	ira_dt_n_t * arg_dt = func_dt->func.args;
	for (pla_expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr, ++arg, ++arg_dt) {
		ev_t ev_arg;

		if (!translate_expr(ctx, arg_expr->opd0.expr, &ev_arg)
			|| !ira_dt_is_equivalent(ev_arg.var->dt, arg_dt->dt)) {
			free(args);
			return false;
		}

		*arg = ev_arg.var->inst_name;
	}

	ira_inst_t call = { .type = IraInstCallFuncPtr, .opd1.hs = callee->var->inst_name, .opd2.size = args_size, .opd3.hss = args };

	push_inst_imm_var0(ctx, &call, out, func_dt->func.ret);

	return true;
}
static bool translate_expr_call(ctx_t * ctx, pla_expr_t * expr, ev_t * out) {
	ev_t callee;

	if (!translate_expr(ctx, expr->opd0.expr, &callee)) {
		return false;
	}

	ira_dt_t * callee_dt = callee.var->dt;

	if (callee_dt->type == IraDtPtr && callee_dt->ptr.body->type == IraDtFunc) {
		return translate_expr_call_func_ptr(ctx, expr, out, &callee);
	}

	return false;
}
static bool translate_expr_mmbr_acc(ctx_t * ctx, pla_expr_t * expr, ev_t * out) {
	ev_t opd_ev;

	if (!translate_expr(ctx, expr->opd0.expr, &opd_ev)) {
		return false;
	}

	ira_dt_t * opd_dt = opd_ev.var->dt;

	ira_dt_t * oper_dt = NULL;

	ira_inst_t mmbr_acc = { .type = IraInstMmbrAcc, .opd1.hs = opd_ev.var->inst_name, .opd2.hs = expr->opd1.hs };

	switch (opd_dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtInt:
		case IraDtPtr:
			return false;
		case IraDtArr:
			if (expr->opd1.hs == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsSizeMmbr)) {
				oper_dt = ctx->pec->dt_spcl.arr_size;
			}
			else if (expr->opd1.hs == pla_ast_t_get_pds(ctx->t_ctx, PlaPdsDataMmbr)) {
				oper_dt = ira_pec_get_dt_ptr(ctx->pec, opd_dt->arr.body);
			}
			else {
				return false;
			}
			break;
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(opd_dt->type);
	}

	push_inst_imm_var0(ctx, &mmbr_acc, out, oper_dt);

	return true;
}
static bool translate_expr_addr_of(ctx_t * ctx, pla_expr_t * expr, ev_t * out) {
	ev_t opd_ev;

	if (!translate_expr(ctx, expr->opd0.expr, &opd_ev)) {
		return false;
	}

	switch (opd_ev.lv_type) {
		case EvLvNone:
			return false;
		case EvLvVar:
		{
			if (!ira_dt_is_ptr_dt_comp(opd_ev.var->dt)) {
				return false;
			}

			ira_inst_t addr_of = { .type = IraInstAddrOf, .opd1.hs = opd_ev.var->inst_name };

			push_inst_imm_var0(ctx, &addr_of, out, ira_pec_get_dt_ptr(ctx->pec, opd_ev.var->dt));

			break;
		}
	}

	return true;
}
static bool translate_expr_cast(ctx_t * ctx, pla_expr_t * expr, ev_t * out) {
	ev_t opd_ev;

	if (!translate_expr(ctx, expr->opd0.expr, &opd_ev)) {
		return false;
	}

	ira_dt_t * to_dt;

	if (!pla_ast_t_calculate_expr_dt(ctx->t_ctx, expr->opd1.expr, &to_dt)) {
		return false;
	}

	//Check dt for castability?

	ira_inst_t cast = { .type = IraInstCast, .opd1.hs = opd_ev.var->inst_name, .opd2.dt = to_dt };

	push_inst_imm_var0(ctx, &cast, out, to_dt);

	return true;
}
static bool translate_expr_bin(ctx_t * ctx, pla_expr_t * expr, ev_t * out) {
	ev_t ev0, ev1;

	if (!translate_expr(ctx, expr->opd0.expr, &ev0)) {
		return false;
	}

	if (!translate_expr(ctx, expr->opd1.expr, &ev1)) {
		return false;
	}

	u_assert(ev0.var->dt->type == IraDtInt && ira_dt_is_equivalent(ev0.var->dt, ev1.var->dt));

	ira_inst_t bin_op = { .opd1.hs = ev0.var->inst_name, .opd2.hs = ev1.var->inst_name };

	switch (expr->type) {
		case PlaExprMul:
			bin_op.type = IraInstMulInt;
			break;
		case PlaExprDiv:
			bin_op.type = IraInstDivInt;
			break;
		case PlaExprMod:
			bin_op.type = IraInstModInt;
			break;
		case PlaExprAdd:
			bin_op.type = IraInstAddInt;
			break;
		case PlaExprSub:
			bin_op.type = IraInstSubInt;
			break;
		default:
			u_assert_switch(expr->type);
	}

	push_inst_imm_var0(ctx, &bin_op, out, ev0.var->dt);

	return true;
}
static bool translate_expr_asgn(ctx_t * ctx, pla_expr_t * expr, ev_t * out) {
	ev_t ev0, ev1;

	if (!translate_expr(ctx, expr->opd1.expr, &ev1)) {
		return false;
	}

	if (!translate_expr(ctx, expr->opd0.expr, &ev0)) {
		return false;
	}

	if (!ira_dt_is_equivalent(ev0.var->dt, ev1.var->dt)) {
		return false;
	}

	switch (ev0.lv_type) {
		case EvLvNone:
			return false;
		case EvLvVar:
		{
			ira_inst_t copy = { .type = IraInstCopy, .opd0.hs = ev0.var->inst_name, .opd1.hs = ev1.var->inst_name };

			ira_func_push_inst(ctx->func, &copy);

			*out = ev0;

			break;
		}
		default:
			u_assert_switch(ev0.type);
	}

	return true;
}
static bool translate_expr(ctx_t * ctx, pla_expr_t * expr, ev_t * out) {
	switch (expr->type) {
		case PlaExprNone:
			return false;
		case PlaExprDtVoid:
			if (!translate_expr_load_val(ctx, expr, out, get_dt_void_val_proc)) {
				return false;
			}
			break;
		case PlaExprDtBool:
			return false;
		case PlaExprDtInt:
			if (!translate_expr_load_val(ctx, expr, out, get_dt_int_val_proc)) {
				return false;
			}
			break;
		case PlaExprDtPtr:
			if (!translate_expr_make_dt_ptr(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprDtArr:
			if (!translate_expr_make_dt_arr(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprDtFunc:
			if (!translate_expr_make_dt_func(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprDtFuncArg:
			return false;
		case PlaExprValVoid:
		case PlaExprValBool:
		case PlaExprChStr:
			if (!translate_expr_load_val(ctx, expr, out, get_cs_val_proc)) {
				return false;
			}
			break;
		case PlaExprNumStr:
			if (!translate_expr_load_val(ctx, expr, out, get_ns_val_proc)) {
				return false;
			}
			break;
		case PlaExprNullofDt:
			if (!translate_expr_load_val(ctx, expr, out, get_null_val_proc)) {
				return false;
			}
			break;
		case PlaExprIdent:
			if (!translate_expr_ident(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprCall:
			if (!translate_expr_call(ctx, expr, out)) {
				return false;
			}
			break;
		case PlaExprCallArg:
			return false;
			//case PlaExprSubscr:
		case PlaExprMmbrAcc:
			if (!translate_expr_mmbr_acc(ctx, expr, out)) {
				return false;
			}
			break;
			//case PlaExprPostInc:
			//case PlaExprPostDec:
			//case PlaExprDeref:
		case PlaExprAddrOf:
			if (!translate_expr_addr_of(ctx, expr, out)) {
				return false;
			}
			break;
			//case PlaExprPreInc:
			//case PlaExprPreDec:
			//case PlaExprLogicNeg:
		case PlaExprCast:
			if (!translate_expr_cast(ctx, expr, out)) {
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
			if (!translate_expr_bin(ctx, expr, out)) {
				return false;
			}
			break;
			//case PlaExprLeShift:
			//case PlaExprRiShift:
			//case PlaExprLess:
			//case PlaExprLessEq:
			//case PlaExprGrtr:
			//case PlaExprGrtrEq:
			//case PlaExprEq:
			//case PlaExprNeq:
			//case PlaExprBitAnd:
			//case PlaExprBitXor:
			//case PlaExprBitOr:
			//case PlaExprLogicAnd:
			//case PlaExprLogicOr:
		case PlaExprAsgn:
			if (!translate_expr_asgn(ctx, expr, out)) {
				return false;
			}
			break;
		default:
			u_assert_switch(expr->type);
	}

	return true;
}

static bool translate_stmt(ctx_t * ctx, pla_stmt_t * stmt);

static bool translate_stmt_blk(ctx_t * ctx, pla_stmt_t * stmt) {
	blk_t blk = { 0 };

	push_blk(ctx, &blk);

	pla_stmt_t * it = stmt->block.body;

	for (; it != NULL; it = it->next) {
		if (!translate_stmt(ctx, it)) {
			break;
		}
	}

	pop_blk(ctx);

	if (it != NULL) {
		return false;
	}

	return true;
}
static bool translate_stmt_expr(ctx_t * ctx, pla_stmt_t * stmt) {
	ev_t ev;

	if (!translate_expr(ctx, stmt->expr.expr, &ev)) {
		return false;
	}

	return true;
}
static bool translate_stmt_vat(ctx_t * ctx, pla_stmt_t * stmt) {
	ira_dt_t * dt;

	if (!pla_ast_t_calculate_expr_dt(ctx->t_ctx, stmt->var.dt_expr, &dt)
		|| !ira_dt_is_var_dt_comp(dt)) {
		return false;
	}

	var_t * var = define_var(ctx, stmt->var.name, dt, true);

	if (var == NULL) {
		return false;
	}

	push_def_var_inst(ctx, var);

	return true;
}
static bool translate_stmt_ret(ctx_t * ctx, pla_stmt_t * stmt) {
	ev_t ev;

	if (!translate_expr(ctx, stmt->expr.expr, &ev)) {
		return false;
	}

	if (ctx->func_ret != NULL) {
		if (!ira_dt_is_equivalent(ctx->func_ret, ev.var->dt)) {
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
static bool translate_stmt(ctx_t * ctx, pla_stmt_t * stmt) {
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
			if (!translate_stmt_vat(ctx, stmt)) {
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

static bool translate_args(ctx_t * ctx) {
	blk_t blk = { 0 };

	push_blk(ctx, &blk);

	bool result = false;

	do {
		if (ctx->func_dt != NULL) {
			ctx->func_ret = ctx->func_dt->func.ret;

			for (ira_dt_n_t * arg = ctx->func_dt->func.args, *arg_end = arg + ctx->func_dt->func.args_size; arg != arg_end; ++arg) {
				var_t * var = define_var(ctx, arg->name, arg->dt, false);

				u_assert(var != NULL);
			}
		}

		if (!translate_stmt(ctx, ctx->stmt)) {
			break;
		}

		if (ctx->func->dt == NULL) {
			if (ctx->func_ret == NULL) {
				break;
			}

			ctx->func->dt = ira_pec_get_dt_func(ctx->pec, ctx->func_ret, 0, NULL);
		}

		result = true;
	} while (false);

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

	u_assert(ctx.blk == NULL);

	return result;
}
