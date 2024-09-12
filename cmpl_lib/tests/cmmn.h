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
	test_from_t from;

	ul_hst_t hst;

	ul_ec_buf_t ec_buf;
	ul_ec_fmtr_t ec_fmtr;

	lnk_pel_t pel;
	mc_pea_t pea;
	ira_pec_t pec;
} test_ctx_t;

static void test_ctx_init(test_ctx_t * ctx, test_from_t from) {
	ctx->from = from;

	ul_hst_init(&ctx->hst);

	ul_ec_buf_init(&ctx->ec_buf);

	ul_ec_fmtr_init(&ctx->ec_fmtr, &ctx->ec_buf.ec);
}
static void test_ctx_cleanup(test_ctx_t * ctx) {
	ira_pec_cleanup(&ctx->pec);

	mc_pea_cleanup(&ctx->pea);

	lnk_pel_cleanup(&ctx->pel);

	ul_ec_fmtr_cleanup(&ctx->ec_fmtr);

	ul_ec_buf_cleanup(&ctx->ec_buf);

	ul_hst_cleanup(&ctx->hst);
}

static void print_test_errs(test_ctx_t * ctx) {
	ul_ec_prntr_t prntr;

	ul_ec_prntr_init_dflt(&prntr);

	ul_ec_prntr_t * prntrs[] = { &prntr };

	ul_ec_buf_print(&ctx->ec_buf, _countof(prntrs), prntrs);

	ul_ec_prntr_cleanup(&prntr);
}

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
		lnk_sect_add_lp(sect, lp->type, lp->stype, lp->label_name, lp->off);
	}
}
