#include "pch.h"
#include "asm_pea_b.h"
#include "asm_frag.h"
#include "lnk_pel.h"

#define BUILD_FRAGS_WORKER_COUNT 4

typedef struct asm_pea_b_ctx_frag {
	asm_frag_t * frag;
} frag_t;

typedef struct asm_pea_b_ctx {
	asm_pea_t * pea;
	lnk_pel_t * out;

	size_t frags_cap;
	frag_t * frags;
	size_t frags_size;
	size_t frags_cur;
	bool frags_err_flag;
} ctx_t;

static bool get_build_frag(ctx_t * ctx, frag_t * out) {
	size_t cur = InterlockedIncrement64(&ctx->frags_cur) - 1;
	
	if (cur >= ctx->frags_size) {
		return false;
	}

	*out = ctx->frags[cur];

	return true;
}
static VOID build_frags_worker(PTP_CALLBACK_INSTANCE itnc, PVOID user_data, PTP_WORK work) {
	ctx_t * ctx = user_data;
	frag_t frag;

	while (get_build_frag(ctx, &frag)) {
		if (!asm_frag_build(frag.frag, ctx->out)) {
			ctx->frags_err_flag = true;
		}
	}
}
static bool build_frags(ctx_t * ctx) {
	lnk_pel_t * out = ctx->out;

	for (asm_frag_t * frag = ctx->pea->frag; frag != NULL; frag = frag->next) {
		if (ctx->frags_size + 1 >= ctx->frags_cap) {
			ul_arr_grow(&ctx->frags_cap, &ctx->frags, sizeof(*ctx->frags), 1);
		}

		ctx->frags[ctx->frags_size++] = (frag_t){ .frag = frag };
	}

	PTP_WORK work = CreateThreadpoolWork(build_frags_worker, ctx, NULL);

	if (work == NULL) {
		return false;
	}

	for (size_t i = 0; i < BUILD_FRAGS_WORKER_COUNT; ++i) {
		SubmitThreadpoolWork(work);
	}

	WaitForThreadpoolWorkCallbacks(work, FALSE);

	CloseThreadpoolWork(work);

	return !ctx->frags_err_flag;
}

static void fill_props(ctx_t * ctx) {
	lnk_pel_t * out = ctx->out;

	out->file_name = lnk_pel_file_name_default;
	out->sett = &lnk_pel_sett_default;
	out->ep_name = ctx->pea->ep_name;
}

static bool build_core(ctx_t * ctx) {
	lnk_pel_init(ctx->out);

	if (!build_frags(ctx)) {
		return false;
	}

	if (!asm_it_build(&ctx->pea->it, ctx->pea->hst, ctx->out)) {
		return false;
	}

	fill_props(ctx);

	return true;
}
bool asm_pea_b_build(asm_pea_t * pea, lnk_pel_t * out) {
	ctx_t ctx = { .pea = pea, .out = out };

	bool res;
	
	__try {
		res = build_core(&ctx);
	}
	__finally {
		free(ctx.frags);
	}

	return res;
}
