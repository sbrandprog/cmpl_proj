#include "any_to_exe.h"

static bool test_proc(test_ctx_t * ctx) {
	ctx->from = TestFromLnk;

	lnk_pel_init(&ctx->pel, &ctx->hst, &ctx->ec_fmtr);

	ctx->pel.sett.apply_mrgr = false;

	ul_hs_t * ep_name = UL_HST_HASHADD_WS(&ctx->hst, L"start");

	ctx->pel.ep_name = ep_name;

	static const uint8_t text_sect_data[] = {
		0x31, 0xC0, 0xC3
	};

	lnk_sect_lp_t text_sect_lps[] = {
		{ .type = LnkSectLpLabel, .stype = LnkSectLpLabelBasic, .label_name = ep_name, .off = 0 }
	};

	lnk_sect_t * text_sect = lnk_sect_create(".text");

	ctx->pel.sect = text_sect;

	copy_sect_data(_countof(text_sect_data), text_sect_data, text_sect);
	text_sect->data_align = 0x10;
	copy_sect_lps(_countof(text_sect_lps), text_sect_lps, text_sect);
	text_sect->data_align_byte = 0xCC;
	text_sect->mem_r = true;
	text_sect->mem_e = true;

	return true;
}
