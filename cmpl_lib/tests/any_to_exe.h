#pragma once
#include "cmmn.h"

static test_ctx_t ctx;

static bool test_proc(test_ctx_t * ctx);

static bool process_test_core(test_ctx_t * ctx)
{
    test_ctx_init(ctx, TestFrom_Count);

    if (!test_proc(ctx))
    {
        printf("error point: test_proc\n");
        return false;
    }

    switch (ctx->from)
    {
        case TestFromIra:
            mc_pea_init(&ctx->pea, &ctx->hst, &ctx->ec_fmtr);

            if (!ira_pec_c_compile(&ctx->pec, &ctx->pea))
            {
                printf("error point: compilation\n");
                return false;
            }
            // fallthrough
        case TestFromMc:
            lnk_pel_init(&ctx->pel, &ctx->hst, &ctx->ec_fmtr);

            if (!mc_pea_b_build(&ctx->pea, &ctx->pel))
            {
                printf("error point: mc build\n");
                return false;
            }
            // fallthrough
        case TestFromLnk:
        {
            ctx->pel.sett.file_name = EXE_NAME;

            if (!lnk_pel_l_link(&ctx->pel))
            {
                printf("error point: link\n");
                return false;
            }

            const char * const args[] = { EXE_NAME, NULL };

            int res = ul_spawn_wait(EXE_NAME, args);

            if (res != 0)
            {
                printf("error point: run (codes: %d 0x%X)\n", res, res);
                return false;
            }
            break;
        }
        default:
            ul_assert_unreachable();
    }

    printf("success\n");

    return true;
}
static bool process_test()
{
    bool res = process_test_core(&ctx);

    if (ctx.ec_buf.rec != NULL)
    {
        if (res)
        {
            printf("error: positive exit status with records in error collector\n");
        }

        res = false;

        print_test_errs(&ctx);
    }

    return res;
}

int main()
{
    if (!process_test())
    {
        return -1;
    }

    test_ctx_cleanup(&ctx);

    return 0;
}
