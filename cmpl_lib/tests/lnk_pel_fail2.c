#include "cmmn.h"

static test_ctx_t ctx;

static void fill_pel()
{
    lnk_pel_init(&ctx.pel, &ctx.hst, &ctx.ec_fmtr);

    ctx.pel.sett.file_name = EXE_NAME;
    ctx.pel.sett.apply_mrgr = false;
    ctx.pel.sett.export_pd = true;

    ul_hs_t * ep_name = UL_HST_HASHADD_WS(&ctx.hst, "start");

    ctx.pel.ep_name = ep_name;

    static const uint8_t text_sect_data[] = {
        0x48, 0x83, 0xEC, 0x28, 0xB9, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48,
        0x89, 0xC1, 0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0xB8, 0x0D, 0x00,
        0x00, 0x00, 0x4C, 0x8D, 0x4C, 0x24, 0x30, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0xB9, 0xE8, 0x03, 0x00, 0x00, 0xFF, 0x15, 0x00, 0x00, 0x00,
        0x00, 0x48, 0x83, 0xC4, 0x28, 0x31, 0xC0, 0xC3
    };
    static const uint8_t rdata_sect_data[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x68, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x6F, 0x72, 0x6C, 0x64, 0x21, 0x0A, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x47, 0x65, 0x74, 0x53, 0x74, 0x64, 0x48, 0x61, 0x6E, 0x64, 0x6C, 0x65, 0x00, 0x00,
        0x00, 0x00, 0x57, 0x72, 0x69, 0x74, 0x65, 0x46, 0x69, 0x6C, 0x65, 0x00, 0x00, 0x00, 0x53, 0x6C,
        0x65, 0x65, 0x70, 0x00, 0x6B, 0x65, 0x72, 0x6E, 0x65, 0x6C, 0x33, 0x32, 0x2E, 0x64, 0x6C, 0x6C,
        0x00
    };

    lnk_sect_lp_t text_sect_lps[] = {
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = UL_HST_HASHADD_WS(&ctx.hst, ".text#p0"), .off = 0x0 },
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = ep_name, .off = 0x0 },
        { .type = LnkSectLpFixup, .stype = LnkSectLpFixupRel8, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "GetStdHandle"), .off = 0xB },
        { .type = LnkSectLpFixup, .stype = LnkSectLpFixupRel8, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "WriteFile"), .off = 0x32 },
        { .type = LnkSectLpFixup, .stype = LnkSectLpFixupRel32, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "Sleepa"), .off = 0x3D },
        { .type = LnkSectLpFixup, .stype = LnkSectLpFixupVa64, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "msg"), .off = 0x14 },
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelProcEnd, .label_name = ep_name, .off = 0x0 },
    };
    lnk_sect_lp_t rdata_sect_lps[] = {
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "#ia:kernel32"), .off = 0x0 },
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "GetStdHandle"), .off = 0x0 },
        { .type = LnkSectLpFixup, .stype = LnkSectLpFixupRva31of64, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "GetStdHandle#n"), .off = 0x0 },
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "WriteFile"), .off = 0x8 },
        { .type = LnkSectLpFixup, .stype = LnkSectLpFixupRva31of64, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "WriteFile#n"), .off = 0x8 },
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "Sleep"), .off = 0x10 },
        { .type = LnkSectLpFixup, .stype = LnkSectLpFixupRva31of64, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "Sleep#n"), .off = 0x10 },
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "msg"), .off = 0x20 },
        { .type = LnkSectLpMark, .stype = LnkSectLpMarkImpStart, .label_name = NULL, .off = 0x30 },
        { .type = LnkSectLpFixup, .stype = LnkSectLpFixupRva32, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "#n:kernel32"), .off = 0x3C },
        { .type = LnkSectLpFixup, .stype = LnkSectLpFixupRva32, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "#ia:kernel32"), .off = 0x40 },
        { .type = LnkSectLpMark, .stype = LnkSectLpMarkImpEnd, .label_name = NULL, .off = 0x58 },
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "GetStdHandle#n"), .off = 0x60 },
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "WriteFile#n"), .off = 0x70 },
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "Sleep#n"), .off = 0x7C },
        { .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = UL_HST_HASHADD_WS(&ctx.hst, "#n:kernel32"), .off = 0x84 },
        { .type = LnkSectLpMark, .stype = LnkSectLpMarkExcptStart, .label_name = NULL, .off = 0x84 },
        { .type = LnkSectLpMark, .stype = LnkSectLpMarkExcptStart, .label_name = NULL, .off = 0x84 }
    };

    lnk_sect_t * text_sect = lnk_sect_create(".text");

    ctx.pel.sect = text_sect;

    copy_sect_data(ul_arr_count(text_sect_data), text_sect_data, text_sect);
    text_sect->data_align = 0x10;
    copy_sect_lps(ul_arr_count(text_sect_lps), text_sect_lps, text_sect);
    text_sect->data_align_byte = 0xCC;
    text_sect->mem_r = true;
    text_sect->mem_e = true;

    lnk_sect_t * rdata_sect = lnk_sect_create(".rdata");

    text_sect->next = rdata_sect;

    copy_sect_data(ul_arr_count(rdata_sect_data), rdata_sect_data, rdata_sect);
    rdata_sect->data_align = 0x10;
    copy_sect_lps(ul_arr_count(rdata_sect_lps), rdata_sect_lps, rdata_sect);
    rdata_sect->data_align_byte = 0x00;
    rdata_sect->mem_r = true;
}
static int main_core()
{
    test_ctx_init(&ctx, TestFrom_Count);

    fill_pel();

    bool res = lnk_pel_l_link(&ctx.pel);

    print_test_errs(&ctx);

    return res ? -1 : 0;
}
int main()
{
    int res = main_core();

    test_ctx_cleanup(&ctx);

    return res;
}
