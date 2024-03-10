#include "pch.h"
#include "pla_ast_p.h"
#include "pla_ast_t.h"
#include "ira_pec_c.h"
#include "asm_pea_b.h"
#include "lnk_pe_l.h"
#include "u_hst.h"
#include "u_misc.h"

static const wchar_t * pla_name = L"test.pla";
static const wchar_t * exe_name = L"test.exe";
static const wchar_t * exe_cmd = L"test.exe";

static u_hst_t hst;
static lnk_pe_t pe;
static asm_pea_t pea;
static ira_pec_t pec;
static pla_ast_t ast;

static bool main_parse() {
	bool result = pla_ast_p_parse(&hst, pla_name, &ast);

	wprintf(L"parsing result: %s\n", result ? L"success" : L"failure");

	return result;
}
static bool main_translate() {
	bool result = pla_ast_t_translate(&ast, &pec);

	wprintf(L"translation result: %s\n", result ? L"success" : L"failure");

	return result;
}
static bool main_compile() {
	bool result = ira_pec_c_compile(&pec, &pea);

	wprintf(L"compilation result: %s\n", result ? L"success" : L"failure");

	return result;
}
static bool main_assemble() {
	bool result = asm_pea_b_build(&pea, &pe);

	wprintf(L"assembling result: %s\n", result ? L"success" : L"failure");

	return result;
}
static bool main_link() {
	pe.file_name = exe_name;

	bool result = lnk_pe_l_link(&pe);

	wprintf(L"link result: %s\n", result ? L"success" : L"failure");

	return result;
}
static void main_run() {
	int ret_code = (int)_wspawnl(_P_WAIT, exe_name, exe_cmd, NULL);

	wprintf(L"program exit code: %d 0x%X\n", ret_code, ret_code);
}

static void utility_lib_test() {
	ul_hst_t ul_hst;

	ul_hst_init(&ul_hst);

	ul_hs_t * str = UL_HST_HASHADD_WS(&ul_hst, L"utility lib test");

	ul_hst_cleanup(&ul_hst);
}

static int main_core() {
	utility_lib_test();

	if (!main_parse()) {
		return -1;
	}

	if (!main_translate()) {
		return -1;
	}

	if (!main_compile()) {
		return -1;
	}

	if (!main_assemble()) {
		return -1;
	}

	if (!main_link()) {
		return -1;
	}

	main_run();

	return 0;
}

int main() {
	int result = main_core();

	lnk_pe_cleanup(&pe);

	asm_pea_cleanup(&pea);

	ira_pec_cleanup(&pec);

	pla_ast_cleanup(&ast);

	u_hst_cleanup(&hst);

	_CrtDumpMemoryLeaks();

	return result;
}