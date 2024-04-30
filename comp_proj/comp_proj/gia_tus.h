#pragma once
#include "gia.h"

struct gia_tus {
	ul_hs_t * name;
	
	gia_tus_t * next;
	
	CRITICAL_SECTION lock;

	size_t src_cap;
	wchar_t * src;
	size_t src_size;
};

gia_tus_t * gia_tus_create(ul_hs_t * name);
void gia_tus_destroy(gia_tus_t * tus);
void gia_tus_destroy_chain(gia_tus_t * tus);

void gia_tus_insert_str_nl(gia_tus_t * tus, size_t ins_pos, size_t str_size, wchar_t * str);

bool gia_tus_read_file_nl(gia_tus_t * tus, const wchar_t * file_name);
