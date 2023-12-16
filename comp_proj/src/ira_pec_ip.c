#include "pch.h"
#include "ira_pec_ip.h"
#include "ira_dt.h"
#include "ira_val.h"
#include "ira_inst.h"
#include "ira_func.h"
#include "ira_lo.h"
#include "ira_pec_c.h"
#include "asm_size.h"
#include "asm_reg.h"
#include "asm_inst.h"
#include "asm_frag.h"
#include "asm_pea.h"
#include "u_assert.h"
#include "u_misc.h"

#define STACK_ALIGN 16
#define STACK_UNIT 8

typedef enum trg {
	TrgCompl,
	TrgIntrp,
	Trg_Count
} trg_t;

typedef struct var var_t;
struct var {
	var_t * next;

	u_hs_t * name;

	ira_dt_t * dt;

	ira_val_t * val;

	size_t size;
	size_t align;

	size_t pos;
};

typedef union inst_opd {
	size_t size;
	u_hs_t * hs;
	u_hs_t ** hss;
	ira_dt_t * dt;
	ira_val_t * val;
	var_t * var;
	var_t ** vars;
} inst_opd_t;
typedef struct inst {
	ira_inst_t * base;

	union {
		inst_opd_t opds[IRA_INST_OPDS_SIZE];
		struct {
			inst_opd_t opd0;
			inst_opd_t opd1;
			inst_opd_t opd2;
			inst_opd_t opd3;
			inst_opd_t opd4;
		};
	};
} inst_t;

typedef struct int_cast_info {
	asm_inst_type_t inst_type;
	asm_reg_t reg0;
	asm_reg_t reg1;
} int_cast_info_t;

typedef struct ira_pec_ip_ctx {
	trg_t trg;

	ira_pec_c_ctx_t * c_ctx;
	ira_lo_t * c_lo;
	asm_frag_t ** c_out;

	ira_val_t ** i_out;

	ira_pec_t * pec;

	ira_func_t * func;

	asm_frag_t * frag;

	var_t * var;

	size_t insts_size;
	inst_t * insts;

	bool has_calls;
	size_t call_args_size_max;
	size_t stack_vars_pos;
	size_t stack_vars_size;

	size_t stack_size;
} ctx_t;

static const wchar_t * trg_to_str[Trg_Count] = {
	[TrgCompl] = L"compiler",
	[TrgIntrp] = L"interpreter",
};

static const asm_reg_t int_type_to_ax[IraInt_Count] = {
	AsmRegAl, AsmRegAx, AsmRegEax, AsmRegRax, AsmRegAl, AsmRegAx, AsmRegEax, AsmRegRax
};
static const asm_reg_t int_type_to_cx[IraInt_Count] = {
	AsmRegCl, AsmRegCx, AsmRegEcx, AsmRegRcx, AsmRegCl, AsmRegCx, AsmRegEcx, AsmRegRcx
};
static const asm_inst_imm_type_t int_type_to_imm_type[IraInt_Count] = {
	AsmInstImm8, AsmInstImm16, AsmInstImm32, AsmInstImm64, AsmInstImm8, AsmInstImm16, AsmInstImm32, AsmInstImm64
};

static const asm_reg_t w64_int_arg_to_reg[4][IraInt_Count] = {
	[0] = { AsmRegCl, AsmRegCx, AsmRegEcx, AsmRegRcx, AsmRegCl, AsmRegCx, AsmRegEcx, AsmRegRcx },
	[1] = { AsmRegDl, AsmRegDx, AsmRegEdx, AsmRegRdx, AsmRegDl, AsmRegDx, AsmRegEdx, AsmRegRdx },
	[2] = { AsmRegR8b, AsmRegR8w, AsmRegR8d, AsmRegR8, AsmRegR8b, AsmRegR8w, AsmRegR8d, AsmRegR8 },
	[3] = { AsmRegR9b, AsmRegR9w, AsmRegR9d, AsmRegR9, AsmRegR9b, AsmRegR9w, AsmRegR9d, AsmRegR9 }
};
static const asm_reg_t w64_ptr_arg_to_reg[4] = {
	[0] = { AsmRegRcx },
	[1] = { AsmRegRdx },
	[2] = { AsmRegR8 },
	[3] = { AsmRegR9 }
};

static const int_cast_info_t int_cast_from_to[IraInt_Count][IraInt_Count] = {
	{ { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMovzx, AsmRegAx, AsmRegCl }, { AsmInstMovzx, AsmRegEax, AsmRegCl }, { AsmInstMovzx, AsmRegRax, AsmRegCl }, { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMovzx, AsmRegAx, AsmRegCl }, { AsmInstMovzx, AsmRegEax, AsmRegCl }, { AsmInstMovzx, AsmRegRax, AsmRegCl } },
	{ { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMovzx, AsmRegEax, AsmRegCx }, { AsmInstMovzx, AsmRegRax, AsmRegCx }, { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMovzx, AsmRegEax, AsmRegCx }, { AsmInstMovzx, AsmRegRax, AsmRegCx } },
	{ { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMov, AsmRegEax, AsmRegEcx }, { AsmInstMov, AsmRegEax, AsmRegEcx }, { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMov, AsmRegEax, AsmRegEcx }, { AsmInstMov, AsmRegEax, AsmRegEcx } },
	{ { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMov, AsmRegEax, AsmRegEcx }, { AsmInstMov, AsmRegRax, AsmRegRcx }, { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMov, AsmRegEax, AsmRegEcx }, { AsmInstMov, AsmRegRax, AsmRegRcx } },

	{ { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMovzx, AsmRegAx, AsmRegCl }, { AsmInstMovzx, AsmRegEax, AsmRegCl }, { AsmInstMovzx, AsmRegRax, AsmRegCl }, { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMovsx, AsmRegAx, AsmRegCl }, { AsmInstMovsx, AsmRegEax, AsmRegCl }, { AsmInstMovsx, AsmRegRax, AsmRegCl } },
	{ { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMovzx, AsmRegEax, AsmRegCx }, { AsmInstMovzx, AsmRegRax, AsmRegCx }, { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMovsx, AsmRegEax, AsmRegCx }, { AsmInstMovsx, AsmRegRax, AsmRegCx } },
	{ { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMov, AsmRegEax, AsmRegEcx }, { AsmInstMov, AsmRegEax, AsmRegEcx }, { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMov, AsmRegEax, AsmRegEcx }, { AsmInstMovsxd, AsmRegRax, AsmRegEcx } },
	{ { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMov, AsmRegEax, AsmRegEcx }, { AsmInstMov, AsmRegRax, AsmRegRcx }, { AsmInstMov, AsmRegAl, AsmRegCl }, { AsmInstMov, AsmRegAx, AsmRegCx }, { AsmInstMov, AsmRegEax, AsmRegEcx }, { AsmInstMov, AsmRegRax, AsmRegRcx } },
};

static void report(ctx_t * ctx, const wchar_t * format, ...) {
	u_assert(ctx->trg <= Trg_Count);

	fwprintf(stderr, L"reporting error in instruction processor [%s]\n", trg_to_str[ctx->trg]);

	if (ctx->c_lo != NULL) {
		fwprintf(stderr, L"processing [%s] function language object\n", ctx->c_lo->full_name->str);
	}
	else {
		fwprintf(stderr, L"processing anonymouse function\n");
	}

	{
		va_list args;

		va_start(args, format);

		vfwprintf(stderr, format, args);

		va_end(args);

		fputwc(L'\n', stderr);
	}
}
static void report_opds_not_equ(ctx_t * ctx, inst_t * inst, size_t first, size_t second) {
	const ira_inst_info_t * info = &ira_inst_infos[inst->base->type];

	report(ctx, L"[%s] requires opd[%zi] and opd[%zi] to have an equivalent data type", info->type_str.str, first, second);
}
static void report_opd_not_equ_dt(ctx_t * ctx, inst_t * inst, size_t first) {
	const ira_inst_info_t * info = &ira_inst_infos[inst->base->type];

	report(ctx, L"[%s] requires opd[%zi] to have an equivalent data type", info->type_str.str, first);
}

static void ctx_cleanup(ctx_t * ctx) {
	for (inst_t * inst = ctx->insts, *inst_end = inst + ctx->insts_size; inst != inst_end; ++inst) {
		ira_inst_t * base = inst->base;

		if (base == NULL) {
			continue;
		}

		const ira_inst_info_t * info = &ira_inst_infos[inst->base->type];

		for (size_t opd = 0; opd < IRA_INST_OPDS_SIZE; ++opd) {
			switch (info->opds[opd]) {
				case IraInstOpdNone:
				case IraInstOpdDt:
				case IraInstOpdVal:
				case IraInstOpdVarDef:
				case IraInstOpdVar:
				case IraInstOpdMmbr:
				case IraInstOpdVarsSize:
				case IraInstOpdIds2:
					break;
				case IraInstOpdVars2:
					free(inst->opds[opd].vars);
					break;
				default:
					u_assert_switch(info->opds[opd]);
			}
		}
	}

	free(ctx->insts);

	for (var_t * var = ctx->var; var != NULL;) {
		var_t * next = var->next;

		ira_val_destroy(var->val);

		free(var);

		var = next;
	}
}

static bool define_var(ctx_t * ctx, ira_dt_t * dt, u_hs_t * name) {
	if (!ira_dt_is_var_comp(dt)) {
		report(ctx, L"can not define a variable with [%s] data type", ira_dt_infos[dt->type].type_str.str);
		return false;
	}

	var_t ** ins = &ctx->var;

	while (*ins != NULL) {
		var_t * var = *ins;

		if (var->name == name) {
			report(ctx, L"variable [%s] already exists", name->str);
			return false;
		}

		ins = &var->next;
	}

	var_t * new_var = malloc(sizeof(*new_var));

	u_assert(new_var != NULL);

	*new_var = (var_t){ .name = name, .dt = dt, .size = ira_dt_get_size(dt), .align = ira_dt_get_align(dt) };

	*ins = new_var;

	return true;
}
static var_t * find_var(ctx_t * ctx, u_hs_t * name) {
	for (var_t * var = ctx->var; var != NULL; var = var->next) {
		if (var->name == name) {
			return var;
		}
	}

	return NULL;
}
static bool prepare_insts_args(ctx_t * ctx) {
	ira_dt_t * func_dt = ctx->func->dt;

	if (func_dt != NULL) {
		for (ira_dt_n_t * arg = func_dt->func.args, *arg_end = arg + func_dt->func.args_size; arg != arg_end; ++arg) {
			if (!define_var(ctx, arg->dt, arg->name)) {
				return false;
			}
		}
	}

	return true;
}
static bool prepare_insts_opds_vars(ctx_t * ctx, inst_t * inst, size_t opd_size, size_t opd) {
	ira_inst_t * base = inst->base;

	size_t vars_size = base->opds[opd_size].size;

	var_t ** vars = malloc(vars_size * sizeof(*vars));

	u_assert(vars != NULL);

	inst->opds[opd].vars = vars;

	u_hs_t ** var_name = base->opds[opd].hss;
	for (var_t ** var = vars, **var_end = var + vars_size; var != var_end; ++var, ++var_name) {
		*var = find_var(ctx, *var_name);

		if (*var == NULL) {
			report(ctx, L"can not find a variable [%s] for [%zi] operand for [%s] instruction", (*var_name)->str, opd, ira_inst_infos[base->type].type_str.str);
			return false;
		}
	}

	return true;
}
static bool prepare_insts_opds(ctx_t * ctx, inst_t * inst, ira_inst_t * ira_inst, const ira_inst_info_t * info) {
	for (size_t opd = 0; opd < IRA_INST_OPDS_SIZE; ++opd) {
		switch (info->opds[opd]) {
			case IraInstOpdNone:
				break;
			case IraInstOpdDt:
				inst->opds[opd].dt = ira_inst->opds[opd].dt;

				if (inst->opds[opd].dt == NULL) {
					report(ctx, L"value for [%zi] operand for [%s] instruction is NULL", opd, ira_inst_infos[ira_inst->type].type_str.str);
					return false;
				}
				break;
			case IraInstOpdVal:
				inst->opds[opd].val = ira_inst->opds[opd].val;

				if (inst->opds[opd].val == NULL) {
					report(ctx, L"value for [%zi] operand for [%s] instruction is NULL", opd, ira_inst_infos[ira_inst->type].type_str.str);
					return false;
				}
				break;
			case IraInstOpdVarDef:
				inst->opds[opd].hs = ira_inst->opds[opd].hs;

				if (inst->opds[opd].hs == NULL) {
					report(ctx, L"value for [%zi] operand for [%s] instruction is NULL", opd, ira_inst_infos[ira_inst->type].type_str.str);
					return false;
				}
				break;
			case IraInstOpdVar:
				inst->opds[opd].var = find_var(ctx, ira_inst->opds[opd].hs);

				if (inst->opds[opd].var == NULL) {
					report(ctx, L"can not find a variable [%s] for [%zi] operand for [%s] instruction", ira_inst->opds[opd].hs->str, opd, ira_inst_infos[ira_inst->type].type_str.str);
					return false;
				}
				break;
			case IraInstOpdMmbr:
				inst->opds[opd].hs = inst->base->opds[opd].hs;

				if (inst->opds[opd].hs == NULL) {
					report(ctx, L"value for [%zi] operand for [%s] instruction is NULL", opd, ira_inst_infos[ira_inst->type].type_str.str);
					return false;
				}
				break;
			case IraInstOpdVarsSize:
				inst->opds[opd].size = ira_inst->opds[opd].size;
				break;
			case IraInstOpdVars2:
				if (!prepare_insts_opds_vars(ctx, inst, 2, opd)) {
					return false;
				}
				break;
			case IraInstOpdIds2:
				inst->opds[opd].hss = ira_inst->opds[opd].hss;
				break;
			default:
				u_assert_switch(info->opds[opd]);
		}
	}

	return true;
}
static bool prepare_insts(ctx_t * ctx) {
	if (!prepare_insts_args(ctx)) {
		return false;
	}

	ira_func_t * func = ctx->func;

	ctx->insts_size = func->insts_size;

	ctx->insts = malloc(ctx->insts_size * sizeof(*ctx->insts));

	u_assert(ctx->insts != NULL);

	memset(ctx->insts, 0, ctx->insts_size * sizeof(*ctx->insts));

	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	inst_t * inst = ctx->insts;
	for (ira_inst_t * ira_inst = func->insts, *ira_inst_end = ira_inst + func->insts_size; ira_inst != ira_inst_end; ++ira_inst, ++inst) {
		if (ira_inst->type >= IraInst_Count) {
			report(ctx, L"unknown instruction type [%i]", ira_inst->type);
			return false;
		}

		inst->base = ira_inst;

		const ira_inst_info_t * info = &ira_inst_infos[ira_inst->type];

		switch (ctx->trg) {
			case TrgCompl:
				if (!info->rt_comp) {
					report(ctx, L"detected an illegal instruction [%s] in %s mode", info->type_str.str, trg_to_str[ctx->trg]);
					return false;
				}
				break;
			case TrgIntrp:
				if (!info->ct_comp) {
					report(ctx, L"detected an illegal instruction [%s] in %s mode", info->type_str.str, trg_to_str[ctx->trg]);
					return false;
				}
				break;
			default:
				u_assert_switch(ctx->trg);
		}

		if (!prepare_insts_opds(ctx, inst, ira_inst, info)) {
			return false;
		}

		switch (ira_inst->type) {
			case IraInstNone:
				break;
			case IraInstDefVar:
				if (!define_var(ctx, inst->opd1.dt, inst->opd0.hs)) {
					return false;
				}
				break;
			case IraInstLoadVal:
				if (!ira_dt_is_equivalent(inst->opd1.val->dt, inst->opd0.var->dt)) {
					report_opds_not_equ(ctx, inst, 1, 0);
					return false;
				}
				break;
			case IraInstCopy:
				if (!ira_dt_is_equivalent(inst->opd1.var->dt, inst->opd0.var->dt)) {
					report_opds_not_equ(ctx, inst, 1, 0);
					return false;
				}
				break;
			case IraInstMakeDtPtr:
				if (inst->opd1.var->dt != dt_dt) {
					report_opd_not_equ_dt(ctx, inst, 1);
					return false;
				}

				if (inst->opd0.var->dt != dt_dt) {
					report_opd_not_equ_dt(ctx, inst, 0);
					return false;
				}
				break;
			case IraInstMakeDtArr:
				if (inst->opd1.var->dt != dt_dt) {
					report_opd_not_equ_dt(ctx, inst, 1);
					return false;
				}

				if (inst->opd0.var->dt != dt_dt) {
					report_opd_not_equ_dt(ctx, inst, 0);
					return false;
				}
				break;
			case IraInstMakeDtFunc:
				for (var_t ** var = inst->opd3.vars, **var_end = var + inst->opd2.size; var != var_end; ++var) {
					if ((*var)->dt != dt_dt) {
						report(ctx, L"[%s] requires arguments in opd[3] to have an equivalent data type", info->type_str.str);
						return false;
					}
				}

				if (inst->opd1.var->dt != dt_dt) {
					report_opd_not_equ_dt(ctx, inst, 1);
					return false;
				}

				if (inst->opd0.var->dt != dt_dt) {
					report_opd_not_equ_dt(ctx, inst, 0);
					return false;
				}

				break;
			case IraInstAddInt:
			case IraInstSubInt:
			case IraInstMulInt:
			case IraInstDivInt:
			case IraInstModInt:
			{
				ira_dt_t * dt = inst->opd1.var->dt;

				if (dt->type != IraDtInt) {
					report(ctx, L"[%s] requires opd[1] to have an integer data type", info->type_str.str);
					return false;
				}

				if (inst->opd2.var->dt != dt) {
					report_opd_not_equ_dt(ctx, inst, 1);
					return false;
				}

				if (inst->opd0.var->dt != dt) {
					report_opd_not_equ_dt(ctx, inst, 1);
					return false;
				}

				break;
			}
			case IraInstMmbrAcc:
			{
				u_hs_t * mmbr = inst->opd2.hs;
				ira_dt_t * opd_dt = inst->opd1.var->dt;
				ira_dt_t * res_dt = NULL;

				switch (opd_dt->type) {
					case IraDtVoid:
					case IraDtInt:
					case IraDtPtr:
					case IraDtFunc:
						report(ctx, L"[%s]: data type [%s] of opd[1] does not support member access:", info->type_str.str, ira_dt_infos->type_str.str);
						return false;
					case IraDtArr:
						if (mmbr == ctx->pec->pds[IraPdsSizeMmbr]) {
							res_dt = res_dt = ctx->pec->dt_spcl.arr_size;
						}
						else if (mmbr == ctx->pec->pds[IraPdsDataMmbr]) {
							res_dt = ira_pec_get_dt_ptr(ctx->pec, opd_dt->arr.body);
						}
						break;
					default:
						u_assert_switch(opd_dt->type);
				}

				if (res_dt != NULL) {
					if (!ira_dt_is_equivalent(inst->opd0.var->dt, res_dt)) {
						report_opd_not_equ_dt(ctx, inst, 0);
						return false;
					}
				}
				else {
					report(ctx, L"[%s]: data type [%s] of opd[1] does not support [%s] member:", info->type_str.str, ira_dt_infos->type_str.str, mmbr->str);
					return false;
				}

				break;
			}
			case IraInstCast:
			{
				ira_dt_t * from = inst->opd1.var->dt;
				ira_dt_t * to = inst->opd2.dt;

				switch (to->type) {
					case IraDtInt:
						switch (from->type) {
							case IraDtInt:
								break;
							default:
								u_assert_switch(from->type);
						}
						break;
					case IraDtPtr:
						switch (from->type) {
							case IraDtPtr:
								break;
							default:
								u_assert_switch(from->type);
						}
						break;
					default:
						u_assert_switch(to->type);
				}

				break;
			}
			case IraInstAddrOf:
			{
				ira_dt_t * ptr_dt = inst->opd0.var->dt;

				if (ptr_dt->type != IraDtPtr) {
					report(ctx, L"[%s] requires opd[0] to have an pointer data type", info->type_str.str);
					return false;
				}

				if (!ira_dt_is_equivalent(inst->opd1.var->dt, ptr_dt->ptr.body)) {
					report_opd_not_equ_dt(ctx, inst, 1);
					return false;
				}
				break;
			}
			case IraInstCallFuncPtr:
			{
				ira_dt_t * ptr_func_dt = inst->opd1.var->dt;

				if (ptr_func_dt->type != IraDtPtr || ptr_func_dt->ptr.body->type != IraDtFunc) {
					report(ctx, L"[%s] requires opd[0] to have an pointer to function data type", info->type_str.str);
					return false;
				}

				ira_dt_t * func_dt = ptr_func_dt->ptr.body;

				if (func_dt->func.args_size != inst->opd2.size) {
					report(ctx, L"[%s] requires opd[2] to match function data type arguments size", info->type_str.str);
					return false;
				}

				ira_dt_n_t * arg_dt = func_dt->func.args;
				for (var_t ** var = inst->opd3.vars, **var_end = var + inst->opd2.size; var != var_end; ++var, ++arg_dt) {
					if (!ira_dt_is_equivalent(arg_dt->dt, (*var)->dt)) {
						report(ctx, L"[%s] requires argument in opd[3] to match function argument data type", info->type_str.str);
						return false;
					}
				}

				if (!ira_dt_is_equivalent(inst->opd0.var->dt, func_dt->func.ret)) {
					report_opd_not_equ_dt(ctx, inst, 0);
					return false;
				}

				ctx->has_calls = true;
				ctx->call_args_size_max = max(ctx->call_args_size_max, inst->opd2.size);

				break;
			}
			case IraInstRet:
				if (!ira_dt_is_equivalent(ctx->func->dt->func.ret, inst->opd0.var->dt)) {
					report_opd_not_equ_dt(ctx, inst, 0);
					return false;
				}
				break;
			default:
				u_assert_switch(ira_inst->type);
		}
	}

	return true;
}


static void push_label(ctx_t * ctx, u_hs_t * name) {
	asm_inst_t label = { .type = AsmInstLabel, .opds = AsmInstLabel, .label = name };

	asm_frag_push_inst(ctx->frag, &label);
}

static bool calculate_stack(ctx_t * ctx) {
	size_t size = 0;

	if (ctx->has_calls) {
		size = u_align_to(size, STACK_ALIGN);

		size_t args_size = max(ctx->call_args_size_max, 4);

		size += args_size * STACK_UNIT;
	}

	{
		size = u_align_to(size, STACK_ALIGN);

		size_t var_pos = 0;

		for (var_t * var = ctx->var; var != NULL; var = var->next) {
			var_pos = u_align_to(var_pos, var->align);

			var->pos = var_pos;

			var_pos += var->size;
		}

		ctx->stack_vars_pos = size;
		ctx->stack_vars_size = var_pos;

		size += var_pos;
	}

	size = u_align_biased_to(size, STACK_ALIGN, STACK_ALIGN - STACK_UNIT);

	if (size >= INT32_MAX) {
		return false;
	}

	ctx->stack_size = size;

	return true;
}

static void save_stack_gpr(ctx_t * ctx, int32_t offset, asm_reg_t reg) {
	asm_inst_t save = { .type = AsmInstMov, .opds = AsmInstOpds_Mem_Reg, .mem_size = asm_reg_get_size(reg), .mem_base = AsmRegRsp, .mem_disp_type = AsmInstDispAuto, .mem_disp = offset, .reg0 = reg };

	asm_frag_push_inst(ctx->frag, &save);
}
static void save_stack_var_off(ctx_t * ctx, var_t * var, asm_reg_t reg, size_t offset) {
	u_assert(asm_reg_is_gpr(reg));

	asm_size_t mem_size = asm_reg_get_size(reg);

	u_assert(asm_size_get_bytes(mem_size) + offset <= var->size);

	save_stack_gpr(ctx, (int32_t)(ctx->stack_vars_pos + var->pos + offset), reg);
}
static void save_stack_var(ctx_t * ctx, var_t * var, asm_reg_t reg) {
	save_stack_var_off(ctx, var, reg, 0);
}
static void load_int(ctx_t * ctx, asm_reg_t reg, asm_inst_imm_type_t imm_type, int64_t imm) {
	u_assert(asm_reg_get_size(reg) == asm_inst_imm_type_to_size[imm_type]);

	asm_inst_t load = { .type = AsmInstMov, .opds = AsmInstOpds_Reg_Imm, .reg0 = reg, .imm0_type = imm_type, .imm0 = imm };

	asm_frag_push_inst(ctx->frag, &load);
}
static void load_label_off(ctx_t * ctx, asm_reg_t reg, u_hs_t * label) {
	u_assert(asm_reg_get_size(reg) == AsmSize64);

	asm_inst_t lea = { .type = AsmInstLea, .opds = AsmInstOpds_Reg_Mem, .reg0 = reg, .mem_base = AsmRegRip, .mem_disp_type = AsmInstDispLabelRel32, .mem_disp_label = label };

	asm_frag_push_inst(ctx->frag, &lea);
}
static void load_label_val(ctx_t * ctx, asm_reg_t reg, u_hs_t * label) {
	asm_inst_t mov = { .type = AsmInstMov, .opds = AsmInstOpds_Reg_Mem, .reg0 = AsmRegRax, .mem_size = asm_reg_get_size(reg), .mem_base = AsmRegRip, .mem_disp_type = AsmInstDispLabelRel32, .mem_disp_label = label };

	asm_frag_push_inst(ctx->frag, &mov);
}
static void load_stack_gpr(ctx_t * ctx, asm_reg_t reg, int32_t offset) {
	asm_inst_t load = { .type = AsmInstMov, .opds = AsmInstOpds_Reg_Mem, .reg0 = reg, .mem_size = asm_reg_get_size(reg), .mem_base = AsmRegRsp, .mem_disp_type = AsmInstDispAuto, .mem_disp = offset };

	asm_frag_push_inst(ctx->frag, &load);
}
static void load_stack_var_off(ctx_t * ctx, asm_reg_t reg, var_t * var, size_t offset) {
	u_assert(asm_reg_is_gpr(reg));

	asm_size_t mem_size = asm_reg_get_size(reg);

	u_assert(asm_size_get_bytes(mem_size) + offset <= var->size);

	load_stack_gpr(ctx, reg, (int32_t)(ctx->stack_vars_pos + var->pos + offset));
}
static void load_stack_var(ctx_t * ctx, asm_reg_t reg, var_t * var) {
	load_stack_var_off(ctx, reg, var, 0);
}

static int32_t w64_get_stack_arg_offset(ctx_t * ctx, size_t arg) {
	return (int32_t)((1 + arg) * STACK_UNIT);
}
static bool w64_pass_caller_ret(ctx_t * ctx, var_t * var) {
	ira_dt_t * var_dt = var->dt;

	switch (var_dt->type) {
		case IraDtVoid:
		case IraDtInt:
		case IraDtPtr:
			break;
		default:
			u_assert_switch(ret_dt->type);
	}

	return false;
}
static void w64_load_caller_arg(ctx_t * ctx, var_t * var, size_t arg) {
	ira_dt_t * var_dt = var->dt;

	asm_reg_t reg;

	int32_t arg_offset = w64_get_stack_arg_offset(ctx, arg);

	switch (var_dt->type) {
		case IraDtVoid:
			break;
		case IraDtInt:
			switch (arg) {
				case 0:
				case 1:
				case 2:
				case 3:
					reg = w64_int_arg_to_reg[arg][var_dt->int_type];
					load_stack_var(ctx, reg, var);
					break;
				default:
					u_assert_switch(arg);
			}
			break;
		case IraDtPtr:
			switch (arg) {
				case 0:
				case 1:
				case 2:
				case 3:
					reg = w64_ptr_arg_to_reg[arg];
					load_stack_var(ctx, reg, var);
					break;
				default:
					load_stack_var(ctx, AsmRegRax, var);
					save_stack_gpr(ctx, arg_offset, AsmRegRax);
					break;
			}
			break;
		default:
			u_assert_switch(var_dt->type);
	}
}
static bool w64_save_callee_ret(ctx_t * ctx) {
	ira_dt_t * func_dt = ctx->func->dt;

	switch (func_dt->func.ret->type) {
		case IraDtVoid:
		case IraDtInt:
		case IraDtPtr:
			break;
		default:
			u_assert_switch(func_dt->func.ret->type);
	}

	return false;
}
static void w64_save_callee_arg(ctx_t * ctx, ira_dt_n_t * arg_dt_n, size_t arg) {
	ira_dt_t * arg_dt = arg_dt_n->dt;

	var_t * var = find_var(ctx, arg_dt_n->name);

	u_assert(var != NULL);

	switch (arg_dt->type) {
		case IraDtVoid:
			break;
		case IraDtInt:
			switch (arg) {
				case 0:
				case 1:
				case 2:
				case 3:
					save_stack_var(ctx, var, w64_int_arg_to_reg[arg][arg_dt_n->dt->int_type]);
					break;
				default:
				{
					asm_reg_t reg = int_type_to_ax[arg_dt->int_type];

					load_stack_gpr(ctx, reg, w64_get_stack_arg_offset(ctx, arg));
					save_stack_var(ctx, var, reg);
					break;
				}
			}
			break;
		case IraDtPtr:
			switch (arg) {
				case 0:
				case 1:
				case 2:
				case 3:
					save_stack_var(ctx, var, w64_ptr_arg_to_reg[arg]);
					break;
				default:
					load_stack_gpr(ctx, AsmRegRax, w64_get_stack_arg_offset(ctx, arg));
					save_stack_var(ctx, var, AsmRegRax);
					break;
			}
			break;
		default:
			u_assert_switch(arg_dt->type);
	}
}

static void emit_prologue_save_args(ctx_t * ctx) {
	size_t arg = 0;

	if (w64_save_callee_ret(ctx)) {
		++arg;
	}

	ira_dt_t * func_dt = ctx->func->dt;

	for (ira_dt_n_t * arg_dt_n = func_dt->func.args, *arg_dt_n_end = arg_dt_n + func_dt->func.args_size; arg_dt_n != arg_dt_n_end; ++arg_dt_n, ++arg) {
		w64_save_callee_arg(ctx, arg_dt_n, arg);
	}
}
static void emit_prologue(ctx_t * ctx) {
	asm_inst_t stack_res = { .type = AsmInstSub, .opds = AsmInstOpds_Reg_Imm, .reg0 = AsmRegRsp, .imm0_type = AsmInstImm32, .imm0 = (int32_t)ctx->stack_size };

	asm_frag_push_inst(ctx->frag, &stack_res);

	emit_prologue_save_args(ctx);
}
static void emit_epilogue(ctx_t * ctx) {
	asm_inst_t stack_free = { .type = AsmInstAdd, .opds = AsmInstOpds_Reg_Imm, .reg0 = AsmRegRsp, .imm0_type = AsmInstImm32, .imm0 = (int32_t)ctx->stack_size };

	asm_frag_push_inst(ctx->frag, &stack_free);
}

static void div_int(ctx_t * ctx, var_t * opd0, var_t * opd1, var_t * div_out, var_t * mod_out) {
	ira_int_type_t int_type = opd0->dt->int_type;
	
	asm_inst_type_t dx_inst_type = AsmInstXor;
	asm_reg_t reg0 = int_type_to_ax[int_type], reg1 = int_type_to_cx[int_type], reg2;

	switch (opd0->dt->int_type) {
		case IraIntS8:
			dx_inst_type = AsmInstCbw;
		case IraIntU8:
			reg2 = AsmRegAh;
			break;
		case IraIntS16:
			dx_inst_type = AsmInstCwd;
		case IraIntU16:
			reg2 = AsmRegDx;
			break;
		case IraIntS32:
			dx_inst_type = AsmInstCdq;
		case IraIntU32:
			reg2 = AsmRegEdx;
			break;
		case IraIntS64:
			dx_inst_type = AsmInstCqo;
		case IraIntU64:
			reg2 = AsmRegRdx;
			break;
		default:
			u_assert_switch(inst->opd0.var->dt->int_type);
	}

	load_stack_var(ctx, reg0, opd0);
	load_stack_var(ctx, reg1, opd1);

	asm_inst_t dx_inst = { .type = dx_inst_type };

	if (dx_inst_type == AsmInstXor) {
		dx_inst.opds = AsmInstOpds_Reg_Reg;
		dx_inst.reg0 = reg2;
		dx_inst.reg1 = reg2;
	}
	else {
		dx_inst.opds = AsmInstOpds_None;
	}

	asm_frag_push_inst(ctx->frag, &dx_inst);

	asm_inst_t div_inst = { .type = dx_inst_type == AsmInstXor ? AsmInstDiv : AsmInstIdiv, .opds = AsmInstOpds_Reg, .reg0 = reg1 };

	asm_frag_push_inst(ctx->frag, &div_inst);

	if (div_out != NULL) {
		save_stack_var(ctx, div_out, reg0);
	}
	if (mod_out != NULL) {
		save_stack_var(ctx, mod_out, reg2);
	}
}

static void compile_load_val_bool(ctx_t * ctx, inst_t * inst) {
	ira_val_t * val = inst->opd1.val;

	load_int(ctx, AsmRegAl, AsmInstImm8, val->bool_val ? 1 : 0);

	save_stack_var(ctx, inst->opd0.var, AsmRegAl);
}
static void compile_load_val_int(ctx_t * ctx, inst_t * inst) {
	ira_val_t * val = inst->opd1.val;

	asm_reg_t reg = int_type_to_ax[val->dt->int_type];
	asm_inst_imm_type_t imm_type = int_type_to_imm_type[val->dt->int_type];

	load_int(ctx, reg, imm_type, val->int_val.si64);

	save_stack_var(ctx, inst->opd0.var, reg);
}
static void compile_load_val_arr(ctx_t * ctx, inst_t * inst) {
	ira_val_t * val = inst->opd1.val;

	ira_dt_t * elem_dt = val->dt->arr.body;

	switch (elem_dt->type) {
		case IraDtInt:
		{
			u_hs_t * arr_label = NULL;

			if (!ira_pec_c_compile_int_arr(ctx->c_ctx, val, &arr_label)) {
				u_assert(false);
			}

			load_int(ctx, AsmRegRax, AsmInstImm64, (int64_t)val->arr.size);
			save_stack_var_off(ctx, inst->opd0.var, AsmRegRax, IRA_DT_ARR_SIZE_OFF);

			load_label_off(ctx, AsmRegRax, arr_label);
			save_stack_var_off(ctx, inst->opd0.var, AsmRegRax, IRA_DT_ARR_DATA_OFF);

			break;
		}
		default:
			u_assert_switch(elem_dt->type);
	}
}
static void compile_load_val(ctx_t * ctx, inst_t * inst) {
	ira_val_t * val = inst->opd1.val;

	switch (val->type) {
		case IraValImmBool:
			compile_load_val_bool(ctx, inst);
			break;
		case IraValImmInt:
			compile_load_val_int(ctx, inst);
			break;
		case IraValImmArr:
			compile_load_val_arr(ctx, inst);
			break;
		case IraValLoPtr:
			switch (val->lo_val->type) {
				case IraLoFunc:
					load_label_off(ctx, AsmRegRax, val->lo_val->full_name);
					save_stack_var(ctx, inst->opd0.var, AsmRegRax);
					break;
				case IraLoImpt:
					load_label_val(ctx, AsmRegRax, val->lo_val->full_name);
					save_stack_var(ctx, inst->opd0.var, AsmRegRax);
					break;
				default:
					u_assert_switch(val->lo_val->type);
			}
			break;
		case IraValNullPtr:
			load_int(ctx, AsmRegRax, AsmInstImm64, 0);
			save_stack_var(ctx, inst->opd0.var, AsmRegRax);
			break;
		default:
			u_assert_switch(var->dt->type);
	}
}
static void compile_copy(ctx_t * ctx, inst_t * inst) {
	ira_dt_t * dt = inst->opd0.var->dt;

	switch (dt->type) {
		case IraDtVoid:
			break;
		case IraDtInt:
		{
			asm_reg_t reg = int_type_to_ax[dt->int_type];

			load_stack_var(ctx, reg, inst->opd1.var);
			save_stack_var(ctx, inst->opd0.var, reg);
			break;
		}
		case IraDtPtr:
			load_stack_var(ctx, AsmRegRax, inst->opd1.var);
			save_stack_var(ctx, inst->opd0.var, AsmRegRax);
			break;
		case IraDtArr:
			load_stack_var_off(ctx, AsmRegRax, inst->opd1.var, 0);
			save_stack_var_off(ctx, inst->opd0.var, AsmRegRax, 0);

			load_stack_var_off(ctx, AsmRegRax, inst->opd1.var, 8);
			save_stack_var_off(ctx, inst->opd0.var, AsmRegRax, 8);
			break;
		default:
			u_assert_switch(dt->type);
	}
}
static void compile_bopr_int(ctx_t * ctx, inst_t * inst, asm_inst_type_t bopr_type) {
	ira_int_type_t int_type = inst->opd0.var->dt->int_type;
	
	asm_reg_t reg0 = int_type_to_ax[int_type], reg1 = int_type_to_cx[int_type];

	load_stack_var(ctx, reg0, inst->opd1.var);
	load_stack_var(ctx, reg1, inst->opd2.var);

	asm_inst_t bopr = { .type = bopr_type, .opds = AsmInstOpds_Reg_Reg, .reg0 = reg0, .reg1 = reg1 };

	asm_frag_push_inst(ctx->frag, &bopr);

	save_stack_var(ctx, inst->opd0.var, reg0);
}
static void compile_div_int(ctx_t * ctx, inst_t * inst) {
	div_int(ctx, inst->opd1.var, inst->opd2.var, inst->opd0.var, NULL);
}
static void compile_mod_int(ctx_t * ctx, inst_t * inst) {
	div_int(ctx, inst->opd1.var, inst->opd2.var, NULL, inst->opd0.var);
}
static void compile_mmbr_acc(ctx_t * ctx, inst_t * inst) {
	u_hs_t * mmbr = inst->opd2.hs;
	ira_dt_t * opd_dt = inst->opd1.var->dt;

	switch (opd_dt->type) {
		case IraDtArr:
			if (mmbr == ctx->pec->pds[IraPdsSizeMmbr]) {
				load_stack_var_off(ctx, AsmRegRax, inst->opd1.var, IRA_DT_ARR_SIZE_OFF);
				save_stack_var(ctx, inst->opd0.var, AsmRegRax);
			}
			else if (mmbr == ctx->pec->pds[IraPdsDataMmbr]) {
				load_stack_var_off(ctx, AsmRegRax, inst->opd1.var, IRA_DT_ARR_DATA_OFF);
				save_stack_var(ctx, inst->opd0.var, AsmRegRax);
			}
			else {
				u_assert_switch(mmbr);
			}
			break;
		default:
			u_assert_switch(opd_dt->type);
	}
}
static void compile_cast_int(ctx_t * ctx, inst_t * inst) {
	ira_dt_t * from = inst->opd1.var->dt;
	ira_dt_t * to = inst->opd2.dt;

	u_assert(from->type == IraDtInt && to->type == IraDtInt);

	load_stack_var(ctx, int_type_to_cx[from->int_type], inst->opd1.var);

	const int_cast_info_t * info = &int_cast_from_to[from->int_type][to->int_type];

	asm_inst_t cast = { .type = info->inst_type, .opds = AsmInstOpds_Reg_Reg, .reg0 = info->reg0, .reg1 = info->reg1 };

	asm_frag_push_inst(ctx->frag, &cast);

	save_stack_var(ctx, inst->opd0.var, info->reg0);
}
static void compile_cast(ctx_t * ctx, inst_t * inst) {
	ira_dt_t * from = inst->opd1.var->dt;
	ira_dt_t * to = inst->opd2.dt;

	switch (to->type) {
		case IraDtInt:
			switch (from->type) {
				case IraDtInt:
					compile_cast_int(ctx, inst);
					break;
				default:
					u_assert_switch(from->type);
			}
			break;
		case IraDtPtr:
			switch (from->type) {
				case IraDtPtr:
					load_stack_var(ctx, AsmRegRax, inst->opd1.var);
					save_stack_var(ctx, inst->opd0.var, AsmRegRax);
					break;
				default:
					u_assert_switch(from->type);
			}
			break;
		default:
			u_assert_switch(to->type);
	}
}
static void compile_addr_of(ctx_t * ctx, inst_t * inst) {
	asm_inst_t lea = { .type = AsmInstLea, .opds = AsmInstOpds_Reg_Mem, .reg0 = AsmRegRax, .mem_base = AsmRegRsp, .mem_disp_type = AsmInstDispAuto, .mem_disp = (int32_t)(ctx->stack_vars_pos + inst->opd1.var->pos) };

	asm_frag_push_inst(ctx->frag, &lea);

	save_stack_var(ctx, inst->opd0.var, AsmRegRax);
}
static void compile_call_func_ptr(ctx_t * ctx, inst_t * inst) {
	size_t arg_off = 0;

	if (w64_pass_caller_ret(ctx, inst->opd0.var)) {
		++arg_off;
	}

	for (var_t ** var = inst->opd3.vars, **var_end = var + inst->opd2.size; var != var_end; ++var, ++arg_off) {
		w64_load_caller_arg(ctx, *var, arg_off);
	}

	load_stack_var(ctx, AsmRegRax, inst->opd1.var);

	asm_inst_t call = { .type = AsmInstCall, .opds = AsmInstOpds_Reg, .reg0 = AsmRegRax };

	asm_frag_push_inst(ctx->frag, &call);

	ira_dt_t * ret_dt = inst->opd0.var->dt;

	switch (ret_dt->type) {
		case IraDtVoid:
			break;
		case IraDtInt:
		{
			asm_reg_t reg = int_type_to_ax[ret_dt->int_type];
			save_stack_var(ctx, inst->opd0.var, reg);
			break;
		}
		case IraDtPtr:
			save_stack_var(ctx, inst->opd0.var, AsmRegRax);
			break;
		default:
			u_assert_switch(ret_dt->type);
	}

}
static void compile_ret(ctx_t * ctx, inst_t * inst) {
	ira_dt_t * dt = inst->opd0.var->dt;

	switch (dt->type) {
		case IraDtVoid:
			break;
		case IraDtInt:
			load_stack_var(ctx, AsmRegEax, inst->opd0.var);
			break;
		default:
			u_assert_switch(dt->type);
	}

	emit_epilogue(ctx);

	asm_inst_t ret = { .type = AsmInstRet, .opds = AsmInstOpds_None };

	asm_frag_push_inst(ctx->frag, &ret);
}

static void compile_insts(ctx_t * ctx) {
	emit_prologue(ctx);

	for (inst_t * inst = ctx->insts, *inst_end = inst + ctx->insts_size; inst != inst_end; ++inst) {
		ira_inst_t * base = inst->base;

		switch (base->type) {
			case IraInstNone:
			case IraInstDefVar:
				break;
			case IraInstLoadVal:
				compile_load_val(ctx, inst);
				break;
			case IraInstCopy:
				compile_copy(ctx, inst);
				break;
			case IraInstAddInt:
				compile_bopr_int(ctx, inst, AsmInstAdd);
				break;
			case IraInstSubInt:
				compile_bopr_int(ctx, inst, AsmInstSub);
				break;
			case IraInstMulInt:
				compile_bopr_int(ctx, inst, AsmInstImul);
				break;
			case IraInstDivInt:
				compile_div_int(ctx, inst);
				break;
			case IraInstModInt:
				compile_mod_int(ctx, inst);
				break;
			case IraInstMmbrAcc:
				compile_mmbr_acc(ctx, inst);
				break;
			case IraInstCast:
				compile_cast(ctx, inst);
				break;
			case IraInstAddrOf:
				compile_addr_of(ctx, inst);
				break;
			case IraInstCallFuncPtr:
				compile_call_func_ptr(ctx, inst);
				break;
			case IraInstRet:
				compile_ret(ctx, inst);
				break;
			default:
				u_assert_switch(base->type);
		}
	}
}

static bool compile_core(ctx_t * ctx) {
	if (ctx->c_lo->type != IraLoFunc) {
		return false;
	}

	ctx->c_out = &ira_pec_c_get_pea(ctx->c_ctx)->frag;
	ctx->pec = ira_pec_c_get_pec(ctx->c_ctx);

	ctx->frag = asm_frag_create(AsmFragProc, ctx->c_out);

	push_label(ctx, ctx->c_lo->full_name);

	ctx->func = ctx->c_lo->func;

	if (!prepare_insts(ctx)) {
		return false;
	}

	if (!calculate_stack(ctx)) {
		return false;
	}

	compile_insts(ctx);

	return true;
}
bool ira_pec_ip_compile(ira_pec_c_ctx_t * c_ctx, ira_lo_t * lo) {
	ctx_t ctx = { .trg = TrgCompl, .c_ctx = c_ctx, .c_lo = lo };

	bool result = compile_core(&ctx);

	ctx_cleanup(&ctx);

	return result;
}

static bool execute_insts_make_dt_func(ctx_t * ctx, inst_t * inst) {
	size_t args_size = inst->opd2.size;

	ira_dt_n_t * args = _malloca(args_size * sizeof(*args));

	u_assert(args != NULL);

	{
		ira_dt_n_t * arg = args;
		u_hs_t ** id = inst->opd4.hss;
		for (var_t ** var = inst->opd3.vars, **var_end = var + args_size; var != var_end; ++var, ++arg, ++id) {
			var_t * var_ptr = *var;

			if (var_ptr->val == NULL) {
				_freea(args);
				return false;
			}

			*arg = (ira_dt_n_t){ .dt = var_ptr->val->dt_val, .name = *id };
		}
	}

	if (inst->opd1.var->val == NULL) {
		_freea(args);
		return false;
	}

	ira_val_destroy(inst->opd0.var->val);

	inst->opd0.var->val = ira_pec_make_val_imm_dt(ctx->pec, ira_pec_get_dt_func(ctx->pec, inst->opd1.var->val->dt_val, args_size, args));

	_freea(args);

	return true;
}
static bool execute_insts(ctx_t * ctx) {
	for (inst_t * inst = ctx->insts, *inst_end = inst + ctx->insts_size; inst != inst_end; ++inst) {
		switch (inst->base->type) {
			case IraInstNone:
			case IraInstDefVar:
				break;
			case IraInstLoadVal:
				ira_val_destroy(inst->opd0.var->val);

				inst->opd0.var->val = ira_val_copy(inst->opd1.val);
				break;
			case IraInstCopy:
				if (inst->opd1.var->val == NULL) {
					return false;
				}

				ira_val_destroy(inst->opd0.var->val);

				inst->opd0.var->val = ira_val_copy(inst->opd1.var->val);
				break;
			case IraInstMakeDtPtr:
				if (inst->opd1.var->val == NULL) {
					return false;
				}

				ira_val_destroy(inst->opd0.var->val);

				inst->opd0.var->val = ira_pec_make_val_imm_dt(ctx->pec, ira_pec_get_dt_ptr(ctx->pec, inst->opd1.var->val->dt_val));
				break;
			case IraInstMakeDtArr:
				if (inst->opd1.var->val == NULL) {
					return false;
				}

				ira_val_destroy(inst->opd0.var->val);

				inst->opd0.var->val = ira_pec_make_val_imm_dt(ctx->pec, ira_pec_get_dt_arr(ctx->pec, inst->opd1.var->val->dt_val));
				break;
			case IraInstMakeDtFunc:
				if (!execute_insts_make_dt_func(ctx, inst)) {
					return false;
				}
				break;
				//case IraInstAddInt:
				//case IraInstSubInt:
				//case IraInstMulInt:
				//case IraInstDivInt:
				//case IraInstModInt:
				//	break;
			case IraInstRet:
				*ctx->i_out = inst->opd0.var->val;

				inst->opd0.var->val = NULL;
				return true;
		}
	}

	return false;
}

static bool interpret_core(ctx_t * ctx) {
	if (!prepare_insts(ctx)) {
		return false;
	}

	if (!execute_insts(ctx)) {
		return false;
	}

	return true;
}
bool ira_pec_ip_interpret(ira_pec_t * pec, ira_func_t * func, ira_val_t ** out) {
	ctx_t ctx = { .trg = TrgIntrp, .pec = pec, .func = func, .i_out = out };

	bool result = interpret_core(&ctx);

	ctx_cleanup(&ctx);

	return result;
}