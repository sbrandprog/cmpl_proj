#include "lnk_to_exe.h"

static bool test_proc(test_ctx_t * ctx) {
	lnk_pel_sett_t sett = lnk_pel_dflt_sett;
	ul_hs_t * ep_name = UL_HST_HASHADD_WS(&ctx->hst, L"start");

	ctx->pel.ep_name = ep_name;
	ctx->pel.file_name = ctx->exe_name;
	ctx->pel.sett = &sett;

	static const uint8_t text_sect_data[] = {
		0x31, 0xC0, 0xC3
	};

	lnk_sect_lp_t text_sect_lps[] = {
		{ .type = LnkSectLpLabel, .stype = LnkSectLpLabelNone, .label_name = ep_name, .offset = 0 }
	};

	lnk_sect_t * text_sect = lnk_sect_create();

	ctx->pel.sect = text_sect;

	text_sect->name = ".text";
	copy_sect_data(_countof(text_sect_data), text_sect_data, text_sect);
	text_sect->data_align = 0x10;
	copy_sect_lps(_countof(text_sect_lps), text_sect_lps, text_sect);
	text_sect->data_align_byte = 0xCC;
	text_sect->mem_r = true;
	text_sect->mem_e = true;

	return lnk_pel_l_link(&ctx->pel);
}
