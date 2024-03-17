#include "pch.h"
#include "asm_pea_b.h"
#include "asm_frag.h"
#include "lnk_pe.h"

typedef struct asm_pea_b_ctx {
	asm_pea_t * pea;

	lnk_pe_t * pe;
} ctx_t;

static bool build_frags(ctx_t * ctx) {
	lnk_pe_t * pe = ctx->pe;

	for (asm_frag_t * frag = ctx->pea->frag; frag != NULL; frag = frag->next) {
		if (!asm_frag_build(frag, pe)) {
			return false;
		}
	}

	return true;
}

static void fill_props(ctx_t * ctx) {
	lnk_pe_t * pe = ctx->pe;

	pe->file_name = lnk_pe_file_name_default;
	pe->sett = &lnk_pe_sett_default;
	pe->ep_name = ctx->pea->ep_name;
}

static bool build_core(ctx_t * ctx) {
	lnk_pe_init(ctx->pe);

	if (!build_frags(ctx)) {
		return false;
	}

	if (!asm_it_build(&ctx->pea->it, ctx->pea->hst, ctx->pe)) {
		return false;
	}

	fill_props(ctx);

	return true;
}
bool asm_pea_b_build(asm_pea_t * pea, lnk_pe_t * out) {
	ctx_t ctx = { .pea = pea, .pe = out };

	bool res;
	
	__try {
		res = build_core(&ctx);
	}
	__finally {
		(void)0;
	}

	return res;
}
