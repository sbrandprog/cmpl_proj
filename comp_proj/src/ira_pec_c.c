#include "pch.h"
#include "ira_pec_c.h"
#include "ira_val.h"
#include "ira_lo.h"
#include "ira_pec_ip.h"
#include "asm_inst.h"
#include "asm_frag.h"
#include "asm_pea.h"
#include "u_misc.h"

#define LO_NAME_DELIM L'.'

#define ARR_UNQ_PREFIX L"#arr"

typedef struct ira_pec_c_ctx {
	ira_pec_t * pec;

	asm_pea_t * out;

	u_hsb_t hsb;
	u_hst_t * hst;

	size_t str_unq_index;
} ctx_t;

static u_hs_t * get_unq_arr_label(ctx_t * ctx) {
	return u_hsb_formatadd(&ctx->hsb, ctx->hst, L"%s%zi", ARR_UNQ_PREFIX, ctx->str_unq_index++);
}

bool ira_pec_c_is_val_compilable(ira_val_t * val) {
	switch (val->type) {
		case IraValImmVoid:
			break;
		case IraValImmDt:
			return false;
		case IraValImmBool:
		case IraValImmInt:
		case IraValLoPtr:
		case IraValNullPtr:
			break;
		case IraValImmArr:
			for (ira_val_t ** elem = val->arr.data, **elem_end = elem + val->arr.size; elem != elem_end; ++elem) {
				if (!ira_pec_c_is_val_compilable(*elem)) {
					return false;
				}
			}
			break;
		case IraValImmTpl:
			for (ira_val_t ** elem = val->tpl.elems, **elem_end = elem + val->dt->tpl.elems_size; elem != elem_end; ++elem) {
				if (!ira_pec_c_is_val_compilable(*elem)) {
					return false;
				}
			}
			break;
		default:
			u_assert_switch(val->type);
	}

	return true;
}
static bool compile_val(ctx_t * ctx, asm_frag_t * frag, ira_val_t * val) {
	{
		size_t dt_align;
		
		if (!ira_dt_get_align(val->dt, &dt_align)) {
			return false;
		}

		asm_inst_t align = { .type = AsmInstAlign, .opds = AsmInstOpds_Imm, .imm0_type = AsmInstImm64, .imm0 = (int64_t)dt_align };

		asm_frag_push_inst(frag, &align);
	}

	asm_inst_t data = { .type = AsmInstData, .opds = AsmInstOpds_Imm };

	switch (val->type) {
		case IraValImmVoid:
			break;
		case IraValImmDt:
			return false;
		case IraValImmBool:
			data.imm0_type = AsmInstImm8;
			data.imm0 = val->bool_val ? 1 : 0;
			
			asm_frag_push_inst(frag, &data);
			break;
		case IraValImmInt:
			data.imm0_type = ira_int_type_to_imm_type[val->dt->int_type];
			data.imm0 = val->int_val.si64;
			
			asm_frag_push_inst(frag, &data);
			break;
		case IraValLoPtr:
			data.imm0_type = AsmInstImmLabelVa64;
			data.imm0_label = val->lo_val->full_name;
			
			asm_frag_push_inst(frag, &data);
			break;
		case IraValNullPtr:
			data.imm0_type = AsmInstImm64;
			data.imm0 = 0;
			
			asm_frag_push_inst(frag, &data);
			break;
		case IraValImmArr:
			for (ira_val_t ** elem = val->arr.data, **elem_end = elem + val->arr.size; elem != elem_end; ++elem) {
				if (!compile_val(ctx, frag, *elem)) {
					return false;
				}
			}
			break;
		case IraValImmTpl:
			for (ira_val_t ** elem = val->tpl.elems, **elem_end = elem + val->dt->tpl.elems_size; elem != elem_end; ++elem) {
				if (!compile_val(ctx, frag, *elem)) {
					return false;
				}
			}
			break;
		default:
			u_assert_switch(val->type);
	}


	return true;
}
bool ira_pec_c_compile_val_frag(ira_pec_c_ctx_t * ctx, ira_val_t * val, u_hs_t ** out_label) {
	asm_frag_t * frag = asm_frag_create(AsmFragRoData, &ctx->out->frag);

	u_hs_t * frag_label = get_unq_arr_label(ctx);

	{
		asm_inst_t label = { .type = AsmInstLabel, .opds = AsmInstLabel, .label = frag_label };

		asm_frag_push_inst(frag, &label);
	}

	if (!compile_val(ctx, frag, val)) {
		return false;
	}

	*out_label = frag_label;

	return true;
}

u_hsb_t * ira_pec_c_get_hsb(ira_pec_c_ctx_t * ctx) {
	return &ctx->hsb;
}
u_hst_t * ira_pec_c_get_hst(ira_pec_c_ctx_t * ctx) {
	return ctx->hst;
}
ira_pec_t * ira_pec_c_get_pec(ira_pec_c_ctx_t * ctx) {
	return ctx->pec;
}
asm_pea_t * ira_pec_c_get_pea(ira_pec_c_ctx_t * ctx) {
	return ctx->out;
}

static bool compile_lo_var(ctx_t * ctx, ira_lo_t * lo) {
	asm_frag_type_t frag_type = lo->var.qdt.qual.const_q ? AsmFragRoData : AsmFragWrData;

	asm_frag_t * frag = asm_frag_create(frag_type, &ctx->out->frag);

	{
		asm_inst_t label = { .type = AsmInstLabel, .opds = AsmInstOpds_Label, .label = lo->full_name };

		asm_frag_push_inst(frag, &label);
	}

	switch (lo->var.val->type) {
		case IraValImmDt:
			return false;
		case IraValImmVoid:
		case IraValImmBool:
		case IraValImmInt:
		case IraValLoPtr:
		case IraValNullPtr:
		case IraValImmTpl:
			if (!compile_val(ctx, frag, lo->var.val)) {
				return false;
			}
			break;
		case IraValImmArr:
		{
			u_hs_t * arr_label;

			if (!ira_pec_c_compile_val_frag(ctx, lo->var.val, &arr_label)) {
				return false;
			}

			{
				asm_inst_t data = { .type = AsmInstData, .opds = AsmInstOpds_Imm, .imm0_type = AsmInstImm64, .imm0 = (int64_t)lo->var.val->arr.size };

				asm_frag_push_inst(frag, &data);
			}

			{
				asm_inst_t data = { .type = AsmInstData, .type = AsmInstData, .opds = AsmInstOpds_Imm, .imm0_type = AsmInstImmLabelVa64, .imm0_label = arr_label };

				asm_frag_push_inst(frag, &data);
			}

			break;
		}
	}

	return true;
}
static bool compile_lo(ctx_t * ctx, ira_lo_t * lo) {
	switch (lo->type) {
		case IraLoNone:
			break;
		case IraLoNspc:
			for (ira_lo_t * it = lo->nspc.body; it != NULL; it = it->next) {
				if (!compile_lo(ctx, it)) {
					return false;
				}
			}
			break;
		case IraLoFunc:
			if (!ira_pec_ip_compile(ctx, lo)) {
				return false;
			}
			break;
		case IraLoImpt:
			asm_it_add_sym(&ctx->out->it, lo->impt.lib_name, lo->impt.sym_name, lo->full_name);
			break;
		case IraLoVar:
			if (!compile_lo_var(ctx, lo)) {
				return false;
			}
			break;
		default:
			u_assert_switch(lo->type);
	}

	return true;
}

static bool compile_core(ctx_t * ctx) {
	ctx->hst = ctx->pec->hst;

	asm_pea_init(ctx->out, ctx->hst);

	if (!compile_lo(ctx, ctx->pec->root)) {
		return false;
	}

	ctx->out->ep_name = ctx->pec->ep_name;

	return true;
}
bool ira_pec_c_compile(ira_pec_t * pec, asm_pea_t * out) {
	ctx_t ctx = { .pec = pec, .out = out };

	bool result = compile_core(&ctx);

	u_hsb_cleanup(&ctx.hsb);

	return result;
}