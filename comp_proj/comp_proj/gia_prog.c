#include "pch.h"
#include "gia_tus.h"
#include "gia_prog.h"

void gia_prog_init(gia_prog_t * prog) {
	*prog = (gia_prog_t){ .out_file_name = GIA_PROG_OUT_FILE_NAME_DFLT };

	InitializeCriticalSection(&prog->lock);

	ul_hst_init(&prog->hst);
}
void gia_prog_cleanup(gia_prog_t * prog) {
	DeleteCriticalSection(&prog->lock);

	ul_hst_cleanup(&prog->hst);

	for (gia_tus_t ** it = prog->tuss, **it_end = it + prog->tuss_size; it != it_end; ++it) {
		gia_tus_t * tus = *it;
	
		gia_tus_destroy(tus);
	}

	free(prog->tuss);

	memset(prog, 0, sizeof(*prog));
}

gia_tus_t * gia_prog_get_tus_nl(gia_prog_t * prog, ul_hs_t * tus_name) {
	for (gia_tus_t ** it = prog->tuss, **it_end = it + prog->tuss_size; it != it_end; ++it) {
		gia_tus_t * tus = *it;
		
		if (tus->name == tus_name) {
			return tus;
		}
	}

	if (prog->tuss_size + 1 > prog->tuss_cap) {
		ul_arr_grow(&prog->tuss_cap, (void**)&prog->tuss, sizeof(*prog->tuss), 1);
	}

	gia_tus_t ** it = &prog->tuss[prog->tuss_size++];

	*it = gia_tus_create(tus_name);

	return *it;
}

bool gia_prog_add_tus_file_nl(gia_prog_t * prog, const wchar_t * file_name) {
	ul_hs_t * tus_name = ul_hst_hashadd(&prog->hst, wcslen(file_name), file_name);

	gia_tus_t * tus = gia_prog_get_tus_nl(prog, tus_name);

	if (!gia_tus_read_file_nl(tus, file_name)) {
		return false;
	}

	return true;
}
