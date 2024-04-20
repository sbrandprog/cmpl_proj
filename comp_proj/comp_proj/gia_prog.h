#pragma once
#include "gia.h"

#define GIA_PROG_OUT_FILE_NAME_DFLT L"out.exe"

struct gia_prog {
	ul_hst_t hst;

	CRITICAL_SECTION lock;

	const wchar_t * out_file_name;

	size_t tuss_cap;
	gia_tus_t ** tuss;
	size_t tuss_size;
};

void gia_prog_init(gia_prog_t * prog);
void gia_prog_cleanup(gia_prog_t * prog);

gia_tus_t * gia_prog_get_tus_nl(gia_prog_t * prog, ul_hs_t * tus_name);

bool gia_prog_add_tus_file_nl(gia_prog_t * prog, const wchar_t * file_name);
