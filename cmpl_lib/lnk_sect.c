#include "lnk_sect.h"

lnk_sect_t * lnk_sect_create(const char * name)
{
    lnk_sect_t * sect = malloc(sizeof(*sect));

    ul_assert(sect != NULL);

    *sect = (lnk_sect_t){ .name = name };

    return sect;
}
void lnk_sect_destroy(lnk_sect_t * sect)
{
    if (sect == NULL)
    {
        return;
    }

    free(sect->data);
    free(sect->lps);
    free(sect);
}

void lnk_sect_destroy_chain(lnk_sect_t * sect)
{
    while (sect != NULL)
    {
        lnk_sect_t * next = sect->next;

        lnk_sect_destroy(sect);

        sect = next;
    }
}

extern inline lnk_sect_t * lnk_sect_create_desc(const lnk_sect_desc_t * desc);

void lnk_sect_add_lp(lnk_sect_t * sect, lnk_sect_lp_type_t type, lnk_sect_lp_stype_t stype, ul_hs_t * label_name, size_t off)
{
    ul_arr_grow(sect->lps_size + 1, &sect->lps_cap, (void **)&sect->lps, sizeof(*sect->lps));

    sect->lps[sect->lps_size++] = (lnk_sect_lp_t){ .type = type, .stype = stype, .label_name = label_name, .off = off };
}

const ul_ros_t lnk_sect_lp_mark_strs[LnkSectLpMark_Count] = {
    [LnkSectLpMarkNone] = UL_ROS_MAKE("None"),
    [LnkSectLpMarkImpStart] = UL_ROS_MAKE("Import data Start"),
    [LnkSectLpMarkImpEnd] = UL_ROS_MAKE("Import data End"),
    [LnkSectLpMarkImpTabStart] = UL_ROS_MAKE("Import table Start"),
    [LnkSectLpMarkImpTabEnd] = UL_ROS_MAKE("Import table End"),
    [LnkSectLpMarkRelocStart] = UL_ROS_MAKE("Base relocations Start"),
    [LnkSectLpMarkRelocEnd] = UL_ROS_MAKE("Base relocations End"),
    [LnkSectLpMarkExcptStart] = UL_ROS_MAKE("Exception data Start"),
    [LnkSectLpMarkExcptEnd] = UL_ROS_MAKE("Exception data End"),
};
const size_t lnk_sect_fixups_size[LnkSectLpFixup_Count] = {
    [LnkSectLpFixupNone] = 0,
    [LnkSectLpFixupRel8] = sizeof(uint8_t),
    [LnkSectLpFixupRel32] = sizeof(uint32_t),
    [LnkSectLpFixupVa64] = sizeof(uint64_t),
    [LnkSectLpFixupRva32] = sizeof(uint32_t),
    [LnkSectLpFixupRva31of64] = sizeof(uint64_t)
};
