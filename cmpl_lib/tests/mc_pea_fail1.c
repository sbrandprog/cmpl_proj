#include <locale.h>

#include "cmmn.h"

static test_ctx_t ctx;

static int main_core()
{
    test_ctx_init(&ctx, TestFrom_Count);

    mc_pea_init(&ctx.pea, &ctx.hst, &ctx.ec_fmtr);

    ctx.pea.frag = mc_frag_create(McFrag_Count);

	mc_inst_t insts[] = {
		{ .type = McInst_Count, .opds = McInstOpds_None },
		{ .type = McInstMov, .opds = McInstOpds_Reg_Reg, .reg0 = McRegAx, .reg1 = McReg_Count },
		{ .type = McInstMov, .opds = McInstOpds_Reg_Reg, .reg0 = McRegAx, .reg1 = McRegEax },
		{ .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegAx, .mem_size = McSizeNone, .mem_base = McRegAx },
		{ .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegAx, .mem_size = McSizeNone, .mem_base = McRegRax, .mem_scale = McSize8, .mem_index = McRegRip },
		{ .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegAx, .mem_size = McSizeNone, .mem_base = McRegRbp, .mem_disp_type = McInstDispNone },
		{ .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegAx, .mem_size = McSizeNone, .mem_base = McRegRax, .mem_scale = McSize8, .mem_index = McRegRsp },
	};

    copy_frag_insts(ul_arr_count(insts), insts, ctx.pea.frag);
	cleanup_src_insts(ul_arr_count(insts), insts);

    mc_pea_it_add_sym(&ctx.pea.it, UL_HST_HASHADD_WS(&ctx.hst, "библиотека.длл"), UL_HST_HASHADD_WS(&ctx.hst, "функция"), UL_HST_HASHADD_WS(&ctx.hst, "имя_компоновки"));

    lnk_pel_init(&ctx.pel, &ctx.hst, &ctx.ec_fmtr);

    bool res = mc_pea_b_build(&ctx.pea, &ctx.pel);

    print_test_errs(&ctx);

    return res ? -1 : 0;
}
int main()
{
    setlocale(LC_ALL, ".utf8");

    int res = main_core();

    test_ctx_cleanup(&ctx);

    return res;
}
