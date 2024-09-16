#pragma once
#include "cmpl_lib/cmpl_lib.h"

#define FILE_NAME_BUF_SIZE 64

#define EXE_NAME L"test.exe"
#define EXE_CMD EXE_NAME

static const wchar_t * file_name = NULL;

static ul_hst_t hst;
static ul_es_ctx_t es_ctx;
static pla_repo_t repo;
static ul_ec_buf_t ec_buf;
static ul_ec_fmtr_t ec_fmtr;

static ul_hs_t * make_hs_from_char(char * nt_str) {
	wchar_t cvt_buf[FILE_NAME_BUF_SIZE];

	wchar_t * ins = cvt_buf, *ins_end = ins + _countof(cvt_buf);
	for (char * cur = nt_str; *cur != 0 && ins != ins_end;) {
		*ins++ = (wchar_t)*cur++;
	}

	if (ins == ins_end) {
		wprintf(L"file name exceeded internal buffer size\n");
		return NULL;
	}

	*ins++ = 0;

	return ul_hst_hashadd(&hst, ins - cvt_buf, cvt_buf);
}

static int main_core(int argc, char * argv[]) {
	if (argc != 2) {
		wprintf(L"expected 1 file name argument\n");
		return -1;
	}

	ul_hst_init(&hst);

	ul_hs_t * file_name = make_hs_from_char(argv[1]);

	if (file_name == NULL) {
		wprintf(L"failed to convert file name to hashed string\n");
		return -1;
	}

	ul_es_init_ctx(&es_ctx);

	pla_repo_init(&repo, &hst, &es_ctx);

	repo.root = pla_pkg_create(NULL);

	ul_ec_buf_init(&ec_buf);

	ul_ec_fmtr_init(&ec_fmtr, &ec_buf.ec);

	if (!pla_pkg_fill_from_list(repo.root, &hst, L"pla_lib", file_name->str, NULL)) {
		wprintf(L"failed to fill repo\n");
		return -1;
	}

	ul_hs_t * first_tus_name;

	{
		wchar_t * file_name_ext = wcsrchr(file_name->str, L'.');

		first_tus_name = file_name_ext == NULL ? file_name : ul_hst_hashadd(&hst, file_name_ext - file_name->str, file_name->str);
	}

	if (!pla_bs_build_nl(&repo, first_tus_name, EXE_NAME, &pla_bs_dflt_sett)) {
		wprintf(L"failed to build executable\n");
		return -1;
	}

	{
		int res = (int)_wspawnl(_P_WAIT, EXE_NAME, EXE_CMD, NULL);

		if (res != 0) {
			wprintf(L"unexpected executable result (codes: %d 0x%X)\n", res, res);
			return -1;
		}
	}

	return 0;
}
int main(int argc, char * argv[]) {
	int res = main_core(argc, argv);

	ul_ec_fmtr_cleanup(&ec_fmtr);

	ul_ec_buf_cleanup(&ec_buf);

	pla_repo_cleanup(&repo);

	ul_es_cleanup_ctx(&es_ctx);

	ul_hst_cleanup(&hst);

	if (_CrtDumpMemoryLeaks() == TRUE) {
		wprintf(L"detected leaks\n");
		return -2;
	}

	return res;
}
