#include "cmpl_lib/cmpl_lib.h"
#include <locale.h>

static const mc_inst_t insts[] = {
	{ .type = McInst_Count, .opds = McInstOpds_None },
	{ .type = McInstMov, .opds = McInstOpds_Reg_Reg, .reg0 = McRegAx, .reg1 = McReg_Count },
	{ .type = McInstMov, .opds = McInstOpds_Reg_Reg, .reg0 = McRegAx, .reg1 = McRegEax },
	{ .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegAx, .mem_size = McSizeNone, .mem_base = McRegAx },
	{ .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegAx, .mem_size = McSizeNone, .mem_base = McRegRax, .mem_scale = McSize8, .mem_index = McRegRip },
	{ .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegAx, .mem_size = McSizeNone, .mem_base = McRegRbp, .mem_disp_type = McInstDispNone },
	{ .type = McInstLea, .opds = McInstOpds_Reg_Mem, .reg0 = McRegAx, .mem_size = McSizeNone, .mem_base = McRegRax, .mem_scale = McSize8, .mem_index = McRegRsp },
};

static ul_hst_t hst;
static ul_ec_buf_t ec_buf;
static ul_ec_fmtr_t ec_fmtr;
static lnk_pel_t pel;
static mc_pea_t pea;

static void copy_frag_insts(size_t insts_size, const mc_inst_t * insts, mc_frag_t * frag) {
	frag->insts_size = 0;

	for (const mc_inst_t * inst = insts, *inst_end = inst + insts_size; inst != inst_end; ++inst) {
		mc_frag_push_inst(frag, inst);
	}
}

static void print_ec_buf() {
	ul_ec_prntr_t prntr;

	ul_ec_prntr_init_dflt(&prntr);

	ul_ec_prntr_t * prntrs[] = { &prntr };

	ul_ec_buf_print(&ec_buf, _countof(prntrs), prntrs);

	ul_ec_prntr_cleanup(&prntr);
}
static int main_core() {
	ul_hst_init(&hst);

	ul_ec_buf_init(&ec_buf);

	ul_ec_fmtr_init(&ec_fmtr, &ec_buf.ec);

	mc_pea_init(&pea, &hst, &ec_fmtr);

	pea.frag = mc_frag_create(McFrag_Count);

	copy_frag_insts(_countof(insts), insts, pea.frag);

	mc_it_add_sym(&pea.it, UL_HST_HASHADD_WS(&hst, L"библиотека.длл"), UL_HST_HASHADD_WS(&hst, L"функция"), UL_HST_HASHADD_WS(&hst, L"имя_компоновки"));

	bool res = mc_pea_b_build(&pea, &pel);

	print_ec_buf();

	if (res) {
		return -1;
	}

	return 0;
}
int main() {
	setlocale(LC_ALL, ".utf8");

	int res = main_core();

	mc_pea_cleanup(&pea);

	lnk_pel_cleanup(&pel);

	ul_ec_fmtr_cleanup(&ec_fmtr);

	ul_ec_buf_cleanup(&ec_buf);

	ul_hst_cleanup(&hst);

	if (_CrtDumpMemoryLeaks() == TRUE) {
		wprintf(L"detected leaks\n");
		return -2;
	}

	return res;
}
