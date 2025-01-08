#pragma once
#include "cmmn.h"

static test_ctx_t ctx;

static bool test_proc(test_ctx_t * ctx);

static bool process_test_core(test_ctx_t * ctx) {
	test_ctx_init(ctx, TestFrom_Count);

	if (!test_proc(ctx)) {
		wprintf(L"error point: test_proc\n");
		return false;
	}

	switch (ctx->from) {
		case TestFromIra:
			mc_pea_init(&ctx->pea, &ctx->hst, &ctx->ec_fmtr);

			if (!ira_pec_c_compile(&ctx->pec, &ctx->pea)) {
				wprintf(L"error point: compilation\n");
				return false;
			}
			//fallthrough
		case TestFromMc:
			lnk_pel_init(&ctx->pel, &ctx->hst, &ctx->ec_fmtr);

			if (!mc_pea_b_build(&ctx->pea, &ctx->pel)) {
				wprintf(L"error point: mc build\n");
				return false;
			}
			//fallthrough
		case TestFromLnk:
			ctx->pel.sett.file_name = EXE_NAME;

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
	bool res = process_test_core(&ctx);

	if (ctx.ec_buf.rec != NULL) {
		if (res) {
			wprintf(L"error: positive exit status with records in error collector\n");
		}

		res = false;

		print_test_errs(&ctx);
	}

	return res;
}

int main() {
	if (!process_test()) {
		return -1;
	}

	test_ctx_cleanup(&ctx);

	return 0;
}
