#include "pch.h"
#include "lnk_sect.h"

lnk_sect_t * lnk_sect_create() {
	lnk_sect_t * sect = malloc(sizeof(*sect));

	ul_raise_check_mem_alloc(sect);

	*sect = (lnk_sect_t){ 0 };

	return sect;
}
void lnk_sect_destroy(lnk_sect_t * sect) {
	if (sect == NULL) {
		return;
	}

	free(sect->data);
	free(sect->lps);
	free(sect);
}

void lnk_sect_destroy_chain(lnk_sect_t * sect) {
	while (sect != NULL) {
		lnk_sect_t * next = sect->next;

		lnk_sect_destroy(sect);

		sect = next;
	}
}

void lnk_sect_add_lp(lnk_sect_t * sect, lnk_sect_lp_type_t type, lnk_sect_lp_stype_t stype, ul_hs_t * label_name, size_t offset) {
	if (sect->lps_size + 1 > sect->lps_cap) {
		ul_arr_grow(&sect->lps_cap, &sect->lps, sizeof(*sect->lps), 1);
	}

	sect->lps[sect->lps_size++] = (lnk_sect_lp_t){ .type = type, .stype = stype, .label_name = label_name, .offset = offset };
}

const size_t lnk_sect_fixups_size[LnkSectFixup_Count] = {
	[LnkSectFixupNone] = 0,
	[LnkSectFixupRel8] = sizeof(uint8_t),
	[LnkSectFixupRel32] = sizeof(uint32_t),
	[LnkSectFixupVa64] = sizeof(uint64_t),
	[LnkSectFixupRva32] = sizeof(uint32_t),
	[LnkSectFixupRva31of64] = sizeof(uint64_t)
};