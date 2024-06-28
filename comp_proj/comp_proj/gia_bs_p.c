#include "pch.h"
#include "pla_cn.h"
#include "pla_tu.h"
#include "pla_pkg.h"
#include "pla_ast.h"
#include "gia_tus_p.h"
#include "gia_pkg.h"
#include "gia_repo.h"
#include "gia_bs_p.h"

typedef struct gia_bs_p_pkg pkg_t;
struct gia_bs_p_pkg {
	gia_pkg_t * base;

	pkg_t * next;

	pkg_t * parent;
	pkg_t * sub_pkg;

	ul_hs_t * full_name;

	pla_pkg_t * pla_pkg;
};

typedef struct gia_bs_p_tus {
	gia_tus_t * base;

	ul_hs_t * full_name;

	pkg_t * parent_pkg;
} tus_t;

typedef struct gia_bs_p_ctx {
	gia_repo_t * repo;
	ul_hs_t * first_tus_name;
	pla_lex_t * lex;
	pla_ast_t * out;

	ul_hsb_t hsb;

	pkg_t * root;

	size_t tuss_size;
	tus_t * tuss;
	size_t tuss_cap;
} ctx_t;

static void destroy_pkg_chain(pkg_t * pkg);

static pkg_t * create_pkg(ctx_t * ctx, gia_pkg_t * base, pkg_t * parent, pla_pkg_t * pla_pkg) {
	pkg_t * pkg = malloc(sizeof(*pkg));

	ul_assert(pkg != NULL);

	*pkg = (pkg_t){ .base = base, .parent = parent, .full_name = base->name, .pla_pkg = pla_pkg };

	if (parent != NULL && parent->full_name != NULL) {
		ul_assert(base->name != NULL);

		pkg->full_name = ul_hsb_formatadd(&ctx->hsb, &ctx->repo->hst, L"%s%c%s", parent->full_name->str, PLA_AST_NAME_DELIM, base->name->str);
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

	gia_pkg_t * base = pkg->base->sub_pkg;

	while (base != NULL && base->name != sub_pkg_name) {
		base = base->next;
	}

	if (base == NULL) {
		return NULL;
	}

	*sub_pkg_ins = create_pkg(ctx, base, pkg, pla_pkg_get_sub_pkg(pkg->pla_pkg, sub_pkg_name));

	return *sub_pkg_ins;
}

static bool find_and_push_tus(ctx_t * ctx, pkg_t * pkg, ul_hs_t * tus_name) {
	for (tus_t * tus = ctx->tuss, *tus_end = tus + ctx->tuss_size; tus != tus_end; ++tus) {
		if (tus->parent_pkg == pkg && tus->base->name == tus_name) {
			return true;
		}
	}

	gia_tus_t * tus = gia_pkg_find_tus(pkg->base, tus_name);

	if (tus == NULL) {
		return false;
	}

	if (ctx->tuss_size + 1 > ctx->tuss_cap) {
		ul_arr_grow(&ctx->tuss_cap, &ctx->tuss, sizeof(*ctx->tuss), 1);
	}

	ul_hs_t * tus_full_name = tus->name;

	if (pkg->full_name != NULL) {
		tus_full_name = ul_hsb_formatadd(&ctx->hsb, &ctx->repo->hst, L"%s%c%s", pkg->full_name->str, PLA_AST_NAME_DELIM, tus->name->str);
	}

	ctx->tuss[ctx->tuss_size++] = (tus_t){ .base = tus, .full_name = tus_full_name, .parent_pkg = pkg };

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
	ul_hsb_init(&ctx->hsb);

	pla_ast_init(ctx->out, &ctx->repo->hst);

	ctx->out->root = pla_pkg_create(NULL);

	ctx->root = create_pkg(ctx, ctx->repo->root, NULL, ctx->out->root);

	if (!find_and_push_tus(ctx, ctx->root, ctx->first_tus_name)) {
		return false;
	}

	for (size_t tuss_cur = 0; tuss_cur < ctx->tuss_size; ++tuss_cur) {
		tus_t tus = ctx->tuss[tuss_cur];

		pla_tu_t * tu = pla_pkg_get_tu(tus.parent_pkg->pla_pkg, tus.base->name);

		if (!gia_tus_p_parse(ctx->lex, tus.base, tus.full_name, tu)) {
			return false;
		}

		for (pla_tu_ref_t * ref = tu->refs, *ref_end = ref + tu->refs_size; ref != ref_end; ++ref) {
			if (!find_and_push_tus_cn(ctx, ref->is_rel ? tus.parent_pkg : ctx->root, ref->cn)) {
				return false;
			}
		}
	}

	return true;
}
bool gia_bs_p_form_ast_nl(gia_repo_t * repo, ul_hs_t * first_tus_name, pla_lex_t * lex, pla_ast_t * out) {
	ctx_t ctx = { .repo = repo, .first_tus_name = first_tus_name, .lex = lex, .out = out };

	bool res = form_core(&ctx);

	free(ctx.tuss);

	destroy_pkg(ctx.root);

	ul_hsb_cleanup(&ctx.hsb);

	return res;
}
