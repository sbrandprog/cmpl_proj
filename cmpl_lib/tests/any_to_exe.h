#pragma once
#include "cmpl_lib/cmpl_lib.h"

#define EXE_NAME L"test.exe"
#define EXE_CMD EXE_NAME

typedef enum test_from {
	TestFromLnk,
	TestFromMc,
	TestFromIra,
	TestFrom_Count
} test_from_t;
typedef struct test_ctx {
	ul_hst_t hst;
	lnk_pel_t pel;
	mc_pea_t pea;
	ira_pec_t pec;

	test_from_t from;
} test_ctx_t;

static void copy_frag_insts(size_t insts_size, const mc_inst_t * insts, mc_frag_t * frag) {
	frag->insts_size = 0;

	for (const mc_inst_t * inst = insts, *inst_end = inst + insts_size; inst != inst_end; ++inst) {
		mc_frag_push_inst(frag, inst);
	}
}

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
	ctx->from = TestFrom_Count;

	if (!test_proc(ctx)) {
		wprintf(L"error point: test_proc\n");
		return false;
	}

	switch (ctx->from) {
		case TestFromIra:
			if (!ira_pec_c_compile(&ctx->pec, &ctx->pea)) {
				wprintf(L"error point: compilation\n");
				return false;
			}
			//fallthrough
		case TestFromMc:
			if (!mc_pea_b_build(&ctx->pea, &ctx->pel)) {
				wprintf(L"error point: mc build\n");
				return false;
			}
			//fallthrough
		case TestFromLnk:
			ctx->pel.file_name = EXE_NAME;

			if (!lnk_pel_l_link(&ctx->pel)) {
				wprintf(L"error point: link\n");
				return false;
			}

			int res = (int)_wspawnl(_P_WAIT, EXE_NAME, EXE_CMD, NULL);

			if (res != 0) {
				wprintf(L"error point: run (codes: %d 0x%X)\n", res, res);
				return false;
			}
			break;
		default:
			ul_assert_unreachable();
	}

	wprintf(L"success\n");

	return true;
}
static bool process_test() {
	test_ctx_t ctx = { 0 };

	ul_hst_init(&ctx.hst);

	bool res = process_test_core(&ctx);

	ira_pec_cleanup(&ctx.pec);

	mc_pea_cleanup(&ctx.pea);

	lnk_pel_cleanup(&ctx.pel);

	ul_hst_cleanup(&ctx.hst);

	return res;
}

int main() {
	if (!process_test()) {
		return -1;
	}

	if (_CrtDumpMemoryLeaks() == TRUE) {
		wprintf(L"detected leaks\n");
		return -2;
	}

	return 0;
}
