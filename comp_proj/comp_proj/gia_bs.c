#include "pch.h"
#include "lnk_pe_l.h"
#include "asm_pea_b.h"
#include "ira_pec_c.h"
#include "pla_lex.h"
#include "pla_ast_t.h"
#include "gia_repo.h"
#include "gia_bs_p.h"

typedef struct gia_bs_ctx {
	gia_repo_t * repo;
	ul_hs_t * first_tus_name;
	const wchar_t * file_name;

	pla_lex_t lex;
	pla_ast_t ast;
	ira_pec_t pec;
	asm_pea_t pea;
	lnk_pe_t pe;
} ctx_t;

static bool build_core(ctx_t * ctx) {
	pla_lex_init(&ctx->lex, &ctx->repo->hst);

	if (!gia_bs_p_form_ast_nl(ctx->repo, ctx->first_tus_name, &ctx->lex, &ctx->ast)) {
		return false;
	}

	if (!pla_ast_t_translate(&ctx->ast, &ctx->pec)) {
		return false;
	}

	if (!ira_pec_c_compile(&ctx->pec, &ctx->pea)) {
		return false;
	}

	if (!asm_pea_b_build(&ctx->pea, &ctx->pe)) {
		return false;
	}

	ctx->pe.file_name = ctx->file_name;

	if (!lnk_pe_l_link(&ctx->pe)) {
		return false;
	}

	return true;
}
bool gia_bs_build_nl(gia_repo_t * repo, ul_hs_t * first_tus_name, const wchar_t * file_name) {
	ctx_t ctx = { .repo = repo, .first_tus_name = first_tus_name, .file_name = file_name };

	bool res;

	__try {
		res = build_core(&ctx);
	}
	__finally {
		lnk_pe_cleanup(&ctx.pe);

		asm_pea_cleanup(&ctx.pea);

		ira_pec_cleanup(&ctx.pec);

		pla_ast_cleanup(&ctx.ast);

		pla_lex_cleanup(&ctx.lex);
	}

	return res;
}
