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
