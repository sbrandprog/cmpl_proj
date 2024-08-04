#include "ira_pec.h"
#include "pla_lex.h"
#include "pla_cn.h"
#include "pla_tu.h"
#include "pla_prsr.h"
#include "pla_tltr.h"
#include "pla_tus.h"
#include "pla_pkg.h"
#include "pla_repo.h"
#include "pla_bs_lpt.h"

typedef struct pla_bs_lpt_pkg pkg_t;
struct pla_bs_lpt_pkg {
	pla_pkg_t * base;

	pkg_t * next;

	pkg_t * parent;
	pkg_t * sub_pkg;

	ul_hs_t * full_name;
};

typedef struct pla_bs_lpt_tus {
	pla_tus_t * base;

	ul_hs_t * full_name;

	pkg_t * parent_pkg;

	pla_tu_t * tu;
} tus_t;

typedef struct pla_bs_lpt_ctx {
	pla_repo_t * repo;
	pla_ec_fmtr_t * ec_fmtr;
	ul_hs_t * first_tus_name;
	ira_pec_t * out;

	ul_hst_t * hst;
	ul_hsb_t hsb;

	pla_lex_t lex;
	pla_prsr_t prsr;
	pla_tltr_t tltr;

	pkg_t * root;

	size_t tuss_size;
	tus_t * tuss;
	size_t tuss_cap;
} ctx_t;

typedef struct pla_bs_lpt_lp_ctx {
	ctx_t * base;
	tus_t * tus;

	wchar_t * ch;
	wchar_t * ch_end;

	pla_lex_src_t lex_src_data;

	pla_prsr_src_t prsr_src_data;
} lp_ctx_t;


static bool get_tus_src_ch(void * src_data, wchar_t * out) {
	lp_ctx_t * ctx = src_data;

	if (ctx->ch == ctx->ch_end) {
		return false;
	}

	*out = *ctx->ch++;

	return true;
}
static bool get_tus_src_tok(void * src_data, pla_tok_t * out) {
	lp_ctx_t * ctx = src_data;

	if (!pla_lex_get_tok(&ctx->base->lex)) {
		return false;
	}

	*out = ctx->base->lex.tok;

	return true;
}


static bool lex_parse_tus_core(lp_ctx_t * ctx) {
	pla_tus_t * base_tus = ctx->tus->base;

	ctx->ch = base_tus->src;
	ctx->ch_end = ctx->ch + base_tus->src_size;

	ctx->lex_src_data = (pla_lex_src_t){ .name = ctx->tus->full_name, .data = ctx, .get_ch_proc = get_tus_src_ch, .line_num = 0, .line_ch = 0 };
	pla_lex_set_src(&ctx->base->lex, &ctx->lex_src_data);

	ctx->prsr_src_data = (pla_prsr_src_t){ .name = ctx->tus->full_name, .data = ctx, .get_tok_proc = get_tus_src_tok };
	pla_prsr_set_src(&ctx->base->prsr, &ctx->prsr_src_data);

	if (!pla_prsr_parse_tu(&ctx->base->prsr, ctx->tus->tu)) {
		return false;
	}

	return true;
}
static bool lex_parse_tus(ctx_t * base, tus_t * tus) {
	lp_ctx_t ctx = { .base = base, .tus = tus };

	bool res = lex_parse_tus_core(&ctx);

	pla_prsr_set_src(&base->prsr, NULL);

	pla_lex_set_src(&base->lex, NULL);

	return res;
}


static void destroy_pkg_chain(pkg_t * pkg);

static pkg_t * create_pkg(ctx_t * ctx, pla_pkg_t * base, pkg_t * parent) {
	pkg_t * pkg = malloc(sizeof(*pkg));

	ul_assert(pkg != NULL);

	*pkg = (pkg_t){ .base = base, .parent = parent, .full_name = base->name };

	if (parent != NULL && parent->full_name != NULL) {
		ul_assert(base->name != NULL);

		pkg->full_name = ul_hsb_formatadd(&ctx->hsb, &ctx->repo->hst, L"%s%c%s", parent->full_name->str, PLA_TU_NAME_DELIM, base->name->str);
	}

	return pkg;
}
static void destroy_pkg(pkg_t * pkg) {
	if (pkg == NULL) {
		return;
	}

	destroy_pkg_chain(pkg->sub_pkg);

	free(pkg);
}
static void destroy_pkg_chain(pkg_t * pkg) {
	while (pkg != NULL) {
		pkg_t * next = pkg->next;

		destroy_pkg(pkg);

		pkg = next;
	}
}

static pkg_t * get_sub_pkg(ctx_t * ctx, pkg_t * pkg, ul_hs_t * sub_pkg_name) {
	pkg_t ** sub_pkg_ins = &pkg->sub_pkg;

	while (*sub_pkg_ins != NULL) {
		pkg_t * sub_pkg = *sub_pkg_ins;

		if (sub_pkg->base->name == sub_pkg_name) {
			return sub_pkg;
		}

		sub_pkg_ins = &(*sub_pkg_ins)->next;
	}

	pla_pkg_t * base = pkg->base->sub_pkg;

	while (base != NULL && base->name != sub_pkg_name) {
		base = base->next;
	}

	if (base == NULL) {
		return NULL;
	}

	*sub_pkg_ins = create_pkg(ctx, base, pkg);

	return *sub_pkg_ins;
}

static bool find_and_push_tus(ctx_t * ctx, pkg_t * pkg, ul_hs_t * tus_name) {
	for (tus_t * tus = ctx->tuss, *tus_end = tus + ctx->tuss_size; tus != tus_end; ++tus) {
		if (tus->parent_pkg == pkg && tus->base->name == tus_name) {
			return true;
		}
	}

	pla_tus_t * base = pla_pkg_find_tus(pkg->base, tus_name);

	if (base == NULL) {
		return false;
	}

	if (ctx->tuss_size + 1 > ctx->tuss_cap) {
		ul_arr_grow(&ctx->tuss_cap, &ctx->tuss, sizeof(*ctx->tuss), 1);
	}

	ul_hs_t * tus_full_name = base->name;

	if (pkg->full_name != NULL) {
		tus_full_name = ul_hsb_formatadd(&ctx->hsb, &ctx->repo->hst, L"%s%c%s", pkg->full_name->str, PLA_TU_NAME_DELIM, base->name->str);
	}

	tus_t * tus = &ctx->tuss[ctx->tuss_size++];
	
	*tus = (tus_t){ .base = base, .full_name = tus_full_name, .parent_pkg = pkg };
	tus->tu = pla_tu_create(base->name);

	return true;
}
static bool find_and_push_tus_cn(ctx_t * ctx, pkg_t * pkg, pla_cn_t * tus_cn) {
	while (tus_cn->sub_name != NULL) {
		pkg_t * sub_pkg = get_sub_pkg(ctx, pkg, tus_cn->name);

		if (sub_pkg == NULL) {
			return false;
		}

		pkg = sub_pkg;
		tus_cn = tus_cn->sub_name;
	}

	return find_and_push_tus(ctx, pkg, tus_cn->name);
}

static bool form_core(ctx_t * ctx) {
	ctx->hst = &ctx->repo->hst;

	ul_hsb_init(&ctx->hsb);

	ira_pec_init(ctx->out, ctx->hst);

	pla_lex_init(&ctx->lex, ctx->hst, ctx->ec_fmtr);

	pla_prsr_init(&ctx->prsr, ctx->ec_fmtr);

	pla_tltr_init(&ctx->tltr, ctx->hst, ctx->ec_fmtr, ctx->out);

	ctx->root = create_pkg(ctx, ctx->repo->root, NULL);

	if (!find_and_push_tus(ctx, ctx->root, ctx->first_tus_name)) {
		return false;
	}

	for (size_t tuss_cur = 0; tuss_cur < ctx->tuss_size; ++tuss_cur) {
		tus_t tus = ctx->tuss[tuss_cur];

		if (!lex_parse_tus(ctx, &tus)) {
			return false;
		}

		for (pla_tu_ref_t * ref = tus.tu->refs, *ref_end = ref + tus.tu->refs_size; ref != ref_end; ++ref) {
			if (!find_and_push_tus_cn(ctx, ref->is_rel ? tus.parent_pkg : ctx->root, ref->cn)) {
				return false;
			}
		}
	}

	for (tus_t * tus = ctx->tuss + ctx->tuss_size, *tus_end = ctx->tuss; tus != tus_end;) {
		--tus;

		pla_tltr_src_t tltr_src = { .name = tus->full_name, .tu = tus->tu };

		if (!pla_tltr_translate(&ctx->tltr, &tltr_src)) {
			return false;
		}
	}

	return true;
}
bool pla_bs_lpt_form_pec_nl(pla_repo_t * repo, pla_ec_fmtr_t * ec_fmtr, ul_hs_t * first_tus_name, ira_pec_t * out) {
	ctx_t ctx = { .repo = repo, .ec_fmtr = ec_fmtr, .first_tus_name = first_tus_name, .out = out };

	bool res = form_core(&ctx);

	for (tus_t * tus = ctx.tuss, *tus_end = tus + ctx.tuss_size; tus != tus_end; ++tus) {
		pla_tu_destroy(tus->tu);
	}

	free(ctx.tuss);

	destroy_pkg(ctx.root);

	pla_tltr_cleanup(&ctx.tltr);

	pla_prsr_cleanup(&ctx.prsr);

	pla_lex_cleanup(&ctx.lex);

	ul_hsb_cleanup(&ctx.hsb);

	return res;
}
