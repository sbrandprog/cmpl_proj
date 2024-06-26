#include "pch.h"
#include "pla_lex.h"
#include "pla_prsr.h"
#include "pla_ast.h"
#include "gia_tus_p.h"

typedef struct gia_prog_p_tus_ctx {
	pla_lex_t * lex;
	gia_tus_t * tus;
	ul_hs_t * tus_full_name;
	pla_tu_t * out;

	pla_prsr_t prsr;

	wchar_t * ch;
	wchar_t * ch_end;

	pla_lex_src_t lex_src_data;

	pla_prsr_src_t prsr_src_data;
} ctx_t;

static bool get_tus_src_ch(void * src_data, wchar_t * out) {
	ctx_t * ctx = src_data;

	if (ctx->ch == ctx->ch_end) {
		return false;
	}

	*out = *ctx->ch++;

	return true;
}
static bool get_tus_src_tok(void * src_data, pla_tok_t * out) {
	ctx_t * ctx = src_data;

	if (!pla_lex_get_tok(ctx->lex)) {
		return false;
	}

	*out = ctx->lex->tok;

	return true;
}

static bool parse_core(ctx_t * ctx) {
	ctx->ch = ctx->tus->src;
	ctx->ch_end = ctx->ch + ctx->tus->src_size;

	ctx->lex_src_data = (pla_lex_src_t){ .name = ctx->tus_full_name, .data = ctx, .get_ch_proc = get_tus_src_ch, .line_num = 0, .line_ch = 0 };
	pla_lex_set_src(ctx->lex, &ctx->lex_src_data);

	pla_prsr_init(&ctx->prsr, ctx->lex->ec_fmtr);

	ctx->prsr_src_data = (pla_prsr_src_t){ .name = ctx->tus_full_name, .data = ctx, .get_tok_proc = get_tus_src_tok };
	pla_prsr_set_src(&ctx->prsr, &ctx->prsr_src_data);

	if (!pla_prsr_parse_tu(&ctx->prsr, ctx->out)) {
		return false;
	}

	pla_lex_set_src(ctx->lex, NULL);

	return true;
}
bool gia_tus_p_parse(pla_lex_t * lex, gia_tus_t * tus, ul_hs_t * tus_full_name, pla_tu_t * out) {
	ctx_t ctx = { .lex = lex, .tus = tus, .tus_full_name = tus_full_name, .out = out };

	EnterCriticalSection(&tus->lock);

	bool res = parse_core(&ctx);

	pla_prsr_cleanup(&ctx.prsr);

	LeaveCriticalSection(&tus->lock);

	return res;
}
