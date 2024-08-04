#include "pla_tus.h"

#define READ_BUF_SIZE 256

pla_tus_t * pla_tus_create(ul_hs_t * name) {
	pla_tus_t * tus = malloc(sizeof(*tus));

	ul_assert(tus != NULL);

	*tus = (pla_tus_t){ .name = name };

	InitializeCriticalSection(&tus->lock);

	return tus;
}
void pla_tus_destroy(pla_tus_t * tus) {
	if (tus == NULL) {
		return;
	}

	DeleteCriticalSection(&tus->lock);

	free(tus->src);

	free(tus);
}
void pla_tus_destroy_chain(pla_tus_t * tus) {
	while (tus != NULL) {
		pla_tus_t * next = tus->next;

		pla_tus_destroy(tus);

		tus = next;
	}
}

void pla_tus_insert_str_nl(pla_tus_t * tus, size_t ins_pos, size_t str_size, wchar_t * str) {
	ul_assert(ins_pos <= tus->src_size);

	if (tus->src_size + str_size > tus->src_cap) {
		ul_arr_grow(&tus->src_cap, &tus->src, sizeof(*tus->src), tus->src_size + str_size - tus->src_cap);
	}

	wmemmove_s(tus->src + ins_pos + str_size, tus->src_cap - ins_pos, tus->src + ins_pos, tus->src_size - ins_pos);
	wmemcpy_s(tus->src + ins_pos, tus->src_cap - ins_pos, str, str_size);
	tus->src_size += str_size;
}

bool pla_tus_read_file_nl(pla_tus_t * tus, const wchar_t * file_name) {
	FILE * file;

	_wfopen_s(&file, file_name, L"r");

	if (file == NULL) {
		return false;
	}

	tus->src_size = 0;

	wchar_t buf[READ_BUF_SIZE];

	while (fgetws(buf, _countof(buf), file) != NULL) {
		pla_tus_insert_str_nl(tus, tus->src_size, wcslen(buf), buf);
	}

	fclose(file);

	return true;
}
