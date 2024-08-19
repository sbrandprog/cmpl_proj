#pragma once
#include "lnk.h"

enum lnk_sect_lp_type {
	LnkSectLpNone,
	LnkSectLpMark,
	LnkSectLpLabel,
	LnkSectLpFixup,
	LnkSectLp_Count
};
enum lnk_sect_lp_stype {
	LnkSectLpMarkNone = 0,
	LnkSectLpMarkImpStart,
	LnkSectLpMarkImpEnd,
	LnkSectLpMarkImpTabStart,
	LnkSectLpMarkImpTabEnd,
	LnkSectLpMarkRelocStart,
	LnkSectLpMarkRelocEnd,
	LnkSectLpMark_Count,

	LnkSectLpLabelNone = 0,
	LnkSectLpLabel_Count,

	LnkSectLpFixupNone = 0,
	LnkSectLpFixupRel8,
	LnkSectLpFixupRel32,
	LnkSectLpFixupVa64,
	LnkSectLpFixupRva32,
	LnkSectLpFixupRva31of64,
	LnkSectLpFixup_Count
};
struct lnk_sect_lp {
	lnk_sect_lp_type_t type;
	lnk_sect_lp_stype_t stype;
	ul_hs_t * label_name;
	size_t off;
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

LNK_API lnk_sect_t * lnk_sect_create();
LNK_API void lnk_sect_destroy(lnk_sect_t * sect);

LNK_API void lnk_sect_destroy_chain(lnk_sect_t * sect);

LNK_API void lnk_sect_add_lp(lnk_sect_t * sect, lnk_sect_lp_type_t type, lnk_sect_lp_stype_t stype, ul_hs_t * label_name, size_t off);

extern LNK_API const size_t lnk_sect_fixups_size[LnkSectLpFixup_Count];
