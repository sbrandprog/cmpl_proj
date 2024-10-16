#pragma once
#include "lnk.h"

#define LNK_PEL_MODULE_MAX_SIZE INT32_MAX
#define LNK_PEL_PD_FILE_EXT L".pd"

struct lnk_pel_sett {
	uint64_t image_base;
	uint64_t sect_align, file_align;

	uint64_t stack_res, stack_com;
	uint64_t heap_res, heap_com;

	uint16_t chars;
	uint16_t os_major, os_minor;
	uint16_t subsys_major, subsys_minor;
	uint16_t subsys;
	uint16_t dll_chars;

	bool make_excpt_sect;
	bool make_base_reloc_sect;
	bool apply_mrgr;
	bool export_pd;

	const char * excpt_sect_name;
	const char * base_reloc_sect_name;

	const wchar_t * file_name;
};

struct lnk_pel {
	ul_hst_t * hst;
	ul_ec_fmtr_t * ec_fmtr;

	lnk_sect_t * sect;

	ul_hs_t * ep_name;

	lnk_pel_sett_t sett;
};

LNK_API void lnk_pel_init(lnk_pel_t * pel, ul_hst_t * hst, ul_ec_fmtr_t * ec_fmtr);
LNK_API void lnk_pel_cleanup(lnk_pel_t * pel);

extern LNK_API const lnk_pel_sett_t lnk_pel_dflt_sett;
