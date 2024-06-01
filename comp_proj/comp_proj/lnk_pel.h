#pragma once
#include "lnk.h"

#define LNK_PEL_MAX_MODULE_SIZE INT32_MAX

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

	bool make_base_reloc;
	const char * base_reloc_name;
};

struct lnk_pel {
	lnk_sect_t * sect;

	ul_hs_t * ep_name;

	const wchar_t * file_name;

	const lnk_pel_sett_t * sett;
};

void lnk_pel_init(lnk_pel_t * pel);
void lnk_pel_cleanup(lnk_pel_t * pel);

lnk_sect_t * lnk_pel_push_new_sect(lnk_pel_t * pe);

extern const lnk_pel_sett_t lnk_pel_sett_default;
extern const wchar_t * lnk_pel_file_name_default;
