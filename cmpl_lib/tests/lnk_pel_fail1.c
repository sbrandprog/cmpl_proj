#include "cmmn.h"

static test_ctx_t ctx;

static void fill_pel() {
	lnk_pel_init(&ctx.pel, &ctx.hst, &ctx.ec_fmtr);

	ctx.pel.sett.file_name = EXE_NAME;

	ul_hs_t * ep_name = UL_HST_HASHADD_WS(&ctx.hst, L"start");

	ctx.pel.ep_name = ep_name;

	static const uint8_t text_sect_data[] = {
		0x31, 0xC0, 0xC3
	};

	lnk_sect_lp_t text_sect_lps[] = {
		{ .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = ep_name, .off = 0 }
	};

	lnk_sect_t * text_sect0 = lnk_sect_create(".text");

	ctx.pel.sect = text_sect0;

	copy_sect_data(_countof(text_sect_data), text_sect_data, text_sect0);
	text_sect0->data_align = 0x10;
	copy_sect_lps(_countof(text_sect_lps), text_sect_lps, text_sect0);
	text_sect0->data_align_byte = 0xCC;
	text_sect0->mem_r = true;
	text_sect0->mem_e = true;

	lnk_sect_t * text_sect1 = lnk_sect_create(".text");

	text_sect0->next = text_sect1;

	copy_sect_data(_countof(text_sect_data), text_sect_data, text_sect1);
	text_sect1->data_align = 0x10;
	copy_sect_lps(_countof(text_sect_lps), text_sect_lps, text_sect1);
	text_sect1->data_align_byte = 0xCC;
	text_sect1->mem_r = false;
	text_sect1->mem_e = true;

	lnk_sect_add_lp(text_sect0, LnkSectLp_Count, 0, NULL, 0);
	lnk_sect_add_lp(text_sect1, LnkSectLpLabel, LnkSectLpLabel_Count, ep_name, 0);
}
static int main_core() {
	test_ctx_init(&ctx, TestFrom_Count);

	fill_pel();

	bool res = lnk_pel_l_link(&ctx.pel);

	print_test_errs(&ctx);

	return res ? -1 : 0;
}
int main() {
	int res = main_core();

	test_ctx_cleanup(&ctx);

	return res;
}
