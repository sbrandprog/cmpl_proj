#include "ira_pec_ip.h"
#include "ira_dt.h"
#include "ira_val.h"
#include "ira_inst.h"
#include "ira_func.h"
#include "ira_lo.h"
#include "ira_pec_c.h"
#include "mc_size.h"
#include "mc_reg.h"
#include "mc_inst.h"
#include "mc_frag.h"
#include "mc_pea.h"

#define MOD_NAME L"ira_pec_ip"

#define GLOBAL_LABEL_DELIM L'$'

#define STACK_ALIGN 16
#define STACK_UNIT 8

typedef struct ira_pec_ip_bb bb_t;
typedef struct ira_pec_ip_bb_cmpl_gpr bb_cmpl_gpr_t;
typedef struct ira_pec_ip_inst inst_t;

typedef enum ira_pec_ip_trg {
	TrgCmpl,
	TrgIntr,
	Trg_Count
} trg_t;

typedef struct ira_pec_ip_var var_t;
typedef struct ira_pec_ip_var_bb_ref {
	bb_t * bb;
} var_bb_ref_t;
struct ira_pec_ip_var {
	var_t * next;

	ul_hs_t * name;

	ira_dt_qdt_t qdt;

	size_t bb_refs_size;
	var_bb_ref_t * bb_refs;
	size_t bb_refs_cap;

	union {
		struct {
			size_t size;
			size_t align;

			size_t stack_pos;
		} cmpl;
		struct {
			ira_val_t * val;
		} intr;
	};
};

typedef struct ira_pec_ip_label label_t;
struct ira_pec_ip_label {
	label_t * next;
	ul_hs_t * name;
	inst_t * inst;

	union {
		struct {
			ul_hs_t * global_name;
		} cmpl;
	};
};

typedef struct ira_pec_ip_bb_var_ref {
	var_t * var;
} bb_var_ref_t;
struct ira_pec_ip_bb {
	bb_t * next;

	size_t var_refs_size;
	bb_var_ref_t * var_refs;
	size_t var_refs_cap;
};
struct ira_pec_ip_bb_cmpl_gpr {
	var_t * var;
	mc_size_t reg_size;
	size_t off;
};
typedef struct ira_pec_ip_bb_cmpl_ctx {
	bb_t * cur_bb;
	bb_cmpl_gpr_t gprs[McRegGpr_Count];
} bb_cmpl_ctx_t;

typedef union ira_pec_ip_inst_opd {
	ira_int_cmp_t int_cmp;
	ira_dt_qual_t dt_qual;
	size_t size;
	ul_hs_t * hs;
	ul_hs_t ** hss;
	ira_dt_func_vas_t * dt_func_vas;
	ira_dt_t * dt;
	label_t * label;
	ira_val_t * val;
	var_t * var;
	var_t ** vars;
	ira_optr_t * optr;
	ira_lo_t * lo;
} inst_opd_t;
struct ira_pec_ip_inst {
	ira_inst_t * base;

	union {
		inst_opd_t opds[IRA_INST_OPDS_SIZE];
		struct {
			inst_opd_t opd0, opd1, opd2, opd3, opd4, opd5;
		};
	};

	bb_t * bb;

	union {
		struct {
			var_t * var;
		} def_var_copy;
		struct {
			size_t scale;
		} shift_ptr;
		struct {
			ira_dt_t * func_dt;
		} call_func_ptr;
		struct {
			ira_dt_t * res_dt;
			struct {
				size_t off;
			} cmpl;
		} mmbr_acc;
	};
};

typedef struct ira_pec_ip_int_cast_info {
	mc_inst_type_t inst_type;
	mc_size_t from;
	mc_size_t to;
} int_cast_info_t;

typedef struct ira_pec_ip_ctx {
	trg_t trg;

	ira_pec_t * pec;
	ira_func_t * func;

	union {
		struct {
			ira_pec_c_ctx_t * base_ctx;
			ira_lo_t * lo;

			ul_hsb_t * hsb;
			ul_hst_t * hst;

			mc_frag_t * frag;
			mc_frag_t * unw_frag;

			size_t stack_size;

			size_t stack_alloc_inst_size;

			size_t first_va_arg_ind;

			bb_cmpl_ctx_t bb_ctx;
		} cmpl;
		struct {
			ira_val_t ** out;
		} intr;
	};

	var_t * var;
	label_t * label;
	bb_t * bb;
	bb_t ** bb_ins;

	size_t insts_size;
	inst_t * insts;

	bool has_calls;
	size_t call_args_size_max;
} ctx_t;

static const wchar_t * trg_to_str[Trg_Count] = {
	[TrgCmpl] = L"compiler",
	[TrgIntr] = L"interpreter",
};

static const mc_reg_gpr_t w64_gpr_args[4] = {
	[0] = McRegGprCx,
	[1] = McRegGprDx,
	[2] = McRegGprR8,
	[3] = McRegGprR9,
};

static const int_cast_info_t int_cast_infos[IraInt_Count][IraInt_Count] = {
	[IraIntU8] = {
		[IraIntU8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntU16] = { .inst_type = McInstMovzx, .from = McSize8, .to = McSize16 },
		[IraIntU32] = { .inst_type = McInstMovzx, .from = McSize8, .to = McSize32 },
		[IraIntU64] = { .inst_type = McInstMovzx, .from = McSize8, .to = McSize64 },

		[IraIntS8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntS16] = { .inst_type = McInstMovzx, .from = McSize8, .to = McSize16 },
		[IraIntS32] = { .inst_type = McInstMovzx, .from = McSize8, .to = McSize32 },
		[IraIntS64] = { .inst_type = McInstMovzx, .from = McSize8, .to = McSize64 },
	},
	[IraIntU16] = {
		[IraIntU8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntU16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntU32] = { .inst_type = McInstMovzx, .from = McSize16, .to = McSize32 },
		[IraIntU64] = { .inst_type = McInstMovzx, .from = McSize16, .to = McSize64 },

		[IraIntS8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntS16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntS32] = { .inst_type = McInstMovzx, .from = McSize16, .to = McSize32 },
		[IraIntS64] = { .inst_type = McInstMovzx, .from = McSize16, .to = McSize64 },
	},
	[IraIntU32] = {
		[IraIntU8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntU16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntU32] = { .inst_type = McInstMov, .from = McSize32, .to = McSize32 },
		[IraIntU64] = { .inst_type = McInstMov, .from = McSize32, .to = McSize32 },

		[IraIntS8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntS16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntS32] = { .inst_type = McInstMov, .from = McSize32, .to = McSize32 },
		[IraIntS64] = { .inst_type = McInstMov, .from = McSize32, .to = McSize32 },
	},
	[IraIntU64] = {
		[IraIntU8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntU16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntU32] = { .inst_type = McInstMov, .from = McSize32, .to = McSize32 },
		[IraIntU64] = { .inst_type = McInstMov, .from = McSize64, .to = McSize64 },

		[IraIntS8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntS16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntS32] = { .inst_type = McInstMov, .from = McSize32, .to = McSize32 },
		[IraIntS64] = { .inst_type = McInstMov, .from = McSize64, .to = McSize64 },
	},

	[IraIntS8] = {
		[IraIntU8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntU16] = { .inst_type = McInstMovzx, .from = McSize8, .to = McSize16 },
		[IraIntU32] = { .inst_type = McInstMovzx, .from = McSize8, .to = McSize32 },
		[IraIntU64] = { .inst_type = McInstMovzx, .from = McSize8, .to = McSize64 },

		[IraIntS8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntS16] = { .inst_type = McInstMovsx, .from = McSize8, .to = McSize16 },
		[IraIntS32] = { .inst_type = McInstMovsx, .from = McSize8, .to = McSize32 },
		[IraIntS64] = { .inst_type = McInstMovsx, .from = McSize8, .to = McSize64 },
	},
	[IraIntS16] = {
		[IraIntU8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntU16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntU32] = { .inst_type = McInstMovzx, .from = McSize16, .to = McSize32 },
		[IraIntU64] = { .inst_type = McInstMovzx, .from = McSize16, .to = McSize64 },

		[IraIntS8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntS16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntS32] = { .inst_type = McInstMovsx, .from = McSize16, .to = McSize32 },
		[IraIntS64] = { .inst_type = McInstMovsx, .from = McSize16, .to = McSize64 },
	},
	[IraIntS32] = {
		[IraIntU8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntU16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntU32] = { .inst_type = McInstMov, .from = McSize32, .to = McSize32 },
		[IraIntU64] = { .inst_type = McInstMov, .from = McSize32, .to = McSize32 },

		[IraIntS8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntS16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntS32] = { .inst_type = McInstMov, .from = McSize32, .to = McSize32 },
		[IraIntS64] = { .inst_type = McInstMovsxd, .from = McSize32, .to = McSize64 },
	},
	[IraIntS64] = {
		[IraIntU8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntU16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntU32] = { .inst_type = McInstMov, .from = McSize32, .to = McSize32 },
		[IraIntU64] = { .inst_type = McInstMov, .from = McSize64, .to = McSize64 },

		[IraIntS8] = { .inst_type = McInstMov, .from = McSize8, .to = McSize8 },
		[IraIntS16] = { .inst_type = McInstMov, .from = McSize16, .to = McSize16 },
		[IraIntS32] = { .inst_type = McInstMov, .from = McSize32, .to = McSize32 },
		[IraIntS64] = { .inst_type = McInstMov, .from = McSize64, .to = McSize64 },
	},
};


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
			case IraInstOpdDtFuncVas:
			case IraInstOpdDt:
			case IraInstOpdLabel:
			case IraInstOpdVal:
			case IraInstOpdVarDef:
			case IraInstOpdVar:
			case IraInstOpdOptr:
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
static void destroy_label(ctx_t * ctx, label_t * label) {
	if (label == NULL) {
		return;
	}

	switch (ctx->trg) {
		case TrgCmpl:
		case TrgIntr:
			break;
		default:
			ul_assert_unreachable();
	}

	free(label);
}
static void destroy_label_chain(ctx_t * ctx, label_t * label) {
	while (label != NULL) {
		label_t * next = label->next;

		destroy_label(ctx, label);

		label = next;
	}
}
static void destroy_var(ctx_t * ctx, var_t * var) {
	if (var == NULL) {
		return;
	}

	switch (ctx->trg) {
		case TrgCmpl:
			break;
		case TrgIntr:
			ira_val_destroy(var->intr.val);
			break;
		default:
			ul_assert_unreachable();
	}

	free(var->bb_refs);
	free(var);
}
static void destroy_var_chain(ctx_t * ctx, var_t * var) {
	while (var != NULL) {
		var_t * next = var->next;

		destroy_var(ctx, var);

		var = next;
	}
}
static void destroy_bb(ctx_t * ctx, bb_t * bb) {
	if (bb == NULL) {
		return;
	}

	free(bb->var_refs);
	free(bb);
}
static void destroy_bb_chain(ctx_t * ctx, bb_t * bb) {
	while (bb != NULL) {
		bb_t * next = bb->next;

		destroy_bb(ctx, bb);

		bb = next;
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

	destroy_label_chain(ctx, ctx->label);

	destroy_var_chain(ctx, ctx->var);

	destroy_bb_chain(ctx, ctx->bb);
}


static void report(ctx_t * ctx, const wchar_t * fmt, ...) {
	if (ctx->pec->ec_fmtr == NULL) {
		return;
	}

	ul_assert(ctx->trg < Trg_Count);

	const wchar_t * func_name;

	switch (ctx->trg) {
		case TrgCmpl:
			func_name = ctx->cmpl.lo->name->str;
			break;
		case TrgIntr:
			func_name = L"anonymous";
			break;
		default:
			ul_assert_unreachable();
	}

	ul_ec_fmtr_format(ctx->pec->ec_fmtr, L"error in [%s], [%s] function", trg_to_str[ctx->trg], func_name);

	{
		va_list args;

		va_start(args, fmt);

		ul_ec_fmtr_format_va(ctx->pec->ec_fmtr, fmt, args);

		va_end(args);
	}

	ul_ec_fmtr_post(ctx->pec->ec_fmtr, UL_EC_REC_TYPE_DFLT, MOD_NAME);
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

static bool init_var(ctx_t * ctx, var_t * var, ira_dt_t * dt, ira_dt_qual_t dt_qual, ul_hs_t * name) {
	*var = (var_t){ .name = name, .qdt = { .dt = dt, .qual = dt_qual } };

	switch (ctx->trg) {
		case TrgCmpl:
			if (!ira_pec_get_dt_size(dt, &var->cmpl.size) || !ira_pec_get_dt_align(dt, &var->cmpl.align)) {
				report(ctx, L"size & alignment of variable [%s] are undefined", var->name->str);
				return false;
			}

			if (var->cmpl.align > STACK_ALIGN) {
				report(ctx, L"variable [%s] alignment [%zi] must be less or equal than stack alignment [%zi]", var->name, var->cmpl.align, STACK_ALIGN);
			}
			break;
		case TrgIntr:
			break;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static var_t * define_var(ctx_t * ctx, ira_dt_t * dt, ira_dt_qual_t dt_qual, ul_hs_t * name) {
	var_t ** ins = &ctx->var;

	while (*ins != NULL) {
		var_t * var = *ins;

		if (var->name == name) {
			report(ctx, L"variable [%s] already exists", name->str);
			return NULL;
		}

		ins = &var->next;
	}

	var_t * new_var = malloc(sizeof(*new_var));

	ul_assert(new_var != NULL);

	if (!init_var(ctx, new_var, dt, dt_qual, name)) {
		free(new_var);
		return NULL;
	}

	*ins = new_var;

	return new_var;
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
static bb_t * push_bb(ctx_t * ctx) {
	if (ctx->bb_ins == NULL) {
		ul_assert(ctx->bb == NULL);

		ctx->bb_ins = &ctx->bb;
	}

	bb_t * new_bb = malloc(sizeof(*new_bb));

	ul_assert(new_bb != NULL);

	*new_bb = (bb_t){ 0 };

	*ctx->bb_ins = new_bb;
	ctx->bb_ins = &new_bb->next;

	return new_bb;
}
static void make_var_bb_ref(ctx_t * ctx, var_t * var, bb_t * bb) {
	for (var_bb_ref_t * ref = var->bb_refs, *ref_end = ref + var->bb_refs_size; ref != ref_end; ++ref) {
		if (ref->bb == bb) {
			return;
		}
	}


	ul_arr_grow(var->bb_refs_size + 1, &var->bb_refs_cap, &var->bb_refs, sizeof(*var->bb_refs));

	var->bb_refs[var->bb_refs_size++] = (var_bb_ref_t){ .bb = bb };

	ul_arr_grow(bb->var_refs_size + 1, &bb->var_refs_cap, &bb->var_refs, sizeof(*bb->var_refs));

	bb->var_refs[bb->var_refs_size++] = (bb_var_ref_t){ .var = var };
}


static bool set_mmbr_acc_ptr_data_tpl(ctx_t * ctx, inst_t * inst, ira_dt_t * dt, ul_hs_t * mmbr) {
	for (ira_dt_ndt_t * elem = dt->tpl.elems, *elem_end = elem + dt->tpl.elems_size; elem != elem_end; ++elem) {
		if (mmbr == elem->name) {
			ira_dt_t * res_dt;

			if (!ira_pec_apply_qual(ctx->pec, elem->dt, dt->tpl.qual, &res_dt)) {
				return false;
			}

			inst->mmbr_acc.res_dt = res_dt;

			if (ctx->trg == TrgCmpl) {
				inst->mmbr_acc.cmpl.off = ira_pec_get_tpl_elem_off(dt, elem - dt->tpl.elems);
			}

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
		case IraDtTpl:
			if (!set_mmbr_acc_ptr_data_tpl(ctx, inst, opd_dt, mmbr)) {
				return false;
			}
			break;
		case IraDtStct:
		{
			ira_dt_t * tpl = opd_dt->stct.tag->tpl;

			if (tpl == NULL || !set_mmbr_acc_ptr_data_tpl(ctx, inst, tpl, mmbr)) {
				return false;
			}
			break;
		}
		case IraDtArr:
			if (!set_mmbr_acc_ptr_data_tpl(ctx, inst, opd_dt->arr.assoc_tpl, mmbr)) {
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
	bb_t * bb = push_bb(ctx);
	ira_dt_t * func_dt = ctx->func->dt;

	if (func_dt != NULL) {
		for (ira_dt_ndt_t * arg = func_dt->func.args, *arg_end = arg + func_dt->func.args_size; arg != arg_end; ++arg) {
			var_t * arg_var = define_var(ctx, arg->dt, ira_dt_qual_none, arg->name);

			if (arg_var == NULL) {
				return false;
			}

			make_var_bb_ref(ctx, arg_var, bb);
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
		if (*var_name == NULL) {
			report(ctx, L"[%s]: [%zi]th variable name in [%zi] operand is invalid", ira_inst_infos[base->type].type_str.str, (size_t)(var_name - base->opds[opd].hss), opd);
			return false;
		}
		
		*var = find_var(ctx, *var_name);

		if (*var == NULL) {
			report(ctx, L"[%s]: can not find a variable [%s] for [%zi] operand", ira_inst_infos[base->type].type_str.str, base->opds[opd].hs->str, opd);
			return false;
		}

		make_var_bb_ref(ctx, *var, inst->bb);
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
			case IraInstOpdDtFuncVas:
				inst->opds[opd].dt_func_vas = ira_inst->opds[opd].dt_func_vas;
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

				make_var_bb_ref(ctx, inst->opds[opd].var, inst->bb);

				break;
			case IraInstOpdOptr:
				if (ira_inst->opds[opd].optr == NULL) {
					report(ctx, L"[%s]: value in [%zi]th operand is invalid", ira_inst_infos[ira_inst->type].type_str.str, opd);
					return false;
				}

				inst->opds[opd].optr = ira_inst->opds[opd].optr;
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
	if (define_var(ctx, inst->opd1.dt, inst->opd2.dt_qual, inst->opd0.hs) == NULL) {
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

	if (ctx->trg == TrgCmpl) {
		if (!ira_pec_c_process_val_compl(ctx->cmpl.base_ctx, inst->opd1.val)) {
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
	var_t * var = define_var(ctx, inst->opd1.var->qdt.dt, inst->opd2.dt_qual, inst->opd0.hs);
	
	if (var == NULL) {
		return false;
	}

	inst->def_var_copy.var = var;

	make_var_bb_ref(ctx, var, inst->bb);

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

	if (!ira_pec_get_dt_size(ptr_dt->ptr.body, &scale)) {
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
static bool prepare_make_dt_tpl(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
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
static bool prepare_unr_optr(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_optr_t * optr = inst->opd1.optr;
	ira_dt_t * dt = inst->opd2.var->qdt.dt;

	switch (optr->type) {
		case IraOptrNone:
			report(ctx, L"[%s]: opd[1]: cannot apply null operator", info->type_str.str);
			return false;
		case IraOptrBltnNegBool:
			if (dt->type != IraDtBool) {
				report(ctx, L"[%s]: opd[2] must have an boolean data type", info->type_str.str);
				return false;
			}

			if (inst->opd0.var->qdt.dt != dt) {
				report_opds_not_equ(ctx, inst, 1, 0);
				return false;
			}
			break;
		case IraOptrBltnNegInt:
			if (dt->type != IraDtInt) {
				report(ctx, L"[%s]: opd[2] must have an integer data type", info->type_str.str);
				return false;
			}

			if (inst->opd0.var->qdt.dt != dt) {
				report_opds_not_equ(ctx, inst, 1, 0);
				return false;
			}
			break;
		case IraOptrBltnMulInt:
		case IraOptrBltnDivInt:
		case IraOptrBltnModInt:
		case IraOptrBltnAddInt:
		case IraOptrBltnSubInt:
		case IraOptrBltnLeShiftInt:
		case IraOptrBltnRiShiftInt:
		case IraOptrBltnLessInt:
		case IraOptrBltnLessEqInt:
		case IraOptrBltnGrtrInt:
		case IraOptrBltnGrtrEqInt:
		case IraOptrBltnEqInt:
		case IraOptrBltnNeqInt:
		case IraOptrBltnAndInt:
		case IraOptrBltnXorInt:
		case IraOptrBltnOrInt:
		case IraOptrBltnLessPtr:
		case IraOptrBltnLessEqPtr:
		case IraOptrBltnGrtrPtr:
		case IraOptrBltnGrtrEqPtr:
		case IraOptrBltnEqPtr:
		case IraOptrBltnNeqPtr:
			report(ctx, L"[%s]: opd[1]: binary operator in unary instruction", info->type_str.str);
			return false;
		default:
			ul_assert_unreachable();
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_bin_optr(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_optr_t * optr = inst->opd1.optr;
	ira_dt_t * left_dt = inst->opd2.var->qdt.dt, * right_dt = inst->opd3.var->qdt.dt;

	switch (optr->type) {
		case IraOptrNone:
			report(ctx, L"[%s]: opd[1]: cannot apply null operator");
			return false;
		case IraOptrBltnNegBool:
		case IraOptrBltnNegInt:
			report(ctx, L"[%s]: opd[1]: unary operator in binary instruction");
			return false;
		case IraOptrBltnMulInt:
		case IraOptrBltnDivInt:
		case IraOptrBltnModInt:
		case IraOptrBltnAddInt:
		case IraOptrBltnSubInt:
		case IraOptrBltnLeShiftInt:
		case IraOptrBltnRiShiftInt:
		case IraOptrBltnAndInt:
		case IraOptrBltnXorInt:
		case IraOptrBltnOrInt:
			if (left_dt->type != IraDtInt) {
				report(ctx, L"[%s]: opd[2] must have an integer data type", info->type_str.str);
				return false;
			}

			if (right_dt != left_dt) {
				report_opds_not_equ(ctx, inst, 2, 3);
				return false;
			}

			if (inst->opd0.var->qdt.dt != left_dt) {
				report_opds_not_equ(ctx, inst, 2, 0);
				return false;
			}
			break;
		case IraOptrBltnLessInt:
		case IraOptrBltnLessEqInt:
		case IraOptrBltnGrtrInt:
		case IraOptrBltnGrtrEqInt:
		case IraOptrBltnEqInt:
		case IraOptrBltnNeqInt:
			if (left_dt->type != IraDtInt) {
				report(ctx, L"[%s]: opd[2] must have an integer data type", info->type_str.str);
				return false;
			}

			if (right_dt != left_dt) {
				report_opds_not_equ(ctx, inst, 2, 3);
				return false;
			}

			if (inst->opd0.var->qdt.dt != &ctx->pec->dt_bool) {
				report(ctx, L"[%s]: opd[0] must have a boolean data type", info->type_str.str);
				return false;
			}
			break;
		case IraOptrBltnLessPtr:
		case IraOptrBltnLessEqPtr:
		case IraOptrBltnGrtrPtr:
		case IraOptrBltnGrtrEqPtr:
		case IraOptrBltnEqPtr:
		case IraOptrBltnNeqPtr:
			if (left_dt->type != IraDtPtr) {
				report(ctx, L"[%s]: opd[2] must have a pointer data type", info->type_str.str);
				return false;
			}

			if (!ira_dt_is_equivalent(right_dt, left_dt)) {
				report_opds_not_equ(ctx, inst, 2, 3);
				return false;
			}

			if (inst->opd0.var->qdt.dt != &ctx->pec->dt_bool) {
				report(ctx, L"[%s]: opd[0] must have a boolean data type", info->type_str.str);
				return false;
			}
			break;
		default:
			ul_assert_unreachable();
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_cast(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	ira_dt_t * from = inst->opd1.var->qdt.dt;
	ira_dt_t * to = inst->opd2.dt;

	if (!ira_dt_is_castable(from, to, false)) {
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

	switch (func_dt->func.vas->type) {
		case IraDtFuncVasNone:
			if (func_dt->func.args_size != inst->opd2.size) {
				report(ctx, L"[%s]: opd[2] must be equal to function data type arguments size", info->type_str.str);
				return false;
			}
			break;
		case IraDtFuncVasCstyle:
			if (func_dt->func.args_size > inst->opd2.size) {
				report(ctx, L"[%s]: opd[2] must be equal or greater than function data type arguments size", info->type_str.str);
				return false;
			}
			break;
		default:
			ul_assert_unreachable();
	}

	inst->call_func_ptr.func_dt = func_dt;

	ira_dt_ndt_t * arg_dt = func_dt->func.args;
	for (var_t ** var = inst->opd3.vars, **var_end = var + func_dt->func.args_size; var != var_end; ++var, ++arg_dt) {
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
static bool prepare_va_start(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (inst->opd0.var->qdt.dt != ctx->pec->dt_spcl.va_elem) {
		report(ctx, L"[%s]: opd[0] must have a va_elem data type", info->type_str.str);
		return false;
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
		return false;
	}

	return true;
}
static bool prepare_va_arg(ctx_t * ctx, inst_t * inst, const ira_inst_info_t * info) {
	if (inst->opd1.var->qdt.dt != ctx->pec->dt_spcl.va_elem) {
		report(ctx, L"[%s]: opd[1] must have a va_elem data type", info->type_str.str);
		return false;
	}

	ira_dt_t * dt = inst->opd0.var->qdt.dt;

	switch (ctx->trg) {
		case TrgCmpl:
			switch (dt->type) {
				case IraDtVoid:
				case IraDtBool:
				case IraDtInt:
				case IraDtVec:
				case IraDtPtr:
				case IraDtTpl:
				case IraDtStct:
				case IraDtArr:
					if (!ira_pec_is_dt_complete(dt)) {
						report(ctx, L"[%s]: data type must be a complete data type", info->type_str.str);
						return false;
					}
					break;
				case IraDtDt:
				case IraDtFunc:
					report(ctx, L"[%s]: invalid data type for compiler mode", info->type_str.str);
					return false;
				default:
					ul_assert_unreachable();
			}
			break;
		case TrgIntr:
			report(ctx, L"[%s]: unimplemented in intepretator mode", info->type_str.str);
			return false;
		default:
			ul_assert_unreachable();
	}

	if (check_var_for_not_const_q(ctx, inst, 0)) {
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
	bb_t * bb = NULL;
	for (ira_inst_t * ira_inst = func->insts, *ira_inst_end = ira_inst + func->insts_size; ira_inst != ira_inst_end; ++ira_inst, ++inst) {
		if (ira_inst->type >= IraInst_Count) {
			report(ctx, L"unknown instruction type [%i]", ira_inst->type);
			return false;
		}

		if (bb == NULL) {
			bb = push_bb(ctx);
		}

		inst->base = ira_inst;

		const ira_inst_info_t * info = &ira_inst_infos[ira_inst->type];

		{
			bool trg_comp;

			switch (ctx->trg) {
				case TrgCmpl:
					trg_comp = info->compl_comp;
					break;
				case TrgIntr:
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

		if (ira_inst->type == IraInstDefLabel) {
			bb = push_bb(ctx);

			inst->bb = bb;
		}
		else if (info->mods_ctl_flow) {
			inst->bb = bb;

			bb = NULL;
		}
		else {
			inst->bb = bb;
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
			[IraInstMakeDtTpl] = prepare_make_dt_tpl,
			[IraInstMakeDtArr] = prepare_make_dt_arr,
			[IraInstMakeDtFunc] = prepare_make_dt_func,
			[IraInstMakeDtConst] = prepare_make_dt_const,
			[IraInstUnrOptr] = prepare_unr_optr,
			[IraInstBinOptr] = prepare_bin_optr,
			[IraInstMmbrAccPtr] = prepare_mmbr_acc_ptr,
			[IraInstCast] = prepare_cast,
			[IraInstCallFuncPtr] = prepare_call_func_ptr,
			[IraInstBru] = prepare_blank,
			[IraInstBrt] = prepare_brc,
			[IraInstBrf] = prepare_brc,
			[IraInstRet] = prepare_ret,
			[IraInstVaStart] = prepare_va_start,
			[IraInstVaArg] = prepare_va_arg
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
		label->cmpl.global_name = ul_hsb_formatadd(ctx->cmpl.hsb, ctx->cmpl.hst, L"%s%c%s", prefix->str, GLOBAL_LABEL_DELIM, label->name->str);
	}
}

static bb_cmpl_gpr_t * get_bb_cmpl_ctx_gpr_from_reg(ctx_t * ctx, mc_reg_t reg) {
	bb_cmpl_ctx_t * bb_ctx = &ctx->cmpl.bb_ctx;

	ul_assert(reg < McReg_Count);

	const mc_reg_info_t * info = &mc_reg_infos[reg];

	ul_assert(info->grps.gpr);

	mc_reg_gpr_t gpr = info->gpr;

	return &bb_ctx->gprs[gpr];
}
static void reset_bb_cmpl_ctx(ctx_t * ctx, bb_t * new_bb) {
	bb_cmpl_ctx_t * bb_ctx = &ctx->cmpl.bb_ctx;

	bb_ctx->cur_bb = new_bb;

	for (mc_reg_gpr_t gpr = 0; gpr < McRegGpr_Count; ++gpr) {
		bb_ctx->gprs[gpr].var = NULL;
	}
}
static void reset_bb_cmpl_gpr_from_reg(ctx_t * ctx, mc_reg_t reg) {
	bb_cmpl_gpr_t * bb_gpr = get_bb_cmpl_ctx_gpr_from_reg(ctx, reg);

	if (bb_gpr != NULL) {
		bb_gpr->var = NULL;
	}
}
static void reset_bb_cmpl_gpr_from_var(ctx_t * ctx, var_t * var) {
	bb_cmpl_ctx_t * bb_ctx = &ctx->cmpl.bb_ctx;

	for (mc_reg_gpr_t gpr = 0; gpr < McRegGpr_Count; ++gpr) {
		if (bb_ctx->gprs[gpr].var == var) {
			bb_ctx->gprs[gpr].var = NULL;
		}
	}
}

static void push_mc_inst(ctx_t * ctx, mc_inst_t * inst) {
	mc_frag_push_inst(ctx->cmpl.frag, inst);
}
static void push_mc_label(ctx_t * ctx, ul_hs_t * name, lnk_sect_lp_stype_t label_stype) {
	mc_inst_t label = { .type = McInstLabel, .opds = McInstOpds_Label, .label_stype = label_stype, .label = name };

	push_mc_inst(ctx, &label);
}
static void push_mc_label_basic(ctx_t * ctx, ul_hs_t * name) {
	push_mc_label(ctx, name, LnkSectLpLabelBasic);
}

static void push_mc_unw_inst(ctx_t * ctx, mc_inst_t * inst) {
	mc_frag_push_inst(ctx->cmpl.unw_frag, inst);
}
static void push_mc_unw_byte(ctx_t * ctx, uint8_t byte) {
	mc_inst_t data = { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = byte };

	push_mc_unw_inst(ctx, &data);
}
static void push_mc_unw_word(ctx_t * ctx, uint16_t word) {
	mc_inst_t data = { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm16, .imm0 = word };

	push_mc_unw_inst(ctx, &data);
}
static void push_mc_unw_label(ctx_t * ctx, ul_hs_t * name, lnk_sect_lp_stype_t label_stype) {
	mc_inst_t label = { .type = McInstLabel, .opds = McInstOpds_Label, .label_stype = label_stype, .label = name };

	push_mc_unw_inst(ctx, &label);
}

static bool calculate_stack(ctx_t * ctx) {
	size_t stack_size = 0;

	if (ctx->has_calls) {
		stack_size = ul_align_to(stack_size, STACK_ALIGN);

		size_t args_size = max(ctx->call_args_size_max, 4);

		stack_size += args_size * STACK_UNIT;
	}

	{
		size_t stack_size_unq_max = stack_size;

		for (bb_t * bb = ctx->bb; bb != NULL; bb = bb->next) {
			size_t stack_size_unq = stack_size;

			for (bb_var_ref_t * ref = bb->var_refs, *ref_end = ref + bb->var_refs_size; ref != ref_end; ++ref) {
				var_t * var = ref->var;

				if (var->bb_refs_size == 1) {
					stack_size_unq = ul_align_to(stack_size_unq, var->cmpl.align);

					var->cmpl.stack_pos = stack_size_unq;

					stack_size_unq += var->cmpl.size;
				}
			}

			stack_size_unq_max = max(stack_size_unq_max, stack_size_unq);
		}

		stack_size = stack_size_unq_max;
	}

	{
		size_t stack_size_shrd = stack_size;

		for (var_t * var = ctx->var; var != NULL; var = var->next) {
			if (var->bb_refs_size > 1) {
				stack_size_shrd = ul_align_to(stack_size_shrd, var->cmpl.align);

				var->cmpl.stack_pos = stack_size_shrd;

				stack_size_shrd += var->cmpl.size;
			}
		}

		stack_size = stack_size_shrd;
	}

	stack_size = ul_align_biased_to(stack_size, STACK_ALIGN, STACK_ALIGN - STACK_UNIT);

	if (stack_size >= INT32_MAX) {
		return false;
	}

	ctx->cmpl.stack_size = stack_size;

	return true;
}

static void save_stack_gpr(ctx_t * ctx, int32_t off, mc_reg_t reg) {
	mc_inst_t save = { .type = McInstMov, .opds = McInstOpds_Mem_Reg, .mem_size = mc_reg_get_size(reg), .mem_base = McRegRsp, .mem_disp_type = McInstDispAuto, .mem_disp = off, .reg0 = reg };

	push_mc_inst(ctx, &save);
}
static void save_stack_var_off(ctx_t * ctx, var_t * var, mc_reg_t reg, size_t off) {
	mc_size_t reg_size = mc_reg_get_size(reg);

	ul_assert(mc_reg_infos[reg].grps.gpr
		&& mc_size_infos[reg_size].bytes + off <= var->cmpl.size);

	save_stack_gpr(ctx, (int32_t)(var->cmpl.stack_pos + off), reg);

	reset_bb_cmpl_gpr_from_var(ctx, var);

	bb_cmpl_gpr_t * bb_gpr = get_bb_cmpl_ctx_gpr_from_reg(ctx, reg);

	if (bb_gpr != NULL) {
		*bb_gpr = (bb_cmpl_gpr_t){ .var = var, .reg_size = reg_size, .off = off };
	}
}
static void save_stack_var(ctx_t * ctx, var_t * var, mc_reg_t reg) {
	save_stack_var_off(ctx, var, reg, 0);
}
static void load_int(ctx_t * ctx, mc_reg_t reg, int64_t imm) {
	mc_inst_imm_type_t imm_type = mc_inst_imm_size_to_type[mc_reg_get_size(reg)];

	ul_assert(imm_type != McInstImmNone);

	mc_inst_t load = { .type = McInstMov, .opds = McInstOpds_Reg_Imm, .reg0 = reg, .imm0_type = imm_type, .imm0 = imm };

	push_mc_inst(ctx, &load);

	reset_bb_cmpl_gpr_from_reg(ctx, reg);
}
static void load_label_addr(ctx_t * ctx, mc_reg_t reg, ul_hs_t * label) {
	ul_assert(mc_reg_get_size(reg) == McSize64);

	mc_inst_t lea = { .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = reg, .mem_base = McRegRip, .mem_disp_type = McInstDispLabelRel32, .mem_disp = 0, .mem_disp_label = label };

	push_mc_inst(ctx, &lea);

	reset_bb_cmpl_gpr_from_reg(ctx, reg);
}
static void load_label_val_off(ctx_t * ctx, mc_reg_t reg, ul_hs_t * label, size_t off) {
	mc_inst_t mov = { .type = McInstMov, .opds = McInstOpds_Reg_Mem, .reg0 = reg, .mem_size = mc_reg_get_size(reg), .mem_base = McRegRip, .mem_disp_type = McInstDispLabelRel32, .mem_disp = (int32_t)off, .mem_disp_label = label };

	push_mc_inst(ctx, &mov);

	reset_bb_cmpl_gpr_from_reg(ctx, reg);
}
static void load_label_val(ctx_t * ctx, mc_reg_t reg, ul_hs_t * label) {
	load_label_val_off(ctx, reg, label, 0);
}
static void load_stack_gpr(ctx_t * ctx, mc_reg_t reg, int32_t off) {
	mc_inst_t load = { .type = McInstMov, .opds = McInstOpds_Reg_Mem, .reg0 = reg, .mem_size = mc_reg_get_size(reg), .mem_base = McRegRsp, .mem_disp_type = McInstDispAuto, .mem_disp = off };

	push_mc_inst(ctx, &load);

	reset_bb_cmpl_gpr_from_reg(ctx, reg);
}
static void load_stack_var_off(ctx_t * ctx, mc_reg_t reg, var_t * var, size_t off) {
	mc_size_t reg_size = mc_reg_get_size(reg);

	ul_assert(mc_reg_infos[reg].grps.gpr
		&& mc_size_infos[reg_size].bytes + off <= var->cmpl.size);

	bb_cmpl_gpr_t * bb_gpr = get_bb_cmpl_ctx_gpr_from_reg(ctx, reg);

	if (bb_gpr == NULL || bb_gpr->var != var || bb_gpr->reg_size != reg_size || bb_gpr->off != off) {
		load_stack_gpr(ctx, reg, (int32_t)(var->cmpl.stack_pos + off));

		if (bb_gpr != NULL) {
			*bb_gpr = (bb_cmpl_gpr_t){ .var = var, .reg_size = reg_size, .off = off };
		}
	}
}
static void load_stack_var(ctx_t * ctx, mc_reg_t reg, var_t * var) {
	load_stack_var_off(ctx, reg, var, 0);
}
static void read_ptr_off(ctx_t * ctx, mc_reg_t reg, mc_reg_t ptr, size_t off) {
	ul_assert(off < INT32_MAX);

	mc_inst_t mov = { .type = McInstMov, .opds = McInstOpds_Reg_Mem, .reg0 = reg, .mem_size = mc_reg_get_size(reg), .mem_base = ptr, .mem_disp_type = McInstDispAuto, .mem_disp = (int32_t)off };

	push_mc_inst(ctx, &mov);

	reset_bb_cmpl_gpr_from_reg(ctx, reg);
}
static void write_ptr_off(ctx_t * ctx, mc_reg_t ptr, mc_reg_t reg, size_t off) {
	ul_assert(off < INT32_MAX);

	mc_inst_t mov = { .type = McInstMov, .opds = McInstOpds_Mem_Reg, .mem_size = mc_reg_get_size(reg), .mem_base = ptr, .mem_disp_type = McInstDispAuto, .mem_disp = (int32_t)off, .reg0 = reg };

	push_mc_inst(ctx, &mov);
}

static mc_reg_t get_gpr_reg_int(mc_reg_gpr_t gpr, ira_int_type_t int_type) {
	mc_size_t reg_size = ira_int_infos[int_type].mc_size;

	mc_reg_t reg = mc_reg_gprs[gpr][reg_size];

	ul_assert(reg != McRegNone);

	return reg;
}
static mc_reg_t get_gpr_reg_dt(mc_reg_gpr_t gpr, ira_dt_t * dt) {
	mc_size_t reg_size;

	switch (dt->type) {
		case IraDtBool:
			reg_size = McSize8;
			break;
		case IraDtInt:
			reg_size = ira_int_infos[dt->int_type].mc_size;
			break;
		case IraDtPtr:
			reg_size = McSize64;
			break;
		default:
			ul_assert_unreachable();
	}

	mc_reg_t reg = mc_reg_gprs[gpr][reg_size];

	ul_assert(reg != McRegNone);

	return reg;
}


static mc_reg_t w64_get_arg_reg(ira_dt_t * dt, size_t arg) {
	ul_assert(arg < 4);
	return get_gpr_reg_dt(w64_gpr_args[arg], dt);
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
static void w64_load_callee_arg(ctx_t * ctx, var_t * var, size_t arg, bool force_null) {
	ira_dt_t * var_dt = var->qdt.dt;

	mc_reg_t reg;

	int32_t arg_off = (int32_t)(arg * STACK_UNIT);

	switch (var_dt->type) {
		case IraDtVoid:
			if (force_null) {
				if (arg < 4) {
					reg = mc_reg_gprs[w64_gpr_args[arg]][McSize64];
					load_int(ctx, reg, 0);
				}
				else {
					load_int(ctx, McRegRax, 0);
					save_stack_gpr(ctx, arg_off, McRegRax);
				}
			}
			
			break;
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
			if (arg < 4) {
				reg = w64_get_arg_reg(var_dt, arg);
				load_stack_var(ctx, reg, var);
			}
			else {
				reg = get_gpr_reg_dt(McRegGprAx, var_dt);
				load_stack_var(ctx, reg, var);
				save_stack_gpr(ctx, arg_off, reg);
			}

			break;
		default:
			ul_assert_unreachable();
	}
}
static void w64_save_callee_ret(ctx_t * ctx, var_t * var) {
	ira_dt_t * var_dt = var->qdt.dt;

	mc_reg_t reg;
	
	reset_bb_cmpl_gpr_from_reg(ctx, McRegRax);
	reset_bb_cmpl_gpr_from_reg(ctx, McRegRcx);
	reset_bb_cmpl_gpr_from_reg(ctx, McRegRdx);
	reset_bb_cmpl_gpr_from_reg(ctx, McRegR8);
	reset_bb_cmpl_gpr_from_reg(ctx, McRegR9);

	switch (var_dt->type) {
		case IraDtVoid:
			break;
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
			reg = get_gpr_reg_dt(McRegGprAx, var_dt);
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
static int32_t w64_calculate_caller_arg_off(ctx_t * ctx, size_t arg) {
	return (int32_t)(ctx->cmpl.stack_size + STACK_UNIT + arg * STACK_UNIT);
}
static void w64_save_caller_arg(ctx_t * ctx, ul_hs_t * arg_name, size_t arg) {
	var_t * var = find_var(ctx, arg_name);

	ul_assert(var != NULL);

	ira_dt_t * var_dt = var->qdt.dt;

	mc_reg_t reg;

	int32_t arg_off = w64_calculate_caller_arg_off(ctx, arg);

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
					reg = get_gpr_reg_dt(McRegGprAx, var_dt);
					load_stack_gpr(ctx, reg, arg_off);
					save_stack_var(ctx, var, reg);
					break;
			}
			break;
		default:
			ul_assert_unreachable();
	}
}
static void w64_save_caller_var_args(ctx_t * ctx, size_t arg) {
	for (; arg < 4; ++arg) {
		mc_reg_t reg = mc_reg_gprs[w64_gpr_args[arg]][McSize64];

		int32_t arg_off = w64_calculate_caller_arg_off(ctx, arg);

		save_stack_gpr(ctx, arg_off, reg);
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
			mc_reg_t reg = get_gpr_reg_dt(McRegGprAx, var_dt);

			load_stack_var(ctx, reg, var);

			break;
		}
		default:
			ul_assert_unreachable();
	}
}
static void w64_load_caller_va_start(ctx_t * ctx, var_t * var) {
	mc_inst_t lea = { .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegRax, .mem_base = McRegRsp, .mem_disp_type = McInstDispAuto, .mem_disp = w64_calculate_caller_arg_off(ctx, ctx->cmpl.first_va_arg_ind) };

	push_mc_inst(ctx, &lea);

	save_stack_var(ctx, var, McRegRax);
}
static void w64_load_caller_va_arg(ctx_t * ctx, var_t * dst, var_t * va_elem_var) {
	load_stack_var(ctx, McRegRax, va_elem_var);

	ira_dt_t * dt = dst->qdt.dt;

	switch (dt->type) {
		case IraDtVoid:
			break;
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
		{
			mc_reg_t reg = get_gpr_reg_dt(McRegGprCx, dt);

			mc_inst_t mov = { .type = McInstMov, .opds = McInstOpds_Reg_Mem, .reg0 = reg, .mem_size = mc_reg_get_size(reg), .mem_base = McRegRax, .mem_disp_type = McInstDispNone };

			push_mc_inst(ctx, &mov);

			save_stack_var(ctx, dst, reg);

			break;
		}
		default:
			ul_assert_unreachable();
	}

	{
		ul_assert(va_elem_var->qdt.dt->type == IraDtPtr);

		mc_inst_t lea = { .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegRax, .mem_base = McRegRax, .mem_disp_type = McInstDispAuto, .mem_disp = (int32_t)va_elem_var->cmpl.size };

		push_mc_inst(ctx, &lea);
	}

	save_stack_var(ctx, va_elem_var, McRegRax);
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

	if (func_dt->func.vas->type == IraDtFuncVasCstyle) {
		ctx->cmpl.first_va_arg_ind = arg;

		w64_save_caller_var_args(ctx, arg);
	}
}
static void emit_unw_ver_flags(ctx_t * ctx) {
	union {
		struct {
			uint8_t ver : 5;
			uint8_t flags : 3;
		};
		uint8_t val;
	} ver_flags;

	ver_flags.ver = 1;
	ver_flags.flags = 0;

	push_mc_unw_byte(ctx, ver_flags.val);
}
static void emit_unw_codes_size(ctx_t * ctx, size_t * out_codes_size) {
	size_t codes_size = 0;

	size_t stack_size = ctx->cmpl.stack_size;

	if (stack_size % 8 == 0 && 8 <= stack_size && stack_size <= 128) {
		codes_size += 1;
	}
	else if (stack_size % 8 == 0 && stack_size < (1ull << (9 + 10)) - 8ull) {
		codes_size += 2;
	}
	else if (stack_size < (1ull << (2 + 10 + 10 + 10)) - 8ull) {
		codes_size += 3;
	}
	else {
		ul_assert_unreachable();
	}

	ul_assert(codes_size < UINT8_MAX);

	push_mc_unw_byte(ctx, (uint8_t)codes_size);

	*out_codes_size = codes_size;
}
static void emit_unw_frame_data(ctx_t * ctx) {
	union {
		struct {
			uint8_t frame_reg : 4;
			uint8_t frame_off : 4;
		};
		uint8_t val;
	} frame_ptr;

	frame_ptr.frame_reg = 0;
	frame_ptr.frame_off = 0;

	push_mc_unw_byte(ctx, frame_ptr.val);
}
static void emit_unw_data(ctx_t * ctx) {
	push_mc_unw_label(ctx, ctx->cmpl.lo->name, LnkSectLpLabelProcUnw);

	emit_unw_ver_flags(ctx);

	size_t stack_alloc_inst_end = ctx->cmpl.stack_alloc_inst_size;

	ul_assert(stack_alloc_inst_end < UINT8_MAX);

	{
		uint8_t prologue_len = (uint8_t)stack_alloc_inst_end;

		push_mc_unw_byte(ctx, prologue_len);
	}

	size_t codes_size;

	emit_unw_codes_size(ctx, &codes_size);

	emit_unw_frame_data(ctx);

	typedef union unw_code {
		struct {
			uint8_t off;
			uint8_t opc : 4;
			uint8_t info : 4;
		};
		uint16_t val;
	} unw_code_t;

	{
		size_t stack_size = ctx->cmpl.stack_size;

		if (stack_size % 8 == 0 && 8 <= stack_size && stack_size <= 128) {
			unw_code_t stack_alloc = { .off = (uint8_t)stack_alloc_inst_end, .opc = 2, .info = stack_size / 8 - 1 };

			push_mc_unw_word(ctx, stack_alloc.val);
		}
		else if (stack_size % 8 == 0 && stack_size < (1ull << (9 + 10)) - 8ull) {
			unw_code_t stack_alloc = { .off = (uint8_t)stack_alloc_inst_end, .opc = 1, .info = 0 };

			push_mc_unw_word(ctx, stack_alloc.val);

			push_mc_unw_word(ctx, (uint16_t)(stack_size / 8));
		}
		else if (stack_size < (1ull << (2 + 10 + 10 + 10)) - 8ull) {
			unw_code_t stack_alloc = { .off = (uint8_t)stack_alloc_inst_end, .opc = 1, .info = 1 };

			push_mc_unw_word(ctx, stack_alloc.val);

			push_mc_unw_word(ctx, (uint16_t)(stack_size & 0xFFFF));

			push_mc_unw_word(ctx, (uint16_t)((stack_size >> 16) & 0xFFFF));
		}
		else {
			ul_assert_unreachable();
		}

		if (codes_size % 2 == 1) {
			push_mc_unw_word(ctx, 0);
		}
	}
}
static void emit_prologue(ctx_t * ctx) {
	mc_inst_t stack_alloc = { .type = McInstSub, .opds = McInstOpds_Reg_Imm, .reg0 = McRegRsp, .imm0_type = McInstImm32, .imm0 = (int32_t)ctx->cmpl.stack_size };

	bool res = mc_inst_get_size(&stack_alloc, &ctx->cmpl.stack_alloc_inst_size);

	ul_assert(res);

	push_mc_inst(ctx, &stack_alloc);

	emit_prologue_save_args(ctx);

	emit_unw_data(ctx);
}
static void emit_epilogue(ctx_t * ctx) {
	mc_inst_t stack_free = { .type = McInstAdd, .opds = McInstOpds_Reg_Imm, .reg0 = McRegRsp, .imm0_type = McInstImm32, .imm0 = (int32_t)ctx->cmpl.stack_size };

	push_mc_inst(ctx, &stack_free);
}

static void addr_of_var(ctx_t * ctx, mc_reg_t reg, var_t * var) {
	mc_inst_t lea = { .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = reg, .mem_base = McRegRsp, .mem_disp_type = McInstDispAuto, .mem_disp = (int32_t)(var->cmpl.stack_pos) };

	push_mc_inst(ctx, &lea);

	reset_bb_cmpl_gpr_from_reg(ctx, reg);
}
static void get_ptr_copy_data(var_t * var, mc_reg_t * reg_out, size_t * off_step_out) {
	if (var->cmpl.align >= 8 && var->cmpl.size % 8 == 0) {
		*reg_out = McRegRax;
		*off_step_out = 8;
	}
	else if (var->cmpl.align >= 4 && var->cmpl.size % 4 == 0) {
		*reg_out = McRegEax;
		*off_step_out = 4;
	}
	else if (var->cmpl.align >= 2 && var->cmpl.size % 2 == 0) {
		*reg_out = McRegAx;
		*off_step_out = 2;
	}
	else {
		*reg_out = McRegAl;
		*off_step_out = 1;
	}
}
static void div_int(ctx_t * ctx, var_t * opd0, var_t * opd1, var_t * div_out, var_t * mod_out) {
	ira_int_type_t int_type = opd0->qdt.dt->int_type;

	mc_inst_type_t dx_inst_type = McInstXor;
	mc_reg_t reg0 = get_gpr_reg_int(McRegGprAx, int_type), reg1 = get_gpr_reg_int(McRegGprCx, int_type), reg2;

	switch (int_type) {
		case IraIntS8:
			dx_inst_type = McInstCbw;
		case IraIntU8:
			reg2 = McRegAh;
			break;
		case IraIntS16:
			dx_inst_type = McInstCwd;
		case IraIntU16:
			reg2 = McRegDx;
			break;
		case IraIntS32:
			dx_inst_type = McInstCdq;
		case IraIntU32:
			reg2 = McRegEdx;
			break;
		case IraIntS64:
			dx_inst_type = McInstCqo;
		case IraIntU64:
			reg2 = McRegRdx;
			break;
		default:
			ul_assert_unreachable();
	}

	reset_bb_cmpl_gpr_from_reg(ctx, reg2);

	load_stack_var(ctx, reg0, opd0);
	load_stack_var(ctx, reg1, opd1);

	mc_inst_t dx_inst = { .type = dx_inst_type };

	if (dx_inst_type == McInstXor) {
		dx_inst.opds = McInstOpds_Reg_Reg;
		dx_inst.reg0 = reg2;
		dx_inst.reg1 = reg2;
	}
	else {
		dx_inst.opds = McInstOpds_None;
	}

	push_mc_inst(ctx, &dx_inst);

	mc_inst_t div_inst = { .type = dx_inst_type == McInstXor ? McInstDiv : McInstIdiv, .opds = McInstOpds_Reg, .reg0 = reg1 };

	push_mc_inst(ctx, &div_inst);

	if (div_out != NULL) {
		save_stack_var(ctx, div_out, reg0);
	}
	else {
		reset_bb_cmpl_gpr_from_reg(ctx, reg0);
	}
	if (mod_out != NULL) {
		save_stack_var(ctx, mod_out, reg2);
	}
}
static ira_int_cmp_t get_int_cmp(ira_optr_type_t optr_type) {
	switch (optr_type) {
		case IraOptrBltnLessInt:
		case IraOptrBltnLessPtr:
			return IraIntCmpLess;
		case IraOptrBltnLessEqInt:
		case IraOptrBltnLessEqPtr:
			return IraIntCmpLessEq;
		case IraOptrBltnGrtrInt:
		case IraOptrBltnGrtrPtr:
			return IraIntCmpGrtr;
		case IraOptrBltnGrtrEqInt:
		case IraOptrBltnGrtrEqPtr:
			return IraIntCmpGrtrEq;
		case IraOptrBltnEqInt:
		case IraOptrBltnEqPtr:
			return IraIntCmpEq;
		case IraOptrBltnNeqInt:
		case IraOptrBltnNeqPtr:
			return IraIntCmpNeq;
		default:
			ul_assert_unreachable();
	}
}
static mc_inst_type_t get_set_inst_type(bool sign, ira_int_cmp_t int_cmp) {
	if (sign) {
		switch (int_cmp) {
			case IraIntCmpLess:
				return McInstSetl;
			case IraIntCmpLessEq:
				return McInstSetle;
			case IraIntCmpEq:
				return McInstSete;
			case IraIntCmpGrtrEq:
				return McInstSetge;
			case IraIntCmpGrtr:
				return McInstSetg;
			case IraIntCmpNeq:
				return McInstSetne;
			default:
				ul_assert_unreachable();
		}
	}
	else {
		switch (int_cmp) {
			case IraIntCmpLess:
				return McInstSetb;
			case IraIntCmpLessEq:
				return McInstSetbe;
			case IraIntCmpEq:
				return McInstSete;
			case IraIntCmpGrtrEq:
				return McInstSetae;
			case IraIntCmpGrtr:
				return McInstSeta;
			case IraIntCmpNeq:
				return McInstSetne;
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
			load_int(ctx, McRegAl, val->bool_val ? 1 : 0);
			save_stack_var(ctx, var, McRegAl);
			break;
		case IraValImmInt:
		{
			mc_reg_t reg = get_gpr_reg_int(McRegGprAx, val->dt->int_type);

			load_int(ctx, reg, val->int_val.si64);
			save_stack_var(ctx, var, reg);
			break;
		}
		case IraValImmPtr:
			load_int(ctx, McRegRax, (int64_t)val->int_val.ui64);
			save_stack_var(ctx, var, McRegRax);
			break;
		case IraValLoPtr:
			switch (val->lo_val->type) {
				case IraLoFunc:
				case IraLoVar:
					load_label_addr(ctx, McRegRax, val->lo_val->name);
					save_stack_var(ctx, var, McRegRax);
					break;
				case IraLoImpt:
					load_label_val(ctx, McRegRax, val->lo_val->name);
					save_stack_var(ctx, var, McRegRax);
					break;
				default:
					ul_assert_unreachable();
			}
			break;
		case IraValImmArr:
		{
			ul_hs_t * arr_label = NULL;

			if (!ira_pec_c_compile_val_frag(ctx->cmpl.base_ctx, val, ctx->cmpl.lo->name, &arr_label)) {
				ul_assert_unreachable();
			}

			ira_dt_t * arr_tpl = val->dt->arr.assoc_tpl;
			
			size_t off;

			off = ira_pec_get_tpl_elem_off(arr_tpl, IRA_DT_ARR_SIZE_IND);
			load_label_val_off(ctx, McRegRax, arr_label, off);
			save_stack_var_off(ctx, var, McRegRax, off);

			off = ira_pec_get_tpl_elem_off(arr_tpl, IRA_DT_ARR_DATA_IND);
			load_label_val_off(ctx, McRegRax, arr_label, off);
			save_stack_var_off(ctx, var, McRegRax, off);

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
			mc_reg_t reg = get_gpr_reg_dt(McRegGprAx, dt);

			load_stack_var_off(ctx, reg, src, src_off);
			save_stack_var(ctx, dst, reg);
			break;
		}
		case IraDtArr:
		{
			ira_dt_t * arr_tpl = dt->arr.assoc_tpl;

			size_t off;

			off = ira_pec_get_tpl_elem_off(arr_tpl, IRA_DT_ARR_SIZE_IND);
			load_stack_var_off(ctx, McRegRax, src, src_off + off);
			save_stack_var_off(ctx, dst, McRegRax, off);

			off = ira_pec_get_tpl_elem_off(arr_tpl, IRA_DT_ARR_DATA_IND);
			load_stack_var_off(ctx, McRegRax, src, src_off + off);
			save_stack_var_off(ctx, dst, McRegRax, off);
			break;
		}
		default:
			ul_assert_unreachable();
	}
}
static void compile_int_like_cmp(ctx_t * ctx, var_t * dst, var_t * src0, var_t * src1, ira_int_cmp_t int_cmp, bool cmp_sign) {
	mc_reg_t reg0 = get_gpr_reg_dt(McRegGprAx, src0->qdt.dt);
	mc_reg_t reg1 = get_gpr_reg_dt(McRegGprCx, src0->qdt.dt);

	load_stack_var(ctx, reg0, src0);
	load_stack_var(ctx, reg1, src1);

	{
		mc_inst_t cmp = { .type = McInstCmp, .opds = McInstOpds_Reg_Reg, .reg0 = reg0, .reg1 = reg1 };

		push_mc_inst(ctx, &cmp);
	}

	{
		mc_inst_t setcc = { .type = get_set_inst_type(cmp_sign, int_cmp), .opds = McInstOpds_Reg, .reg0 = McRegAl };

		push_mc_inst(ctx, &setcc);
	}

	save_stack_var(ctx, dst, McRegAl);
}
static void compile_int_like_cast(ctx_t * ctx, var_t * dst, var_t * src, ira_int_type_t from, ira_int_type_t to) {
	mc_reg_gpr_t gpr_from = McRegGprAx, gpr_to = McRegGprAx;

	const int_cast_info_t * info = &int_cast_infos[from][to];

	load_stack_var(ctx, get_gpr_reg_int(gpr_from, from), src);

	mc_inst_t cast = { .type = info->inst_type, .opds = McInstOpds_Reg_Reg, .reg0 = mc_reg_gprs[gpr_to][info->to], .reg1 = mc_reg_gprs[gpr_from][info->from] };

	push_mc_inst(ctx, &cast);

	save_stack_var(ctx, dst, get_gpr_reg_int(gpr_to, to));
}

static void compile_blank(ctx_t * ctx, inst_t * inst) {
	return;
}
static void compile_def_label(ctx_t * ctx, inst_t * inst) {
	push_mc_label_basic(ctx, inst->opd0.label->cmpl.global_name);
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
	addr_of_var(ctx, McRegRax, inst->opd1.var);
	save_stack_var(ctx, inst->opd0.var, McRegRax);
}
static void compile_read_ptr(ctx_t * ctx, inst_t * inst) {
	var_t * ptr_var = inst->opd1.var;

	load_stack_var(ctx, McRegRcx, ptr_var);

	var_t * dst_var = inst->opd0.var;

	size_t off_step = 1;
	mc_reg_t reg = McRegAl;

	get_ptr_copy_data(dst_var, &reg, &off_step);

	for (size_t off = 0; off < dst_var->cmpl.size; off += off_step) {
		read_ptr_off(ctx, reg, McRegRcx, off);
		save_stack_var_off(ctx, dst_var, reg, off);
	}

	reset_bb_cmpl_gpr_from_var(ctx, dst_var);
}
static void compile_write_ptr(ctx_t * ctx, inst_t * inst) {
	var_t * ptr_var = inst->opd0.var;

	load_stack_var(ctx, McRegRcx, ptr_var);

	var_t * src_var = inst->opd1.var;

	size_t off_step = 1;
	mc_reg_t reg = McRegAl;

	get_ptr_copy_data(src_var, &reg, &off_step);

	for (size_t off = 0; off < src_var->cmpl.size; off += off_step) {
		load_stack_var_off(ctx, reg, src_var, off);
		write_ptr_off(ctx, McRegRcx, reg, off);
	}

	reset_bb_cmpl_ctx(ctx, ctx->cmpl.bb_ctx.cur_bb);
}
static void compile_shift_ptr(ctx_t * ctx, inst_t * inst) {
	var_t * index_var = inst->opd2.var;
	ira_dt_t * index_dt = index_var->qdt.dt;

	ira_int_type_t index_int_type;

	{
		ira_int_type_t index_type_from = index_dt->int_type;

		index_int_type = ira_int_infos[index_type_from].sign ? IraIntS64 : IraIntU64;

		const int_cast_info_t * info = &int_cast_infos[index_type_from][index_int_type];

		load_stack_var(ctx, get_gpr_reg_int(McRegGprCx, index_type_from), index_var);

		mc_inst_t cast = { .type = info->inst_type, .opds = McInstOpds_Reg_Reg, .reg0 = mc_reg_gprs[McRegGprCx][info->to], .reg1 = mc_reg_gprs[McRegGprCx][info->from] };

		push_mc_inst(ctx, &cast);
	}

	{
		load_int(ctx, McRegRax, (int64_t)inst->shift_ptr.scale);

		mc_inst_t imul = { .type = McInstImul, .opds = McInstOpds_Reg_Reg, .reg0 = McRegRcx, .reg1 = McRegRax };

		push_mc_inst(ctx, &imul);
	}

	load_stack_var(ctx, McRegRax, inst->opd1.var);

	mc_inst_t add = { .type = McInstAdd, .opds = McInstOpds_Reg_Reg, .reg0 = McRegRax, .reg1 = McRegRcx };

	push_mc_inst(ctx, &add);

	save_stack_var(ctx, inst->opd0.var, McRegRax);

	reset_bb_cmpl_gpr_from_reg(ctx, McRegRcx);
}
static void compile_unr_optr(ctx_t * ctx, inst_t * inst) {
	ira_optr_t * optr = inst->opd1.optr;

	switch (optr->type) {
		case IraOptrBltnNegBool:
		{
			load_stack_var(ctx, McRegAl, inst->opd2.var);

			mc_inst_t test = { .type = McInstTest, .opds = McInstOpds_Reg_Reg, .reg0 = McRegAl, .reg1 = McRegAl };

			push_mc_inst(ctx, &test);

			mc_inst_t set = { .type = McInstSetz, .opds = McInstOpds_Reg, .reg0 = McRegAl };

			push_mc_inst(ctx, &set);

			save_stack_var(ctx, inst->opd0.var, McRegAl);

			break;
		}
		case IraOptrBltnNegInt:
		{
			mc_inst_type_t inst_type;

			switch (optr->type) {
				case IraOptrBltnNegInt:
					inst_type = McInstNeg;
					break;
				default:
					ul_assert_unreachable();
			}

			ira_int_type_t int_type = inst->opd0.var->qdt.dt->int_type;

			mc_reg_t reg = get_gpr_reg_int(McRegGprAx, int_type);

			load_stack_var(ctx, reg, inst->opd2.var);

			mc_inst_t unr_opr = { .type = inst_type, .opds = McInstOpds_Reg, .reg0 = reg };

			push_mc_inst(ctx, &unr_opr);

			save_stack_var(ctx, inst->opd0.var, reg);

			break;
		}
		default:
			ul_assert_unreachable();
	}
}
static void compile_bin_optr(ctx_t * ctx, inst_t * inst) {
	ira_optr_t * optr = inst->opd1.optr;

	switch (optr->type) {
		case IraOptrBltnDivInt:
			div_int(ctx, inst->opd2.var, inst->opd3.var, inst->opd0.var, NULL);
			break;
		case IraOptrBltnModInt:
			div_int(ctx, inst->opd2.var, inst->opd3.var, NULL, inst->opd0.var);
			break;
		case IraOptrBltnMulInt:
		case IraOptrBltnAddInt:
		case IraOptrBltnSubInt:
		case IraOptrBltnAndInt:
		case IraOptrBltnXorInt:
		case IraOptrBltnOrInt:
		{
			ira_int_type_t int_type = inst->opd0.var->qdt.dt->int_type;

			mc_inst_type_t inst_type;

			switch (optr->type) {
				case IraOptrBltnMulInt:
					inst_type = McInstImul;
					break;
				case IraOptrBltnAddInt:
					inst_type = McInstAdd;
					break;
				case IraOptrBltnSubInt:
					inst_type = McInstSub;
					break;
				case IraOptrBltnAndInt:
					inst_type = McInstAnd;
					break;
				case IraOptrBltnXorInt:
					inst_type = McInstXor;
					break;
				case IraOptrBltnOrInt:
					inst_type = McInstOr;
					break;
				default:
					ul_assert_unreachable();
			}

			mc_reg_t reg0 = get_gpr_reg_int(McRegGprAx, int_type), reg1 = get_gpr_reg_int(McRegGprCx, int_type);

			load_stack_var(ctx, reg0, inst->opd2.var);
			load_stack_var(ctx, reg1, inst->opd3.var);

			mc_inst_t bin_opr = { .type = inst_type, .opds = McInstOpds_Reg_Reg, .reg0 = reg0, .reg1 = reg1 };

			push_mc_inst(ctx, &bin_opr);

			save_stack_var(ctx, inst->opd0.var, reg0);

			break;
		}
		case IraOptrBltnLeShiftInt:
		case IraOptrBltnRiShiftInt:
		{
			ira_int_type_t int_type = inst->opd0.var->qdt.dt->int_type;

			mc_inst_type_t inst_type;

			switch (optr->type) {
				case IraOptrBltnLeShiftInt:
					if (ira_int_infos[int_type].sign) {
						inst_type = McInstSal;
					}
					else {
						inst_type = McInstShl;
					}
					break;
				case IraOptrBltnRiShiftInt:
					if (ira_int_infos[int_type].sign) {
						inst_type = McInstSar;
					}
					else {
						inst_type = McInstShr;
					}
					break;
				default:
					ul_assert_unreachable();
			}

			load_stack_var(ctx, get_gpr_reg_int(McRegGprCx, int_type), inst->opd3.var);

			mc_reg_t shift_reg = get_gpr_reg_int(McRegGprAx, int_type);

			load_stack_var(ctx, shift_reg, inst->opd2.var);

			mc_inst_t shift = { .type = inst_type, .opds = McInstOpds_Reg_Reg, .reg0 = shift_reg, .reg1 = McRegCl };

			push_mc_inst(ctx, &shift);

			save_stack_var(ctx, inst->opd0.var, shift_reg);

			break;
		}
		case IraOptrBltnLessInt:
		case IraOptrBltnLessEqInt:
		case IraOptrBltnGrtrInt:
		case IraOptrBltnGrtrEqInt:
		case IraOptrBltnEqInt:
		case IraOptrBltnNeqInt:
		{
			ira_int_type_t int_type = inst->opd2.var->qdt.dt->int_type;

			ira_int_cmp_t int_cmp = get_int_cmp(optr->type);

			compile_int_like_cmp(ctx, inst->opd0.var, inst->opd2.var, inst->opd3.var, int_cmp, ira_int_infos[int_type].sign);

			break;
		}
		case IraOptrBltnLessPtr:
		case IraOptrBltnLessEqPtr:
		case IraOptrBltnGrtrPtr:
		case IraOptrBltnGrtrEqPtr:
		case IraOptrBltnEqPtr:
		case IraOptrBltnNeqPtr:
		{
			ira_int_cmp_t int_cmp = get_int_cmp(optr->type);

			compile_int_like_cmp(ctx, inst->opd0.var, inst->opd2.var, inst->opd3.var, int_cmp, false);

			break;
		}
		default:
			ul_assert_unreachable();
	}
}
static void compile_mmbr_acc_ptr(ctx_t * ctx, inst_t * inst) {
	var_t * opd_var = inst->opd1.var;

	load_stack_var(ctx, McRegRax, opd_var);

	mc_inst_t lea = { .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegRax, .mem_base = McRegRax, .mem_disp_type = McInstDispAuto, .mem_disp = (int32_t)inst->mmbr_acc.cmpl.off };

	push_mc_inst(ctx, &lea);

	save_stack_var(ctx, inst->opd0.var, McRegRax);
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
		case IraDtVoid:
			load_int(ctx, McRegRax, 0);
			save_stack_var(ctx, inst->opd0.var, McRegRax);
			break;
		case IraDtInt:
			compile_int_like_cast(ctx, inst->opd0.var, inst->opd1.var, from->int_type, IraIntU64);
			break;
		case IraDtPtr:
			load_stack_var(ctx, McRegRax, inst->opd1.var);
			save_stack_var(ctx, inst->opd0.var, McRegRax);
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
	ira_dt_t * func_dt = inst->call_func_ptr.func_dt;

	size_t arg_off = 0;

	if (w64_load_callee_ret_link(ctx, inst->opd0.var)) {
		++arg_off;
	}

	size_t arg_cur = 0;
	for (var_t ** var = inst->opd3.vars, **var_end = var + inst->opd2.size; var != var_end; ++var, ++arg_off, ++arg_cur) {
		bool force_null = false;

		switch (func_dt->func.vas->type) {
			case IraDtFuncVasNone:
				break;
			case IraDtFuncVasCstyle:
				if (arg_cur >= func_dt->func.args_size) {
					force_null = true;
				}
				break;
			default:
				ul_assert_unreachable();
		}

		w64_load_callee_arg(ctx, *var, arg_off, force_null);
	}

	load_stack_var(ctx, McRegRax, inst->opd1.var);

	mc_inst_t call = { .type = McInstCall, .opds = McInstOpds_Reg, .reg0 = McRegRax };

	push_mc_inst(ctx, &call);

	w64_save_callee_ret(ctx, inst->opd0.var);
}
static void compile_bru(ctx_t * ctx, inst_t * inst) {
	mc_inst_t jmp = { .type = McInstJmp, .opds = McInstOpds_Imm, .imm0_type = McInstImmLabelRel32, .imm0_label = inst->opd0.label->cmpl.global_name };

	push_mc_inst(ctx, &jmp);
}
static void compile_brc(ctx_t * ctx, inst_t * inst) {
	mc_inst_type_t jump_type;

	switch (inst->base->type) {
		case IraInstBrt:
			jump_type = McInstJnz;
			break;
		case IraInstBrf:
			jump_type = McInstJz;
			break;
		default:
			ul_assert_unreachable();
	}

	load_stack_var(ctx, McRegAl, inst->opd1.var);

	mc_inst_t test = { .type = McInstTest, .opds = McInstOpds_Reg_Reg, .reg0 = McRegAl, .reg1 = McRegAl };

	push_mc_inst(ctx, &test);

	mc_inst_t jump = { .type = jump_type, .opds = McInstOpds_Imm, .imm0_type = McInstImmLabelRel32, .imm0_label = inst->opd0.label->cmpl.global_name };

	push_mc_inst(ctx, &jump);
}
static void compile_ret(ctx_t * ctx, inst_t * inst) {
	w64_load_caller_ret(ctx, inst->opd0.var);

	emit_epilogue(ctx);

	mc_inst_t ret = { .type = McInstRet, .opds = McInstOpds_None };

	push_mc_inst(ctx, &ret);
}
static void compile_va_start(ctx_t * ctx, inst_t * inst) {
	w64_load_caller_va_start(ctx, inst->opd0.var);
}
static void compile_va_arg(ctx_t * ctx, inst_t * inst) {
	w64_load_caller_va_arg(ctx, inst->opd0.var, inst->opd1.var);
}

static void compile_insts(ctx_t * ctx) {
	emit_prologue(ctx);

	for (inst_t * inst = ctx->insts, *inst_end = inst + ctx->insts_size; inst != inst_end; ++inst) {
		if (ctx->cmpl.bb_ctx.cur_bb != inst->bb) {
			reset_bb_cmpl_ctx(ctx, inst->bb);
		}

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
			[IraInstUnrOptr] = compile_unr_optr,
			[IraInstBinOptr] = compile_bin_optr,
			[IraInstMmbrAccPtr] = compile_mmbr_acc_ptr,
			[IraInstCast] = compile_cast,
			[IraInstCallFuncPtr] = compile_call_func_ptr,
			[IraInstBru] = compile_bru,
			[IraInstBrt] = compile_brc,
			[IraInstBrf] = compile_brc,
			[IraInstRet] = compile_ret,
			[IraInstVaStart] = compile_va_start,
			[IraInstVaArg] = compile_va_arg
		};

		compile_inst_proc_t * proc = compile_insts_procs[inst->base->type];

		ul_assert(proc != NULL);

		proc(ctx, inst);
	}

	push_mc_label(ctx, ctx->cmpl.lo->name, LnkSectLpLabelProcEnd);
}

static bool compile_core(ctx_t * ctx) {
	if (ctx->cmpl.lo->type != IraLoFunc) {
		return false;
	}

	ctx->cmpl.hsb = ira_pec_c_get_hsb(ctx->cmpl.base_ctx);
	ctx->cmpl.hst = ira_pec_c_get_hst(ctx->cmpl.base_ctx);
	ctx->pec = ira_pec_c_get_pec(ctx->cmpl.base_ctx);

	ctx->cmpl.frag = ira_pec_c_get_frag(ctx->cmpl.base_ctx, McFragCode, ctx->cmpl.lo->name);
	ctx->cmpl.unw_frag = ira_pec_c_get_frag(ctx->cmpl.base_ctx, McFragUnw, NULL);

	ctx->func = ctx->cmpl.lo->func;

	if (!prepare_insts(ctx)) {
		return false;
	}

	form_global_label_names(ctx, ctx->cmpl.lo->name);

	if (!calculate_stack(ctx)) {
		return false;
	}

	compile_insts(ctx);

	return true;
}
bool ira_pec_ip_compile(ira_pec_c_ctx_t * c_ctx, ira_lo_t * lo) {
	ctx_t ctx = { .trg = TrgCmpl, .cmpl.base_ctx = c_ctx, .cmpl.lo = lo };

	bool res = compile_core(&ctx);

	ctx_cleanup(&ctx);

	return res;
}


static void clear_var_val(ctx_t * ctx, var_t * var) {
	if (var->intr.val == NULL) {
		return;
	}

	ira_val_destroy(var->intr.val);

	var->intr.val = NULL;
}
static bool check_var_for_val(ctx_t * ctx, inst_t * inst, size_t opd) {
	if (inst->opds[opd].var->intr.val == NULL) {
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

	inst->opd0.var->intr.val = ira_val_copy(inst->opd1.val);

	return true;
}
static bool execute_copy(ctx_t * ctx, inst_t * inst) {
	if (!check_var_for_val(ctx, inst, 1)) {
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	inst->opd0.var->intr.val = ira_val_copy(inst->opd1.var->intr.val);

	return true;
}
static bool execute_read_ptr(ctx_t * ctx, inst_t * inst) {
	if (!check_var_for_val(ctx, inst, 1)) {
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	ira_val_t * val = inst->opd1.var->intr.val;

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
					inst->opd0.var->intr.val = ira_val_copy(lo->var.val);
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

	if (!get_size_from_val(inst->opd2.var->intr.val, &vec_size)) {
		report(ctx, L"[%s]: failed to get size of vector", ira_inst_infos[inst->base->type].type_str.str);
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	ira_dt_t * res_dt;

	if (!ira_pec_get_dt_vec(ctx->pec, vec_size, inst->opd1.var->intr.val->dt_val, inst->opd3.dt_qual, &res_dt)) {
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->pec, res_dt, &inst->opd0.var->intr.val)) {
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

	if (!ira_pec_get_dt_ptr(ctx->pec, inst->opd1.var->intr.val->dt_val, inst->opd2.dt_qual, &res_dt)) {
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->pec, res_dt, &inst->opd0.var->intr.val)) {
		return false;
	}

	return true;
}
static bool execute_make_dt_tpl(ctx_t * ctx, inst_t * inst) {
	size_t elems_size = inst->opd2.size;

	ira_dt_ndt_t * elems = _malloca(elems_size * sizeof(*elems));

	ul_assert(elems != NULL);

	{
		ira_dt_ndt_t * elem = elems;
		ul_hs_t ** id = inst->opd4.hss;
		for (var_t ** var = inst->opd3.vars, **var_end = var + elems_size; var != var_end; ++var, ++elem, ++id) {
			var_t * var_ptr = *var;

			if (var_ptr->intr.val == NULL) {
				report(ctx, L"[%s]: one of elements var value is NULL", ira_inst_infos[inst->base->type].type_str.str);
				_freea(elems);
				return false;
			}

			*elem = (ira_dt_ndt_t){ .dt = var_ptr->intr.val->dt_val, .name = *id };
		}
	}

	clear_var_val(ctx, inst->opd0.var);

	ira_dt_t * res_dt;

	if (!ira_pec_get_dt_tpl(ctx->pec, elems_size, elems, inst->opd1.dt_qual, &res_dt)) {
		_freea(elems);
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->pec, res_dt, &inst->opd0.var->intr.val)) {
		_freea(elems);
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

	if (!ira_pec_get_dt_arr(ctx->pec, inst->opd1.var->intr.val->dt_val, inst->opd2.dt_qual, &res_dt)) {
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->pec, res_dt, &inst->opd0.var->intr.val)) {
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

			if (var_ptr->intr.val == NULL) {
				report(ctx, L"[%s]: one of arguments var value is NULL", ira_inst_infos[inst->base->type].type_str.str);
				_freea(args);
				return false;
			}

			*arg = (ira_dt_ndt_t){ .dt = var_ptr->intr.val->dt_val, .name = *id };
		}
	}

	if (!check_var_for_val(ctx, inst, 1)) {
		_freea(args);
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	ira_dt_t * res_dt;

	if (!ira_pec_get_dt_func(ctx->pec, inst->opd1.var->intr.val->dt_val, args_size, args, inst->opd5.dt_func_vas, &res_dt)) {
		_freea(args);
		return false;
	}

	if (!ira_pec_make_val_imm_dt(ctx->pec, res_dt, &inst->opd0.var->intr.val)) {
		_freea(args);
		return false;
	}

	_freea(args);

	return true;
}
static bool execute_make_dt_const(ctx_t * ctx, inst_t * inst) {
	if (!check_var_for_val(ctx, inst, 1)) {
		return false;
	}

	ira_dt_t * dt = inst->opd1.var->intr.val->dt_val;

	if (!ira_pec_apply_qual(ctx->pec, dt, ira_dt_qual_const, &dt)) {
		return false;
	}

	clear_var_val(ctx, inst->opd0.var);

	if (!ira_pec_make_val_imm_dt(ctx->pec, dt, &inst->opd0.var->intr.val)) {
		return false;
	}

	return true;
}
static bool execute_ret(ctx_t * ctx, inst_t * inst) {
	if (!check_var_for_val(ctx, inst, 0)) {
		return false;
	}

	*ctx->intr.out = inst->opd0.var->intr.val;

	inst->opd0.var->intr.val = NULL;

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
			[IraInstMakeDtTpl] = execute_make_dt_tpl,
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
	ctx_t ctx = { .trg = TrgIntr, .pec = pec, .func = func, .intr.out = out };

	bool res = interpret_core(&ctx);

	ctx_cleanup(&ctx);

	return res;
}
