#include "lnk_pel_l.h"
#include "mc_pea_b.h"
#include "ira_pec_c.h"
#include "pla_ec_prntr.h"
#include "pla_repo.h"
#include "pla_bs_lpt.h"

typedef struct pla_bs_ctx {
	pla_repo_t * repo;
	ul_hs_t * first_tus_name;
	const wchar_t * file_name;
	const pla_bs_sett_t * sett;

	ul_ec_buf_t ec_buf;
	ul_ec_fmtr_t ec_fmtr;
	ira_pec_t ira_pec;
	mc_pea_t mc_pea;
	lnk_pel_t lnk_pe;
} ctx_t;

static void print_errs(ctx_t * ctx) {
	ul_ec_prntr_t dflt_prntr;
	pla_ec_prntr_t pla_prntr;

	ul_ec_prntr_init_dflt(&dflt_prntr);

	pla_ec_prntr_init(&pla_prntr, ctx->repo);

	ul_ec_prntr_t * prntrs[] = { &pla_prntr.prntr, &dflt_prntr };

	ul_ec_buf_print(&ctx->ec_buf, _countof(prntrs), prntrs);

	pla_ec_prntr_cleanup(&pla_prntr);

	ul_ec_prntr_cleanup(&dflt_prntr);
}
static bool build_core(ctx_t * ctx) {
	ul_ec_buf_init(&ctx->ec_buf);

	ul_ec_fmtr_init(&ctx->ec_fmtr, &ctx->ec_buf.ec);
	
	if (!pla_bs_lpt_form_pec_nl(ctx->repo, &ctx->ec_fmtr, ctx->first_tus_name, &ctx->ira_pec)
		|| ctx->ec_buf.rec != NULL) {
		return false;
	}

	if (!ira_pec_c_compile(&ctx->ira_pec, &ctx->mc_pea)
		|| ctx->ec_buf.rec != NULL) {
		return false;
	}

	if (!mc_pea_b_build(&ctx->mc_pea, &ctx->lnk_pe)) {
		return false;
	}

	ctx->lnk_pe.file_name = ctx->file_name;
	ctx->lnk_pe.sett.export_pd = ctx->sett->export_pd;

	if (!lnk_pel_l_link(&ctx->lnk_pe)) {
		return false;
	}

	return true;
}
bool pla_bs_build_nl(pla_repo_t * repo, ul_hs_t * first_tus_name, const wchar_t * file_name, const pla_bs_sett_t * sett) {
	ctx_t ctx = { .repo = repo, .first_tus_name = first_tus_name, .file_name = file_name, .sett = sett };

	bool res = build_core(&ctx);

	if (ctx.ec_buf.rec != NULL) {
		res = false;

		print_errs(&ctx);
	}

	lnk_pel_cleanup(&ctx.lnk_pe);

	mc_pea_cleanup(&ctx.mc_pea);

	ira_pec_cleanup(&ctx.ira_pec);

	ul_ec_fmtr_cleanup(&ctx.ec_fmtr);

	ul_ec_buf_cleanup(&ctx.ec_buf);

	return res;
}

const pla_bs_sett_t pla_bs_dflt_sett = { .export_pd = false };
