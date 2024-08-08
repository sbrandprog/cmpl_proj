#pragma once
#include "cmpl_lib/cmpl_lib.h"

#define EXE_NAME L"test.exe"
#define EXE_CMD EXE_NAME

typedef struct test_ctx {
	const wchar_t * exe_name;

	ul_hst_t hst;
	lnk_pel_t pel;
} test_ctx_t;

static void copy_sect_data(size_t data_size, const uint8_t * data, lnk_sect_t * sect) {
	free(sect->data);
	sect->data_size = 0;
	sect->data = NULL;
	sect->data_cap = 0;

	uint8_t * new_data = malloc(sizeof(*new_data) * data_size);

	ul_assert(new_data != NULL);

	memcpy_s(new_data, sizeof(*new_data) * data_size, data, sizeof(*data) * data_size);

	sect->data_size = data_size;
	sect->data = new_data;
	sect->data_cap = data_size;
}
static void copy_sect_lps(size_t lps_size, const lnk_sect_lp_t * lps, lnk_sect_t * sect) {
	sect->lps_size = 0;

	for (const lnk_sect_lp_t * lp = lps, *lp_end = lp + lps_size; lp != lp_end; ++lp) {
		lnk_sect_add_lp(sect, lp->type, lp->stype, lp->label_name, lp->offset);
	}
}

static bool test_proc(test_ctx_t * ctx);

static bool process_test_core(test_ctx_t * ctx) {
	if (!test_proc(ctx)) {
		return false;
	}

	int res = (int)_wspawnl(_P_WAIT, EXE_NAME, EXE_CMD, NULL);

	if (res != 0) {
		return false;
	}

	return true;
}
static bool process_test() {
	test_ctx_t ctx = { .exe_name = EXE_NAME };

	ul_hst_init(&ctx.hst);

	lnk_pel_init(&ctx.pel);

	bool res = process_test_core(&ctx);

	lnk_pel_cleanup(&ctx.pel);

	ul_hst_cleanup(&ctx.hst);

	return res;
}

int main() {
	if (!process_test()) {
		return -1;
	}

	if (_CrtDumpMemoryLeaks() == TRUE) {
		return -2;
	}

	return 0;
}
