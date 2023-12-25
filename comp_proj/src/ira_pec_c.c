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

bool ira_pec_c_compile_int_arr(ira_pec_c_ctx_t * ctx, ira_val_t * arr, u_hs_t ** out_label) {
	if (arr->type != IraValImmArr
		|| arr->dt->type != IraDtArr
		|| arr->dt->arr.body->type != IraDtInt) {
		return false;
	}

	asm_frag_t * frag = asm_frag_create(AsmFragData, &ctx->out->frag);

	u_hs_t * frag_label = get_unq_arr_label(ctx);

	{
		asm_inst_t label = { .type = AsmInstLabel, .opds = AsmInstLabel, .label = frag_label };

		asm_frag_push_inst(frag, &label);
	}

	ira_dt_t * elem_dt = arr->dt->arr.body;

	asm_inst_imm_type_t imm_type;

	switch (elem_dt->int_type) {
		case IraIntU8:
		case IraIntS8:
			imm_type = AsmInstImm8;
			break;
		case IraIntU16:
		case IraIntS16:
			imm_type = AsmInstImm16;
			break;
		case IraIntU32:
		case IraIntS32:
			imm_type = AsmInstImm32;
			break;
		case IraIntU64:
		case IraIntS64:
			imm_type = AsmInstImm64;
			break;
		default:
			u_assert_switch(elem_dt->int_type);
	}

	{
		asm_inst_t data = { .type = AsmInstData, .opds = AsmInstOpds_Imm, .imm0_type = imm_type };

		for (ira_val_t ** elem = arr->arr.data, **elem_end = elem + arr->arr.size; elem != elem_end; ++elem) {
			data.imm0 = (*elem)->int_val.si64;

			asm_frag_push_inst(frag, &data);
		}
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