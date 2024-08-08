#pragma once
#include "cmpl_lib/cmpl_lib.h"

#define EXE_NAME L"test.exe"
#define EXE_CMD EXE_NAME

typedef struct test_ctx {
	ul_hst_t hst;
	lnk_pel_t pel;
	mc_pea_t pea;
} test_ctx_t;

static void copy_frag_insts(size_t insts_size, const mc_inst_t * insts, mc_frag_t * frag) {
	frag->insts_size = 0;

	for (const mc_inst_t * inst = insts, *inst_end = inst + insts_size; inst != inst_end; ++inst) {
		mc_frag_push_inst(frag, inst);
	}
}

static bool test_proc(test_ctx_t * ctx);

static bool process_test_core(test_ctx_t * ctx) {
	if (!test_proc(ctx)) {
		return false;
	}

	if (!mc_pea_b_build(&ctx->pea, &ctx->pel)) {
		return false;
	}

	ctx->pel.file_name = EXE_NAME;

	if (!lnk_pel_l_link(&ctx->pel)) {
		return false;
	}

	int res = (int)_wspawnl(_P_WAIT, EXE_NAME, EXE_CMD, NULL);

	if (res != 0) {
		return false;
	}

	return true;
}
static bool process_test() {
	test_ctx_t ctx = { 0 };

	ul_hst_init(&ctx.hst);

	mc_pea_init(&ctx.pea, &ctx.hst);

	bool res = process_test_core(&ctx);

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
		return -2;
	}

	return 0;
}
