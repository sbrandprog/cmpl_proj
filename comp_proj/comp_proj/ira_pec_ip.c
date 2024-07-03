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

#define GLOBAL_LABEL_DELIM L'$'

#define STACK_ALIGN 16
#define STACK_UNIT 8

typedef struct inst inst_t;

typedef enum trg {
	TrgCompl,
	TrgIntrp,
	Trg_Count
} trg_t;

typedef struct ira_pec_ip_var var_t;
struct ira_pec_ip_var {
	var_t * next;

	ul_hs_t * name;

	ira_dt_qdt_t qdt;

	ira_val_t * val;

	size_t size;
	size_t align;

	size_t pos;
};

typedef struct ira_pec_ip_label label_t;
struct ira_pec_ip_label {
	label_t * next;
	ul_hs_t * name;
	inst_t * inst;
	ul_hs_t * global_name;
};

typedef union inst_opd {
	ira_int_cmp_t int_cmp;
	ira_dt_qual_t dt_qual;
	size_t size;
	ul_hs_t * hs;
	ul_hs_t ** hss;
	ira_dt_t * dt;
	label_t * label;
	ira_val_t * val;
	var_t * var;
	var_t ** vars;
	ira_lo_t * lo;
} inst_opd_t;
struct inst {
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

	union {
		struct {
			var_t * var;
		} def_var_copy;
		struct {
			size_t scale;
		} shift_ptr;
		struct {
			ira_dt_t * res_dt;
			size_t off;
		} mmbr_acc;
	};
};

typedef struct int_cast_info {
	asm_inst_type_t inst_type;
	asm_size_t from;
	asm_size_t to;
} int_cast_info_t;

typedef struct ira_pec_ip_ctx {
	trg_t trg;

	bool report_head_printed;

	ira_pec_c_ctx_t * c_ctx;
	ira_lo_t * c_lo;

	ira_val_t ** i_out;

	ul_hsb_t * hsb;
	ul_hst_t * hst;
	ira_pec_t * pec;

	ira_func_t * func;

	asm_frag_t * frag;

	var_t * var;
	label_t * label;

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

static const int_cast_info_t int_cast_infos[IraInt_Count][IraInt_Count] = {
	[IraIntU8] = {
		[IraIntU8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntU16] = { .inst_type = AsmInstMovzx, .from = AsmSize8, .to = AsmSize16 },
		[IraIntU32] = { .inst_type = AsmInstMovzx, .from = AsmSize8, .to = AsmSize32 },
		[IraIntU64] = { .inst_type = AsmInstMovzx, .from = AsmSize8, .to = AsmSize64 },

		[IraIntS8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntS16] = { .inst_type = AsmInstMovzx, .from = AsmSize8, .to = AsmSize16 },
		[IraIntS32] = { .inst_type = AsmInstMovzx, .from = AsmSize8, .to = AsmSize32 },
		[IraIntS64] = { .inst_type = AsmInstMovzx, .from = AsmSize8, .to = AsmSize64 },
	},
	[IraIntU16] = {
		[IraIntU8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntU16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntU32] = { .inst_type = AsmInstMovzx, .from = AsmSize16, .to = AsmSize32 },
		[IraIntU64] = { .inst_type = AsmInstMovzx, .from = AsmSize16, .to = AsmSize64 },

		[IraIntS8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntS16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntS32] = { .inst_type = AsmInstMovzx, .from = AsmSize16, .to = AsmSize32 },
		[IraIntS64] = { .inst_type = AsmInstMovzx, .from = AsmSize16, .to = AsmSize64 },
	},
	[IraIntU32] = {
		[IraIntU8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntU16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntU32] = { .inst_type = AsmInstMov, .from = AsmSize32, .to = AsmSize32 },
		[IraIntU64] = { .inst_type = AsmInstMov, .from = AsmSize32, .to = AsmSize32 },

		[IraIntS8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntS16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntS32] = { .inst_type = AsmInstMov, .from = AsmSize32, .to = AsmSize32 },
		[IraIntS64] = { .inst_type = AsmInstMov, .from = AsmSize32, .to = AsmSize32 },
	},
	[IraIntU64] = {
		[IraIntU8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntU16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntU32] = { .inst_type = AsmInstMov, .from = AsmSize32, .to = AsmSize32 },
		[IraIntU64] = { .inst_type = AsmInstMov, .from = AsmSize64, .to = AsmSize64 },

		[IraIntS8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntS16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntS32] = { .inst_type = AsmInstMov, .from = AsmSize32, .to = AsmSize32 },
		[IraIntS64] = { .inst_type = AsmInstMov, .from = AsmSize64, .to = AsmSize64 },
	},

	[IraIntS8] = {
		[IraIntU8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntU16] = { .inst_type = AsmInstMovzx, .from = AsmSize8, .to = AsmSize16 },
		[IraIntU32] = { .inst_type = AsmInstMovzx, .from = AsmSize8, .to = AsmSize32 },
		[IraIntU64] = { .inst_type = AsmInstMovzx, .from = AsmSize8, .to = AsmSize64 },

		[IraIntS8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntS16] = { .inst_type = AsmInstMovsx, .from = AsmSize8, .to = AsmSize16 },
		[IraIntS32] = { .inst_type = AsmInstMovsx, .from = AsmSize8, .to = AsmSize32 },
		[IraIntS64] = { .inst_type = AsmInstMovsx, .from = AsmSize8, .to = AsmSize64 },
	},
	[IraIntS16] = {
		[IraIntU8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntU16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntU32] = { .inst_type = AsmInstMovzx, .from = AsmSize16, .to = AsmSize32 },
		[IraIntU64] = { .inst_type = AsmInstMovzx, .from = AsmSize16, .to = AsmSize64 },

		[IraIntS8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntS16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntS32] = { .inst_type = AsmInstMovsx, .from = AsmSize16, .to = AsmSize32 },
		[IraIntS64] = { .inst_type = AsmInstMovsx, .from = AsmSize16, .to = AsmSize64 },
	},
	[IraIntS32] = {
		[IraIntU8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntU16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntU32] = { .inst_type = AsmInstMov, .from = AsmSize32, .to = AsmSize32 },
		[IraIntU64] = { .inst_type = AsmInstMov, .from = AsmSize32, .to = AsmSize32 },

		[IraIntS8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntS16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntS32] = { .inst_type = AsmInstMov, .from = AsmSize32, .to = AsmSize32 },
		[IraIntS64] = { .inst_type = AsmInstMovsxd, .from = AsmSize32, .to = AsmSize64 },
	},
	[IraIntS64] = {
		[IraIntU8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntU16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntU32] = { .inst_type = AsmInstMov, .from = AsmSize32, .to = AsmSize32 },
		[IraIntU64] = { .inst_type = AsmInstMov, .from = AsmSize64, .to = AsmSize64 },

		[IraIntS8] = { .inst_type = AsmInstMov, .from = AsmSize8, .to = AsmSize8 },
		[IraIntS16] = { .inst_type = AsmInstMov, .from = AsmSize16, .to = AsmSize16 },
		[IraIntS32] = { .inst_type = AsmInstMov, .from = AsmSize32, .to = AsmSize32 },
		[IraIntS64] = { .inst_type = AsmInstMov, .from = AsmSize64, .to = AsmSize64 },
	},
};

static void report(ctx_t * ctx, const wchar_t * format, ...) {
	ul_assert(ctx->trg < Trg_Count);

	if (!ctx->report_head_printed) {
		fwprintf(stderr, L"reporting error in instruction processor [%s]\n", trg_to_str[ctx->trg]);

		if (ctx->c_lo != NULL) {
			fwprintf(stderr, L"processing [%s] function language object\n", ctx->c_lo->full_name->str);
		}
		else {
			fwprintf(stderr, L"processing anonymouse function\n");
		}

		ctx->report_head_printed = true;
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

	report(ctx, L"[%s] requires opd[%zi] to have an 'data type' data type", info->type_str.str, first);
}
static void report_var_const_q(ctx_t * ctx, inst_t * inst, size_t opd) {
	const ira_inst_info_t * info = &ira_inst_infos[inst->base->type];

	report(ctx, L"[%s]: opd[0] var must not have a const qualifier", info->type_str.str);
}

static bool check_var_for_dt_dt(ctx_t * ctx, inst_t * inst, size_t opd) {
	if (inst->opds[opd].var->qdt.dt != &ctx->pec->dt_dt) {
		report_opd_not_equ_dt(ctx, inst, opd);
		return false;
	}

	return true;
}
static bool check_var_for_not_const_q(ctx_t * ctx, inst_t * inst, size_t opd) {
	if (inst->opds[opd].var->qdt.qual.const_q != false) {
		report_var_const_q(ctx, inst, opd);
		return false;
	}

	return false;
}

static void cleanup_inst(ctx_t * ctx, inst_t * inst) {
	ira_inst_t * base = inst->base;

	if (base == NULL) {
		return;
	}

	const ira_inst_info_t * info = &ira_inst_infos[inst->base->type];

	for (size_t opd = 0; opd < IRA_INST_OPDS_SIZE; ++opd) {
		switch (info->opds[opd]) {
			case IraInstOpdNone:
			case IraInstOpdIntCmp:
			case IraInstOpdDtQual:
			case IraInstOpdDt:
			case IraInstOpdLabel:
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
				ul_assert_unreachable();
		}
	}
}
static void ctx_cleanup(ctx_t * ctx) {
	for (inst_t * inst = ctx->insts, *inst_end = inst + ctx->insts_size; inst != inst_end; ++inst) {
		if (inst->base == NULL) {
			break;
		}

		cleanup_inst(ctx, inst);
	}

	free(ctx->insts);

	for (label_t * label = ctx->label; label != NULL; ) {
		label_t * next = label->next;

		free(label);

		label = next;
	}

	for (var_t * var = ctx->var; var != NULL;) {
		var_t * next = var->next;

		ira_val_destroy(var->val);

		free(var);

		var = next;
	}
}

static bool define_var(ctx_t * ctx, ira_dt_t * dt, ira_dt_qual_t dt_qual, ul_hs_t * name) {
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

	ul_assert(new_var != NULL);

	size_t dt_size, dt_align;

	if (!ira_dt_get_size(dt, &dt_size) || !ira_dt_get_align(dt, &dt_align)) {
		report(ctx, L"size & alignment of variable [%s] are undefined");
		return false;
	}

	*new_var = (var_t){ .name = name, .qdt = { .dt = dt, .qual = dt_qual }, .size = dt_size, .align = dt_align };

	*ins = new_var;

	return true;
}
static var_t * find_var(ctx_t * ctx, ul_hs_t * name) {
	for (var_t * var = ctx->var; var != NULL; var = var->next) {
		if (var->name == name) {
			return var;
		}
	}

	return NULL;
}
static label_t * get_label(ctx_t * ctx, ul_hs_t * name) {
	label_t ** ins = &ctx->label;

	for (; *ins != NULL; ins = &(*ins)->next) {
		label_t * label = *ins;

		if (label->name == name) {
			return label;
		}
	}

	label_t * new_label = malloc(sizeof(*new_label));

	ul_assert(new_label != NULL);

	*new_label = (label_t){ .name = name };

	*ins = new_label;

	return new_label;
}
static bool define_label(ctx_t * ctx, inst_t * inst, label_t * label) {
	if (label->inst != NULL) {
		report(ctx, L">1 instructions define [%s] label", label->name->str);
		return false;
	}

	label->inst = inst;

	return true;
}

static bool set_mmbr_acc_ptr_data_stct(ctx_t * ctx, inst_t * inst, ira_dt_t * dt, ul_hs_t * mmbr) {
	ira_dt_sd_t * sd = dt->stct.lo->dt_stct.sd;

	if (sd == NULL) {
		return false;
	}

	ira_dt_ndt_t * elem = sd->elems, * elem_end = elem + sd->elems_size;

	for (; elem != elem_end; ++elem) {
		if (mmbr == elem->name) {
			ira_dt_t * res_dt;

			if (!ira_pec_apply_qual(ctx->pec, elem->dt, dt->stct.qual, &res_dt)) {
				return false;
			}

			inst->mmbr_acc.res_dt = res_dt;
			inst->mmbr_acc.off = ira_dt_get_sd_elem_off(sd, elem - sd->elems);

			return true;
		}
	}

	return false;
}
static bool set_mmbr_acc_ptr_data(ctx_t * ctx, inst_t * inst, ira_dt_t * opd_dt, ul_hs_t * mmbr) {
	switch (opd_dt->type) {
		case IraDtVoid:
		case IraDtBool:
		case IraDtInt:
		case IraDtVec:
		case IraDtPtr:
			return false;
		case IraDtStct:
			if (!set_mmbr_acc_ptr_data_stct(ctx, inst, opd_dt, mmbr)) {
				return false;
			}
			break;
		case IraDtArr:
			if (!set_mmbr_acc_ptr_data_stct(ctx, inst, opd_dt->arr.assoc_stct, mmbr)) {
				return false;
			}
			break;
		case IraDtFunc:
			return false;
		default:
			ul_assert_unreachable();
	}

	return true;
}

static bool prepare_insts_args(ctx_t * ctx) {
	ira_dt_t * func_dt = ctx->func->dt;

	if (func_dt != NULL) {
		for (ira_dt_ndt_t * arg = func_dt->func.args, *arg_end = arg + func_dt->func.args_size; arg != arg_end; ++arg) {
			if (!define_var(ctx, arg->dt, ira_dt_qual_none, arg->name)) {
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

	ul_assert(vars != NULL);

	inst->opds[opd].vars = vars;

	ul_hs_t ** var_name = base->opds[opd].hss;
	for (var_t ** var = vars, **var_end = var + vars_size; var != var_end; ++var, ++var_name) {
		*var = find_var(ctx, *var_name);

		if (*var == NULL) {
			report(ctx, L"[%s]: can not find a variable [%s] for [%zi] operand", ira_inst_infos[base->type].type_str.str, base->opds[opd].hs->str, opd);
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
			case IraInstOpdIntCmp:
				inst->opds[opd].int_cmp = ira_inst->opds[opd].int_cmp;

				if (inst->opds[opd].int_cmp >= IraIntCmp_Count) {
					report(ctx, L"[%s]: value in [%zi]th operand is invalid", ira_inst_infos[ira_inst->type].type_str.str, opd);
					return false;
				}
				break;
			case IraInstOpdDtQual:
				inst->opds[opd].dt_qual = ira_inst->opds[opd].dt_qual;
				break;
			case IraInstOpdDt:
				inst->opds[opd].dt = ira_inst->opds[opd].dt;

				if (inst->opds[opd].dt == NULL) {
					report(ctx, L"[%s]: value in [%zi]th operand is invalid", ira_inst_infos[ira_inst->type].type_str.str, opd);
					return false;
				}
				break;
			case IraInstOpdLabel:
				if (ira_inst->opds[opd].hs == NULL) {
					report(ctx, L"[%s]: value in [%zi]th operand is invalid", ira_inst_infos[ira_inst->type].type_str.str, opd);
					return false;
				}

				inst->opds[opd].label = get_label(ctx, ira_inst->opds[opd].hs);
				break;
			case IraInstOpdVal:
				inst->opds[opd].val = ira_inst->opds[opd].val;

				if (inst->opds[opd].val == NULL) {
					report(ctx, L"[%s]: value in [%zi]th operand is invalid", ira_inst_infos[ira_inst->type].type_str.str, opd);
					return false;
				}
				break;
			case IraInstOpdVarDef:
				inst->opds[opd].hs = ira_inst->opds[opd].hs;

				if (inst->opds[opd].hs == NULL) {
					report(ctx, L"[%s]: value in [%zi]th operand is invalid", ira_inst_infos[ira_inst->type].type_str.str, opd);
					return false;
				}
				break;
			case IraInstOpdVar:
				if (ira_inst->opds[opd].hs == NULL) {
					report(ctx, L"[%s]: value in [%zi]th operand is invalid", ira_inst_infos[ira_inst->type].type_str.str, opd);
					return false;
				}

				inst->opds[opd].var = find_var(ctx, ira_inst->opds[opd].hs);

				if (inst->opds[opd].var == NULL) {
					report(ctx, L"[%s]: can not find a variable [%s] for [%zi] operand", ira_inst_infos[ira_inst->type].type_str.str, ira_inst->opds[opd].hs->str, opd);
					return false;
				}
				break;
			case IraInstOpdMmbr:
				inst->opds[opd].hs = inst->base->opds[opd].hs;

				if (inst->opds[opd].hs == NULL) {
					report(ctx, L"[%s]: value in [%zi]th operand is invalid", ira_inst_infos[ira_inst->type].type_str.str, opd);
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
				ul_assert_unreachable();
		}
	}

	return true;
}
static bool prepare_blank(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	return true;
}
static bool prepare_def_var(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (!define_var(ctx, inst->opd1.dt, inst->opd2.dt_qual, inst->opd0.hs)) {
		return false;
	}

	return true;
}
static bool prepare_def_label(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (!define_label(ctx, inst, inst->opd0.label)) {
		return false;
	}

	return true;
}
static bool prepare_load_val(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (!ira_dt_is_equivalent(inst->opd1.val->dt, inst->opd0.var->qdt.dt)) {
		report_opds_not_equ(ctx, inst, 1, 0);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	if (ctx->trg == TrgCompl) {
		if (!ira_pec_c_process_val_compl(ctx->c_ctx, inst->opd1.val)) {
			report(ctx, L"[%s]: value in opd[1] is non compilable", ira_inst_infos[inst->base->type].type_str.str);
			return false;
		}
	}

	return true;
}
static bool prepare_copy(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * opd0_dt = inst->opd0.var->qdt.dt, * opd1_dt = inst->opd1.var->qdt.dt;

	if (!ira_dt_is_equivalent(opd1_dt, opd0_dt)) {
		report_opds_not_equ(ctx, inst, 1, 0);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_def_var_copy(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (!define_var(ctx, inst->opd1.var->qdt.dt, inst->opd2.dt_qual, inst->opd0.hs)) {
		return false;
	}

	inst->def_var_copy.var = find_var(ctx, inst->opd0.hs);

	ul_assert(inst->def_var_copy.var != NULL);

	return true;
}
static bool prepare_addr_of(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	ira_dt_t * ptr_dt = inst->opd0.var->qdt.dt;

	if (ptr_dt->type != IraDtPtr) {
		report(ctx, L"[%s]: opd[0] must have a pointer data type", info->type_str.str);
		return false;
	}

	if (!ira_dt_is_equivalent(inst->opd1.var->qdt.dt, ptr_dt->ptr.body)) {
		report_opd_not_equ_dt(ctx, inst, 1);
		return false;
	}

	if (!ira_dt_is_qual_equal(inst->opd1.var->qdt.qual, ptr_dt->ptr.qual)) {
		report(ctx, L"[%s]: opd[0] must have data type with equivalent qualifiers", info->type_str.str);
		return false;
	}

	return true;
}
static bool prepare_read_ptr(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * ptr_dt = inst->opd1.var->qdt.dt;

	if (ptr_dt->type != IraDtPtr) {
		report(ctx, L"[%s]: opd[1] must have a pointer data type", info->type_str.str);
		return false;
	}

	if (!ira_dt_is_equivalent(inst->opd0.var->qdt.dt, ptr_dt->ptr.body)) {
		report_opds_not_equ(ctx, inst, 0, 1);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_write_ptr(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * ptr_dt = inst->opd0.var->qdt.dt;

	if (ptr_dt->type != IraDtPtr) {
		report(ctx, L"[%s]: opd[0] must have a pointer data type", info->type_str.str);
		return false;
	}

	if (!ira_dt_is_equivalent(inst->opd1.var->qdt.dt, ptr_dt->ptr.body)) {
		report_opds_not_equ(ctx, inst, 1, 0);
		return false;
	}

	if (ptr_dt->ptr.qual.const_q == true) {
		report(ctx, L"[%s]: pointer of opd[0] must not have const qualifier", info->type_str.str);
		return false;
	}

	return true;
}
static bool prepare_shift_ptr(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * ptr_dt = inst->opd1.var->qdt.dt;

	if (ptr_dt->type != IraDtPtr) {
		report(ctx, L"[%s]: opd[1] must have a pointer data type", info->type_str.str);
		return false;
	}

	if (!ira_dt_is_equivalent(inst->opd0.var->qdt.dt, ptr_dt)) {
		report_opds_not_equ(ctx, inst, 0, 1);
		return false;
	}

	size_t scale;

	if (!ira_dt_get_size(ptr_dt->ptr.body, &scale)) {
		report(ctx, L"[%s]: opd[1] must have a [pointer to a sizable data type] data type", info->type_str.str);
		return false;
	}

	inst->shift_ptr.scale = scale;

	if (inst->opd2.var->qdt.dt->type != IraDtInt) {
		report(ctx, L"[%s]: opd[2] must have an integer data type", info->type_str.str);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_make_dt_vec(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (!check_var_for_dt_dt(ctx, inst, 1)) {
		return false;
	}

	ira_dt_t * dt = inst->opd2.var->qdt.dt;

	if (dt->type != IraDtInt) {
		report(ctx, L"[%s]: opd[2] must have an integer data type", info->type_str.str);
		return false;
	}

	if (!check_var_for_dt_dt(ctx, inst, 0)) {
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_make_dt_ptr(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (!check_var_for_dt_dt(ctx, inst, 1)) {
		return false;
	}

	if (!check_var_for_dt_dt(ctx, inst, 0)) {
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_make_dt_arr(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (!check_var_for_dt_dt(ctx, inst, 1)) {
		return false;
	}

	if (!check_var_for_dt_dt(ctx, inst, 0)) {
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_make_dt_stct(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	for (var_t ** var = inst->opd3.vars, **var_end = var + inst->opd2.size; var != var_end; ++var) {
		if ((*var)->qdt.dt != dt_dt) {
			report(ctx, L"[%s]: elements in opd[3] must have an 'data type' data type", info->type_str.str);
			return false;
		}
	}

	if (!check_var_for_dt_dt(ctx, inst, 0)) {
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_make_dt_func(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * dt_dt = &ctx->pec->dt_dt;

	for (var_t ** var = inst->opd3.vars, **var_end = var + inst->opd2.size; var != var_end; ++var) {
		if ((*var)->qdt.dt != dt_dt) {
			report(ctx, L"[%s]: arguments in opd[3] must have an 'data type' data type", info->type_str.str);
			return false;
		}
	}

	if (!check_var_for_dt_dt(ctx, inst, 1)) {
		return false;
	}

	if (!check_var_for_dt_dt(ctx, inst, 0)) {
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_make_dt_const(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (!check_var_for_dt_dt(ctx, inst, 1)) {
		return false;
	}

	if (!check_var_for_dt_dt(ctx, inst, 0)) {
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_unr_bool(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * dt = inst->opd1.var->qdt.dt;

	if (dt->type != IraDtBool) {
		report(ctx, L"[%s]: opd[1] must have an boolean data type", info->type_str.str);
		return false;
	}

	if (inst->opd0.var->qdt.dt != dt) {
		report_opds_not_equ(ctx, inst, 1, 0);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_unr_int(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * dt = inst->opd1.var->qdt.dt;

	if (dt->type != IraDtInt) {
		report(ctx, L"[%s]: opd[1] must have an integer data type", info->type_str.str);
		return false;
	}

	if (inst->opd0.var->qdt.dt != dt) {
		report_opds_not_equ(ctx, inst, 1, 0);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_bin_int(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * dt = inst->opd1.var->qdt.dt;

	if (dt->type != IraDtInt) {
		report(ctx, L"[%s]: opd[1] must have an integer data type", info->type_str.str);
		return false;
	}

	if (inst->opd2.var->qdt.dt != dt) {
		report_opds_not_equ(ctx, inst, 1, 2);
		return false;
	}

	if (inst->opd0.var->qdt.dt != dt) {
		report_opds_not_equ(ctx, inst, 1, 0);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_cmp_int(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * dt = inst->opd1.var->qdt.dt;

	if (dt->type != IraDtInt) {
		report(ctx, L"[%s]: opd[1] must have an integer data type", info->type_str.str);
		return false;
	}

	if (inst->opd2.var->qdt.dt != dt) {
		report_opds_not_equ(ctx, inst, 1, 2);
		return false;
	}

	if (inst->opd0.var->qdt.dt != &ctx->pec->dt_bool) {
		report(ctx, L"[%s]: opd[0] must have a boolean data type", info->type_str.str);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_cmp_ptr(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * dt = inst->opd1.var->qdt.dt;

	if (dt->type != IraDtPtr) {
		report(ctx, L"[%s]: opd[1] must have a pointer data type", info->type_str.str);
		return false;
	}

	if (!ira_dt_is_equivalent(inst->opd2.var->qdt.dt, dt)) {
		report_opds_not_equ(ctx, inst, 1, 2);
		return false;
	}

	if (inst->opd0.var->qdt.dt != &ctx->pec->dt_bool) {
		report(ctx, L"[%s]: opd[0] must have a boolean data type", info->type_str.str);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_cast(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * from = inst->opd1.var->qdt.dt;
	ira_dt_t * to = inst->opd2.dt;

	if (!ira_dt_is_castable(from, to)) {
		report(ctx, L"cannot cast operand of [%s] data type to a [%s] data type", ira_dt_infos[from->type].type_str.str, ira_dt_infos[to->type].type_str.str);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_call_func_ptr(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * ptr_func_dt = inst->opd1.var->qdt.dt;

	if (ptr_func_dt->type != IraDtPtr || ptr_func_dt->ptr.body->type != IraDtFunc) {
		report(ctx, L"[%s]: opd[0] must have a pointer to function data type", info->type_str.str);
		return false;
	}

	ira_dt_t * func_dt = ptr_func_dt->ptr.body;

	if (func_dt->func.args_size != inst->opd2.size) {
		report(ctx, L"[%s]: opd[2] must match function data type arguments size", info->type_str.str);
		return false;
	}

	ira_dt_ndt_t * arg_dt = func_dt->func.args;
	for (var_t ** var = inst->opd3.vars, **var_end = var + inst->opd2.size; var != var_end; ++var, ++arg_dt) {
		if (!ira_dt_is_equivalent(arg_dt->dt, (*var)->qdt.dt)) {
			report(ctx, L"[%s]: argument in opd[3] must match function argument data type", info->type_str.str);
			return false;
		}
	}

	if (!ira_dt_is_equivalent(inst->opd0.var->qdt.dt, func_dt->func.ret)) {
		report_opd_not_equ_dt(ctx, inst, 0);
		return false;
	}

	ctx->has_calls = true;
	ctx->call_args_size_max = max(ctx->call_args_size_max, inst->opd2.size);

	return true;
}
static bool prepare_mmbr_acc_ptr(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * opd_ptr_dt = inst->opd1.var->qdt.dt;

	if (opd_ptr_dt->type != IraDtPtr) {
		report(ctx, L"[%s]: opd[1] must have a pointer data type", info->type_str.str);
		return false;
	}

	ul_hs_t * mmbr = inst->opd2.hs;
	ira_dt_t * opd_dt = opd_ptr_dt->ptr.body;

	if (!set_mmbr_acc_ptr_data(ctx, inst, opd_dt, mmbr)) {
		report(ctx, L"[%s]: data type of opd[1] does not support pointer member access or [%s] member", info->type_str.str, mmbr->str);
		return false;
	}

	ira_dt_t * res_ptr_dt;

	if (!ira_pec_get_dt_ptr(ctx->pec, inst->mmbr_acc.res_dt, opd_ptr_dt->ptr.qual, &res_ptr_dt)) {
		report(ctx, L"[%s]: get_dt function error", info->type_str.str);
		return false;
	}

	if (!ira_dt_is_equivalent(inst->opd0.var->qdt.dt, res_ptr_dt)) {
		report(ctx, L"[%s]: opd[0] must have a pointer to [%s] member data type", info->type_str.str, mmbr->str);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_brc(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (inst->opd1.var->qdt.dt != &ctx->pec->dt_bool) {
		report(ctx, L"[%s]: opd[1] must have a boolean data type", info->type_str.str);
		return false;
	}

	return true;
}
static bool prepare_ret(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (!ira_dt_is_equivalent(ctx->func->dt->func.ret, inst->opd0.var->qdt.dt)) {
		report_opd_not_equ_dt(ctx, inst, 0);
		return false;
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

	ul_assert(ctx->insts != NULL);

	memset(ctx->insts, 0, ctx->insts_size * sizeof(*ctx->insts));

	inst_t * inst = ctx->insts;
	for (ira_inst_t * ira_inst = func->insts, *ira_inst_end = ira_inst + func->insts_size; ira_inst != ira_inst_end; ++ira_inst, ++inst) {
		if (ira_inst->type >= IraInst_Count) {
			report(ctx, L"unknown instruction type [%i]", ira_inst->type);
			return false;
		}

		inst->base = ira_inst;

		const ira_inst_info_t * info = &ira_inst_infos[ira_inst->type];

		{
			bool trg_comp;

			switch (ctx->trg) {
				case TrgCompl:
					trg_comp = info->compl_comp;
					break;
				case TrgIntrp:
					trg_comp = info->intrp_comp;
					break;
				default:
					ul_assert_unreachable();
			}

			if (!trg_comp) {
				report(ctx, L"[%s]: instruction is illegal in %s mode", info->type_str.str, trg_to_str[ctx->trg]);
				return false;
			}
		}

		if (!prepare_insts_opds(ctx, inst, ira_inst, info)) {
			return false;
		}

		typedef bool prepare_proc_t(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info);

		static prepare_proc_t * const prepare_insts_procs[IraInst_Count] = {
			[IraInstNone] = prepare_blank,
			[IraInstDefVar] = prepare_def_var,
			[IraInstDefLabel] = prepare_def_label,
			[IraInstLoadVal] = prepare_load_val,
			[IraInstCopy] = prepare_copy,
			[IraInstDefVarCopy] = prepare_def_var_copy,
			[IraInstAddrOf] = prepare_addr_of,
			[IraInstReadPtr] = prepare_read_ptr,
			[IraInstWritePtr] = prepare_write_ptr,
			[IraInstShiftPtr] = prepare_shift_ptr,
			[IraInstMakeDtVec] = prepare_make_dt_vec,
			[IraInstMakeDtPtr] = prepare_make_dt_ptr,
			[IraInstMakeDtStct] = prepare_make_dt_stct,
			[IraInstMakeDtArr] = prepare_make_dt_arr,
			[IraInstMakeDtFunc] = prepare_make_dt_func,
			[IraInstMakeDtConst] = prepare_make_dt_const,
			[IraInstNegBool] = prepare_unr_bool,
			[IraInstNegInt] = prepare_unr_int,
			[IraInstAddInt] = prepare_bin_int,
			[IraInstSubInt] = prepare_bin_int,
			[IraInstMulInt] = prepare_bin_int,
			[IraInstDivInt] = prepare_bin_int,
			[IraInstModInt] = prepare_bin_int,
			[IraInstLeShiftInt] = prepare_bin_int,
			[IraInstRiShiftInt] = prepare_bin_int,
			[IraInstCmpInt] = prepare_cmp_int,
			[IraInstAndInt] = prepare_bin_int,
			[IraInstXorInt] = prepare_bin_int,
			[IraInstOrInt] = prepare_bin_int,
			[IraInstCmpPtr] = prepare_cmp_ptr,
			[IraInstMmbrAccPtr] = prepare_mmbr_acc_ptr,
			[IraInstCast] = prepare_cast,
			[IraInstCallFuncPtr] = prepare_call_func_ptr,
			[IraInstBru] = prepare_blank,
			[IraInstBrt] = prepare_brc,
			[IraInstBrf] = prepare_brc,
			[IraInstRet] = prepare_ret,
		};

		prepare_proc_t * proc = prepare_insts_procs[ira_inst->type];

		ul_assert(proc != NULL);

		if (!proc(ctx, inst, info)) {
			return false;
		}
	}

	for (label_t * label = ctx->label; label != NULL; label = label->next) {
		if (label->inst == NULL) {
			report(ctx, L"could not find an instruction for [%s] label", label->name->str);
			return false;
		}
	}

	return true;
}


static void form_global_label_names(ctx_t * ctx, ul_hs_t * prefix) {
	for (label_t * label = ctx->label; label != NULL; label = label->next) {
		label->global_name = ul_hsb_formatadd(ctx->hsb, ctx->hst, L"%s%c%s", prefix->str, GLOBAL_LABEL_DELIM, label->name->str);
	}
}

static void push_label(ctx_t * ctx, ul_hs_t * name) {
	asm_inst_t label = { .type = AsmInstLabel, .opds = AsmInstLabel, .label = name };

	asm_frag_push_inst(ctx->frag, &label);
}

static bool calculate_stack(ctx_t * ctx) {
	size_t size = 0;

	if (ctx->has_calls) {
		size = ul_align_to(size, STACK_ALIGN);

		size_t args_size = max(ctx->call_args_size_max, 4);

		size += args_size * STACK_UNIT;
	}

	{
		size = ul_align_to(size, STACK_ALIGN);

		size_t var_pos = 0;

		for (var_t * var = ctx->var; var != NULL; var = var->next) {
			var_pos = ul_align_to(var_pos, var->align);

			var->pos = var_pos;

			var_pos += var->size;
		}

		ctx->stack_vars_pos = size;
		ctx->stack_vars_size = var_pos;

		size += var_pos;
	}

	size = ul_align_biased_to(size, STACK_ALIGN, STACK_ALIGN - STACK_UNIT);

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
	ul_assert(asm_reg_infos[reg].grps.gpr
		&& asm_size_infos[asm_reg_get_size(reg)].bytes + offset <= var->size);

	save_stack_gpr(ctx, (int32_t)(ctx->stack_vars_pos + var->pos + offset), reg);
}
static void save_stack_var(ctx_t * ctx, var_t * var, asm_reg_t reg) {
	save_stack_var_off(ctx, var, reg, 0);
}
static void load_int(ctx_t * ctx, asm_reg_t reg, int64_t imm) {
	asm_inst_imm_type_t imm_type = asm_inst_imm_size_to_type[asm_reg_get_size(reg)];

	ul_assert(imm_type != AsmInstImmNone);

	asm_inst_t load = { .type = AsmInstMov, .opds = AsmInstOpds_Reg_Imm, .reg0 = reg, .imm0_type = imm_type, .imm0 = imm };

	asm_frag_push_inst(ctx->frag, &load);
}
static void load_label_off(ctx_t * ctx, asm_reg_t reg, ul_hs_t * label) {
	ul_assert(asm_reg_get_size(reg) == AsmSize64);

	asm_inst_t lea = { .type = AsmInstLea, .opds = AsmInstOpds_Reg_Mem, .reg0 = reg, .mem_base = AsmRegRip, .mem_disp_type = AsmInstDispLabelRel32, .mem_disp_label = label };

	asm_frag_push_inst(ctx->frag, &lea);
}
static void load_label_val(ctx_t * ctx, asm_reg_t reg, ul_hs_t * label) {
	asm_inst_t mov = { .type = AsmInstMov, .opds = AsmInstOpds_Reg_Mem, .reg0 = reg, .mem_size = asm_reg_get_size(reg), .mem_base = AsmRegRip, .mem_disp_type = AsmInstDispLabelRel32, .mem_disp_label = label };

	asm_frag_push_inst(ctx->frag, &mov);
}
static void load_stack_gpr(ctx_t * ctx, asm_reg_t reg, int32_t offset) {
	asm_inst_t load = { .type = AsmInstMov, .opds = AsmInstOpds_Reg_Mem, .reg0 = reg, .mem_size = asm_reg_get_size(reg), .mem_base = AsmRegRsp, .mem_disp_type = AsmInstDispAuto, .mem_disp = offset };

	asm_frag_push_inst(ctx->frag, &load);
}
static void load_stack_var_off(ctx_t * ctx, asm_reg_t reg, var_t * var, size_t offset) {
	ul_assert(asm_reg_infos[reg].grps.gpr
		&& asm_size_infos[asm_reg_get_size(reg)].bytes + offset <= var->size);

	load_stack_gpr(ctx, reg, (int32_t)(ctx->stack_vars_pos + var->pos + offset));
}
static void load_stack_var(ctx_t * ctx, asm_reg_t reg, var_t * var) {
	load_stack_var_off(ctx, reg, var, 0);
}
static void read_ptr_off(ctx_t * ctx, asm_reg_t reg, asm_reg_t ptr, size_t offset) {
	ul_assert(offset < INT32_MAX);

	asm_inst_t mov = { .type = AsmInstMov, .opds = AsmInstOpds_Reg_Mem, .reg0 = reg, .mem_size = asm_reg_get_size(reg), .mem_base = ptr, .mem_disp_type = AsmInstDispAuto, .mem_disp = (int32_t)offset };

	asm_frag_push_inst(ctx->frag, &mov);
}
static void write_ptr_off(ctx_t * ctx, asm_reg_t ptr, asm_reg_t reg, size_t offset) {
	ul_assert(offset < INT32_MAX);

	asm_inst_t mov = { .type = AsmInstMov, .opds = AsmInstOpds_Mem_Reg, .mem_size = asm_reg_get_size(reg), .mem_base = ptr, .mem_disp_type = AsmInstDispAuto, .mem_disp = (int32_t)offset, .reg0 = reg };

	asm_frag_push_inst(ctx->frag, &mov);
}

static asm_reg_t get_gpr_reg_int(asm_reg_gpr_t gpr, ira_int_type_t int_type) {
	asm_size_t reg_size = ira_int_infos[int_type].asm_size;

	asm_reg_t reg = asm_reg_gprs[gpr][reg_size];

	ul_assert(reg != AsmRegNone);

	return reg;
}
static asm_reg_t get_gpr_reg_dt(asm_reg_gpr_t gpr, ira_dt_t * dt) {
	asm_size_t reg_size;

	switch (dt->type) {
		case IraDtBool:
			reg_size = AsmSize8;
			break;
		case IraDtInt:
			reg_size = ira_int_infos[dt->int_type].asm_size;
			break;
		case IraDtPtr:
			reg_size = AsmSize64;
			break;
		default:
			ul_assert_unreachable();
	}

	asm_reg_t reg = asm_reg_gprs[gpr][reg_size];

	ul_assert(reg != AsmRegNone);

	return reg;
}

static asm_reg_t w64_get_arg_reg(ira_dt_t * dt, size_t arg) {
	ul_assert(arg < 4);

	asm_reg_t reg;

	switch (dt->type) {
		case IraDtBool:
			reg = w64_int_arg_to_reg[arg][IraIntU8];
			break;
		case IraDtInt:
			reg = w64_int_arg_to_reg[arg][dt->int_type];
			break;
		case IraDtPtr:
			reg = w64_int_arg_to_reg[arg][IraIntU64];
			break;
		default:
			ul_assert_unreachable();
	}

	return reg;
}
static bool w64_load_callee_ret_link(ctx_t * ctx, var_t * var) {
	ira_dt_t * var_dt = var->qdt.dt;

	switch (var_dt->type) {
		case IraDtVoid:
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
			return false;
		default:
			ul_assert_unreachable();
	}
}
static void w64_load_callee_arg(ctx_t * ctx, var_t * var, size_t arg) {
	ira_dt_t * var_dt = var->qdt.dt;

	asm_reg_t reg;

	int32_t arg_offset = (int32_t)(arg * STACK_UNIT);

	switch (var_dt->type) {
		case IraDtVoid:
			break;
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
			switch (arg) {
				case 0:
				case 1:
				case 2:
				case 3:
					reg = w64_get_arg_reg(var_dt, arg);
					load_stack_var(ctx, reg, var);
					break;
				default:
					reg = get_gpr_reg_dt(AsmRegGprAx, var_dt);
					load_stack_var(ctx, reg, var);
					save_stack_gpr(ctx, arg_offset, reg);
					break;
			}
			break;
		default:
			ul_assert_unreachable();
	}
}
static void w64_save_callee_ret(ctx_t * ctx, var_t * var) {
	ira_dt_t * var_dt = var->qdt.dt;

	asm_reg_t reg;

	switch (var_dt->type) {
		case IraDtVoid:
			break;
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
			reg = get_gpr_reg_dt(AsmRegGprAx, var_dt);
			save_stack_var(ctx, var, reg);
			break;
		default:
			ul_assert_unreachable();
	}
}
static bool w64_save_caller_ret_link(ctx_t * ctx) {
	ira_dt_t * var_dt = ctx->func->dt->func.ret;

	switch (var_dt->type) {
		case IraDtVoid:
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
			return false;
		default:
			ul_assert_unreachable();
	}
}
static void w64_save_caller_arg(ctx_t * ctx, ul_hs_t * arg_name, size_t arg) {
	var_t * var = find_var(ctx, arg_name);

	ul_assert(var != NULL);

	ira_dt_t * var_dt = var->qdt.dt;

	asm_reg_t reg;

	int32_t arg_offset = (int32_t)(ctx->stack_size + STACK_UNIT + arg * STACK_UNIT);

	switch (var_dt->type) {
		case IraDtVoid:
			break;
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
			switch (arg) {
				case 0:
				case 1:
				case 2:
				case 3:
					reg = w64_get_arg_reg(var_dt, arg);
					save_stack_var(ctx, var, reg);
					break;
				default:
					reg = get_gpr_reg_dt(AsmRegGprAx, var_dt);
					load_stack_gpr(ctx, reg, arg_offset);
					save_stack_var(ctx, var, reg);
					break;
			}
			break;
		default:
			ul_assert_unreachable();
	}
}
static void w64_load_caller_ret(ctx_t * ctx, var_t * var) {
	ira_dt_t * var_dt = var->qdt.dt;

	switch (var_dt->type) {
		case IraDtVoid:
			break;
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
		{
			asm_reg_t reg = get_gpr_reg_dt(AsmRegGprAx, var_dt);

			load_stack_var(ctx, reg, var);

			break;
		}
		default:
			ul_assert_unreachable();
	}
}

static void emit_prologue_save_args(ctx_t * ctx) {
	size_t arg = 0;

	if (w64_save_caller_ret_link(ctx)) {
		++arg;
	}

	ira_dt_t * func_dt = ctx->func->dt;

	for (ira_dt_ndt_t * arg_dt_n = func_dt->func.args, *arg_dt_n_end = arg_dt_n + func_dt->func.args_size; arg_dt_n != arg_dt_n_end; ++arg_dt_n, ++arg) {
		w64_save_caller_arg(ctx, arg_dt_n->name, arg);
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

static void addr_of_var(ctx_t * ctx, asm_reg_t reg, var_t * var) {
	asm_inst_t lea = { .type = AsmInstLea, .opds = AsmInstOpds_Reg_Mem, .reg0 = reg, .mem_base = AsmRegRsp, .mem_disp_type = AsmInstDispAuto, .mem_disp = (int32_t)(ctx->stack_vars_pos + var->pos) };

	asm_frag_push_inst(ctx->frag, &lea);
}
static void get_ptr_copy_data(var_t * var, asm_reg_t * reg_out, size_t * off_step_out) {
	if (var->align >= 8 && var->size % 8 == 0) {
		*reg_out = AsmRegRax;
		*off_step_out = 8;
	}
	else if (var->align >= 4 && var->size % 4 == 0) {
		*reg_out = AsmRegEax;
		*off_step_out = 4;
	}
	else if (var->align >= 2 && var->size % 2 == 0) {
		*reg_out = AsmRegAx;
		*off_step_out = 2;
	}
	else {
		*reg_out = AsmRegAl;
		*off_step_out = 1;
	}
}
static void div_int(ctx_t * ctx, var_t * opd0, var_t * opd1, var_t * div_out, var_t * mod_out) {
	ira_int_type_t int_type = opd0->qdt.dt->int_type;

	asm_inst_type_t dx_inst_type = AsmInstXor;
	asm_reg_t reg0 = get_gpr_reg_int(AsmRegGprAx, int_type), reg1 = get_gpr_reg_int(AsmRegGprCx, int_type), reg2;

	switch (int_type) {
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
			ul_assert_unreachable();
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
static asm_inst_type_t get_set_inst_type(bool sign, ira_int_cmp_t int_cmp) {
	if (sign) {
		switch (int_cmp) {
			case IraIntCmpLess:
				return AsmInstSetl;
			case IraIntCmpLessEq:
				return AsmInstSetle;
			case IraIntCmpEq:
				return AsmInstSete;
			case IraIntCmpGrtrEq:
				return AsmInstSetge;
			case IraIntCmpGrtr:
				return AsmInstSetg;
			case IraIntCmpNeq:
				return AsmInstSetne;
			default:
				ul_assert_unreachable();
		}
	}
	else {
		switch (int_cmp) {
			case IraIntCmpLess:
				return AsmInstSetb;
			case IraIntCmpLessEq:
				return AsmInstSetbe;
			case IraIntCmpEq:
				return AsmInstSete;
			case IraIntCmpGrtrEq:
				return AsmInstSetae;
			case IraIntCmpGrtr:
				return AsmInstSeta;
			case IraIntCmpNeq:
				return AsmInstSetne;
			default:
				ul_assert_unreachable();
		}
	}
}


static void compile_load_val_impl(ctx_t * ctx, var_t * var, ira_val_t * val) {
	switch (val->type) {
		case IraValImmVoid:
			break;
		case IraValImmBool:
			load_int(ctx, AsmRegAl, val->bool_val ? 1 : 0);
			save_stack_var(ctx, var, AsmRegAl);
			break;
		case IraValImmInt:
		{
			asm_reg_t reg = get_gpr_reg_int(AsmRegGprAx, val->dt->int_type);

			load_int(ctx, reg, val->int_val.si64);
			save_stack_var(ctx, var, reg);
			break;
		}
		case IraValImmPtr:
			load_int(ctx, AsmRegRax, (int64_t)val->int_val.ui64);
			save_stack_var(ctx, var, AsmRegRax);
			break;
		case IraValLoPtr:
			switch (val->lo_val->type) {
				case IraLoFunc:
				case IraLoVar:
					load_label_off(ctx, AsmRegRax, val->lo_val->full_name);
					save_stack_var(ctx, var, AsmRegRax);
					break;
				case IraLoImpt:
					load_label_val(ctx, AsmRegRax, val->lo_val->full_name);
					save_stack_var(ctx, var, AsmRegRax);
					break;
				default:
					ul_assert_unreachable();
			}
			break;
		case IraValImmArr:
		{
			ul_hs_t * arr_label = NULL;

			if (!ira_pec_c_compile_val_frag(ctx->c_ctx, val, &arr_label)) {
				ul_assert_unreachable();
			}

			ira_dt_sd_t * sd = val->dt->arr.assoc_stct->stct.lo->dt_stct.sd;
			
			ul_assert(sd != NULL);

			size_t off;

			off = ira_dt_get_sd_elem_off(sd, IRA_DT_ARR_SIZE_IND);
			load_int(ctx, AsmRegRax, (int64_t)val->arr_val.size);
			save_stack_var_off(ctx, var, AsmRegRax, off);

			off = ira_dt_get_sd_elem_off(sd, IRA_DT_ARR_DATA_IND);
			load_label_off(ctx, AsmRegRax, arr_label);
			save_stack_var_off(ctx, var, AsmRegRax, off);

			break;
		}
		default:
			ul_assert_unreachable();
	}
}
static void compile_copy_impl(ctx_t * ctx, var_t * dst, var_t * src, size_t src_off) {
	ira_dt_t * dt = dst->qdt.dt;

	switch (dt->type) {
		case IraDtVoid:
			break;
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
		{
			asm_reg_t reg = get_gpr_reg_dt(AsmRegGprAx, dt);

			load_stack_var_off(ctx, reg, src, src_off);
			save_stack_var(ctx, dst, reg);
			break;
		}
		case IraDtArr:
		{
			ira_dt_sd_t * sd = dt->arr.assoc_stct->stct.lo->dt_stct.sd;
			ul_assert(sd != NULL);

			size_t off;

			off = ira_dt_get_sd_elem_off(sd, IRA_DT_ARR_SIZE_IND);
			load_stack_var_off(ctx, AsmRegRax, src, src_off + off);
			save_stack_var_off(ctx, dst, AsmRegRax, off);

			off = ira_dt_get_sd_elem_off(sd, IRA_DT_ARR_DATA_IND);
			load_stack_var_off(ctx, AsmRegRax, src, src_off + off);
			save_stack_var_off(ctx, dst, AsmRegRax, off);
			break;
		}
		default:
			ul_assert_unreachable();
	}
}
static void compile_int_like_cmp(ctx_t * ctx, var_t * dst, var_t * src0, var_t * src1, ira_int_cmp_t int_cmp, bool cmp_sign) {
	asm_reg_t reg0 = get_gpr_reg_dt(AsmRegGprAx, src0->qdt.dt);
	asm_reg_t reg1 = get_gpr_reg_dt(AsmRegGprCx, src0->qdt.dt);

	load_stack_var(ctx, reg0, src0);
	load_stack_var(ctx, reg1, src1);

	{
		asm_inst_t cmp = { .type = AsmInstCmp, .opds = AsmInstOpds_Reg_Reg, .reg0 = reg0, .reg1 = reg1 };

		asm_frag_push_inst(ctx->frag, &cmp);
	}

	{
		asm_inst_t setcc = { .type = get_set_inst_type(cmp_sign, int_cmp), .opds = AsmInstOpds_Reg, .reg0 = AsmRegAl };

		asm_frag_push_inst(ctx->frag, &setcc);
	}

	save_stack_var(ctx, dst, AsmRegAl);
}
static void compile_int_like_cast(ctx_t * ctx, var_t * dst, var_t * src, ira_int_type_t from, ira_int_type_t to) {
	asm_reg_gpr_t gpr_from = AsmRegGprAx, gpr_to = AsmRegGprAx;

	const int_cast_info_t * info = &int_cast_infos[from][to];

	load_stack_var(ctx, get_gpr_reg_int(gpr_from, from), src);

	asm_inst_t cast = { .type = info->inst_type, .opds = AsmInstOpds_Reg_Reg, .reg0 = asm_reg_gprs[gpr_to][info->to], .reg1 = asm_reg_gprs[gpr_from][info->from] };

	asm_frag_push_inst(ctx->frag, &cast);

	save_stack_var(ctx, dst, get_gpr_reg_int(gpr_to, to));
}

static void compile_blank(ctx_t * ctx, inst_t * inst) {
	return;
}
static void compile_def_label(ctx_t * ctx, inst_t * inst) {
	push_label(ctx, inst->opd0.label->global_name);
}
static void compile_load_val(ctx_t * ctx, inst_t * inst) {
	compile_load_val_impl(ctx, inst->opd0.var, inst->opd1.val);
}
static void compile_copy(ctx_t * ctx, inst_t * inst) {
	compile_copy_impl(ctx, inst->opd0.var, inst->opd1.var, 0);
}
static void compile_def_var_copy(ctx_t * ctx, inst_t * inst) {
	compile_copy_impl(ctx, inst->def_var_copy.var, inst->opd1.var, 0);
}
static void compile_addr_of(ctx_t * ctx, inst_t * inst) {
	addr_of_var(ctx, AsmRegRax, inst->opd1.var);
	save_stack_var(ctx, inst->opd0.var, AsmRegRax);
}
static void compile_read_ptr(ctx_t * ctx, inst_t * inst) {
	var_t * ptr_var = inst->opd1.var;

	load_stack_var(ctx, AsmRegRcx, ptr_var);

	var_t * dst_var = inst->opd0.var;

	size_t off_step = 1;
	asm_reg_t reg = AsmRegAl;

	get_ptr_copy_data(dst_var, &reg, &off_step);

	for (size_t off = 0; off < dst_var->size; off += off_step) {
		read_ptr_off(ctx, reg, AsmRegRcx, off);
		save_stack_var_off(ctx, dst_var, reg, off);
	}
}
static void compile_write_ptr(ctx_t * ctx, inst_t * inst) {
	var_t * ptr_var = inst->opd0.var;

	load_stack_var(ctx, AsmRegRcx, ptr_var);

	var_t * src_var = inst->opd1.var;

	size_t off_step = 1;
	asm_reg_t reg = AsmRegAl;

	get_ptr_copy_data(src_var, &reg, &off_step);

	for (size_t off = 0; off < src_var->size; off += off_step) {
		load_stack_var_off(ctx, reg, src_var, off);
		write_ptr_off(ctx, AsmRegRcx, reg, off);
	}
}
static void compile_shift_ptr(ctx_t * ctx, inst_t * inst) {
	var_t * index_var = inst->opd2.var;
	ira_dt_t * index_dt = index_var->qdt.dt;

	ira_int_type_t index_int_type;

	{
		ira_int_type_t index_type_from = index_dt->int_type;

		index_int_type = ira_int_infos[index_type_from].sign ? IraIntS64 : IraIntU64;

		const int_cast_info_t * info = &int_cast_infos[index_type_from][index_int_type];

		load_stack_var(ctx, get_gpr_reg_int(AsmRegGprCx, index_type_from), index_var);

		asm_inst_t cast = { .type = info->inst_type, .opds = AsmInstOpds_Reg_Reg, .reg0 = asm_reg_gprs[AsmRegGprCx][info->to], .reg1 = asm_reg_gprs[AsmRegGprCx][info->from] };

		asm_frag_push_inst(ctx->frag, &cast);
	}

	{
		load_int(ctx, AsmRegRax, (int64_t)inst->shift_ptr.scale);

		asm_inst_t imul = { .type = AsmInstImul, .opds = AsmInstOpds_Reg_Reg, .reg0 = AsmRegRcx, .reg1 = AsmRegRax };

		asm_frag_push_inst(ctx->frag, &imul);
	}

	load_stack_var(ctx, AsmRegRax, inst->opd1.var);

	asm_inst_t add = { .type = AsmInstAdd, .opds = AsmInstOpds_Reg_Reg, .reg0 = AsmRegRax, .reg1 = AsmRegRcx };

	asm_frag_push_inst(ctx->frag, &add);

	save_stack_var(ctx, inst->opd0.var, AsmRegRax);
}
static void compile_neg_bool(ctx_t * ctx, inst_t * inst) {
	load_stack_var(ctx, AsmRegAl, inst->opd1.var);

	asm_inst_t test = { .type = AsmInstTest, .opds = AsmInstOpds_Reg_Reg, .reg0 = AsmRegAl, .reg1 = AsmRegAl };

	asm_frag_push_inst(ctx->frag, &test);

	asm_inst_t set = { .type = AsmInstSetz, .opds = AsmInstOpds_Reg, .reg0 = AsmRegAl };

	asm_frag_push_inst(ctx->frag, &set);

	save_stack_var(ctx, inst->opd0.var, AsmRegAl);
}
static void compile_unr_int(ctx_t * ctx, inst_t * inst) {
	asm_inst_type_t inst_type;

	switch (inst->base->type) {
		case IraInstNegInt:
			inst_type = AsmInstNeg;
			break;
		default:
			ul_assert_unreachable();
	}

	ira_int_type_t int_type = inst->opd0.var->qdt.dt->int_type;

	asm_reg_t reg = get_gpr_reg_int(AsmRegGprAx, int_type);

	load_stack_var(ctx, reg, inst->opd1.var);

	asm_inst_t unr_opr = { .type = inst_type, .opds = AsmInstOpds_Reg, .reg0 = reg };

	asm_frag_push_inst(ctx->frag, &unr_opr);

	save_stack_var(ctx, inst->opd0.var, reg);
}
static void compile_bin_int(ctx_t * ctx, inst_t * inst) {
	ira_int_type_t int_type = inst->opd0.var->qdt.dt->int_type;

	asm_inst_type_t inst_type;

	switch (inst->base->type) {
		case IraInstAddInt:
			inst_type = AsmInstAdd;
			break;
		case IraInstSubInt:
			inst_type = AsmInstSub;
			break;
		case IraInstMulInt:
			inst_type = AsmInstImul;
			break;
		case IraInstAndInt:
			inst_type = AsmInstAnd;
			break;
		case IraInstXorInt:
			inst_type = AsmInstXor;
			break;
		case IraInstOrInt:
			inst_type = AsmInstOr;
			break;
		default:
			ul_assert_unreachable();
	}

	asm_reg_t reg0 = get_gpr_reg_int(AsmRegGprAx, int_type), reg1 = get_gpr_reg_int(AsmRegGprCx, int_type);

	load_stack_var(ctx, reg0, inst->opd1.var);
	load_stack_var(ctx, reg1, inst->opd2.var);

	asm_inst_t bin_opr = { .type = inst_type, .opds = AsmInstOpds_Reg_Reg, .reg0 = reg0, .reg1 = reg1 };

	asm_frag_push_inst(ctx->frag, &bin_opr);

	save_stack_var(ctx, inst->opd0.var, reg0);
}
static void compile_div_int(ctx_t * ctx, inst_t * inst) {
	div_int(ctx, inst->opd1.var, inst->opd2.var, inst->opd0.var, NULL);
}
static void compile_mod_int(ctx_t * ctx, inst_t * inst) {
	div_int(ctx, inst->opd1.var, inst->opd2.var, NULL, inst->opd0.var);
}
static void compile_shift_int(ctx_t * ctx, inst_t * inst) {
	ira_int_type_t int_type = inst->opd0.var->qdt.dt->int_type;

	asm_inst_type_t inst_type;

	switch (inst->base->type) {
		case IraInstLeShiftInt:
			if (ira_int_infos[int_type].sign) {
				inst_type = AsmInstSal;
			}
			else {
				inst_type = AsmInstShl;
			}
			break;
		case IraInstRiShiftInt:
			if (ira_int_infos[int_type].sign) {
				inst_type = AsmInstSar;
			}
			else {
				inst_type = AsmInstShr;
			}
			break;
		default:
			ul_assert_unreachable();
	}

	load_stack_var(ctx, get_gpr_reg_int(AsmRegGprCx, int_type), inst->opd2.var);

	asm_reg_t shift_reg = get_gpr_reg_int(AsmRegGprAx, int_type);

	load_stack_var(ctx, shift_reg, inst->opd1.var);

	asm_inst_t shift = { .type = inst_type, .opds = AsmInstOpds_Reg_Reg, .reg0 = shift_reg, .reg1 = AsmRegCl };

	asm_frag_push_inst(ctx->frag, &shift);

	save_stack_var(ctx, inst->opd0.var, shift_reg);
}
static void compile_cmp_int(ctx_t * ctx, inst_t * inst) {
	ira_int_type_t int_type = inst->opd1.var->qdt.dt->int_type;

	compile_int_like_cmp(ctx, inst->opd0.var, inst->opd1.var, inst->opd2.var, inst->opd3.int_cmp, ira_int_infos[int_type].sign);
}
static void compile_cmp_ptr(ctx_t * ctx, inst_t * inst) {
	compile_int_like_cmp(ctx, inst->opd0.var, inst->opd1.var, inst->opd2.var, inst->opd3.int_cmp, false);
}
static void compile_mmbr_acc_ptr(ctx_t * ctx, inst_t * inst) {
	var_t * opd_var = inst->opd1.var;

	load_stack_var(ctx, AsmRegRax, opd_var);

	asm_inst_t lea = { .type = AsmInstLea, .opds = AsmInstOpds_Reg_Mem, .reg0 = AsmRegRax, .mem_base = AsmRegRax, .mem_disp_type = AsmInstDispAuto, .mem_disp = (int32_t)inst->mmbr_acc.off };

	asm_frag_push_inst(ctx->frag, &lea);

	save_stack_var(ctx, inst->opd0.var, AsmRegRax);
}
static void compile_cast_to_int(ctx_t * ctx, inst_t * inst, ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtInt:
			compile_int_like_cast(ctx, inst->opd0.var, inst->opd1.var, from->int_type, to->int_type);
			break;
		case IraDtPtr:
			compile_int_like_cast(ctx, inst->opd0.var, inst->opd1.var, IraIntU64, to->int_type);
			break;
		default:
			ul_assert_unreachable();
	}
}
static void compile_cast_to_ptr(ctx_t * ctx, inst_t * inst, ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtInt:
			compile_int_like_cast(ctx, inst->opd0.var, inst->opd1.var, from->int_type, IraIntU64);
			break;
		case IraDtPtr:
			load_stack_var(ctx, AsmRegRax, inst->opd1.var);
			save_stack_var(ctx, inst->opd0.var, AsmRegRax);
			break;
		default:
			ul_assert_unreachable();
	}
}
static void compile_cast(ctx_t * ctx, inst_t * inst) {
	ira_dt_t * from = inst->opd1.var->qdt.dt;
	ira_dt_t * to = inst->opd2.dt;

	switch (to->type) {
		case IraDtInt:
			compile_cast_to_int(ctx, inst, from, to);
			break;
		case IraDtPtr:
			compile_cast_to_ptr(ctx, inst, from, to);
			break;
		default:
			ul_assert_unreachable();
	}
}
static void compile_call_func_ptr(ctx_t * ctx, inst_t * inst) {
	size_t arg_off = 0;

	if (w64_load_callee_ret_link(ctx, inst->opd0.var)) {
		++arg_off;
	}

	for (var_t ** var = inst->opd3.vars, **var_end = var + inst->opd2.size; var != var_end; ++var, ++arg_off) {
		w64_load_callee_arg(ctx, *var, arg_off);
	}

	load_stack_var(ctx, AsmRegRax, inst->opd1.var);

	asm_inst_t call = { .type = AsmInstCall, .opds = AsmInstOpds_Reg, .reg0 = AsmRegRax };

	asm_frag_push_inst(ctx->frag, &call);

	w64_save_callee_ret(ctx, inst->opd0.var);
}
static void compile_bru(ctx_t * ctx, inst_t * inst) {
	asm_inst_t jmp = { .type = AsmInstJmp, .opds = AsmInstOpds_Imm, .imm0_type = AsmInstImmLabelRel32, .imm0_label = inst->opd0.label->global_name };

	asm_frag_push_inst(ctx->frag, &jmp);
}
static void compile_brc(ctx_t * ctx, inst_t * inst) {
	asm_inst_type_t jump_type;

	switch (inst->base->type) {
		case IraInstBrt:
			jump_type = AsmInstJnz;
			break;
		case IraInstBrf:
			jump_type = AsmInstJz;
			break;
		default:
			ul_assert_unreachable();
	}

	load_stack_var(ctx, AsmRegAl, inst->opd1.var);

	asm_inst_t test = { .type = AsmInstTest, .opds = AsmInstOpds_Reg_Reg, .reg0 = AsmRegAl, .reg1 = AsmRegAl };

	asm_frag_push_inst(ctx->frag, &test);

	asm_inst_t jump = { .type = jump_type, .opds = AsmInstOpds_Imm, .imm0_type = AsmInstImmLabelRel32, .imm0_label = inst->opd0.label->global_name };

	asm_frag_push_inst(ctx->frag, &jump);
}
static void compile_ret(ctx_t * ctx, inst_t * inst) {
	w64_load_caller_ret(ctx, inst->opd0.var);

	emit_epilogue(ctx);

	asm_inst_t ret = { .type = AsmInstRet, .opds = AsmInstOpds_None };

	asm_frag_push_inst(ctx->frag, &ret);
}

static void compile_insts(ctx_t * ctx) {
	emit_prologue(ctx);

	for (inst_t * inst = ctx->insts, *inst_end = inst + ctx->insts_size; inst != inst_end; ++inst) {
		typedef void compile_inst_proc_t(ctx_t * ctx, inst_t * inst);

		static compile_inst_proc_t * const compile_insts_procs[IraInst_Count] = {
			[IraInstNone] = compile_blank,
			[IraInstDefVar] = compile_blank,
			[IraInstDefLabel] = compile_def_label,
			[IraInstLoadVal] = compile_load_val,
			[IraInstCopy] = compile_copy,
			[IraInstDefVarCopy] = compile_def_var_copy,
			[IraInstAddrOf] = compile_addr_of,
			[IraInstReadPtr] = compile_read_ptr,
			[IraInstWritePtr] = compile_write_ptr,
			[IraInstShiftPtr] = compile_shift_ptr,
			[IraInstNegBool] = compile_neg_bool,
			[IraInstNegInt] = compile_unr_int,
			[IraInstAddInt] = compile_bin_int,
			[IraInstSubInt] = compile_bin_int,
			[IraInstMulInt] = compile_bin_int,
			[IraInstDivInt] = compile_div_int,
			[IraInstModInt] = compile_mod_int,
			[IraInstLeShiftInt] = compile_shift_int,
			[IraInstRiShiftInt] = compile_shift_int,
			[IraInstAndInt] = compile_bin_int,
			[IraInstXorInt] = compile_bin_int,
			[IraInstOrInt] = compile_bin_int,
			[IraInstCmpInt] = compile_cmp_int,
			[IraInstCmpPtr] = compile_cmp_ptr,
			[IraInstMmbrAccPtr] = compile_mmbr_acc_ptr,
			[IraInstCast] = compile_cast,
			[IraInstCallFuncPtr] = compile_call_func_ptr,
			[IraInstBru] = compile_bru,
			[IraInstBrt] = compile_brc,
			[IraInstBrf] = compile_brc,
			[IraInstRet] = compile_ret,
		};

		compile_inst_proc_t * proc = compile_insts_procs[inst->base->type];

		ul_assert(proc != NULL);

		proc(ctx, inst);
	}
}

static bool compile_core(ctx_t * ctx) {
	if (ctx->c_lo->type != IraLoFunc) {
		return false;
	}

	ctx->hsb = ira_pec_c_get_hsb(ctx->c_ctx);
	ctx->hst = ira_pec_c_get_hst(ctx->c_ctx);
	ctx->pec = ira_pec_c_get_pec(ctx->c_ctx);

	ctx->frag = ira_pec_c_get_frag(ctx->c_ctx, AsmFragProc);

	push_label(ctx, ctx->c_lo->full_name);

	ctx->func = ctx->c_lo->func;

	if (!prepare_insts(ctx)) {
		return false;
	}

	form_global_label_names(ctx, ctx->c_lo->full_name);

	if (!calculate_stack(ctx)) {
		return false;
	}

	compile_insts(ctx);

	return true;
}
bool ira_pec_ip_compile(ira_pec_c_ctx_t * c_ctx, ira_lo_t * lo) {
	ctx_t ctx = { .trg = TrgCompl, .c_ctx = c_ctx, .c_lo = lo };

	bool res = compile_core(&ctx);

	ctx_cleanup(&ctx);

	return res;
}


static void clear_var_val(ctx_t * ctx, var_t * var) {
	if (var->val == NULL) {
		return;
	}

	ira_val_destroy(var->val);

	var->val = NULL;
}
static bool check_var_for_val(ctx_t * ctx, inst_t * inst, size_t opd) {
	if (inst->opds[opd].var->val == NULL) {
		report(ctx, L"[%s]: value in opd[%zi] var is NULL", ira_inst_infos[inst->base->type].type_str.str, opd);
		return false;
	}

	return true;
}
static bool get_size_from_val(ira_val_t * val, size_t * out) {
	switch (val->type) {
		case IraValImmVoid:
		case IraValImmDt:
		case IraValImmBool:
			return false;
		case IraValImmInt:
			switch (val->dt->int_type) {
				case IraIntU8:
				case IraIntU16:
				case IraIntU32:
				case IraIntU64:
					*out = val->int_val.ui64;
					break;
				case IraIntS8:
					if (val->int_val.si8 < 0) {
						return false;
					}
					*out = (size_t)val->int_val.si8;
					break;
				case IraIntS16:
					if (val->int_val.si16 < 0) {
						return false;
					}
					*out = (size_t)val->int_val.si16;
					break;
				case IraIntS32:
					if (val->int_val.si32 < 0) {
						return false;
					}
					*out = (size_t)val->int_val.si32;
					break;
				case IraIntS64:
					if (val->int_val.si64 < 0) {
						return false;
					}
					*out = (size_t)val->int_val.si64;
					break;
				default:
					ul_assert_unreachable();
			}
			break;
		case IraValImmVec:
		case IraValImmPtr:
		case IraValLoPtr:
		case IraValImmStct:
		case IraValImmArr:
			return false;
		default:
			ul_assert_unreachable();
	}

	return true;
}

static bool execute_blank(ctx_t * ctx, inst_t * inst) {
	return true;
}
static bool execute_load_val(ctx_t * ctx, inst_t * inst) {
	clear_var_val(ctx, inst->opd0.var);

	inst->opd0.var->val = ira_val_copy(inst->opd1.val);

	return true;
}
static bool execute_copy(ctx_t * ctx, inst_t * inst) {
	if (!check_var_for_val(ctx, inst, 1)) {
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	inst->opd0.var->val = ira_val_copy(inst->opd1.var->val);

	return true;
}
static bool execute_read_ptr(ctx_t * ctx, inst_t * inst) {
	if (!check_var_for_val(ctx, inst, 1)) {
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	ira_val_t * val = inst->opd1.var->val;

	switch (val->type) {
		case IraValImmPtr:
			report(ctx, L"[%s]: no read of integer pointers in interpreter mode", ira_inst_infos[inst->base->type].type_str.str);
			return false;
		case IraValLoPtr:
		{
			ira_lo_t * lo = val->lo_val;

			ul_assert(lo != NULL);

			switch (lo->type) {
				case IraLoVar:
					inst->opd0.var->val = ira_val_copy(lo->var.val);
					break;
				default:
					report(ctx, L"[%s]: read of unsupported language object", ira_inst_infos[inst->base->type].type_str.str);
					return false;
			}

			break;
		}
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool execute_make_dt_vec(ctx_t * ctx, inst_t * inst) {
	if (!check_var_for_val(ctx, inst, 1)) {
		return false;
	}

	if (!check_var_for_val(ctx, inst, 2)) {
		return false;
	}

	size_t vec_size = 0;

	if (!get_size_from_val(inst->opd2.var->val, &vec_size)) {
		report(ctx, L"[%s]: failed to get size of vector", ira_inst_infos[inst->base->type].type_str.str);
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	ira_dt_t * res_dt;

	if (!ira_pec_get_dt_vec(ctx->pec, vec_size, inst->opd1.var->val->dt_val, inst->opd3.dt_qual, &res_dt)) {
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->pec, res_dt, &inst->opd0.var->val)) {
		return false;
	}

	return true;
}
static bool execute_make_dt_ptr(ctx_t * ctx, inst_t * inst) {
	if (!check_var_for_val(ctx, inst, 1)) {
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	ira_dt_t * res_dt;

	if (!ira_pec_get_dt_ptr(ctx->pec, inst->opd1.var->val->dt_val, inst->opd2.dt_qual, &res_dt)) {
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->pec, res_dt, &inst->opd0.var->val)) {
		return false;
	}

	return true;
}
static bool execute_make_dt_stct(ctx_t * ctx, inst_t * inst) {
	size_t elems_size = inst->opd2.size;

	ira_dt_ndt_t * elems = _malloca(elems_size * sizeof(*elems));

	ul_assert(elems != NULL);

	{
		ira_dt_ndt_t * elem = elems;
		ul_hs_t ** id = inst->opd4.hss;
		for (var_t ** var = inst->opd3.vars, **var_end = var + elems_size; var != var_end; ++var, ++elem, ++id) {
			var_t * var_ptr = *var;

			if (var_ptr->val == NULL) {
				report(ctx, L"[%s]: one of elements var value is NULL", ira_inst_infos[inst->base->type].type_str.str);
				_freea(elems);
				return false;
			}

			*elem = (ira_dt_ndt_t){ .dt = var_ptr->val->dt_val, .name = *id };
		}
	}

	clear_var_val(ctx, inst->opd0.var);

	ira_dt_t * res_dt;

	if (!ira_pec_get_dt_stct(ctx->pec, elems_size, elems, inst->opd1.dt_qual, &res_dt)) {
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->pec, res_dt, &inst->opd0.var->val)) {
		return false;
	}

	_freea(elems);

	return true;
}
static bool execute_make_dt_arr(ctx_t * ctx, inst_t * inst) {
	if (!check_var_for_val(ctx, inst, 1)) {
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	ira_dt_t * res_dt;

	if (!ira_pec_get_dt_arr(ctx->pec, inst->opd1.var->val->dt_val, inst->opd2.dt_qual, &res_dt)) {
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->pec, res_dt, &inst->opd0.var->val)) {
		return false;
	}

	return true;
}
static bool execute_make_dt_func(ctx_t * ctx, inst_t * inst) {
	size_t args_size = inst->opd2.size;

	ira_dt_ndt_t * args = _malloca(args_size * sizeof(*args));

	ul_assert(args != NULL);

	{
		ira_dt_ndt_t * arg = args;
		ul_hs_t ** id = inst->opd4.hss;
		for (var_t ** var = inst->opd3.vars, **var_end = var + args_size; var != var_end; ++var, ++arg, ++id) {
			var_t * var_ptr = *var;

			if (var_ptr->val == NULL) {
				report(ctx, L"[%s]: one of arguments var value is NULL", ira_inst_infos[inst->base->type].type_str.str);
				_freea(args);
				return false;
			}

			*arg = (ira_dt_ndt_t){ .dt = var_ptr->val->dt_val, .name = *id };
		}
	}

	if (!check_var_for_val(ctx, inst, 1)) {
		_freea(args);
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	ira_dt_t * res_dt;

	if (!ira_pec_get_dt_func(ctx->pec, inst->opd1.var->val->dt_val, args_size, args, &res_dt)) {
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->pec, res_dt, &inst->opd0.var->val)) {
		return false;
	}

	_freea(args);

	return true;
}
static bool execute_make_dt_const(ctx_t * ctx, inst_t * inst) {
	if (!check_var_for_val(ctx, inst, 1)) {
		return false;
	}

	ira_dt_t * dt = inst->opd1.var->val->dt_val;

	if (!ira_pec_apply_qual(ctx->pec, dt, ira_dt_qual_const, &dt)) {
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	if (!ira_pec_make_val_imm_dt(ctx->pec, dt, &inst->opd0.var->val)) {
		return false;
	}

	return true;
}
static bool execute_ret(ctx_t * ctx, inst_t * inst) {
	if (!check_var_for_val(ctx, inst, 0)) {
		return false;
	}

	*ctx->i_out = inst->opd0.var->val;

	inst->opd0.var->val = NULL;

	return true;
}
static bool execute_insts(ctx_t * ctx) {
	for (inst_t * inst = ctx->insts, *inst_end = inst + ctx->insts_size; inst != inst_end; ) {
		typedef bool execute_inst_proc_t(ctx_t * ctx, inst_t * inst);
		
		static execute_inst_proc_t * const execute_insts_procs[IraInst_Count] = {
			[IraInstNone] = execute_blank,
			[IraInstDefVar] = execute_blank,
			[IraInstDefLabel] = execute_blank,
			[IraInstLoadVal] = execute_load_val,
			[IraInstCopy] = execute_copy,
			[IraInstReadPtr] = execute_read_ptr,
			[IraInstMakeDtVec] = execute_make_dt_vec,
			[IraInstMakeDtPtr] = execute_make_dt_ptr,
			[IraInstMakeDtStct] = execute_make_dt_stct,
			[IraInstMakeDtArr] = execute_make_dt_arr,
			[IraInstMakeDtFunc] = execute_make_dt_func,
			[IraInstMakeDtConst] = execute_make_dt_const,
			[IraInstRet] = execute_ret
		};

		execute_inst_proc_t * proc = execute_insts_procs[inst->base->type];

		ul_assert(proc != NULL);

		if (!proc(ctx, inst)) {
			return false;
		}

		switch (inst->base->type) {
			case IraInstRet:
				return true;
			default:
				++inst;
				break;
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

	bool res = interpret_core(&ctx);

	ctx_cleanup(&ctx);

	return res;
}
