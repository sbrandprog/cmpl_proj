#include "mc_pea_b.h"
#include "lnk_pel.h"
#include "lnk_sect.h"
#include "mc_frag.h"

#define MOD_NAME "mc_pea_b"

#define BUILD_FRAGS_WORKER_COUNT 4

typedef struct mc_pea_b_ctx_frag
{
    mc_frag_t * frag;
    lnk_sect_t * sect;
} frag_t;

typedef struct mc_pea_b_ctx
{
    mc_pea_t * pea;
    lnk_pel_t * out;

    bool is_rptd;

    size_t frags_size;
    frag_t * frags;
    size_t frags_cap;
} ctx_t;

static void report(ctx_t * ctx, const char * fmt, ...)
{
    ctx->is_rptd = true;

    if (ctx->pea->ec_fmtr == NULL)
    {
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

static void form_frags_list(ctx_t * ctx)
{
    for (mc_frag_t * frag = ctx->pea->frag; frag != NULL; frag = frag->next)
    {
        ul_arr_grow(ctx->frags_size + 1, &ctx->frags_cap, &ctx->frags, sizeof(*ctx->frags));

        ctx->frags[ctx->frags_size++] = (frag_t){ .frag = frag };
    }
}
static void process_frags_list(ctx_t * ctx)
{
    bool res = true;

    for (frag_t *frag = ctx->frags, *frag_end = ctx->frags + ctx->frags_size; frag != frag_end; ++frag)
    {
        if (!mc_frag_build(frag->frag, ctx->pea->ec_fmtr, &frag->sect))
        {
            res = false;
        }
    }

    if (!res)
    {
        report(ctx, "failed to build one or more fragments");
    }
}
static void insert_sects(ctx_t * ctx)
{
    lnk_sect_t ** ins = &ctx->out->sect;

    while (*ins != NULL)
    {
        ins = &(*ins)->next;
    }

    for (frag_t *frag = ctx->frags, *frag_end = frag + ctx->frags_size; frag != frag_end; ++frag)
    {
        *ins = frag->sect;

        frag->sect = NULL;

        ins = &(*ins)->next;
    }
}
static void build_frags(ctx_t * ctx)
{
    form_frags_list(ctx);

    process_frags_list(ctx);

    insert_sects(ctx);
}

static void fill_props(ctx_t * ctx)
{
    lnk_pel_t * out = ctx->out;

    out->ep_name = ctx->pea->ep_name;
}

static void build_core(ctx_t * ctx)
{
    build_frags(ctx);

    if (!mc_pea_it_build(&ctx->pea->it, ctx->out))
    {
        report(ctx, "failed to build import table");
    }

    fill_props(ctx);
}
bool mc_pea_b_build(mc_pea_t * pea, lnk_pel_t * out)
{
    ctx_t ctx = { .pea = pea, .out = out };

    build_core(&ctx);

    for (frag_t *frag = ctx.frags, *frag_end = frag + ctx.frags_size; frag != frag_end; ++frag)
    {
        lnk_sect_destroy(frag->sect);
    }

    free(ctx.frags);

    return !ctx.is_rptd;
}
