#include "pch.h"
#include "pla_lex.h"
#include "pla_tu_p.h"
#include "pla_ast.h"
#include "gia_tus.h"
#include "gia_prog_p.h"

typedef struct gia_prog_p_ctx {
	gia_prog_t * prog;
	pla_ast_t * out;

	pla_lex_t lex;
} ctx_t;

typedef struct gia_prog_p_tus_ctx {
	gia_tus_t * tus;
	pla_lex_t * lex;

	wchar_t * ch;
	wchar_t * ch_end;

	pla_tu_t * tu;
} tus_ctx_t;

static bool get_tus_src_ch(void * src_data, wchar_t * out) {
	tus_ctx_t * ctx = src_data;

	if (ctx->ch == ctx->ch_end) {
		return false;
	}

	*out = *ctx->ch++;

	return true;
}
static bool get_tus_tok(void * src_data, pla_tok_t * out) {
	tus_ctx_t * ctx = src_data;

	if (!pla_lex_get_tok(ctx->lex)) {
		return false;
	}

	*out = ctx->lex->tok;

	return true;
}

static bool parse_tus_core(tus_ctx_t * tus_ctx) {
	gia_tus_t * tus = tus_ctx->tus;
	
	tus_ctx->ch = tus->src;
	tus_ctx->ch_end = tus_ctx->ch + tus->src_size;

	pla_lex_set_src(tus_ctx->lex, get_tus_src_ch, tus_ctx, 0, 0);

	if (!pla_tu_p_parse(tus_ctx->tus->name, get_tus_tok, tus_ctx, &tus_ctx->tu)) {
		return false;
	}

	return true;
}
static bool parse_tus(ctx_t * ctx, gia_tus_t * tus) {
	EnterCriticalSection(&tus->lock);

	tus_ctx_t tus_ctx = (tus_ctx_t){ .tus = tus, .lex = &ctx->lex };

	bool res;

	__try {
		res = parse_tus_core(&tus_ctx);
	}
	__finally {
		if (res) {
			tus_ctx.tu->next = ctx->out->tu;
			ctx->out->tu = tus_ctx.tu;
		}
		else {
			pla_tu_destroy(tus_ctx.tu);
		}

		LeaveCriticalSection(&tus->lock);
	}

	return res;
}

static bool parse_core(ctx_t * ctx) {
	pla_ast_init(ctx->out, &ctx->prog->hst);

	pla_lex_init(&ctx->lex, &ctx->prog->hst);

	for (gia_tus_t ** it = ctx->prog->tuss, **it_end = it + ctx->prog->tuss_size; it != it_end; ++it) {
		gia_tus_t * tus = *it;

		if (!parse_tus(ctx, tus)) {
			return false;
		}
	}

	return true;
}
bool gia_prog_p_parse_nl(gia_prog_t * prog, pla_ast_t * out) {
	ctx_t ctx = { .prog = prog, .out = out };

	bool res;

	__try {
		res = parse_core(&ctx);
	}
	__finally {
		pla_lex_cleanup(&ctx.lex);
	}

	return res;
}
