#include "pch.h"
#include "gia_tus.h"

#define READ_BUF_SIZE 256

gia_tus_t * gia_tus_create(ul_hs_t * name) {
	gia_tus_t * tus = malloc(sizeof(*tus));

	ul_assert(tus != NULL);

	*tus = (gia_tus_t){ .name = name };

	InitializeCriticalSection(&tus->lock);

	return tus;
}
void gia_tus_destroy(gia_tus_t * tus) {
	if (tus == NULL) {
		return;
	}

	DeleteCriticalSection(&tus->lock);

	free(tus->src);

	free(tus);
}

void gia_tus_insert_str_nl(gia_tus_t * tus, size_t ins_pos, size_t str_size, wchar_t * str) {
	ul_assert(ins_pos <= tus->src_size);

	if (tus->src_size + str_size > tus->src_cap) {
		ul_arr_grow(&tus->src_cap, &tus->src, sizeof(*tus->src), str_size);
	}

	wmemmove_s(tus->src + ins_pos + str_size, tus->src_cap - ins_pos, tus->src + ins_pos, tus->src_size - ins_pos);
	wmemcpy_s(tus->src + ins_pos, tus->src_cap - ins_pos, str, str_size);
	tus->src_size += str_size;
}

bool gia_tus_read_file_nl(gia_tus_t * tus, const wchar_t * file_name) {
	FILE * file;

	_wfopen_s(&file, file_name, L"r");

	if (file == NULL) {
		return false;
	}

	tus->src_size = 0;

	wchar_t buf[READ_BUF_SIZE];

	while (fgetws(buf, _countof(buf), file) != NULL) {
		gia_tus_insert_str_nl(tus, tus->src_size, wcslen(buf), buf);
	}

	fclose(file);

	return true;
}
