#include "pch.h"
#include "lnk_pel_l.h"
#include "asm_pea_b.h"
#include "ira_pec_c.h"
#include "pla_ec_buf.h"
#include "pla_ec_fmtr.h"
#include "pla_lex.h"
#include "pla_tu.h"
#include "pla_prsr.h"
#include "pla_tltr.h"
#include "gia_tus.h"
#include "gia_pkg.h"
#include "gia_repo.h"
#include "gia_bs_lpt.h"

#define EDGE_LINES_SIZE 1
#define TAB_SIZE 4
#define PRINT_ERR_INDENT TAB_SIZE

typedef struct gia_bs_ctx {
	gia_repo_t * repo;
	ul_hs_t * first_tus_name;
	const wchar_t * file_name;

	pla_ec_buf_t pla_ec_buf;
	pla_ec_fmtr_t pla_ec_fmtr;
	ira_pec_t ira_pec;
	asm_pea_t asm_pea;
	lnk_pel_t lnk_pe;
} ctx_t;

static wchar_t get_group_letter(size_t group) {
	switch (group) {
		case PLA_LEX_EC_GROUP:
			return L'L';
		case PLA_PRSR_EC_GROUP:
			return L'P';
		case PLA_TLTR_EC_GROUP:
			return L'T';
		default:
			return L'.';
	}
}
static gia_tus_t * get_tus_from_src_name(ctx_t * ctx, ul_hs_t * src_name) {
	gia_pkg_t * pkg = ctx->repo->root;

	wchar_t * name = src_name->str, * name_end = name + src_name->size;

	while (true) {
		wchar_t * delim = wmemchr(name, PLA_TU_NAME_DELIM, name_end - name);

		if (delim == NULL) {
			break;
		}

		gia_pkg_t * sub_pkg = gia_pkg_find_sub_pkg(pkg, ul_hst_hashadd(&ctx->repo->hst, delim - name, name));

		if (sub_pkg == NULL) {
			return NULL;
		}

		name = delim + 1;
		pkg = sub_pkg;
	}

	return gia_pkg_find_tus(pkg, ul_hst_hashadd(&ctx->repo->hst, name_end - name, name));
}
static size_t get_number_size_ch(size_t num) {
	wchar_t buf[UL_SIZE_T_NUM_SIZE + 1];

	int res = swprintf(buf, _countof(buf), L"%zi", num);

	ul_assert(res >= 0);

	return (size_t)res;
}
static size_t convert_ch_to_col(size_t ch, wchar_t * str, wchar_t * str_end) {
	ul_assert(str + ch <= str_end);

	size_t col = 0;

	for (wchar_t * cur = str, * cur_end = str + ch; cur != cur_end; ++cur) {
		if (*cur != L'\t') {
			++col;
		}
		else {
			col += TAB_SIZE - col % TAB_SIZE;
		}
	}

	return col;
}
static void putnwc(wchar_t ch, size_t size, FILE * file) {
	wchar_t buf[16];

	wmemset(buf, ch, _countof(buf));

	size_t div = size / _countof(buf), mod = size % _countof(buf);

	while (div > 0) {
		--div;

		wprintf(L"%.*s", (int)_countof(buf), buf);
	}

	wprintf(L"%.*s", (int)mod, buf);
}
static void print_tus_err_data(ctx_t * ctx, gia_tus_t * tus, pla_ec_err_t * err) {
	wchar_t * src = tus->src, * src_end = src + tus->src_size;
	
	size_t line_i = 0;

	for (size_t line_i_end = max(err->pos_start.line_num, EDGE_LINES_SIZE) - EDGE_LINES_SIZE; line_i < line_i_end; ++line_i) {
		wchar_t * src_nl = wmemchr(src, L'\n', src_end - src);

		if (src_nl == NULL) {
			putnwc(L' ', PRINT_ERR_INDENT, stdout);
			wprintf(L"/ invalid error positions /\n");
			return;
		}

		src = src_nl + 1;
	}

	size_t line_i_end = err->pos_end.line_num + EDGE_LINES_SIZE * 2;
	size_t line_num_size = get_number_size_ch(line_i_end) + 1;

	for (; line_i < line_i_end; ++line_i) {
		if (src == src_end) {
			break;
		}
		
		wchar_t * src_nl = wmemchr(src, L'\n', src_end - src);

		if (src_nl == NULL) {
			src_nl = src_end;
		}

		size_t line_len = 0;

		{
			putnwc(L' ', PRINT_ERR_INDENT, stdout);
			wprintf(L"%-*zi", (int)line_num_size, line_i + 1);

			for (wchar_t * cur = src; cur != src_nl; ++cur) {
				if (*cur != L'\t') {
					putwc(*cur, stdout);
					++line_len;
				}
				else {
					size_t spaces = TAB_SIZE - line_len % TAB_SIZE;

					putnwc(L' ', spaces, stdout);
					line_len += spaces;
				}
			}
			
			fputwc(L'\n', stdout);
		}

		if (err->pos_start.line_num <= line_i && line_i <= err->pos_end.line_num) {
			size_t hl_start = line_i == err->pos_start.line_num ? convert_ch_to_col(err->pos_start.line_ch, src, src_nl) : 0;
			size_t hl_end = line_i == err->pos_end.line_num ? convert_ch_to_col(err->pos_end.line_ch, src, src_nl) : line_len;

			putnwc(L' ', PRINT_ERR_INDENT + line_num_size + hl_start, stdout);
			putnwc(L'^', hl_end - hl_start, stdout);
			fputwc(L'\n', stdout);
		}

		src = src_nl + 1;
	}
}
static void print_ec_buf(ctx_t * ctx) {
	for (pla_ec_err_t * err = ctx->pla_ec_buf.errs, *err_end = err + ctx->pla_ec_buf.errs_size; err != err_end; ++err) {
		wchar_t group_letter = get_group_letter(err->group);

		wchar_t * src_name_str = L"/ no source name /";
		gia_tus_t * tus = NULL;

		if (err->src_name != NULL) {
			src_name_str = err->src_name->str;

			tus = get_tus_from_src_name(ctx, err->src_name);
		}

		wprintf(L"%c %s\n", group_letter, src_name_str);

		if (tus != NULL) {
			print_tus_err_data(ctx, tus, err);
		}
		else {
			putnwc(L' ', PRINT_ERR_INDENT, stdout);
			wprintf(L"/ no tus /\n");
		}

		putnwc(L' ', PRINT_ERR_INDENT, stdout);
		wprintf(L"%s\n", err->msg->str);
	}
}

static bool build_core(ctx_t * ctx) {
	pla_ec_buf_init(&ctx->pla_ec_buf);

	pla_ec_fmtr_init(&ctx->pla_ec_fmtr, &ctx->pla_ec_buf.ec, &ctx->repo->hst);
	
	if (!gia_bs_lpt_form_pec_nl(ctx->repo, &ctx->pla_ec_fmtr, ctx->first_tus_name, &ctx->ira_pec)
		|| ctx->pla_ec_buf.errs_size > 0) {
		print_ec_buf(ctx);
		return false;
	}

	if (!ira_pec_c_compile(&ctx->ira_pec, &ctx->asm_pea)) {
		return false;
	}

	if (!asm_pea_b_build(&ctx->asm_pea, &ctx->lnk_pe)) {
		return false;
	}

	ctx->lnk_pe.file_name = ctx->file_name;

	if (!lnk_pel_l_link(&ctx->lnk_pe)) {
		return false;
	}

	return true;
}
bool gia_bs_build_nl(gia_repo_t * repo, ul_hs_t * first_tus_name, const wchar_t * file_name) {
	ctx_t ctx = { .repo = repo, .first_tus_name = first_tus_name, .file_name = file_name };

	bool res = build_core(&ctx);

	lnk_pel_cleanup(&ctx.lnk_pe);

	asm_pea_cleanup(&ctx.asm_pea);

	ira_pec_cleanup(&ctx.ira_pec);

	pla_ec_fmtr_cleanup(&ctx.pla_ec_fmtr);

	pla_ec_buf_cleanup(&ctx.pla_ec_buf);

	return res;
}
