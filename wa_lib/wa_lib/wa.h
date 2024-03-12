#pragma once

#if defined WA_BUILD_DLL
#	define WA_SYMBOL __declspec(dllexport)
#else
#	define WA_SYMBOL __declspec(dllimport)
#endif

typedef enum wa_style_col_type wa_style_col_type_t;
typedef struct wa_style_col wa_style_col_t;
typedef enum wa_style_font_type wa_style_font_type_t;
typedef struct wa_style_font wa_style_font_t;
typedef struct wa_style wa_style_t;

typedef struct wa_ctx wa_ctx_t;

typedef struct wa_wnd_sp wa_wnd_sp_t;
typedef struct wa_wnd_cd wa_wnd_cd_t;

typedef LONG wa_ctl_get_w_proc_t(void * ctl_ptr, RECT * parent_rect);
typedef LONG wa_ctl_get_h_proc_t(void * ctl_ptr, RECT * parent_rect);
typedef struct wa_ctl_data wa_ctl_data_t;

typedef WNDCLASSEXW wa_wcr_wnd_cls_desc_proc_t();
typedef enum wa_wcr_wnd_type wa_wcr_wnd_type_t;
typedef struct wa_wcr wa_wcr_t;
