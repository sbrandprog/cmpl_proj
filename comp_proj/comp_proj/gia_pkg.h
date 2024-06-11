#pragma once
#include "gia.h"

struct gia_pkg {
	ul_hs_t * name;

	gia_pkg_t * next;

	gia_pkg_t * sub_pkg;

	gia_tus_t * tus;
};

gia_pkg_t * gia_pkg_create(ul_hs_t * name);
void gia_pkg_destroy(gia_pkg_t * pkg);
void gia_pkg_destroy_chain(gia_pkg_t * pkg);

gia_pkg_t * gia_pkg_get_sub_pkg(gia_pkg_t * pkg, ul_hs_t * sub_pkg_name);
gia_tus_t * gia_pkg_get_tus(gia_pkg_t * pkg, ul_hs_t * tus_name);

bool gia_pkg_fill_from_dir(gia_pkg_t * pkg, ul_hst_t * hst, const wchar_t * dir_path);
bool gia_pkg_fill_from_list(gia_pkg_t * pkg, ul_hst_t * hst, ...);
