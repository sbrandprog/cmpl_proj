#pragma once
#include "mc/mc_lib.h"

#define EXE_NAME "test.exe"
#define EXE_CMD EXE_NAME

typedef struct test_ctx
{
    ul_hst_t hst;

    ul_ec_buf_t ec_buf;
    ul_ec_fmtr_t ec_fmtr;

    lnk_pel_t pel;
	mc_pea_t pea;
} test_ctx_t;

static void test_ctx_init(test_ctx_t * ctx)
{
    ul_hst_init(&ctx->hst);

    ul_ec_buf_init(&ctx->ec_buf);

    ul_ec_fmtr_init(&ctx->ec_fmtr, &ctx->ec_buf.ec);
}
static void test_ctx_cleanup(test_ctx_t * ctx)
{
	mc_pea_cleanup(&ctx->pea);

    lnk_pel_cleanup(&ctx->pel);

    ul_ec_fmtr_cleanup(&ctx->ec_fmtr);

    ul_ec_buf_cleanup(&ctx->ec_buf);

    ul_hst_cleanup(&ctx->hst);
}

static void print_test_errs(test_ctx_t * ctx)
{
    ul_ec_prntr_t prntr;

    ul_ec_prntr_init_dflt(&prntr);

    ul_ec_prntr_t * prntrs[] = { &prntr };

    ul_ec_buf_print(&ctx->ec_buf, ul_arr_count(prntrs), prntrs);

    ul_ec_prntr_cleanup(&prntr);
}

static void copy_frag_insts(size_t insts_size, const mc_inst_t * insts, mc_frag_t * frag)
{
    frag->insts_size = 0;

    for (const mc_inst_t *inst = insts, *inst_end = inst + insts_size; inst != inst_end; ++inst)
    {
        mc_frag_push_inst(frag, inst);
    }
}

