#include "pla_tus.h"
#include "pla_pkg.h"
#include "pla_repo.h"
#include "pla_ec_prntr.h"
#include "pla_lex.h"
#include "pla_prsr.h"
#include "pla_tu.h"
#include "pla_tltr.h"

#define EDGE_LINES_SIZE 1
#define TAB_SIZE 4
#define PRINT_ERR_INDENT TAB_SIZE

static wchar_t get_mod_letter(const wchar_t * mod_name) {
	if (wcscmp(mod_name, PLA_LEX_MOD_NAME) == 0) {
		return L'L';
	}
	else if (wcscmp(mod_name, PLA_PRSR_MOD_NAME) == 0) {
		return L'P';
	}
	else if (wcscmp(mod_name, PLA_TLTR_MOD_NAME) == 0) {
		return L'T';
	}
	else {
		return L'.';
	}
}
static pla_tus_t * get_tus_from_src_name(pla_repo_t * repo, const wchar_t * src_name) {
	if (src_name == NULL) {
		return NULL;
	}

	pla_pkg_t * pkg = repo->root;

	const wchar_t * name = src_name, * name_end = name + wcslen(src_name);

	while (true) {
		wchar_t * delim = wmemchr(name, PLA_TU_NAME_DELIM, name_end - name);

		if (delim == NULL) {
			break;
		}

		pla_pkg_t * sub_pkg = pla_pkg_find_sub_pkg(pkg, ul_hst_hashadd(repo->hst, delim - name, name));

		if (sub_pkg == NULL) {
			return NULL;
		}

		name = delim + 1;
		pkg = sub_pkg;
	}

	return pla_pkg_find_tus(pkg, ul_hst_hashadd(repo->hst, name_end - name, name));
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

	for (wchar_t * cur = str, *cur_end = str + ch; cur != cur_end; ++cur) {
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
static void print_tus_err_data(pla_tus_t * tus, pla_ec_err_t * err) {
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
static bool print_proc(pla_ec_prntr_t * prntr, ul_ec_rec_t * rec) {
	pla_ec_err_t err;

	if (!pla_ec_scan(rec, &err)) {
		return false;
	}

	wchar_t group_letter = get_mod_letter(err.mod_name);

	const wchar_t * src_name_str = err.src_name != NULL ? err.src_name : L"/ no source name /";

	wprintf(L"%c %s\n", group_letter, src_name_str);

	pla_tus_t * tus = get_tus_from_src_name(prntr->repo, err.src_name);

	if (tus != NULL) {
		print_tus_err_data(tus, &err);
	}
	else {
		putnwc(L' ', PRINT_ERR_INDENT, stdout);
		wprintf(L"/ no tus /\n");
	}

	putnwc(L' ', PRINT_ERR_INDENT, stdout);
	wprintf(L"%s\n", err.msg);

	return true;
}

void pla_ec_prntr_init(pla_ec_prntr_t * prntr, pla_repo_t * repo) {
	*prntr = (pla_ec_prntr_t){ .repo = repo };

	ul_ec_prntr_init(&prntr->prntr, prntr, print_proc);
}
void pla_ec_prntr_cleanup(pla_ec_prntr_t * prntr) {
	ul_ec_prntr_cleanup(&prntr->prntr);

	memset(prntr, 0, sizeof(*prntr));
}
