#include "any_to_exe.h"

static bool test_proc(test_ctx_t * ctx)
{
    ctx->from = TestFromMc;

    mc_pea_init(&ctx->pea, &ctx->hst, &ctx->ec_fmtr);

    ul_hs_t * ep_name = UL_HST_HASHADD_WS(&ctx->hst, "main");

    ctx->pea.ep_name = ep_name;

    mc_frag_t * main_proc = mc_frag_create(McFragCode);

    ctx->pea.frag = main_proc;

    mc_inst_t main_proc_insts[] = {
        { .type = McInstLabel, .opds = McInstOpds_Label, .label_stype = LnkSectLpLabelBasic, .label = ep_name },
        { .type = McInstXor, .opds = McInstOpds_Reg_Reg, .reg0 = McRegEax, .reg1 = McRegEax },
        { .type = McInstRet, .opds = McInstOpds_None }
    };

    copy_frag_insts(ul_arr_count(main_proc_insts), main_proc_insts, main_proc);
	cleanup_src_insts(ul_arr_count(main_proc_insts), main_proc_insts);

    return true;
}
