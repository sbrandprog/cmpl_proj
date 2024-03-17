#include "pch.h"
#include "lnk_pe.h"
#include "lnk_sect.h"

void lnk_pe_init(lnk_pe_t * pe) {
	*pe = (lnk_pe_t){ 0 };

	InitializeCriticalSection(&pe->sect_lock);
}
void lnk_pe_cleanup(lnk_pe_t * pe) {
	DeleteCriticalSection(&pe->sect_lock);

	lnk_sect_destroy_chain(pe->sect);

	memset(pe, 0, sizeof(*pe));
}

static lnk_sect_t * push_pe_new_sect_nl(lnk_pe_t * pe) {
	lnk_sect_t * sect = lnk_sect_create();

	sect->next = pe->sect;
	pe->sect = sect;

	return sect;
}
lnk_sect_t * lnk_pe_push_new_sect(lnk_pe_t * pe) {
	EnterCriticalSection(&pe->sect_lock);

	lnk_sect_t * res;

	__try {
		res = push_pe_new_sect_nl(pe);
	}
	__finally {
		LeaveCriticalSection(&pe->sect_lock);
	}

	return res;
}

const lnk_pe_sett_t lnk_pe_sett_default = {
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
const wchar_t * lnk_pe_file_name_default = L"out.exe";
