#include "pch.h"
#include "wa_lib/wa_ctx.h"
#include "wa_lib/wa_wnd.h"
#include "wa_lib/wa_host.h"
#include "wa_lib/wa_wcr.h"
#include "gia_tus.h"
#include "gia_pkg.h"
#include "gia_repo_view.h"
#include "gia_bs.h"
#include "gia_edit.h"

static const wchar_t * exe_name = L"test.exe";

static ul_es_ctx_t main_es_ctx;
static ul_hst_t main_hst;
static wa_ctx_t main_wa_ctx;
static wa_wcr_t main_wcr;
static gia_repo_t main_repo;

static bool main_fill_repo_nl() {
	main_repo.root = gia_pkg_create(NULL);

	if (!gia_pkg_fill_from_list(main_repo.root, &main_repo.hst, L"pla_lib", L"test.pla", NULL)) {
		return false;
	}

	return true;
}
static bool main_fill_repo() {
	gia_repo_init(&main_repo, &main_es_ctx);

	EnterCriticalSection(&main_repo.lock);

	bool res;

	__try {
		res = main_fill_repo_nl();
	}
	__finally {
		LeaveCriticalSection(&main_repo.lock);
	}

	if (!res) {
		return false;
	}

	gia_repo_broadcast_upd(&main_repo);
	
	return true;
}
static bool main_gui_register_clss() {
	if (!wa_wcr_init(&main_wcr, &main_wa_ctx)) {
		return false;
	}

	static wa_wcr_wnd_cls_desc_proc_t * const procs[] = {
		gia_repo_view_get_wnd_cls_desc,
		gia_edit_get_wnd_cls_desc
	};

	for (wa_wcr_wnd_cls_desc_proc_t * const * it = procs, * const * it_end = it + _countof(procs); it != it_end; ++it) {
		if (!wa_wcr_register_p(&main_wcr, *it)) {
			return false;
		}
	}

	return true;
}
static bool main_gui() {
	if (!wa_ctx_init(&main_wa_ctx, &main_es_ctx, &main_hst)) {
		return false;
	}

	if (!wa_ctx_init_buf_paint(&main_wa_ctx)) {
		return false;
	}

	if (!main_gui_register_clss()) {
		return false;
	}

	ul_hs_t * main_wnd_name = UL_HST_HASHADD_WS(&main_hst, L"main window");

	HWND main_wnd = wa_wnd_create(&main_wa_ctx, L"wa_host", NULL, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, WS_EX_COMPOSITED, L"size", wa_host_dflt_size, L"text", main_wnd_name, NULL);

	if (main_wnd == NULL) {
		return false;
	}

	/*
	HWND splt0 = wa_wnd_create(&main_wa_ctx, L"wa_splt", main_wnd, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_COMPOSITED, NULL);

	if (splt0 == NULL) {
		return false;
	}

	HWND repo_view = wa_wnd_create(&main_wa_ctx, L"gia_repo_view", splt0, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_COMPOSITED, NULL);

	if (repo_view == NULL) {
		return false;
	}

	gia_repo_attach_lsnr(&main_repo, gia_repo_view_get_lsnr(repo_view));
	*/

	gia_tus_t * edit_tus = gia_pkg_get_tus(main_repo.root, UL_HST_HASHADD_WS(&main_repo.hst, L"test"));

	if (edit_tus == NULL) {
		return false;
	}

	HWND gia_edit = wa_wnd_create(&main_wa_ctx, L"gia_edit", main_wnd, WS_CHILD | WS_VISIBLE, WS_EX_COMPOSITED, gia_edit_prop_exe_name, exe_name, gia_edit_prop_repo, &main_repo, gia_edit_prop_tus, edit_tus, NULL);

	if (gia_edit == NULL) {
		return false;
	}

	wa_ctx_use_show_cmd(&main_wa_ctx, main_wnd);

	wa_ctx_run_msg_loop(&main_wa_ctx);

	wa_ctx_cleanup_buf_paint(&main_wa_ctx);

	return true;
}

static int main_core() {
	ul_es_init_ctx(&main_es_ctx);

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

		ul_es_cleanup_ctx(&main_es_ctx);
	}

	_CrtDumpMemoryLeaks();

	return res;
}
