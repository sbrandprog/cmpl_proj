#include "pch.h"
#include "lnk_pel.h"
#include "lnk_sect.h"

void lnk_pel_init(lnk_pel_t * pe) {
	*pe = (lnk_pel_t){ 0 };
}
void lnk_pel_cleanup(lnk_pel_t * pe) {
	lnk_sect_destroy_chain(pe->sect);

	memset(pe, 0, sizeof(*pe));
}

lnk_sect_t * lnk_pel_push_new_sect(lnk_pel_t * pe) {
	lnk_sect_t * sect = lnk_sect_create();

	lnk_sect_t * next;

	do {
		next = pe->sect;

		sect->next = pe->sect;
	} while (InterlockedCompareExchangePointer(&pe->sect, sect, next) != next);

	return sect;
}

const lnk_pel_sett_t lnk_pel_sett_default = {
	.image_base = 0x140000000,
	.sect_align = 0x1000, .file_align = 0x200,

	.stack_res = 0x100000, .stack_com = 0x1000,
	.heap_res = 0x100000, .heap_com = 0x1000,

	.chars = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE,
	.os_major = 6, .os_minor = 0,
	.subsys_major = 6, .subsys_minor = 0,
	.subsys = IMAGE_SUBSYSTEM_WINDOWS_CUI,
	.dll_chars = IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA | IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE | IMAGE_DLLCHARACTERISTICS_NX_COMPAT | IMAGE_DLLCHARACTERISTICS_NO_SEH | IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE,

	.make_base_reloc = true,
	.base_reloc_name = ".reloc"
};
const wchar_t * lnk_pel_file_name_default = L"out.exe";
