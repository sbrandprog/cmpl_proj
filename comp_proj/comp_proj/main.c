#include "pch.h"
#include "wa_lib/wa_ctx.h"
#include "wa_lib/wa_wnd.h"
#include "wa_lib/wa_host.h"
#include "wa_lib/wa_wcr.h"
#include "gia_prog.h"
#include "gia_edit.h"

static const ul_ros_t files_to_parse[] = {
	UL_ROS_MAKE(L"test.pla"),
	UL_ROS_MAKE(L"pla_lib/std/io.pla"),
	UL_ROS_MAKE(L"pla_lib/w64/kernel32.pla")
};

static const wchar_t * pla_name = L"test.pla";
static const wchar_t * exe_name = L"test.exe";

static ul_hst_t main_hst;
static wa_ctx_t main_wa_ctx;
static wa_wcr_t main_wcr;
static gia_prog_t main_prog;

static bool main_init_prog() {
	gia_prog_init(&main_prog);

	main_prog.out_file_name = exe_name;

	for (const ul_ros_t * file = files_to_parse, *file_end = file + _countof(files_to_parse); file != file_end; ++file) {
		if (!gia_prog_add_tus_file_nl(&main_prog, file->str)) {
			return false;
		}
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

	gia_tus_t * edit_tus = gia_prog_get_tus_nl(&main_prog, ul_hst_hashadd(&main_prog.hst, wcslen(pla_name), pla_name));

	if (edit_tus == NULL) {
		return false;
	}

	HWND pla_edit = wa_wnd_create(&main_wa_ctx, L"gia_edit", main_wnd, WS_CHILD | WS_VISIBLE, WS_EX_COMPOSITED, gia_edit_prop_exe_name, exe_name, gia_edit_prop_prog, &main_prog, gia_edit_prop_tus, edit_tus, NULL);

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

	if (!main_init_prog()) {
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

		gia_prog_cleanup(&main_prog);

		ul_hst_cleanup(&main_hst);
	}

	_CrtDumpMemoryLeaks();

	return res;
}
