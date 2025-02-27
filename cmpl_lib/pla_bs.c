#include "ira_pec_c.h"
#include "lnk_pel_l.h"
#include "mc_pea_b.h"
#include "pla_bs_lpt.h"
#include "pla_ec_prntr.h"
#include "pla_repo.h"

typedef struct pla_bs_ctx
{
    const pla_bs_src_t * src;

    ul_ec_buf_t ec_buf;
    ul_ec_fmtr_t ec_fmtr;
    ira_pec_t ira_pec;
    mc_pea_t mc_pea;
    lnk_pel_t lnk_pel;
} ctx_t;


void pla_bs_src_init(pla_bs_src_t * src, pla_repo_t * repo, ul_hs_t * first_tus_name)
{
    *src = (pla_bs_src_t){ .repo = repo, .first_tus_name = first_tus_name, .lnk_sett = lnk_pel_dflt_sett };
}
void pla_bs_src_cleanup(pla_bs_src_t * src)
{
    memset(src, 0, sizeof(*src));
}


static void print_errs(ctx_t * ctx)
{
    ul_ec_prntr_t dflt_prntr;
    pla_ec_prntr_t pla_prntr;

    ul_ec_prntr_init_dflt(&dflt_prntr);

    pla_ec_prntr_init(&pla_prntr, ctx->src->repo);

    ul_ec_prntr_t * prntrs[] = { &pla_prntr.prntr, &dflt_prntr };

    ul_ec_buf_print(&ctx->ec_buf, ul_arr_count(prntrs), prntrs);

    pla_ec_prntr_cleanup(&pla_prntr);

    ul_ec_prntr_cleanup(&dflt_prntr);
}
static bool build_core(ctx_t * ctx)
{
    const pla_bs_src_t * src = ctx->src;

    ul_ec_buf_init(&ctx->ec_buf);

    ul_ec_fmtr_init(&ctx->ec_fmtr, &ctx->ec_buf.ec);

    if (!pla_bs_lpt_form_pec_nl(src->repo, &ctx->ec_fmtr, src->first_tus_name, &ctx->ira_pec)
        || ctx->ec_buf.rec != NULL)
    {
        return false;
    }

    mc_pea_init(&ctx->mc_pea, src->repo->hst, &ctx->ec_fmtr);

    if (!ira_pec_c_compile(&ctx->ira_pec, &ctx->mc_pea)
        || ctx->ec_buf.rec != NULL)
    {
        return false;
    }

    lnk_pel_init(&ctx->lnk_pel, src->repo->hst, &ctx->ec_fmtr);

    if (!mc_pea_b_build(&ctx->mc_pea, &ctx->lnk_pel))
    {
        return false;
    }

    ctx->lnk_pel.sett = ctx->src->lnk_sett;

    if (!lnk_pel_l_link(&ctx->lnk_pel))
    {
        return false;
    }

    return true;
}
bool pla_bs_build_nl(const pla_bs_src_t * src)
{
    ctx_t ctx = { .src = src };

    bool res = build_core(&ctx);

    if (ctx.ec_buf.rec != NULL)
    {
        res = false;

        print_errs(&ctx);
    }

    lnk_pel_cleanup(&ctx.lnk_pel);

    mc_pea_cleanup(&ctx.mc_pea);

    ira_pec_cleanup(&ctx.ira_pec);

    ul_ec_fmtr_cleanup(&ctx.ec_fmtr);

    ul_ec_buf_cleanup(&ctx.ec_buf);

    return res;
}
