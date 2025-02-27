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
        { .type = McInstSub, .opds = McInstOpds_Reg_Imm, .reg0 = McRegRsp, .imm0_type = McInstImm8, .imm0 = 0x28 },
        { .type = McInstMov, .opds = McInstOpds_Reg_Imm, .reg0 = McRegEcx, .imm0_type = McInstImm32, .imm0 = -11 },
        { .type = McInstCall, .opds = McInstOpds_Mem, .mem_base = McRegRip, .mem_disp_type = McInstDispLabelRel32, .mem_disp = 0, .mem_disp_label = UL_HST_HASHADD_WS(&ctx->hst, "GetStdHandle") },
        { .type = McInstMov, .opds = McInstOpds_Reg_Reg, .reg0 = McRegRcx, .reg1 = McRegRax },
        { .type = McInstMov, .opds = McInstOpds_Reg_Imm, .reg0 = McRegRdx, .imm0_type = McInstImmLabelVa64, .imm0_label = UL_HST_HASHADD_WS(&ctx->hst, "msg") },
        { .type = McInstMov, .opds = McInstOpds_Reg_Imm, .reg0 = McRegR8d, .imm0_type = McInstImm32, .imm0 = 13 },
        { .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegR9, .mem_base = McRegRsp, .mem_disp = 0x30 },
        { .type = McInstMov, .opds = McInstOpds_Mem_Imm, .mem_base = McRegRsp, .mem_disp = 0x20, .mem_size = McSize64, .imm0_type = McInstImm32, .imm0 = 0 },
        { .type = McInstCall, .opds = McInstOpds_Mem, .mem_base = McRegRip, .mem_disp_type = McInstDispLabelRel32, .mem_disp = 0, .mem_disp_label = UL_HST_HASHADD_WS(&ctx->hst, "WriteFile") },
        { .type = McInstMov, .opds = McInstOpds_Reg_Imm, .reg0 = McRegEcx, .imm0_type = McInstImm32, .imm0 = 1000 },
        { .type = McInstCall, .opds = McInstOpds_Mem, .mem_base = McRegRip, .mem_disp_type = McInstDispLabelRel32, .mem_disp = 0, .mem_disp_label = UL_HST_HASHADD_WS(&ctx->hst, "Sleep") },
        { .type = McInstAdd, .opds = McInstOpds_Reg_Imm, .reg0 = McRegRsp, .imm0_type = McInstImm8, .imm0 = 0x28 },
        { .type = McInstXor, .opds = McInstOpds_Reg_Reg, .reg0 = McRegEax, .reg1 = McRegEax },
        { .type = McInstRet, .opds = McInstOpds_None },
    };

    copy_frag_insts(ul_arr_count(main_proc_insts), main_proc_insts, main_proc);

    mc_frag_t * data_frag = mc_frag_create(McFragRoData);

    main_proc->next = data_frag;

    mc_inst_t data_frag_insts[] = {
        { .type = McInstLabel, .opds = McInstOpds_Label, .label_stype = LnkSectLpLabelBasic, .label = UL_HST_HASHADD_WS(&ctx->hst, "msg") },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = 'H' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = 'e' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = 'l' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = 'l' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = 'o' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = ' ' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = 'w' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = 'o' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = 'r' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = 'l' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = 'd' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = '!' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = '\n' },
        { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm8, .imm0 = 0 },
    };

    copy_frag_insts(ul_arr_count(data_frag_insts), data_frag_insts, data_frag);

    {
        ul_hs_t * lib_name = UL_HST_HASHADD_WS(&ctx->hst, "kernel32.dll");

        mc_pea_it_add_sym(&ctx->pea.it, lib_name, UL_HST_HASHADD_WS(&ctx->hst, "GetStdHandle"), UL_HST_HASHADD_WS(&ctx->hst, "GetStdHandle"));
        mc_pea_it_add_sym(&ctx->pea.it, lib_name, UL_HST_HASHADD_WS(&ctx->hst, "WriteFile"), UL_HST_HASHADD_WS(&ctx->hst, "WriteFile"));
        mc_pea_it_add_sym(&ctx->pea.it, lib_name, UL_HST_HASHADD_WS(&ctx->hst, "Sleep"), UL_HST_HASHADD_WS(&ctx->hst, "Sleep"));
    }

    return true;
}
