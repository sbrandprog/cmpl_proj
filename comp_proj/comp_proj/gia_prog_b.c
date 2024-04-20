#include "pch.h"
#include "lnk_pe_l.h"
#include "asm_pea_b.h"
#include "ira_pec_c.h"
#include "pla_ast_t.h"
#include "gia_prog_p.h"
#include "gia_prog_b.h"

typedef struct gia_prog_b_ctx {
	gia_prog_t * prog;

	ul_hst_t * hst;

	lnk_pe_t lnk_pe;
	asm_pea_t asm_pea;
	ira_pec_t ira_pec;
	pla_ast_t pla_ast;
} ctx_t;

static bool build_core(ctx_t * ctx) {
	ctx->hst = &ctx->prog->hst;

	if (!gia_prog_p_parse_nl(ctx->prog, &ctx->pla_ast)) {
		return false;
	}

	if (!pla_ast_t_translate(&ctx->pla_ast, &ctx->ira_pec)) {
		return false;
	}

	if (!ira_pec_c_compile(&ctx->ira_pec, &ctx->asm_pea)) {
		return false;
	}

	if (!asm_pea_b_build(&ctx->asm_pea, &ctx->lnk_pe)) {
		return false;
	}

	ctx->lnk_pe.file_name = ctx->prog->out_file_name;

	if (!lnk_pe_l_link(&ctx->lnk_pe)) {
		return false;
	}

	return true;
}
bool gia_prog_b_build_nl(gia_prog_t * prog) {
	ctx_t ctx = { .prog = prog };

	bool res;
	
	__try {
		res = build_core(&ctx);
	}
	__finally {
		lnk_pe_cleanup(&ctx.lnk_pe);

		asm_pea_cleanup(&ctx.asm_pea);

		ira_pec_cleanup(&ctx.ira_pec);

		pla_ast_cleanup(&ctx.pla_ast);
	}

	return res;
}
