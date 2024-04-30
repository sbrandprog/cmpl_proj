#include "pch.h"
#include "wa_lib/wa_ctx.h"
#include "wa_lib/wa_wnd.h"
#include "wa_lib/wa_host.h"
#include "wa_lib/wa_wcr.h"
#include "gia_tus.h"
#include "gia_pkg.h"
#include "gia_repo.h"
#include "gia_bs.h"
#include "gia_edit.h"

static const wchar_t * exe_name = L"test.exe";

static ul_hst_t main_hst;
static wa_ctx_t main_wa_ctx;
static wa_wcr_t main_wcr;
static gia_repo_t main_repo;

static bool main_fill_repo() {
	gia_repo_init(&main_repo);

	main_repo.root = gia_pkg_create(NULL);

	gia_pkg_t * pkg_pla_lib = gia_pkg_get_sub_pkg(main_repo.root, UL_HST_HASHADD_WS(&main_repo.hst, L"pla_lib"));

	gia_pkg_t * pkg_w64 = gia_pkg_get_sub_pkg(pkg_pla_lib, UL_HST_HASHADD_WS(&main_repo.hst, L"w64"));

	gia_tus_t * tus_kernel32 = gia_pkg_get_tus(pkg_w64, UL_HST_HASHADD_WS(&main_repo.hst, L"kernel32"));

	if (!gia_tus_read_file_nl(tus_kernel32, L"pla_lib/w64/kernel32.pla")) {
		return false;
	}

	gia_pkg_t * pkg_std = gia_pkg_get_sub_pkg(pkg_pla_lib, UL_HST_HASHADD_WS(&main_repo.hst, L"std"));

	gia_tus_t * tus_io = gia_pkg_get_tus(pkg_std, UL_HST_HASHADD_WS(&main_repo.hst, L"io"));

	if (!gia_tus_read_file_nl(tus_io, L"pla_lib/std/io.pla")) {
		return false;
	}

	gia_tus_t * tus_test = gia_pkg_get_tus(main_repo.root, UL_HST_HASHADD_WS(&main_repo.hst, L"test"));

	if (!gia_tus_read_file_nl(tus_test, L"test.pla")) {
		return false;
	}

	return true;
}
static bool main_gui() {
	if (!wa_ctx_init(&main_wa_ctx, &main_hst)) {
		return false;
	}

	if (!wa_ctx_init_buf_paint(&main_wa_ctx)) {
		return false;
	}

	if (!wa_wcr_init(&main_wcr, &main_wa_ctx)) {
		return false;
	}

	if (!wa_wcr_register_p(&main_wcr, gia_edit_get_wnd_cls_desc)) {
		return false;
	}

	ul_hs_t * main_wnd_name = UL_HST_HASHADD_WS(&main_hst, L"main window");

	HWND main_wnd = wa_wnd_create(&main_wa_ctx, L"wa_host", NULL, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, WS_EX_COMPOSITED, L"size", wa_host_dflt_size, L"text", main_wnd_name, NULL);

	if (main_wnd == NULL) {
		return false;
	}

	gia_tus_t * edit_tus = gia_pkg_get_tus(main_repo.root, UL_HST_HASHADD_WS(&main_repo.hst, L"test"));

	if (edit_tus == NULL) {
		return false;
	}

	HWND pla_edit = wa_wnd_create(&main_wa_ctx, L"gia_edit", main_wnd, WS_CHILD | WS_VISIBLE, WS_EX_COMPOSITED, gia_edit_prop_exe_name, exe_name, gia_edit_prop_repo, &main_repo, gia_edit_prop_tus, edit_tus, NULL);

	if (pla_edit == NULL) {
		return false;
	}

	wa_ctx_use_show_cmd(&main_wa_ctx, main_wnd);

	wa_ctx_run_msg_loop(&main_wa_ctx);

	wa_ctx_cleanup_buf_paint(&main_wa_ctx);

	return true;
}

static int main_core() {
	ul_hst_init(&main_hst);

	if (!main_fill_repo()) {
		return false;
	}

	if (!main_gui()) {
		return -1;
	}

	return 0;
}

int main() {
	int res;
	
	__try {
		res = main_core();
	}
	__finally {
		wa_wcr_cleanup(&main_wcr);

		wa_ctx_cleanup(&main_wa_ctx);

		gia_repo_cleanup(&main_repo);

		ul_hst_cleanup(&main_hst);
	}

	_CrtDumpMemoryLeaks();

	return res;
}
