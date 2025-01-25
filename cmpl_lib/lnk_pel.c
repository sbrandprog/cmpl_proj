#include "lnk_pel.h"
#include "lnk_pe.h"
#include "lnk_sect.h"

void lnk_pel_init(lnk_pel_t * pe, ul_hst_t * hst, ul_ec_fmtr_t * ec_fmtr)
{
    *pe = (lnk_pel_t){ .hst = hst, .ec_fmtr = ec_fmtr, .sett = lnk_pel_dflt_sett };
}
void lnk_pel_cleanup(lnk_pel_t * pe)
{
    lnk_sect_destroy_chain(pe->sect);

    memset(pe, 0, sizeof(*pe));
}

const lnk_pel_sett_t lnk_pel_dflt_sett = {
    .image_base = 0x140000000,
    .sect_align = 0x1000,
    .file_align = 0x200,

    .stack_res = 0x100000,
    .stack_com = 0x1000,
    .heap_res = 0x100000,
    .heap_com = 0x1000,

    .chars = LnkPeCharsExecutableImage | LnkPeCharsLargeAddressAware,
    .os_major = 6,
    .os_minor = 0,
    .subsys_major = 6,
    .subsys_minor = 0,
    .subsys = LnkPeSubsysWindowsCui,
    .dll_chars = LnkPeDllCharsHighEntropyVa | LnkPeDllCharsDynamicBase | LnkPeDllCharsNxCompat | LnkPeDllCharsNoSeh | LnkPeDllCharsTerminalServerAware,

    .make_excpt_sect = true,
    .make_base_reloc_sect = true,
    .apply_mrgr = true,
    .export_pd = false,

    .excpt_sect_name = ".pdata",
    .base_reloc_sect_name = ".reloc",

    .file_name = L"out.exe"
};
