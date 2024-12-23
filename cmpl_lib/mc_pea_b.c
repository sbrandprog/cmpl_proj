#include "mc_pea_b.h"
#include "mc_frag.h"
#include "lnk_sect.h"
#include "lnk_pel.h"

#define MOD_NAME L"mc_pea_b"

#define BUILD_FRAGS_WORKER_COUNT 4

typedef struct mc_pea_b_ctx_frag {
	mc_frag_t * frag;
	lnk_sect_t * sect;
} frag_t;

typedef struct mc_pea_b_ctx {
	mc_pea_t * pea;
	lnk_pel_t * out;

	bool is_rptd;

	size_t frags_size;
	frag_t * frags;
	size_t frags_cap;

	PTP_WORK build_work;
	size_t frags_cur;
	bool frags_err_flag;
} ctx_t;

static void report(ctx_t * ctx, const wchar_t * fmt, ...) {
	ctx->is_rptd = true;

	if (ctx->pea->ec_fmtr == NULL) {
		return;
	}

	{
		va_list args;

		va_start(args, fmt);

		ul_ec_fmtr_format_va(ctx->pea->ec_fmtr, fmt, args);

		va_end(args);
	}

	ul_ec_fmtr_post(ctx->pea->ec_fmtr, UL_EC_REC_TYPE_DFLT, MOD_NAME);
}

static bool get_build_frag(ctx_t * ctx, frag_t ** out) {
	size_t cur = InterlockedIncrement64(&ctx->frags_cur) - 1;
	
	if (cur >= ctx->frags_size) {
		return false;
	}

	*out = &ctx->frags[cur];

	return true;
}
static VOID build_frags_worker_proc(PTP_CALLBACK_INSTANCE itnc, PVOID user_data, PTP_WORK work) {
	ctx_t * ctx = user_data;
	frag_t * frag;

	while (get_build_frag(ctx, &frag)) {
		if (!mc_frag_build(frag->frag, ctx->pea->ec_fmtr, &frag->sect)) {
			ctx->frags_err_flag = true;
		}
	}
}

static void form_frags_list(ctx_t * ctx) {
	for (mc_frag_t * frag = ctx->pea->frag; frag != NULL; frag = frag->next) {
		ul_arr_grow(ctx->frags_size + 1, &ctx->frags_cap, &ctx->frags, sizeof(*ctx->frags));

		ctx->frags[ctx->frags_size++] = (frag_t){ .frag = frag };
	}
}
static void do_work(ctx_t * ctx) {
	ctx->build_work = CreateThreadpoolWork(build_frags_worker_proc, ctx, NULL);

	if (ctx->build_work == NULL) {
		report(ctx, L"failed to initialize parallel worker");
		return;
	}

	for (size_t i = 0; i < BUILD_FRAGS_WORKER_COUNT; ++i) {
		SubmitThreadpoolWork(ctx->build_work);
	}

	WaitForThreadpoolWorkCallbacks(ctx->build_work, FALSE);

	CloseThreadpoolWork(ctx->build_work);

	ctx->build_work = NULL;

	if (ctx->frags_err_flag) {
		report(ctx, L"failed to build one or more fragments");
	}
}
static void insert_sects(ctx_t * ctx) {
	lnk_sect_t ** ins = &ctx->out->sect;

	while (*ins != NULL) {
		ins = &(*ins)->next;
	}

	for (frag_t * frag = ctx->frags, *frag_end = frag + ctx->frags_size; frag != frag_end; ++frag) {
		*ins = frag->sect;

		frag->sect = NULL;

		ins = &(*ins)->next;
	}
}
static void build_frags(ctx_t * ctx) {
	form_frags_list(ctx);

	do_work(ctx);

	insert_sects(ctx);
}

static void fill_props(ctx_t * ctx) {
	lnk_pel_t * out = ctx->out;

	out->ep_name = ctx->pea->ep_name;
}

static void build_core(ctx_t * ctx) {
	build_frags(ctx);

	if (!mc_it_build(&ctx->pea->it, ctx->out)) {
		report(ctx, L"failed to build import table");
	}

	fill_props(ctx);
}
bool mc_pea_b_build(mc_pea_t * pea, lnk_pel_t * out) {
	ctx_t ctx = { .pea = pea, .out = out };

	build_core(&ctx);

	if (ctx.build_work != NULL) {
		CloseThreadpoolWork(ctx.build_work);
	}

	for (frag_t * frag = ctx.frags, *frag_end = frag + ctx.frags_size; frag != frag_end; ++frag) {
		lnk_sect_destroy(frag->sect);
	}

	free(ctx.frags);

	return !ctx.is_rptd;
}
