#pragma once
#include "lnk.h"
#include "u_hs.h"

enum lnk_sect_lp_type {
	LnkSectLpNone,
	LnkSectLpMark,
	LnkSectLpLabel,
	LnkSectLpFixup,
	LnkSectLp_Count
};
enum lnk_sect_lp_stype {
	LnkSectLabelNone = 0,
	LnkSectLabel_Count = 0,

	LnkSectFixupNone = 0,
	LnkSectFixupRel8,
	LnkSectFixupRel32,
	LnkSectFixupVa64,
	LnkSectFixupRva32,
	LnkSectFixupRva31of64,
	LnkSectFixup_Count,

	LnkSectMarkNone = 0,
	LnkSectMarkImpStart,
	LnkSectMarkImpEnd,
	LnkSectMarkImpTabStart,
	LnkSectMarkImpTabEnd,
	LnkSectMarkRelocStart,
	LnkSectMarkRelocEnd,
	LnkSectMark_Count
};
struct lnk_sect_lp {
	lnk_sect_lp_type_t type;
	lnk_sect_lp_stype_t stype;
	u_hs_t * label_name;
	size_t offset;
};

struct lnk_sect {
	lnk_sect_t * next;

	const char * name;

	size_t data_size;
	uint8_t * data;
	size_t data_cap;
	size_t data_align;

	size_t lps_size;
	lnk_sect_lp_t * lps;
	size_t lps_cap;

	uint8_t data_align_byte;

	bool mem_r, mem_w, mem_e, mem_disc;
};

lnk_sect_t * lnk_sect_create(lnk_sect_t ** ins);
void lnk_sect_destroy(lnk_sect_t * sect);

void lnk_sect_destroy_chain(lnk_sect_t * sect);

void lnk_sect_add_lp(lnk_sect_t * sect, lnk_sect_lp_type_t type, lnk_sect_lp_stype_t stype, u_hs_t * label_name, size_t offset);

const size_t lnk_sect_fixups_size[LnkSectFixup_Count];
