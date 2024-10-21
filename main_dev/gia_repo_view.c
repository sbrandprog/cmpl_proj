#include "gia_repo_view.h"

typedef struct gia_repo_view_tus tus_t;
struct gia_repo_view_tus {
	tus_t * next;

	ul_hs_t * name;
};
typedef struct gia_repo_view_pkg pkg_t;
struct gia_repo_view_pkg {
	pkg_t * next;

	ul_hs_t * name;

	pkg_t * sub_pkg;
	tus_t * tus;
};

typedef struct gia_repo_view_data {
	HWND hw;
	wa_ctx_t * ctx;
	wa_ctl_data_t ctl_data;

	pkg_t * root;

	pla_repo_lsnr_t repo_lsnr;
} wnd_data_t;

typedef struct gia_repo_view_disp_ctx {
	wnd_data_t * data;
	HDC hdc;
	RECT * rect;

	size_t f_w;
	size_t f_h;

	size_t pos_x;
	size_t pos_y;
} disp_ctx_t;

static void destroy_tus_chain(tus_t * tus);
static void destroy_pkg_chain(pkg_t * pkg);

static tus_t * create_tus(ul_hs_t * name) {
	tus_t * tus = malloc(sizeof(*tus));

	ul_assert(tus != NULL);

	*tus = (tus_t){ .name = name };

	return tus;
}
static void destroy_tus(tus_t * tus) {
	if (tus == NULL) {
		return;
	}

	free(tus);
}
static void destroy_tus_chain(tus_t * tus) {
	while (tus != NULL) {
		tus_t * next = tus->next;

		destroy_tus(tus);

		tus = next;
	}
}

static pkg_t * create_pkg(ul_hs_t * name) {
	pkg_t * pkg = malloc(sizeof(*pkg));

	ul_assert(pkg != NULL);

	*pkg = (pkg_t){ .name = name };

	return pkg;
}
static void destroy_pkg(pkg_t * pkg) {
	if (pkg == NULL) {
		return;
	}

	destroy_pkg_chain(pkg->sub_pkg);
	destroy_tus_chain(pkg->tus);

	free(pkg);
}
static void destroy_pkg_chain(pkg_t * pkg) {
	while (pkg != NULL) {
		pkg_t * next = pkg->next;

		destroy_pkg(pkg);

		pkg = next;
	}
}

static void display_pkg(disp_ctx_t * ctx, pkg_t * pkg) {
	if (pkg != ctx->data->root) {
		TextOutW(ctx->hdc, (int)(ctx->f_w * ctx->pos_x), (int)(ctx->f_h * ctx->pos_y), pkg->name->str, (int)pkg->name->size);

		++ctx->pos_y;
		++ctx->pos_x;
	}

	for (pkg_t * sub_pkg = pkg->sub_pkg; sub_pkg != NULL; sub_pkg = sub_pkg->next) {
		display_pkg(ctx, sub_pkg);
	}

	for (tus_t * tus = pkg->tus; tus != NULL; tus = tus->next) {
		TextOutW(ctx->hdc, (int)(ctx->f_w * ctx->pos_x), (int)(ctx->f_h * ctx->pos_y), tus->name->str, (int)tus->name->size);

		++ctx->pos_y;
	}

	if (pkg != ctx->data->root) {
		--ctx->pos_x;
	}
}
static void redraw_repo(wnd_data_t * data, HDC hdc, RECT * rect) {
	disp_ctx_t ctx = { .data = data, .hdc = hdc, .rect = rect };

	wa_style_font_t * font = &data->ctx->style.fonts[WaStyleFontFxd];

	SelectFont(hdc, font->hf);

	ctx.f_w = font->f_w;
	ctx.f_h = font->f_h;

	display_pkg(&ctx, data->root);
}
static void redraw(void * user_data, HDC hdc, RECT * rect) {
	wnd_data_t * data = user_data;

	FillRect(hdc, rect, data->ctx->style.cols[WaStyleColBg].hb);

	if (data->root != NULL) {
		redraw_repo(data, hdc, rect);
	}
}

static void copy_repo(wnd_data_t * data, pla_pkg_t * pkg, pkg_t ** out) {
	*out = create_pkg(pkg->name);

	pkg_t ** sub_pkg_ins = &(*out)->sub_pkg;

	for (pla_pkg_t * sub_pkg = pkg->sub_pkg; sub_pkg != NULL; sub_pkg = sub_pkg->next, sub_pkg_ins = &(*sub_pkg_ins)->next) {
		copy_repo(data, sub_pkg, sub_pkg_ins);
	}

	tus_t ** tus_ins = &(*out)->tus;

	for (pla_tus_t * tus = pkg->tus; tus != NULL; tus = tus->next, tus_ins = &(*tus_ins)->next) {
		*tus_ins = create_tus(tus->name);
	}
}
static void update_repo_view_proc_nl(void * user_data, pla_repo_t * repo) {
	wnd_data_t * data = user_data;

	destroy_pkg(data->root);

	copy_repo(data, repo->root, &data->root);
}

static LRESULT wnd_proc_data(wnd_data_t * data, UINT msg, WPARAM wp, LPARAM lp) {
	HWND hw = data->hw;
	wa_ctx_t * ctx = data->ctx;

	switch (msg) {
		case WM_PAINT:
		{
			PAINTSTRUCT ps;

			HDC hdc = BeginPaint(hw, &ps);

			wa_wnd_paint_buf(hw, hdc, data, redraw);

			EndPaint(hw, &ps);

			return 0;
		}
		case WM_ERASEBKGND:
			return TRUE;
	}

	return DefWindowProcW(hw, msg, wp, lp);
}
static LRESULT wnd_proc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
	wnd_data_t * data = wa_wnd_get_fp(hw);

	switch (msg) {
		case WM_NCCREATE:
			if (data == NULL) {
				wa_wnd_cd_t * cd = wa_wnd_get_cd(lp);

				data = malloc(sizeof(*data));

				if (data == NULL) {
					return FALSE;
				}

				*data = (wnd_data_t){ .hw = hw, .ctx = cd->ctx };

				wa_wnd_set_fp(hw, data);

				wa_ctl_data_init(&data->ctl_data, data);

				if (!wa_ctl_set_data(hw, &data->ctl_data)) {
					return FALSE;
				}

				pla_repo_lsnr_init(&data->repo_lsnr, data, update_repo_view_proc_nl, data->ctx->es_ctx);
			}
			break;
		case WM_NCDESTROY:
			if (data != NULL) {
				pla_repo_lsnr_cleanup(&data->repo_lsnr);

				destroy_pkg(data->root);

				wa_ctl_remove_data(hw);

				wa_ctl_data_cleanup(&data->ctl_data);

				free(data);

				data = NULL;

				wa_wnd_set_fp(hw, data);
			}
			break;
		default:
			if (data != NULL) {
				return wnd_proc_data(data, msg, wp, lp);
			}
			break;
	}
	
	return DefWindowProcW(hw, msg, wp, lp);
}

WNDCLASSEX gia_repo_view_get_wnd_cls_desc() {
	WNDCLASSEX wnd_cls_desc = {
		.style = CS_VREDRAW | CS_HREDRAW | CS_PARENTDC,
		.lpfnWndProc = wnd_proc,
		.cbWndExtra = sizeof(wnd_data_t *),
		.lpszClassName = GIA_REPO_VIEW_WND_CLS_NAME
	};

	return wnd_cls_desc;
}

pla_repo_lsnr_t * gia_repo_view_get_lsnr(HWND hw) {
	ul_assert(wa_wnd_check_cls(hw, GIA_REPO_VIEW_WND_CLS_NAME));

	wnd_data_t * data = wa_wnd_get_fp(hw);

	return &data->repo_lsnr;
}
